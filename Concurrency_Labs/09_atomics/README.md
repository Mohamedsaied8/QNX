# Lab 09 — Atomics & a spinlock

## Concept

`std::atomic<T>` makes operations on a single variable **indivisible** and gives
you control over **memory ordering** (how reads/writes around it may be
reordered). Two everyday uses:

- A **lock-free counter / flag** — cheaper than a mutex because uncontended
  atomics never trap into the kernel.
- A building block for **synchronization primitives**, e.g. a spinlock from
  `std::atomic_flag` (`test_and_set` / `clear`).

`std::atomic_flag::test_and_set()` atomically sets the flag *and returns its
previous value* — the minimal primitive needed to build mutual exclusion.

## The bugs (starter)

1. The spinlock uses a **plain `bool`**: "check then set" is two steps, so two
   threads can both see `false` and both enter. The protected counter comes out
   wrong (and TSan flags the race).
2. A plain-`bool` stop flag may be **cached in a register** by the optimizer, so
   the worker never notices it changed and loops forever.

## Task

- Rebuild `SpinLock` on `std::atomic_flag`.
- Make the stop flag `std::atomic<bool>`.
- Confirm Part A under ThreadSanitizer:
  ```sh
  g++ -std=c++17 -g -fsanitize=thread -pthread starter.cpp -o t && ./t
  ```

## Expected output (solution)

```
counter = 200000  (expected 200000)
heartbeat stopped after <large number> iterations
```

## When NOT to use a spinlock

A spinlock **burns a whole core while waiting**. It only wins for critical
sections of a few instructions where the wait is shorter than a context switch.
For anything longer — or on a single-core system where spinning just starves the
holder — use a `std::mutex`, which sleeps.

## Memory ordering (one paragraph)

The default, `memory_order_seq_cst`, is the easiest to reason about and the
right starting point. `acquire`/`release` (used in the spinlock) is the next
step: a `release` store publishes everything written before it to whoever does a
matching `acquire` load. Only reach for `relaxed` (used for the standalone
counter/flag, where no other data is being published) once you can prove no
other memory is being handed off. Get it correct first, then optimize ordering.

## QNX / RTOS note

On a single-core MCU, an atomic also protects against an **ISR** preempting a
read-modify-write — something a mutex can't do (you can't block an interrupt on
a mutex). For data shared between an interrupt handler and a thread,
`std::atomic` (or a lock-free ring buffer) is usually the only correct choice.
On multicore QNX/ARM, atomics lower to `LDREX/STREX`; the same code is portable.
