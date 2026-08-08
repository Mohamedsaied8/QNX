// Lab 03 — Thread lifetime: join vs detach  (STARTER, contains a BUG)
//
// oops() launches a worker that holds a REFERENCE to a local variable, then
// detach()es it and returns. The local is destroyed, but the detached thread
// keeps using the reference: a use-after-free. This may "work" by luck, print
// garbage, or crash — classic undefined behaviour.
//
// Confirm it with AddressSanitizer:
//   g++ -std=c++17 -g -fsanitize=address -pthread starter.cpp -o a && ./a
//
// TASK:
//   Fix oops() so the worker can never outlive the data it touches. Two valid
//   approaches:
//     (A) join() before oops() returns, OR
//     (B) give the worker its OWN copy of the data (capture/pass by value).
//   See solution.cpp for both, plus an RAII "thread guard" that joins
//   automatically.

#include <iostream>
#include <thread>

struct Worker
{
    int& state;                       // reference to caller's local
    explicit Worker(int& s) : state(s) {}
    void operator()() const
    {
        for (int j = 0; j < 1'000'000; ++j)
            // `state` may already be destroyed by the time we get here.
            if (j % 250000 == 0)
                std::cout << "worker sees state = " << state << "\n";
    }
};

void oops()
{
    int local_state = 42;
    Worker w(local_state);
    std::thread t(w);
    t.detach();                       // BUG: t runs free, local_state dies below
}                                     // local_state destroyed -> dangling ref

int main()
{
    oops();
    // Give the detached thread a chance to run and (likely) misbehave.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "main done\n";
    return 0;
}
