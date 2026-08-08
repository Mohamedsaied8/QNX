// Lab 06 — Deadlock: the AB-BA lock-ordering bug  (STARTER, it HANGS)
//
// transfer() locks the source account, then the destination account. We run two
// transfers in opposite directions at the same time:
//     thread 1:  A -> B    locks A, then wants B
//     thread 2:  B -> A    locks B, then wants A
// If both grab their first lock before either grabs the second, each waits
// forever for a lock the other holds. Classic circular-wait DEADLOCK.
//
// It won't hang every run (timing-dependent) — that's what makes deadlocks
// nasty. The loop in main() runs many rounds to make it show up fast.
//
// TASK: make transfer() deadlock-free. The idiomatic C++17 fix is to acquire
//       BOTH mutexes atomically with std::scoped_lock (or std::lock). See
//       solution.cpp. A debugging trick — detect the hang with:
//           valgrind --tool=helgrind ./lab06_starter

#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

struct Account
{
    std::mutex m;
    long balance;
    explicit Account(long b) : balance(b) {}
};

void transfer(Account& from, Account& to, long amount)
{
    std::lock_guard<std::mutex> l1(from.m);          // grab source first
    std::this_thread::sleep_for(std::chrono::microseconds(10)); // widen the window
    std::lock_guard<std::mutex> l2(to.m);            // BUG: opposite order in the
    from.balance -= amount;                          //      other thread -> deadlock
    to.balance   += amount;
}

int main()
{
    Account a(1000), b(1000);

    for (int round = 0; round < 1000; ++round) {
        std::thread t1(transfer, std::ref(a), std::ref(b), 1);
        std::thread t2(transfer, std::ref(b), std::ref(a), 1);  // opposite order
        t1.join();
        t2.join();
    }

    std::cout << "a=" << a.balance << " b=" << b.balance
              << " (total should stay 2000)\n";
    return 0;
}
