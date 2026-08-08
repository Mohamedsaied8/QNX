// Lab 07 — Condition variables: a bounded producer/consumer queue  (SOLUTION)
//
// A reusable, thread-safe bounded queue. Consumers SLEEP until data is
// available; producers SLEEP when the queue is full (backpressure). A close()
// method lets producers signal "no more data" so consumers drain and then exit
// cleanly. This BoundedQueue<T> is the same component used by the capstone
// project's pipeline.

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>

template <class T>
class BoundedQueue
{
public:
    explicit BoundedQueue(std::size_t capacity) : cap_(capacity) {}

    // Blocks while full. Returns false if the queue was closed.
    bool push(T value)
    {
        std::unique_lock<std::mutex> lk(m_);
        not_full_.wait(lk, [this] { return q_.size() < cap_ || closed_; });
        if (closed_) return false;
        q_.push(std::move(value));
        lk.unlock();
        not_empty_.notify_one();          // wake one waiting consumer
        return true;
    }

    // Blocks while empty. Returns std::nullopt once the queue is closed AND
    // drained — that is the consumer's signal to stop.
    std::optional<T> pop()
    {
        std::unique_lock<std::mutex> lk(m_);
        not_empty_.wait(lk, [this] { return !q_.empty() || closed_; });
        if (q_.empty()) return std::nullopt;   // closed and drained
        T value = std::move(q_.front());
        q_.pop();
        lk.unlock();
        not_full_.notify_one();           // wake one waiting producer
        return value;
    }

    void close()
    {
        { std::lock_guard<std::mutex> lk(m_); closed_ = true; }
        not_empty_.notify_all();          // wake everyone so they can exit
        not_full_.notify_all();
    }

private:
    std::mutex              m_;
    std::condition_variable not_empty_, not_full_;
    std::queue<T>           q_;
    std::size_t             cap_;
    bool                    closed_ = false;
};

int main()
{
    BoundedQueue<int> queue(4);           // small capacity to exercise backpressure

    std::thread producer([&] {
        for (int reading = 1; reading <= 10; ++reading)
            queue.push(reading);
        queue.close();                    // no more data
    });

    std::thread consumer([&] {
        while (auto v = queue.pop())      // stops when pop() returns nullopt
            std::cout << "logged " << *v << '\n';
    });

    producer.join();
    consumer.join();
    std::cout << "pipeline drained, clean shutdown.\n";
    return 0;
}

// Why the lambda predicate in wait()? It handles SPURIOUS WAKEUPS: a CV may
// return from wait() without a notify. Re-checking the condition in a predicate
// (wait re-evaluates it in a loop) is mandatory, not optional.
