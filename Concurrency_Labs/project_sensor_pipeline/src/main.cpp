// =============================================================================
//  Capstone project — Real-time sensor acquisition & processing pipeline
// =============================================================================
//
//  A small but realistic embedded data pipeline:
//
//      [ sensors ]      [ bounded queue ]    [ worker pool ]    [ stats/logger ]
//   Temperature  --\                       /-- worker 0 --\
//   Accelerometer ---> input queue (FIFO) ---- worker 1 ----> output queue --> stats
//   GPS          --/                       \-- worker N --/
//
//  Three things this demonstrates — the WHOLE point of the module:
//
//   1. THROUGHPUT   Processing each sample is CPU-bound. The `bench` mode runs
//                   the same workload serially and then across a worker pool and
//                   prints the measured speedup. Concurrency turns idle cores
//                   into throughput.
//
//   2. RESPONSIVENESS  In `live` mode the stats dashboard keeps updating on time
//                   even while the workers are saturated, because acquisition,
//                   processing and reporting run on separate threads. A
//                   single-threaded version would freeze the display whenever it
//                   was busy computing.
//
//   3. DECOUPLING & CLEAN SHUTDOWN  The bounded queue absorbs bursts and applies
//                   backpressure; an atomic flag + queue.close() bring every
//                   thread down in order with no leaks and no lost data.
//
//  Usage:
//      sensor_pipeline             # live mode, ~3 s, then clean shutdown
//      sensor_pipeline live  [secs]
//      sensor_pipeline bench [samples] [workers]
//
//  Build via the Labs CMake tree, or:
//      g++ -std=c++17 -O2 -pthread -Iinclude src/main.cpp -o sensor_pipeline
// =============================================================================

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "bounded_queue.h"
#include "processing.h"
#include "types.h"

using namespace std::chrono_literals;

// -----------------------------------------------------------------------------
//  BENCH MODE — serial vs concurrent, measured.
// -----------------------------------------------------------------------------
namespace bench {

std::vector<Sample> make_workload(std::size_t n)
{
    std::vector<Sample> v;
    v.reserve(n);
    const auto now = Clock::now();
    for (std::uint64_t i = 0; i < n; ++i)
        v.push_back(Sample{SensorType::Temperature, i,
                           20.0 + (i % 100) * 0.1, now});
    return v;
}

// Accumulate something derived from every result so the optimizer can't delete
// the work. Returned as an integer checksum.
long long run_serial(const std::vector<Sample>& work)
{
    long long checksum = 0;
    for (const Sample& s : work)
        checksum += static_cast<long long>(process(s).value);
    return checksum;
}

long long run_concurrent(const std::vector<Sample>& work, unsigned workers)
{
    BoundedQueue<Sample> in(256);
    std::atomic<long long> checksum{0};

    // One producer feeds the queue; closing it tells workers to drain and stop.
    std::thread producer([&] {
        for (const Sample& s : work) in.push(s);
        in.close();
    });

    std::vector<std::thread> pool;
    for (unsigned w = 0; w < workers; ++w)
        pool.emplace_back([&] {
            long long local = 0;
            while (auto s = in.pop())
                local += static_cast<long long>(process(*s).value);
            checksum.fetch_add(local, std::memory_order_relaxed);
        });

    producer.join();
    for (auto& t : pool) t.join();
    return checksum.load();
}

int main(std::size_t samples, unsigned workers)
{
    const auto work = make_workload(samples);

    std::cout << "Benchmark: " << samples << " samples, "
              << kWorkIterations << " compute-iterations each, "
              << workers << " workers\n"
              << "----------------------------------------------------------\n";

    auto t0  = Clock::now();
    auto cs1 = run_serial(work);
    auto t1  = Clock::now();
    auto cs2 = run_concurrent(work, workers);
    auto t2  = Clock::now();

    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    const double serial_ms     = ms(t0, t1);
    const double concurrent_ms = ms(t1, t2);

    std::cout << "serial      : " << serial_ms     << " ms\n"
              << "concurrent  : " << concurrent_ms << " ms\n"
              << "speedup     : " << (serial_ms / concurrent_ms) << "x"
              << "   (ideal ~" << workers << "x)\n";

    if (cs1 != cs2)   // sanity: both paths must compute the same thing
        std::cout << "WARNING: checksums differ (" << cs1 << " vs " << cs2 << ")\n";
    return 0;
}

} // namespace bench

// -----------------------------------------------------------------------------
//  LIVE MODE — a running pipeline with a periodic dashboard and clean shutdown.
// -----------------------------------------------------------------------------
namespace live {

std::atomic<bool> g_running{true};   // set false to begin shutdown

// One sensor: emits samples at a fixed period until shutdown.
void sensor(SensorType type, std::chrono::milliseconds period,
            BoundedQueue<Sample>& out)
{
    std::uint64_t seq = 0;
    while (g_running.load(std::memory_order_relaxed)) {
        Sample s{type, seq++, 20.0 + (seq % 50) * 0.3, Clock::now()};
        if (!out.push(s)) break;        // queue closed -> stop
        std::this_thread::sleep_for(period);
    }
}

// One worker: process samples until the input is closed and drained.
void worker(BoundedQueue<Sample>& in, BoundedQueue<Result>& out)
{
    while (auto s = in.pop()) {
        Result r = process(*s);
        if (!out.push(r)) break;
    }
}

// Stats/logger: aggregate results and print a dashboard line periodically. This
// thread stays responsive (prints on schedule) even while workers are saturated.
void reporter(BoundedQueue<Result>& in)
{
    std::uint64_t counts[3]    = {0, 0, 0};
    long long     max_latency  = 0;
    long long     sum_latency  = 0;
    std::uint64_t total        = 0;
    auto          last_print   = Clock::now();

    auto print = [&] {
        std::cout << "[stats] processed=" << total
                  << "  temp=" << counts[0]
                  << " accel=" << counts[1]
                  << " gps=" << counts[2]
                  << "  avg_latency="
                  << (total ? sum_latency / static_cast<long long>(total) : 0)
                  << "us  max_latency=" << max_latency << "us\n";
    };

    while (auto r = in.pop()) {
        ++total;
        ++counts[static_cast<int>(r->type)];
        const long long lat = r->latency.count();
        sum_latency += lat;
        if (lat > max_latency) max_latency = lat;

        if (Clock::now() - last_print >= 500ms) {
            print();
            last_print = Clock::now();
        }
    }
    std::cout << "[stats] FINAL: ";
    print();
}

int main(int seconds)
{
    const unsigned n_workers =
        std::max(2u, std::thread::hardware_concurrency());

    BoundedQueue<Sample> input(128);
    BoundedQueue<Result> output(128);

    std::cout << "Live pipeline: " << n_workers << " workers, running "
              << seconds << "s. Ctrl-C or wait for clean shutdown.\n";

    // Stage 3: reporter.
    std::thread rep(reporter, std::ref(output));

    // Stage 2: worker pool.
    std::vector<std::thread> workers;
    for (unsigned w = 0; w < n_workers; ++w)
        workers.emplace_back(worker, std::ref(input), std::ref(output));

    // Stage 1: sensors at different rates (continuous / periodic / sporadic).
    std::vector<std::thread> sensors;
    sensors.emplace_back(sensor, SensorType::Temperature,   50ms, std::ref(input));
    sensors.emplace_back(sensor, SensorType::Accelerometer, 10ms, std::ref(input));
    sensors.emplace_back(sensor, SensorType::Gps,          200ms, std::ref(input));

    // Let it run, then bring it down IN ORDER.
    std::this_thread::sleep_for(std::chrono::seconds(seconds));

    g_running.store(false);             // 1) tell sensors to stop
    for (auto& s : sensors) s.join();   // 2) wait for them to finish

    input.close();                      // 3) workers drain remaining samples...
    for (auto& w : workers) w.join();   //    ...then exit

    output.close();                     // 4) reporter drains results...
    rep.join();                         //    ...then exits

    std::cout << "Pipeline shut down cleanly. No data lost, no threads leaked.\n";
    return 0;
}

} // namespace live

// -----------------------------------------------------------------------------
int main(int argc, char** argv)
{
    std::string mode = (argc > 1) ? argv[1] : "live";

    if (mode == "bench") {
        std::size_t samples = (argc > 2) ? std::strtoul(argv[2], nullptr, 10) : 2000;
        unsigned    workers = (argc > 3) ? std::strtoul(argv[3], nullptr, 10)
                                         : std::max(2u, std::thread::hardware_concurrency());
        return bench::main(samples, workers);
    }
    if (mode == "live") {
        int seconds = (argc > 2) ? std::atoi(argv[2]) : 3;
        return live::main(seconds);
    }

    std::cerr << "usage: " << argv[0] << " [live [secs] | bench [samples] [workers]]\n";
    return 1;
}
