// Lab 05 — Mutex & RAII  (SOLUTION)
//
// One guard per critical section, on BOTH the writer and the reader. RAII means
// the mutex is released automatically when the guard goes out of scope — even
// if the body throws. Never call mutex.lock()/unlock() by hand in real code.

#include <iostream>
#include <list>
#include <mutex>
#include <thread>

std::list<int> myList;
std::mutex     myMutex;

void addToList(int max, int interval)
{
    std::lock_guard<std::mutex> guard(myMutex);   // exactly ONE lock
    for (int i = 0; i < max; ++i)
        if (i % interval == 0)
            myList.push_back(i);
}                                                 // guard releases here

void printList()
{
    std::lock_guard<std::mutex> guard(myMutex);   // reader is protected too
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

    // Useful RAII lock vocabulary (C++17):
    //   std::lock_guard   - simplest: lock now, unlock at scope end.
    //   std::scoped_lock  - like lock_guard but locks 1..N mutexes deadlock-free
    //                       (see Lab 06).
    //   std::unique_lock  - movable, can unlock/relock early; required by
    //                       condition_variable (see Lab 07). Slightly heavier.
    return 0;
}
