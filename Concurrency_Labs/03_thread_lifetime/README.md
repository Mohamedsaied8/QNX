# Lab 03 — Thread lifetime: join vs detach

## Concept

A thread has two possible fates:

- **`join()`** — the calling thread waits here until the worker finishes.
- **`detach()`** — the worker runs independently; you give up the ability to
  wait for it or get its result.

`detach()` is dangerous because the worker can **outlive the data it
references**. The starter does exactly this: a worker holds `int&` to a local,
the function returns, the local is destroyed, and the still-running worker reads
freed memory — undefined behaviour.

The fix is a lifetime rule: **a thread must never outlive anything it borrows.**
Either join before the data dies, or give the thread its own copy.

## Task

1. Build and run `lab03_starter`. Then rebuild with AddressSanitizer and watch
   it report the use-after-free:
   ```sh
   g++ -std=c++17 -g -fsanitize=address -pthread starter.cpp -o a && ./a
   ```
2. Fix it two ways (join, and own-a-copy). Compare with `solution.cpp`.
3. Study the `ThreadGuard` RAII type — it joins in its destructor so the thread
   is joined *even if an exception unwinds the stack*. This is the safe default.

## Expected output (solution)

```
[join]  worker sees state = 42
[copy]  worker owns state = 99
main done
```

## QNX / RTOS note

On an RTOS you rarely `detach()` worker threads: long-lived tasks are usually
created once at startup with explicit priorities and live for the life of the
system, and you *want* to be able to join/cancel them during shutdown. A
detached thread you cannot join is a thread you cannot cleanly stop — a problem
when a watchdog or supervisor needs every task to confirm it has terminated.
C++20's `std::jthread` bakes the `ThreadGuard` idea into the standard library
(auto-join + cooperative `stop_token`); if your toolchain has it, prefer it.
