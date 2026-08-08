# Lab 10 — Thread pool

## Concept

Creating a `std::thread` per task is wasteful and unpredictable — thread
creation costs time and memory, and on an embedded target you want to *bound*
how many threads exist. A **thread pool** creates a fixed number of workers once,
then feeds them tasks through a shared, mutex-protected queue. Workers sleep on a
condition variable when idle and wake when work arrives.

`submit()` wraps each task in a `std::packaged_task` and returns a
`std::future`, so the caller gets the result back — and any exception propagates
through `future.get()` just like Lab 08.

This lab is the synthesis of the module: threads + mutex + condition variable +
RAII shutdown + futures, in one ~70-line component.

## Task

> Heads-up: the unfinished skeleton **aborts** when you run it. The empty
> destructor never joins the workers, so the `std::vector<std::thread>` is
> destroyed while its threads are still joinable → `std::terminate()` (exactly
> the rule from Lab 01). Completing TODO #3 fixes that.

Finish the three TODOs in `starter.cpp`:

1. `submit()` — enqueue the job under the lock, then `notify_one()`.
2. `worker()` — `cv_.wait` for a job or stop; pop and run it *outside* the lock.
3. `~ThreadPool()` — set `stop_`, `notify_all()`, and join every worker.

## Expected output (solution)

```
1 4 9 16 25 36 49 64
```

(The numbers may print in submission order because we `get()` them in order, but
they are computed concurrently by the 4 workers.)

## Design notes to discuss

- **Run the job outside the lock.** Holding the mutex while executing a task
  would serialize the whole pool. Pop under the lock, release, *then* run.
- **Clean shutdown.** `stop_ && jobs_.empty()` lets workers drain remaining work
  before exiting — change it to exit immediately and discuss the trade-off.
- **Bounded vs unbounded queue.** This pool's queue is unbounded; pair it with
  the bounded queue from Lab 07 if producers can outrun workers (backpressure).

## QNX / RTOS note

A thread pool is the *real-time-friendly* alternative to `std::async`: you create
the workers at startup, pin them to **fixed priorities** (and on QNX possibly to
specific cores via `ThreadCtl`/`runmask`), and never pay for thread creation on
the critical path. Many RTOS frameworks ship exactly this as a "worker task" or
"deferred procedure call" mechanism. Sizing the pool to
`hardware_concurrency()` (Lab 00) is the usual starting point for CPU-bound work.
