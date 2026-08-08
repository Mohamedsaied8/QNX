#include <iostream>
#include <thread>
#include <list>
#include <algorithm>
#include <mutex>
#include <chrono>

using namespace std;

template<class T>
class Lock
{
    public:
    Lock(T& mutex) :mutex_{mutex}
    {
        mutex_.lock();
    }
    ~Lock()
    {
        mutex_.unlock();
    }
    private:
    T& mutex_;
};

// a global variable
std::list<int> myList;

// a global instance of std::mutex to protect global variable
std::mutex myMutex;

void addToList(int max, int interval)
{
    // the access to this function is mutually exclusive
    {
        std::lock_guard<std::mutex> guard(myMutex);
        Lock<std::mutex> l(myMutex);
        for (int i = 0; i < max; i++) {
            if( (i % interval) == 0) myList.push_back(i);
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void printList()
{
    // the access to this function is mutually exclusive
   // std::lock_guard<std::mutex> guard(myMutex);
    for (auto itr = myList.begin(), end_itr = myList.end(); itr != end_itr; ++itr ) {
        cout << *itr << ",";
    }
}

int main()
{
    int max = 100;

    std::thread t1(addToList, max, 1);
    std::thread t3(printList);

    t1.join();
    t3.join();

    return 0;
}