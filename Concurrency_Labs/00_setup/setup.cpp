// Lab 00 — Setup & hardware concurrency
//
// Goal: confirm your toolchain builds and links threads, and discover how many
// hardware threads the target exposes. On an embedded board this number tells
// you how much *real* parallelism you have — above it, threads time-slice.
//
// Build:  see Labs/README.md, or:
//   g++ -std=c++17 -O2 -pthread setup.cpp -o setup && ./setup

#include <iostream>
#include <thread>

int main()
{
    const unsigned n = std::thread::hardware_concurrency();

    std::cout << "C++ standard in use: " << __cplusplus << '\n';
    std::cout << "hardware_concurrency() = " << n
              << (n == 0 ? "  (unknown / not computable)\n" : " hardware thread(s)\n");

    // Launch one helper thread just to prove linking against the threads
    // library works on this toolchain.
    std::thread t([] {
        std::cout << "Hello from a worker thread (id "
                  << std::this_thread::get_id() << ")\n";
    });
    t.join();

    std::cout << "Main thread id " << std::this_thread::get_id() << "\n";
    std::cout << "Toolchain OK — you are ready for the labs.\n";
    return 0;
}
