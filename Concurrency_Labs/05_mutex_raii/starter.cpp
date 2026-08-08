// Lab 05 — Mutex & RAII  (STARTER, contains a BUG — it HANGS)
//
// This program locks the SAME non-recursive mutex twice in one thread, then
// hangs forever. This is a real bug that shipped in the original course code.
//
// BUG #1 (the hang): addToList() takes `myMutex` with a lock_guard AND with a
//   second custom Lock — both on the SAME std::mutex. A std::mutex is NOT
//   recursive: the second lock() blocks waiting for the first to release, but
//   the first won't release until the function returns. Self-deadlock.
//
// BUG #2 (a data race): printList() reads myList with NO lock while addToList()
//   may still be writing it. (Run under ThreadSanitizer to see it.)
//
// TASK:
//   1. Remove the double lock — keep exactly ONE guard in addToList().
//   2. Protect printList() with the same mutex.
//   Compare with solution.cpp, which also shows std::scoped_lock and a correct
//   custom RAII guard.

#include <chrono>
#include <iostream>
#include <list>
#include <mutex>
#include <thread>

// A minimal RAII lock — locks in ctor, unlocks in dtor (this part is fine).
template <class M>
class Lock
{
    M& m_;
public:
    explicit Lock(M& m) : m_(m) { m_.lock(); }
    ~Lock() { m_.unlock(); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
};

std::list<int> myList;
std::mutex     myMutex;

void addToList(int max, int interval)
{
    std::lock_guard<std::mutex> guard(myMutex);   // first lock
    Lock<std::mutex> l(myMutex);                  // BUG #1: second lock -> hang
    for (int i = 0; i < max; ++i)
        if (i % interval == 0)
            myList.push_back(i);
}

void printList()
{
    // BUG #2: no lock here.
    for (int v : myList)
        std::cout << v << ',';
    std::cout << '\n';
}

int main()
{
    std::thread t1(addToList, 100, 1);
    std::thread t2(printList);
    t1.join();
    t2.join();
    return 0;
}
