// Lab 06 — Deadlock  (SOLUTION)
//
// Fix: acquire BOTH mutexes as one atomic step with std::scoped_lock (C++17).
// It uses a deadlock-avoidance algorithm internally, so the *order* you list the
// mutexes no longer matters — the AB-BA cycle is impossible.
//
// The general rule when scoped_lock isn't applicable: impose a GLOBAL LOCK
// ORDER (e.g. always lock the account with the lower address first) and never
// deviate. scoped_lock just does that for you, correctly.

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
    // Lock both mutexes together, deadlock-free, regardless of argument order.
    std::scoped_lock lock(from.m, to.m);   // CTAD deduces the mutex types
    from.balance -= amount;
    to.balance   += amount;
}

int main()
{
    Account a(1000), b(1000);

    for (int round = 0; round < 1000; ++round) {
        std::thread t1(transfer, std::ref(a), std::ref(b), 1);
        std::thread t2(transfer, std::ref(b), std::ref(a), 1);
        t1.join();
        t2.join();
    }

    std::cout << "a=" << a.balance << " b=" << b.balance
              << " (total should stay 2000)\n";

    // Pre-C++17 equivalent:
    //   std::lock(from.m, to.m);
    //   std::lock_guard<std::mutex> g1(from.m, std::adopt_lock);
    //   std::lock_guard<std::mutex> g2(to.m,   std::adopt_lock);
    return 0;
}
