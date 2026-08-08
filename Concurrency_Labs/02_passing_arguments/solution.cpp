// Lab 02 — Passing arguments to threads  (SOLUTION)
//
// std::thread copies (decays) BOTH its callable and its arguments into
// thread-local storage. To make the thread operate on your actual object, wrap
// it in std::ref — the reference wrapper survives the copy and re-expands to a
// real reference inside the thread.

#include <functional>   // std::ref
#include <iostream>
#include <thread>

struct Accumulator
{
    long total = 0;
    void operator()(int n)
    {
        for (int i = 0; i < n; ++i)
            total += i;
    }
};

int main()
{
    Accumulator acc;

    // std::ref(acc) passes a reference to the real object, so its `total`
    // is updated. The same trick works for reference *arguments*, e.g.
    //     void calibrate(int& r);  ->  std::thread t(calibrate, std::ref(value));
    std::thread t(std::ref(acc), 100);
    t.join();

    std::cout << "acc.total = " << acc.total
              << "  (expected 4950)\n";

    // CAUTION (see Lab 03): std::ref hands the thread a pointer to `acc`. That
    // is only safe because we join() before `acc` goes out of scope. With
    // detach(), the thread could outlive `acc` -> dangling reference + UB.
    return 0;
}
