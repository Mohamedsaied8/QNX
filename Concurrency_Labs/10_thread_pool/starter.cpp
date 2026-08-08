// Lab 10 — Thread pool  (STARTER — a skeleton with TODOs)
//
// A thread pool creates a FIXED set of worker threads once, then feeds them
// tasks through a shared queue. This avoids the cost (and unpredictability) of
// creating a thread per job — essential on embedded systems where you size your
// threads at startup.
//
// This skeleton has the structure but three gaps. Until you finish them the
// program will hang (workers never get work). Fill in the TODOs, then compare
// with solution.cpp.
//
//   TODO #1  submit(): push the task under the lock, then notify ONE worker.
//   TODO #2  worker(): wait on the CV until there is a task OR we're stopping;
//            pop one task and run it (release the lock before running!).
//   TODO #3  destructor: set stop_, notify ALL workers, join them.

#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
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
        // TODO #3: signal stop, wake everyone, join all workers.
    }

    // Submit a callable, get a future for its result.
    template <class F>
    auto submit(F f) -> std::future<decltype(f())>
    {
        using R = decltype(f());
        auto task = std::make_shared<std::packaged_task<R()>>(std::move(f));
        std::future<R> fut = task->get_future();
        {
            std::lock_guard<std::mutex> lk(m_);
            // TODO #1: enqueue a job that invokes (*task)().
        }
        // TODO #1 (cont.): notify a worker that a job is available.
        return fut;
    }

private:
    void worker()
    {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lk(m_);
                // TODO #2: wait until !jobs_.empty() || stop_.
                //          if stopping and no jobs left, return.
                //          otherwise pop one job into `job`.
                return;   // placeholder so the skeleton compiles
            }
            job();        // run outside the lock
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

    // Submit 8 jobs that each return their square.
    std::vector<std::future<int>> results;
    for (int i = 1; i <= 8; ++i)
        results.push_back(pool.submit([i] { return i * i; }));

    for (auto& f : results)
        std::cout << f.get() << ' ';
    std::cout << '\n';
    return 0;
}
