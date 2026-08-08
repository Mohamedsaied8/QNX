// Lab 09 — Atomics & a spinlock  (SOLUTION)
//
// Part A: a correct spinlock from std::atomic_flag. test_and_set() atomically
//         sets the flag and returns its PREVIOUS value, so exactly one thread
//         can transition false->true and enter the critical section.
// Part B: a std::atomic<bool> stop flag the worker reliably observes.
//
// A spinlock is the right tool only for VERY short critical sections where
// sleeping (a mutex) would cost more than spinning. For anything that may block
// for a while, use a std::mutex — spinning wastes a core.

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

// --- Part A: a correct spinlock -----------------------------------------
class SpinLock
{
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
public:
    void lock()
    {
        // Returns the OLD value. Keep spinning while it was already set.
        while (flag_.test_and_set(std::memory_order_acquire)) { /* spin */ }
    }
    void unlock()
    {
        flag_.clear(std::memory_order_release);
    }
};

SpinLock spin;
long     counter = 0;

void bump(int n)
{
    for (int i = 0; i < n; ++i) {
        spin.lock();
        ++counter;
        spin.unlock();
    }
}

// --- Part B: a stop flag the worker actually sees -----------------------
std::atomic<bool> keep_running{true};

void heartbeat()
{
    long beats = 0;
    while (keep_running.load(std::memory_order_relaxed))
        ++beats;
    std::cout << "heartbeat stopped after " << beats << " iterations\n";
}

int main()
{
    constexpr int N = 100000;
    {
        std::thread a(bump, N), b(bump, N);
        a.join(); b.join();
        std::cout << "counter = " << counter
                  << "  (expected " << 2 * N << ")\n";
    }
    {
        std::thread h(heartbeat);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        keep_running.store(false);        // worker is guaranteed to observe this
        h.join();
    }
    return 0;
}
