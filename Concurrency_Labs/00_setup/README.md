# Lab 00 — Setup & hardware concurrency

## Concept

A *thread* is an independently scheduled flow of execution that shares the
process's address space. `std::thread::hardware_concurrency()` reports how many
threads can *truly* run at once (cores × SMT). Spawning more threads than that
is legal and common, but they then **time-slice** — concurrency without
parallelism.

## Task

1. Build the whole lab tree (see `Labs/README.md`).
2. Run `lab00_setup` and note your `hardware_concurrency()` value — you'll refer
   back to it when reasoning about speedup in later labs and the project.

## Expected output (numbers vary by machine)

```
C++ standard in use: 201703
hardware_concurrency() = 8 hardware thread(s)
Hello from a worker thread (id 140...)
Main thread id 140...
Toolchain OK — you are ready for the labs.
```

## QNX / RTOS note

On QNX, threads are POSIX `pthread`s scheduled by the microkernel. Unlike a
general-purpose Linux scheduler, QNX is **priority-preemptive** by default: the
highest-priority ready thread runs immediately, system-wide. That makes
`hardware_concurrency()` only half the story — *which* thread runs is decided by
priority, not fairness. Keep that in mind: on an RTOS, a busy high-priority
thread can starve everything below it.
