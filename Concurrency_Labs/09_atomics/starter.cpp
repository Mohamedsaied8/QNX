// Lab 09 — Atomics & a spinlock  (STARTER, contains a BUG)
//
// Part A: a hand-rolled spinlock built on a PLAIN bool. It is broken: testing
//         and setting a non-atomic bool is itself a data race, so two threads
//         can both see `locked == false` and both enter the critical section.
//         The protected counter ends up wrong.
//
// Part B: a `keep_running` flag used to stop a worker. As a plain bool, the
//         compiler may cache it in a register and the worker may never see the
//         change -> it can run forever.
//
// TASK:
//   A) Reimplement SpinLock using std::atomic_flag (test_and_set / clear).
//   B) Make `keep_running` a std::atomic<bool>.
//   Compare with solution.cpp. Verify Part A with ThreadSanitizer.

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

// --- Part A: a BROKEN spinlock ------------------------------------------
class SpinLock
{
    bool locked_ = false;                 // BUG: plain bool, not atomic
public:
    void lock()
    {
        while (locked_) { /* spin */ }    // racy test...
        locked_ = true;                   // ...and racy set: two threads can pass
    }
    void unlock() { locked_ = false; }
};

SpinLock spin;
long     counter = 0;

void bump(int n)
{
    for (int i = 0; i < n; ++i) {
        spin.lock();
        ++counter;                        // should be protected, but isn't
        spin.unlock();
    }
}

int main()
{
    constexpr int N = 100000;
    std::thread a(bump, N), b(bump, N);
    a.join(); b.join();
    std::cout << "counter = " << counter << "  (expected " << 2 * N << ")\n";
    return 0;
}
