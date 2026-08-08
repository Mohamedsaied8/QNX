// processing.h — the CPU-bound work each sample requires.
//
// Real pipelines do real work here: digital filtering, FFTs, sensor fusion,
// CRC/encryption, image processing. We simulate a fixed, tunable amount of
// floating-point work so the cost is deterministic and the serial-vs-concurrent
// benchmark is meaningful. Crucially this function is PURE (no shared state), so
// many workers can run it in parallel with zero synchronization — a deliberate
// design choice the project README discusses.
#pragma once

#include <cmath>
#include "types.h"

// Tunable: how heavy each sample is. Higher => parallelism matters more.
inline constexpr int kWorkIterations = 20000;

inline double heavy_compute(double seed)
{
    double acc = seed + 1.0;
    for (int i = 0; i < kWorkIterations; ++i)
        acc = std::sqrt(acc * 1.0000001 + 1.0);   // defeats constant-folding
    return acc;
}

// Process one raw sample into a Result. Stateless and thread-safe.
inline Result process(const Sample& s)
{
    const double processed = heavy_compute(s.raw);
    const auto   now       = Clock::now();
    return Result{
        s.type,
        s.seq,
        processed,
        std::chrono::duration_cast<std::chrono::microseconds>(now - s.sampled_at)
    };
}
