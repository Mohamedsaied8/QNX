# Lab 05 — Mutex & RAII

## Concept

A **mutex** (mutual exclusion) lets only one thread into a *critical section* at
a time. The correct way to use one in C++ is never to call `lock()`/`unlock()`
directly, but to wrap it in an **RAII guard** that locks on construction and
unlocks on destruction. That guarantees the unlock happens on *every* exit path,
including exceptions and early `return`s.

C++17 RAII locks:

| Type | Use it when |
|------|-------------|
| `std::lock_guard` | the common case: lock for one scope |
| `std::scoped_lock` | locking **one or more** mutexes together, deadlock-free |
| `std::unique_lock` | you need to unlock/relock early, or a condition variable |

## The bug

The starter locks **the same non-recursive `std::mutex` twice** in one thread
(a `lock_guard` *and* a custom `Lock`). The second `lock()` blocks forever
because the first hasn't released — a **self-deadlock**. The program hangs.
There's also a second, quieter bug: `printList()` reads the shared list with no
lock at all (a data race).

## Task

1. Run `lab05_starter` — it hangs. Kill it with Ctrl-C.
2. Remove the redundant second lock (keep exactly one guard).
3. Add a guard to `printList()`.
4. Verify the data race is gone:
   ```sh
   g++ -std=c++17 -g -fsanitize=thread -pthread solution.cpp -o t && ./t
   ```

## Expected output (solution)

A single comma-separated line of the values pushed (order of the two threads may
vary; with a lock the list is never seen half-written):

```
0,1,2,3, ... ,99,
```

## QNX / RTOS note

If you genuinely need to re-enter a lock you already hold, use
`std::recursive_mutex` — but treat that as a design smell, not a fix. The bigger
RTOS concern is **priority inversion**: a low-priority thread holding a mutex can
block a high-priority thread indefinitely while a medium-priority thread runs.
QNX's `pthread_mutex` supports **priority inheritance**
(`PTHREAD_PRIO_INHERIT`), which temporarily boosts the holder to the waiter's
priority. The C++ standard mutex gives no such guarantee — another reason hard
real-time code configures POSIX mutex attributes directly.
