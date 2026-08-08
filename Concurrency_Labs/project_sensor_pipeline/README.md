# Capstone Project — Real-time sensor acquisition & processing pipeline

This project ties the whole module together into one small but realistic
embedded system, and — most importantly — lets students **measure** why
concurrency matters instead of taking it on faith.

```
 [ sensors ]          [ bounded queue ]      [ worker pool ]      [ stats/logger ]
 Temperature ─┐                            ┌── worker 0 ──┐
 Accelerometer├──▶  input  (FIFO, bounded) ┼── worker 1 ──┼──▶ output ──▶  reporter
 GPS ─────────┘                            └── worker N ──┘                (dashboard)
```

Each stage runs on its own thread(s) and is connected only by the
**`BoundedQueue<T>`** built in Lab 07. The processing step (`processing.h`) is a
deliberately CPU-bound, *pure* function — so workers parallelize with zero
shared state — standing in for real work like filtering, sensor fusion, FFT,
or CRC.

## What it demonstrates

| # | Property | How you see it |
|---|----------|----------------|
| 1 | **Throughput** | `bench` mode runs the identical workload serially, then across a worker pool, and prints the measured speedup. |
| 2 | **Responsiveness** | In `live` mode the dashboard keeps updating on schedule even while every core is busy processing. |
| 3 | **Decoupling / backpressure** | The bounded queue absorbs sensor bursts; a slow stage throttles the fast one instead of exploding memory. |
| 4 | **Clean shutdown** | An `atomic<bool>` flag + `queue.close()` stop all stages in order — no lost samples, no leaked threads. |

## Build & run

```sh
# from the Labs build tree:
cmake --build build -j
./build/project_sensor_pipeline/sensor_pipeline            # live, ~3 s
./build/project_sensor_pipeline/sensor_pipeline live 5     # live, 5 s
./build/project_sensor_pipeline/sensor_pipeline bench 2000 # serial vs concurrent
./build/project_sensor_pipeline/sensor_pipeline bench 4000 8

# or standalone:
g++ -std=c++17 -O2 -pthread -Iinclude src/main.cpp -o sensor_pipeline
```

### Example `bench` output (8-core machine)

```
Benchmark: 2000 samples, 20000 compute-iterations each, 8 workers
----------------------------------------------------------
serial      : 612 ms
concurrent  : 92 ms
speedup     : 6.6x   (ideal ~8x)
```

The gap between the measured speedup and the ideal (`workers`×) is the lesson:
queueing overhead, the producer being a single thread, memory bandwidth, and
Amdahl's law all eat into perfect scaling. Have students vary `workers` and
`kWorkIterations` and plot it.

### Example `live` output

```
Live pipeline: 8 workers, running 3s. ...
[stats] processed=143  temp=21 accel=109 gps=13  avg_latency=84us  max_latency=903us
[stats] processed=402  temp=58 accel=309 gps=35  avg_latency=77us  max_latency=903us
...
[stats] FINAL: [stats] processed=820 ...
Pipeline shut down cleanly. No data lost, no threads leaked.
```

## Suggested student exercises / extensions

1. **Measure the cost of the wrong design.** Remove the worker pool and process
   inline in the sensor threads. Watch latency and the dashboard's update
   regularity degrade. This is the "why bother" baseline.
2. **Shrink the queue** to capacity 1 and observe backpressure throttle the fast
   accelerometer sensor. Print drops/blocks.
3. **Add a data race on purpose**: give the workers a shared running average
   without a lock, then catch it with ThreadSanitizer
   (`cmake -DENABLE_TSAN=ON`). Fix it with a `std::atomic` or by keeping the
   aggregate in the single reporter thread (the design used here).
4. **Priorities (QNX).** Run the sensor threads at a higher priority than the
   workers and discuss why acquisition must not be starved by processing.
5. **Bounded latency.** Add a deadline: flag any `Result` whose `latency`
   exceeds, say, 5 ms. Relate to soft vs hard real-time.
6. **Replace `std::thread`+queue with the Lab 10 thread pool** and compare.

## QNX / RTOS mapping

This architecture is exactly how a real embedded device is structured, and maps
cleanly onto an RTOS:

- **Sensor threads** → high-priority periodic tasks (often driven by a timer or
  hardware interrupt / pulse). On QNX, an ISR delivers a *pulse* to a waiting
  thread via `MsgReceivePulse`.
- **Bounded queue** → on QNX you would frequently replace shared-memory + CV
  with **native message passing** (`ChannelCreate` / `MsgSend` / `MsgReceive`):
  a server thread blocks in `MsgReceive` and the kernel hands it the sender's
  priority, eliminating priority inversion at the hand-off.
- **Worker pool** → fixed-priority worker tasks created at startup; never spawn
  threads on the hot path.
- **Reporter** → a low-priority logging/telemetry task that must not delay
  acquisition or control.
- **Shutdown** → a supervisor/watchdog that signals each task and confirms
  termination — the `g_running` flag + `close()` here is the portable analogue.

The portable C++17 in this project is the *concept*; on a shipping QNX system
you would tune priorities and likely swap the queue for channels, but the
pipeline shape — acquire, decouple, process in parallel, report — is identical.
