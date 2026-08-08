// Lab 07 — Condition variables: producer/consumer  (STARTER, has BUGS)
//
// A sensor thread (producer) pushes readings into a queue; a logger thread
// (consumer) pops and prints them. This version is wrong in three ways:
//
//   BUG #1 (busy-wait): the consumer SPINS on `while (queue.empty())` holding
//          the lock — it burns 100% CPU and may livelock the producer out.
//   BUG #2 (no shutdown): both loops are `while (true)`; the program never ends
//          and join() would block forever.
//   BUG #3 (unbounded): the producer never waits, so the queue can grow without
//          limit (no backpressure).
//
// TASK: rewrite using a std::condition_variable so the consumer SLEEPS until
//       data arrives, the producer waits when the queue is full, and a `done`
//       flag lets both threads exit cleanly so we can join() them.
//       See solution.cpp for a reusable bounded-queue class.

#include <chrono>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

std::mutex      m;
std::queue<int> q;

void producer()
{
    for (int reading = 1; reading <= 10; ++reading) {
        std::lock_guard<std::mutex> lk(m);
        q.push(reading);                       // BUG #3: never blocks if full
    }
    // BUG #2: no way to signal "no more data"
}

void consumer()
{
    while (true) {                             // BUG #2: never exits (no shutdown)
        int  v;
        bool have = false;
        {
            std::lock_guard<std::mutex> lk(m);
            if (!q.empty()) {
                v = q.front();
                q.pop();
                have = true;
            }
        }
        if (have) {
            std::cout << "logged " << v << '\n';
        } else {
            // BUG #1: NAIVE POLLING. When the queue is empty we just sleep a
            // bit and check again. This wastes CPU on pointless wakeups and adds
            // up to 1 ms of latency to every reading. A condition variable would
            // sleep exactly until data arrives and wake immediately. After the
            // 10 readings are consumed this polls forever (BUG #2: no shutdown).
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

int main()
{
    std::thread p(producer);
    std::thread c(consumer);
    p.join();
    c.join();          // would hang forever in this broken version
    return 0;
}
