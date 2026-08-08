// Lab 02 — Passing arguments to threads  (STARTER, contains a BUG)
//
// An Accumulator object sums sensor readings into its own `total` member. We
// hand it to a std::thread and expect `acc.total` to hold the sum afterwards...
// but it stays 0. The work happened — just not where we can see it.
//
// BUG TO FIND:
//   std::thread COPIES everything you give it — the arguments AND the callable
//   object itself — into thread-local storage. The worker runs on a private
//   COPY of `acc`, so it updates the copy's `total`, not ours. The copy is
//   destroyed when the thread ends and the result is lost.
//
// TASK:
//   Make the thread operate on the REAL `acc`, so `acc.total` is 4950 after the
//   join. (Look up std::ref.) Then read the README's warning about lifetimes.

#include <iostream>
#include <thread>

struct Accumulator
{
    long total = 0;
    void operator()(int n)
    {
        for (int i = 0; i < n; ++i)
            total += i;          // updates THIS object's total
    }
};

int main()
{
    Accumulator acc;

    std::thread t(acc, 100);     // BUG: `acc` is copied into the thread
    t.join();

    std::cout << "acc.total = " << acc.total
              << "  (expected 4950)\n";
    return 0;
}
