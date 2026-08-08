# Lab 06 — Deadlock

## Concept

A **deadlock** is a set of threads each waiting for a resource another holds, so
none can proceed. The textbook recipe (Coffman conditions) needs all four:
mutual exclusion, hold-and-wait, no preemption, and **circular wait**. Break any
one and the deadlock can't form. The easiest to break in code is circular wait:
**always acquire multiple locks in the same global order.**

The starter has two threads transferring money in opposite directions; each
locks one account then reaches for the other in the opposite order — an AB-BA
cycle.

## Task

1. Run `lab06_starter`. It hangs within a few rounds (the `sleep` widens the
   race window so it's reliable). Kill with Ctrl-C.
2. Optionally watch Helgrind diagnose the lock-order inversion:
   ```sh
   valgrind --tool=helgrind ./build/06_deadlock/lab06_starter
   ```
3. Fix `transfer()` with `std::scoped_lock(from.m, to.m)` so both locks are
   taken atomically. The total must always stay 2000.

## Expected output (solution)

```
a=1000 b=1000 (total should stay 2000)
```

(The per-account balances net out because each round does A→B and B→A by 1.)

## Two ways to avoid deadlock

- **`std::scoped_lock` / `std::lock`** — locks N mutexes with a deadlock-free
  algorithm. Preferred whenever you take more than one lock at once.
- **Global lock ordering** — if you must lock separately, define an order (e.g.
  by address or by a fixed ID) and never violate it.

## QNX / RTOS note

On a priority-preemptive RTOS, deadlock's cousin is **priority inversion**:
a high-priority task blocks on a mutex held by a low-priority task that never
gets scheduled. This famously nearly killed the *Mars Pathfinder* mission. The
fix there is **priority inheritance** on the mutex (QNX:
`PTHREAD_PRIO_INHERIT`), not lock ordering. Lock ordering prevents deadlock;
priority inheritance bounds inversion. An embedded engineer needs both tools.
