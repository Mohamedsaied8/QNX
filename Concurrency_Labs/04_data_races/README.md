# Lab 04 — Data races: read-modify-write is not atomic

## Concept

A **data race** is two threads accessing the same memory location concurrently,
where at least one access is a write, with no synchronization between them. In
C++ a data race is **undefined behaviour** — the program is allowed to do
anything, not merely produce a wrong number.

`++count` looks atomic but compiles to *load → add → store*. If two threads
interleave between the load and the store, they both read the same old value and
one increment vanishes. With a million iterations per thread you lose thousands.

## Task

1. Run `lab04_starter` several times — note the count is wrong and *varies*.
2. Confirm the race with ThreadSanitizer:
   ```sh
   g++ -std=c++17 -g -fsanitize=thread -pthread starter.cpp -o t && ./t
   ```
   TSan prints a "data race" report pointing at `++sample_count`.
3. Fix it two ways: `std::atomic<long>`, and `std::mutex` + `lock_guard`.
   When is each appropriate? (See the discussion in `solution.cpp`.)

## Expected output

```
sample_count = 1683204  (expected 2000000)   # starter — wrong & non-deterministic
atomic_count = 2000000  (expected 2000000)   # solution
mutex_count  = 2000000  (expected 2000000)
```

## Discussion

- **One value, simple op** → `std::atomic` (lock-free, fast).
- **Multiple values that must stay consistent**, or a compound operation →
  `std::mutex` protecting the whole critical section.
- `volatile` does **not** fix this. `volatile` is about memory-mapped I/O, not
  thread synchronization. This is the single most common embedded misconception
  — call it out in lecture.

## QNX / RTOS note

The race exists even on a single-core MCU: an interrupt or a context switch can
preempt the thread *between* the load and the store. On QNX, `std::atomic`
lowers to the CPU's atomic instructions (e.g. `LDREX/STREX` on ARM); a
`pthread_mutex` traps into the microkernel only when contended. For an
interrupt-shared variable specifically, you also need the data to be atomic with
respect to the ISR — another reason to reach for `std::atomic`.
