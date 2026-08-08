# Lab 01 — Creating threads

## Concept

`std::thread t(callable, args...)` starts a new thread *immediately*. You must
later `join()` it (wait for it) or `detach()` it (let it run free). If a
`std::thread` object is destroyed while still *joinable*, the program calls
`std::terminate()` and dies — this is the language forcing you to make a
deliberate lifetime decision.

## Task

The starter runs an LCD refresh and an LED blink **serially**, so the LED never
gets a turn. Put each on its own `std::thread` and `join()` both. See
`starter.cpp` for the step-by-step TODO.

## Expected output (solution — order will vary)

```
[LCD] Temperature: 43
[LED] ON
[LCD] Temperature: 43
[LED] off
...
Both tasks complete.
```

The `[LCD]` and `[LED]` lines now interleave: proof they run concurrently.

> Note: `std::cout` from two threads can interleave *within* a line on some
> systems — that's a foreshadowing of Lab 04 (data races) and Lab 05 (mutexes).

## QNX / RTOS note

`std::thread` is implemented on POSIX `pthread_create`. On QNX you would more
often create threads via `pthread_create` directly so you can set a
**scheduling policy and priority** in the `pthread_attr_t` (e.g.
`SCHED_FIFO`, priority 20). The C++ standard library gives you no portable way
to set priority — for hard real-time work you drop to the POSIX API. The
*concept* (start, then join) is identical.
