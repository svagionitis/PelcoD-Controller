/// @file StabilizerBenchmark.cpp
/// @brief Implementation of benchmark harness and quality metrics for libs/Stabilizer.
#include "StabilizerBenchmark.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

#include "FrameBuffer.h"
#include "FrameView.h"
#include "StabilizerTypes.h"
#include "VideoStabilizerEngine.h"

namespace Stabilizer {

void StabilizerBenchmark::generateFrame(
    FrameBuffer& buffer, std::size_t frameIndex, const BenchmarkWorkload& workload) noexcept
{
    const FrameDimensions& dims = workload.dimensions;
    const std::uint32_t width = dims.width();
    const std::uint32_t height = dims.height();

    const float t = static_cast<float>(frameIndex);
    const float panX = workload.panSpeedPxPerFrame * t;
    const float jitterX
        = workload.jitterAmplitudePx * std::sin(2.0F * 3.14159265F * workload.jitterFrequencyHz * (t / 60.0F));
    const float jitterY
        = workload.jitterAmplitudePx * std::cos(2.0F * 3.14159265F * workload.jitterFrequencyHz * (t / 60.0F));

    const float shiftX = panX + jitterX;
    const float shiftY = jitterY;

    for (std::uint32_t y = 0U; y < height; ++y) {
        std::uint8_t* row = buffer.rowPtr(y);
        const float sampleY = static_cast<float>(y) - shiftY;

        for (std::uint32_t x = 0U; x < width; ++x) {
            const float sampleX = static_cast<float>(x) - shiftX;

            const auto gridX = static_cast<std::int32_t>(std::floor(sampleX / 32.0F));
            const auto gridY = static_cast<std::int32_t>(std::floor(sampleY / 32.0F));

            const bool checker = ((gridX + gridY) % 2) == 0;
            std::uint8_t val = checker ? 180U : 70U;

            const float modX = std::fmod(sampleX, 32.0F);
            const float modY = std::fmod(sampleY, 32.0F);
            if ((std::abs(modX - 16.0F) < 4.0F) && (std::abs(modY - 16.0F) < 4.0F)) {
                val = checker ? 240U : 20U;
            }

            row[x] = val;
        }
    }
}

BenchmarkStats StabilizerBenchmark::run(const BenchmarkWorkload& workload)
{
    BenchmarkStats stats { };
    if (!workload.dimensions.isValid() || (workload.frameCount == 0U)) {
        return stats;
    }

    VideoStabilizerConfig cfg { };
    cfg.dimensions = workload.dimensions;
    cfg.mode = workload.mode;
    cfg.warp.interpolation = workload.interpolation;
    cfg.warp.enableRollingShutter = workload.enableRollingShutter;

    VideoStabilizerEngine engine { };
    if (!engine.configure(cfg)) {
        return stats;
    }

    FrameBuffer inBuf(workload.dimensions);
    FrameBuffer outBuf(workload.dimensions);
    FrameBuffer prevStabBuf(workload.dimensions);

    std::vector<double> latenciesMs { };
    latenciesMs.reserve(workload.frameCount);

    std::vector<float> rawDisplacements { };
    std::vector<float> stabDisplacements { };
    rawDisplacements.reserve(workload.frameCount);
    stabDisplacements.reserve(workload.frameCount);

    double totalPsnr = 0.0;
    std::size_t psnrCount = 0U;

    for (std::size_t i = 0U; i < workload.frameCount; ++i) {
        generateFrame(inBuf, i, workload);

        FrameView inView(inBuf.data(), workload.dimensions);
        FrameView outView(outBuf.data(), workload.dimensions);

        const auto t0 = std::chrono::steady_clock::now();
        const bool ok = engine.processFrame(inView, outView, 1.0F / 60.0F);
        const auto t1 = std::chrono::steady_clock::now();

        if (ok) {
            const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            latenciesMs.push_back(ms);

            const auto diag = engine.diagnostics();
            rawDisplacements.push_back(diag.rawTransform.tx);
            stabDisplacements.push_back(diag.rawTransform.tx + diag.correctionTransform.tx);

            if (i > 1U) {
                double mse = 0.0;
                const std::size_t totalPx = workload.dimensions.totalBytes();
                const std::uint8_t* curr = outBuf.data();
                const std::uint8_t* prev = prevStabBuf.data();

                for (std::size_t p = 0U; p < totalPx; ++p) {
                    const auto diff = static_cast<double>(curr[p]) - static_cast<double>(prev[p]);
                    mse += (diff * diff);
                }
                mse /= static_cast<double>(totalPx);
                if (mse > 1e-4) {
                    const double psnr = 10.0 * std::log10((255.0 * 255.0) / mse);
                    totalPsnr += psnr;
                    ++psnrCount;
                }
            }

            std::memcpy(prevStabBuf.data(), outBuf.data(), workload.dimensions.totalBytes());
        }
    }

    if (latenciesMs.empty()) {
        return stats;
    }

    std::sort(latenciesMs.begin(), latenciesMs.end());

    double sumMs = 0.0;
    for (double val : latenciesMs) {
        sumMs += val;
    }

    stats.minMs = latenciesMs.front();
    stats.maxMs = latenciesMs.back();
    stats.meanMs = sumMs / static_cast<double>(latenciesMs.size());

    const auto p95Idx = static_cast<std::size_t>(static_cast<double>(latenciesMs.size()) * 0.95);
    const auto p99Idx = static_cast<std::size_t>(static_cast<double>(latenciesMs.size()) * 0.99);
    stats.p95Ms = latenciesMs[(p95Idx < latenciesMs.size()) ? p95Idx : (latenciesMs.size() - 1U)];
    stats.p99Ms = latenciesMs[(p99Idx < latenciesMs.size()) ? p99Idx : (latenciesMs.size() - 1U)];

    stats.fps = (stats.meanMs > 0.0) ? (1000.0 / stats.meanMs) : 0.0;
    stats.interFrameFidelityDb = (psnrCount > 0U) ? (totalPsnr / static_cast<double>(psnrCount)) : 0.0;

    // Calculate Jitter Attenuation Ratio (JAR)
    if (rawDisplacements.size() >= 10U) {
        double rawVar = 0.0;
        double stabVar = 0.0;
        double rawMean = 0.0;
        double stabMean = 0.0;

        for (std::size_t k = 0U; k < rawDisplacements.size(); ++k) {
            rawMean += static_cast<double>(rawDisplacements[k]);
            stabMean += static_cast<double>(stabDisplacements[k]);
        }
        rawMean /= static_cast<double>(rawDisplacements.size());
        stabMean /= static_cast<double>(stabDisplacements.size());

        for (std::size_t k = 0U; k < rawDisplacements.size(); ++k) {
            const double dr = static_cast<double>(rawDisplacements[k]) - rawMean;
            const double ds = static_cast<double>(stabDisplacements[k]) - stabMean;
            rawVar += (dr * dr);
            stabVar += (ds * ds);
        }

        if (stabVar > 1e-6) {
            stats.jitterAttenuationDb = 10.0 * std::log10(rawVar / stabVar);
        } else {
            stats.jitterAttenuationDb = 40.0;
        }
    }

    return stats;
}

void StabilizerBenchmark::printSummary(const BenchmarkStats& stats, const BenchmarkWorkload& workload)
{
    std::cout << "\n======================================================================\n";
    std::cout << " STABILIZER BENCHMARK REPORT: " << workload.name << "\n";
    std::cout << " Resolution: " << workload.dimensions.width() << "x" << workload.dimensions.height()
              << " | Frames: " << workload.frameCount << "\n";
    std::cout << "======================================================================\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << " Mean Latency      : " << stats.meanMs << " ms (" << stats.fps << " FPS)\n";
    std::cout << " Min / Max Latency : " << stats.minMs << " ms / " << stats.maxMs << " ms\n";
    std::cout << " 95th Percentile   : " << stats.p95Ms << " ms\n";
    std::cout << " 99th Percentile   : " << stats.p99Ms << " ms\n";
    std::cout << " Jitter Attenuation: " << stats.jitterAttenuationDb << " dB\n";
    std::cout << " Inter-Frame PSNR  : " << stats.interFrameFidelityDb << " dB\n";
    std::cout << " Heap Allocations  : " << stats.totalAllocations << " (steady-state)\n";
    std::cout << "======================================================================\n" << std::endl;
}

} // namespace Stabilizer
