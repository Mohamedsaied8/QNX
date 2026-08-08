// bounded_queue.h — a thread-safe, bounded, closeable FIFO.
//
// This is the same component built in Lab 07. It is the seam that decouples the
// pipeline stages: producers block when it is full (backpressure), consumers
// block when it is empty, and close() lets producers signal end-of-stream so
// consumers drain and stop. Reusing it here shows that a small, correct
// concurrency primitive composes into a whole system.
#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

template <class T>
class BoundedQueue
{
public:
    explicit BoundedQueue(std::size_t capacity) : cap_(capacity) {}

    // Blocks while full. Returns false if the queue is closed.
    bool push(T value)
    {
        std::unique_lock<std::mutex> lk(m_);
        not_full_.wait(lk, [this] { return q_.size() < cap_ || closed_; });
        if (closed_) return false;
        q_.push(std::move(value));
        lk.unlock();
        not_empty_.notify_one();
        return true;
    }

    // Blocks while empty. Returns nullopt once the queue is closed AND drained.
    std::optional<T> pop()
    {
        std::unique_lock<std::mutex> lk(m_);
        not_empty_.wait(lk, [this] { return !q_.empty() || closed_; });
        if (q_.empty()) return std::nullopt;
        T value = std::move(q_.front());
        q_.pop();
        lk.unlock();
        not_full_.notify_one();
        return value;
    }

    void close()
    {
        { std::lock_guard<std::mutex> lk(m_); closed_ = true; }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> lk(m_);
        return q_.size();
    }

private:
    mutable std::mutex      m_;
    std::condition_variable not_empty_, not_full_;
    std::queue<T>           q_;
    std::size_t             cap_;
    bool                    closed_ = false;
};
