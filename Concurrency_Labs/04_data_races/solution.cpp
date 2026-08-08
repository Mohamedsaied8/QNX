// Lab 04 — Data races  (SOLUTION)
//
// Two correct fixes are shown side by side:
//   (1) std::atomic<long>  — the increment becomes one indivisible operation.
//                            Best for a simple counter: lock-free and fast.
//   (2) std::mutex         — serialise the whole read-modify-write.
//                            Necessary when you must protect MORE than one value
//                            or a non-trivial operation as a unit.
// Both reliably produce 2,000,000.

#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>

// --- (1) atomic ----------------------------------------------------------
std::atomic<long> atomic_count{0};

void record_atomic(int n)
{
    for (int i = 0; i < n; ++i)
        ++atomic_count;                 // indivisible: no torn updates
}

// --- (2) mutex -----------------------------------------------------------
long mutex_count = 0;
std::mutex count_mutex;

void record_mutex(int n)
{
    for (int i = 0; i < n; ++i) {
        std::lock_guard<std::mutex> lk(count_mutex);
        ++mutex_count;                  // only one thread in here at a time
    }
}

int main()
{
    constexpr int N = 1'000'000;

    {
        std::thread a(record_atomic, N), b(record_atomic, N);
        a.join(); b.join();
        std::cout << "atomic_count = " << atomic_count.load()
                  << "  (expected " << 2 * N << ")\n";
    }
    {
        std::thread a(record_mutex, N), b(record_mutex, N);
        a.join(); b.join();
        std::cout << "mutex_count  = " << mutex_count
                  << "  (expected " << 2 * N << ")\n";
    }

    // Lesson: for ONE counter, atomic is far cheaper (no blocking). Reach for a
    // mutex when you must keep several variables consistent together. Measure:
    // the atomic loop here is typically several times faster than the mutex one.
    return 0;
}
