// types.h — data that flows through the pipeline.
#pragma once

#include <chrono>
#include <cstdint>
#include <string>

using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

enum class SensorType { Temperature, Accelerometer, Gps };

inline const char* name(SensorType t)
{
    switch (t) {
        case SensorType::Temperature:   return "Temperature";
        case SensorType::Accelerometer: return "Accelerometer";
        case SensorType::Gps:           return "GPS";
    }
    return "?";
}

// A raw reading as it leaves a sensor.
struct Sample
{
    SensorType type;
    std::uint64_t seq;     // per-sensor sequence number
    double     raw;        // raw measurement
    TimePoint  sampled_at; // when the sensor produced it (for latency)
};

// The result after the (CPU-bound) processing stage.
struct Result
{
    SensorType type;
    std::uint64_t seq;
    double     value;                       // processed/filtered value
    std::chrono::microseconds latency;      // sampled_at -> processed
};
