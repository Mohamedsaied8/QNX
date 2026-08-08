# C++ Concurrency Labs

Hands-on labs for the **Embedded Linux & QNX** course, concurrency module.
All code is portable **C++17** and builds with **CMake**. Each lab folder also
contains a short *QNX / RTOS note* connecting the standard-C++ concept to its
real-time embedded counterpart (priorities, scheduling, message passing).

## Why this module exists

Embedded systems are inherently concurrent: a sensor is sampled while a display
is refreshed while a control loop runs while a log is written. Doing this on one
thread of execution means *something is always waiting*. Concurrency lets the CPU
stay responsive and meet deadlines — but it also opens the door to **data races,
deadlocks, and priority inversion**. These labs teach both halves: how to use the
tools, and how the tools break.

## How the labs are structured

Each lab is a self-contained folder:

```
NN_topic/
├── README.md      ← concept, the task, expected output, QNX note
├── starter.cpp    ← compiles, but is INCOMPLETE or contains a deliberate BUG
└── solution.cpp   ← the worked answer
```

Work through `starter.cpp` first (find/fix the bug or fill the TODOs), then
compare with `solution.cpp`. Several starters deliberately misbehave — running a
broken program and *seeing* the race or deadlock is the point.

## Lab index

| # | Topic | Key idea | Bug to find / task |
|---|-------|----------|--------------------|
| 00 | Setup & hardware concurrency | toolchain check, `hardware_concurrency()` | build everything |
| 01 | Creating threads | `std::thread`, `join()` | launch an LCD task |
| 02 | Passing arguments | by-value copy vs `std::ref` | a "lost update" via copy |
| 03 | Thread lifetime | `join` vs `detach`, dangling refs | use-after-free via `detach` |
| 04 | Data races | read-modify-write is not atomic | a torn temperature counter |
| 05 | Mutex & RAII | `lock_guard`, `scoped_lock`, custom guard | a double-lock self-deadlock |
| 06 | Deadlock | lock ordering, `std::scoped_lock` | classic AB–BA deadlock |
| 07 | Condition variables | bounded producer/consumer | busy-wait → CV, clean shutdown |
| 08 | async / future / promise | returning values & exceptions | propagate an exception |
| 09 | Atomics | `std::atomic`, a spinlock | lock-free counter |
| 10 | Thread pool | task queue + workers | finish the pool |
| — | **Project**: sensor pipeline | everything together, measured | build a real-time pipeline |

## Building

Requires CMake ≥ 3.16 and a C++17 compiler (GCC ≥ 7, Clang ≥ 5, or a QNX 7.x
toolchain).

```sh
cd Labs
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

Every lab produces two executables, e.g. `lab04_starter` and `lab04_solution`.
Run one with:

```sh
./build/04_data_races/lab04_solution
```

### Build a single lab without CMake

Each lab is one translation unit, so you can also compile directly:

```sh
g++ -std=c++17 -O2 -pthread 04_data_races/solution.cpp -o /tmp/lab04 && /tmp/lab04
```

On Linux you **must** pass `-pthread`. On QNX, link against `libc` (threads are
in libc) — `qcc -Vgcc_ntoaarch64le -std=gnu++17 file.cpp -o file`.

## Tools every embedded developer should know

These catch concurrency bugs the compiler can't:

```sh
# Data races and use-after-free at runtime:
g++ -std=c++17 -g -fsanitize=thread   -pthread file.cpp -o t && ./t   # ThreadSanitizer
g++ -std=c++17 -g -fsanitize=address  -pthread file.cpp -o a && ./a   # AddressSanitizer

# Deadlocks / lock-order problems:
valgrind --tool=helgrind ./your_program
```

Several labs ask you to confirm the bug with **ThreadSanitizer** — get used to
it; it is the single most valuable tool in this module.

> **If ThreadSanitizer dies with `FATAL: ThreadSanitizer: unexpected memory
> mapping`** you are on a recent Linux kernel whose ASLR entropy
> (`vm.mmap_rnd_bits`) is higher than the sanitizer runtime supports. Two fixes:
> ```sh
> setarch -R ./your_tsan_binary              # per-run: disable ASLR (no sudo)
> sudo sysctl -w vm.mmap_rnd_bits=28         # system-wide, until reboot
> ```
> This is an environment quirk, not a bug in your program.
