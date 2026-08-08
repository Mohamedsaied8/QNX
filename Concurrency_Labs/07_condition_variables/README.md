# Lab 07 — Condition variables: producer/consumer

## Concept

A **condition variable** lets a thread *sleep* until some condition becomes true,
instead of busy-waiting (spinning). It always pairs with a mutex and a
predicate:

```cpp
std::unique_lock<std::mutex> lk(m);
cv.wait(lk, [&]{ return condition; });   // atomically unlock+sleep; relock+check on wake
```

Three rules you must internalize:

1. **Always use a predicate.** `wait` can return *spuriously* (no notify). The
   predicate form re-checks in a loop, so a spurious wake is harmless.
2. **Hold the mutex when you change the shared state** the predicate inspects,
   then `notify`.
3. **`unique_lock`, not `lock_guard`** — the CV needs to unlock and relock the
   mutex for you.

This is the backbone of every embedded data pipeline: an ISR or sensor thread
*produces*, a worker thread *consumes*, decoupled by a queue.

## The bugs (starter)

- **Naive polling**: when the queue is empty the consumer sleeps 1 ms and checks
  again. It works, but it wastes CPU on pointless wakeups and adds up to 1 ms of
  latency to every reading — a condition variable sleeps *exactly* until data
  arrives and wakes immediately.
- **No shutdown**: `while(true)` loops mean `join()` never returns. After the 10
  readings are consumed the consumer polls forever, so the program hangs (kill it
  with Ctrl-C).
- **Unbounded**: the producer never blocks, so a slow consumer lets the queue
  grow without limit.

## Task

Replace the spin with a `std::condition_variable`. Build a **bounded** queue:
consumers wait on "not empty", producers wait on "not full", and a `close()`
signals end-of-stream so consumers drain and exit. The provided
`BoundedQueue<T>` is reused by the capstone project — study it.

## Expected output (solution)

```
logged 1
logged 2
...
logged 10
pipeline drained, clean shutdown.
```

## QNX / RTOS note

`std::condition_variable` maps to `pthread_cond_t`. On QNX the more *native*
idiom for this producer/consumer hand-off is **message passing**:
`ChannelCreate` / `MsgSend` / `MsgReceive`, where a server thread blocks in
`MsgReceive` until a client sends — the kernel does the wait/wake and even hands
over priority (the receiver runs at the sender's priority). It's the same
producer/consumer shape, but synchronization and scheduling are unified by the
microkernel instead of layered on top of shared memory + a CV.
