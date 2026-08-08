// Lab 03 — Thread lifetime: join vs detach  (SOLUTION)
//
// The danger in the starter was detach()ing a thread that referenced a local.
// Two fixes are shown:
//   (A) join() before the data goes out of scope  -> safe to keep the reference
//   (B) give the worker its own value             -> safe even if detached
// Plus an RAII ThreadGuard that guarantees join() even if an exception is thrown
// (the "thread-safe destructor" pattern from the slides).

#include <iostream>
#include <thread>
#include <utility>

// --- RAII guard: joins the thread in its destructor, no matter how we leave ---
class ThreadGuard
{
    std::thread t_;
public:
    explicit ThreadGuard(std::thread t) : t_(std::move(t)) {}
    ~ThreadGuard() { if (t_.joinable()) t_.join(); }

    ThreadGuard(const ThreadGuard&)            = delete;  // non-copyable
    ThreadGuard& operator=(const ThreadGuard&) = delete;
};

// (A) Worker keeps a reference, but we JOIN before the referent dies.
void safe_with_join()
{
    int local_state = 42;
    auto work = [&local_state] {
        std::cout << "[join]  worker sees state = " << local_state << "\n";
    };
    ThreadGuard guard{std::thread(work)};   // joins automatically on scope exit
}                                           // local_state outlives the join

// (B) Worker owns a COPY, so it is safe even if detached.
void safe_with_copy()
{
    int local_state = 99;
    std::thread t([local_state] {           // captured BY VALUE
        std::cout << "[copy]  worker owns state = " << local_state << "\n";
    });
    t.detach();                             // safe: nothing it touches can die
}

int main()
{
    safe_with_join();
    safe_with_copy();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "main done\n";
    return 0;
}
