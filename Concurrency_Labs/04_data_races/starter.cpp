// Lab 04 — Data races: read-modify-write is NOT atomic  (STARTER, contains a BUG)
//
// Two sensor threads each record 1,000,000 samples by incrementing a shared
// counter. The total SHOULD be 2,000,000. Run it a few times: you'll get a
// different, smaller number every time.
//
// WHY: `++sample_count` is really three machine steps —
//        load  reg, [sample_count]
//        add   reg, 1
//        store [sample_count], reg
// If two threads interleave between load and store, one increment is lost.
// This is a DATA RACE: concurrent access to one location, at least one a write,
// with no synchronization. It is undefined behaviour, not "just" a wrong number.
//
// Confirm it with ThreadSanitizer:
//   g++ -std=c++17 -g -fsanitize=thread -pthread starter.cpp -o t && ./t
//
// TASK: make the final count reliably 2,000,000. See the README for the two
//       idiomatic fixes (mutex vs std::atomic) and compare with solution.cpp.

#include <iostream>
#include <thread>

// `volatile` here is ONLY so the optimizer can't fold the whole loop into a
// single `+= n` — that would hide the race at -O2. volatile forces a real
// load+add+store each iteration, which makes the lost updates reliably visible.
// IMPORTANT: volatile does NOT make this thread-safe (the count is still wrong).
// That is the lesson — see the README.
volatile long sample_count = 0;   // shared, unprotected

void record_samples(int n)
{
    for (int i = 0; i < n; ++i)
        ++sample_count;         // BUG: racy read-modify-write (load, add, store)
}

int main()
{
    constexpr int N = 1'000'000;

    std::thread s1(record_samples, N);
    std::thread s2(record_samples, N);
    s1.join();
    s2.join();

    std::cout << "sample_count = " << sample_count
              << "  (expected " << 2 * N << ")\n";
    return 0;
}
