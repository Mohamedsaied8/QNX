// Lab 08 — async / future / promise  (STARTER, contains BUGS)
//
// We want to run a calibration computation on another thread and get either its
// RESULT or its EXCEPTION back on the main thread. A std::promise/std::future
// pair is the one-shot channel for exactly that.
//
// BUGS TO FIX:
//   #1  `result` is read uninitialized — set_value() stores garbage.
//   #2  the worker computes nothing; calibrate() should actually run.
//   #3  if calibrate() throws (negative input), the exception is never delivered
//       to the future, so the worker swallows it.
//
// TASK: make the worker put calibrate(input)'s value into the promise on
//       success, and forward the exception with set_exception() on failure.
//       Then call it once with a valid input and once with a negative one and
//       observe future.get() either returning the value or RE-THROWING.

#include <cmath>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

// Pretend calibration: valid for x >= 0, throws otherwise.
double calibrate(double x)
{
    if (x < 0)
        throw std::out_of_range("calibration input must be >= 0");
    return std::sqrt(x) * 1.5;
}

void worker(std::promise<double> result_promise, double input)
{
    double result;                              // BUG #1: uninitialised
    // BUG #2 / #3: never calls calibrate(), never handles its exception.
    result_promise.set_value(result);           // stores garbage
}

int main()
{
    std::promise<double> p;
    std::future<double>  f = p.get_future();

    std::thread t(worker, std::move(p), 16.0);

    // do other work here while the worker runs...
    double value = f.get();                      // should be 6.0 for input 16
    std::cout << "calibrated value = " << value << '\n';

    t.join();
    return 0;
}
