// Lab 10 — Thread pool  (SOLUTION)
//
// A fixed set of worker threads pulls tasks from a shared queue. submit() wraps
// the callable in a packaged_task so the caller gets a future for the result
// (and any exception propagates through it). This ties together every primitive
// from the module: threads, a mutex-protected queue, a condition variable, RAII
// shutdown, and futures. The capstone project's worker stage is this pattern.

#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool
{
public:
    explicit ThreadPool(unsigned n)
    {
        for (unsigned i = 0; i < n; ++i)
            workers_.emplace_back([this] { worker(); });
    }

    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lk(m_);
            stop_ = true;
        }
        cv_.notify_all();                 // wake every worker so it can exit
        for (auto& t : workers_)
            if (t.joinable()) t.join();
    }

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <class F>
    auto submit(F f) -> std::future<decltype(f())>
    {
        using R  = decltype(f());
        auto task = std::make_shared<std::packaged_task<R()>>(std::move(f));
        std::future<R> fut = task->get_future();
        {
            std::lock_guard<std::mutex> lk(m_);
            jobs_.emplace([task] { (*task)(); });   // type-erase into void()
        }
        cv_.notify_one();
        return fut;
    }

private:
    void worker()
    {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lk(m_);
                cv_.wait(lk, [this] { return stop_ || !jobs_.empty(); });
                if (stop_ && jobs_.empty())
                    return;                         // drained -> exit
                job = std::move(jobs_.front());
                jobs_.pop();
            }
            job();                                  // run OUTSIDE the lock
        }
    }

    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> jobs_;
    std::mutex                        m_;
    std::condition_variable           cv_;
    bool                              stop_ = false;
};

int main()
{
    ThreadPool pool(4);

    std::vector<std::future<int>> results;
    for (int i = 1; i <= 8; ++i)
        results.push_back(pool.submit([i] { return i * i; }));

    for (auto& f : results)
        std::cout << f.get() << ' ';                // 1 4 9 16 25 36 49 64
    std::cout << '\n';
    return 0;
}
