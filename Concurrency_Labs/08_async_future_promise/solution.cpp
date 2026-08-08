// Lab 08 — async / future / promise  (SOLUTION)
//
// Shows the two idioms:
//   (A) std::promise / std::future  — manual one-shot channel between threads.
//       Use set_value() on success, set_exception() on failure; future.get()
//       returns the value or RE-THROWS the exception in the waiting thread.
//   (B) std::async                  — fire a callable and get a future directly;
//       exceptions propagate automatically through get(). Much less boilerplate.

#include <cmath>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

double calibrate(double x)
{
    if (x < 0)
        throw std::out_of_range("calibration input must be >= 0");
    return std::sqrt(x) * 1.5;
}

// (A) promise/future: the worker delivers value OR exception through the promise.
void worker(std::promise<double> result_promise, double input)
{
    try {
        result_promise.set_value(calibrate(input));        // success path
    } catch (...) {
        result_promise.set_exception(std::current_exception());  // failure path
    }
}

int main()
{
    // --- (A) manual promise/future, valid input ---
    {
        std::promise<double> p;
        std::future<double>  f = p.get_future();
        std::thread t(worker, std::move(p), 16.0);
        std::cout << "[promise] calibrated value = " << f.get() << '\n';  // 6.0
        t.join();
    }

    // --- (A) manual promise/future, invalid input -> get() re-throws ---
    {
        std::promise<double> p;
        std::future<double>  f = p.get_future();
        std::thread t(worker, std::move(p), -4.0);
        try {
            double v = f.get();                       // throws here
            std::cout << "[promise] value = " << v << '\n';
        } catch (const std::exception& e) {
            std::cout << "[promise] caught from worker: " << e.what() << '\n';
        }
        t.join();
    }

    // --- (B) std::async does all of the above with no promise plumbing ---
    {
        std::future<double> f = std::async(std::launch::async, calibrate, 25.0);
        std::cout << "[async]  calibrated value = " << f.get() << '\n';    // 7.5
    }
    {
        auto f = std::async(std::launch::async, calibrate, -1.0);
        try {
            f.get();
        } catch (const std::exception& e) {
            std::cout << "[async]  caught: " << e.what() << '\n';
        }
    }
    return 0;
}
