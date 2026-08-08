# Lab 08 — async / future / promise

## Concept

Threads return `void` and can't easily report errors. The `<future>` library
fixes both:

- **`std::promise<T>`** is the *producing* end, **`std::future<T>`** the
  *consuming* end of a one-shot channel. The worker calls `set_value()` or
  `set_exception()`; the waiter calls `future.get()`, which **blocks until ready
  and either returns the value or re-throws the exception** in the waiting
  thread.
- **`std::async`** wraps the whole pattern: hand it a callable, get a future
  back, and exceptions propagate through `get()` automatically — no promise
  plumbing. Use `std::launch::async` to force a real thread (the default policy
  may run it lazily on `get()`).

This is how you get a *result* or a *failure* out of background work — the
natural fit for "compute this calibration / FFT / checksum off the main loop".

## The bugs (starter)

1. `result` is read uninitialized.
2. The worker never actually calls `calibrate()`.
3. If `calibrate()` throws, the exception is swallowed instead of being sent to
   the future via `set_exception()`.

## Task

Make the worker deliver the value on success and the exception on failure, then
exercise both a valid input (16 → 6.0) and an invalid one (−4 → throws). Then
rewrite it with `std::async` and notice how much disappears.

## Expected output (solution)

```
[promise] calibrated value = 6
[promise] caught from worker: calibration input must be >= 0
[async]  calibrated value = 7.5
[async]  caught: calibration input must be >= 0
```

## QNX / RTOS note

`std::async` is convenient but **gives you no control over which thread, what
priority, or thread reuse** — implementations may spawn a fresh thread per call.
For hard real-time work you don't want unpredictable thread creation on your
critical path; you'd pre-create a pool of priority-configured worker threads
(Lab 10) and hand them tasks. Keep `std::async`/`std::future` for non-realtime
background jobs (config load, diagnostics), and prefer an explicit pool for the
control path.
