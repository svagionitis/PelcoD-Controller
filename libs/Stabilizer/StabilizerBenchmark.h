/// @file StabilizerBenchmark.h
/// @brief High-precision performance and quality benchmarking harness for libs/Stabilizer.
#pragma once

#include <cstddef>
#include <cstdint>

#include <string>
#include <vector>

#include "FrameBuffer.h"
#include "StabilizerTypes.h"
#include "VideoStabilizerEngine.h"
#include "WarpEngine.h"

namespace Stabilizer {

/// @brief Statistical summary of benchmark execution timings.
struct BenchmarkStats {
    double meanMs { 0.0 };
    double minMs { 0.0 };
    double maxMs { 0.0 };
    double p95Ms { 0.0 };
    double p99Ms { 0.0 };
    double fps { 0.0 };
    double jitterAttenuationDb { 0.0 };
    double interFrameFidelityDb { 0.0 };
    std::size_t totalAllocations { 0U };
};

/// @brief Workload profile configuring benchmark scenario.
struct BenchmarkWorkload {
    std::string name { "1080p60_SyntheticJitter" };
    FrameDimensions dimensions { 1920U, 1080U };
    std::size_t frameCount { 120U };
    float jitterAmplitudePx { 6.0F };
    float jitterFrequencyHz { 12.0F };
    float panSpeedPxPerFrame { 1.5F };
    StabilizationMode mode { StabilizationMode::Affine };
    WarpInterpolation interpolation { WarpInterpolation::Bilinear };
    bool enableRollingShutter { false };
};

/// @brief Automated benchmark execution runner and quality evaluator.
class StabilizerBenchmark {
public:
    StabilizerBenchmark() = default;
    ~StabilizerBenchmark() = default;

    /// @brief Execute a configured benchmark workload.
    /// @param workload Scenario configuration parameters.
    /// @return Benchmark statistics and quality scores.
    [[nodiscard]] BenchmarkStats run(const BenchmarkWorkload& workload);

    /// @brief Generate formatted console summary of benchmark results.
    /// @param stats Computed statistics.
    /// @param workload Workload executed.
    static void printSummary(const BenchmarkStats& stats, const BenchmarkWorkload& workload);

    /// @brief Synthesize a test frame with corner patterns displaced by jitter and pan.
    /// @param buffer Destination frame buffer.
    /// @param frameIndex Frame sequence index.
    /// @param workload Workload profile defining displacement parameters.
    static void generateFrame(FrameBuffer& buffer, std::size_t frameIndex, const BenchmarkWorkload& workload) noexcept;
};

} // namespace Stabilizer
