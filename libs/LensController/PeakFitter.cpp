/// @file PeakFitter.cpp
/// @brief Implementation of sub-step parabolic and Gaussian optical peak fitting algorithms.

#include "PeakFitter.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <cmath>

namespace LensControl {

PeakFitResult PeakFitter::fitParabolic(const SamplePoint& p1, const SamplePoint& p2, const SamplePoint& p3) noexcept
{
    PeakFitResult result { };
    result.vertex = p2.pos;
    result.peakScore = static_cast<float>(p2.score.metric);
    result.isValid = false;

    const auto z1 = static_cast<float>(p1.pos.value());
    const auto z2 = static_cast<float>(p2.pos.value());
    const auto z3 = static_cast<float>(p3.pos.value());

    const auto m1 = static_cast<float>(p1.score.metric);
    const auto m2 = static_cast<float>(p2.score.metric);
    const auto m3 = static_cast<float>(p3.score.metric);

    // Coordinate ordering verification
    if (z1 >= z2 || z2 >= z3) {
        return result;
    }

    // Peak convexity verification (p2 must be strictly greater than neighbors)
    if (m2 <= m1 || m2 <= m3) {
        return result;
    }

    const float h1 = z2 - z1;
    const float h2 = z3 - z2;
    const float denom = h1 * h2 * (h1 + h2);

    if (denom <= 1e-6F) {
        return result;
    }

    const float deltaM1 = m2 - m1;
    const float deltaM2 = m3 - m2;

    const float a = (h1 * deltaM2 - h2 * deltaM1) / denom;
    const float b = ((h1 * h1 * deltaM2) + (h2 * h2 * deltaM1)) / denom;

    // Must be convex upward (a < 0) for a true maximum
    if (a >= 0.0F) {
        return result;
    }

    const float offset = -b / (2.0F * a);

    // Limit offset to half-step interval [-h1/2, +h2/2]
    const float clampedOffset = std::clamp(offset, -h1 * 0.5F, h2 * 0.5F);
    const float vertexFloat = z2 + clampedOffset;
    const float clampedVertex = std::clamp(vertexFloat, 0.0F, 65535.0F);

    result.vertex = MotorPosition { static_cast<std::uint16_t>(std::round(clampedVertex)) };
    result.subStepOffset = clampedOffset;
    result.curvature = a;

    const float fittedScore = (a * clampedOffset * clampedOffset) + (b * clampedOffset) + m2;
    result.peakScore = std::max(m2, fittedScore);
    result.rSquared = 1.0F;
    result.isValid = true;

    return result;
}

PeakFitResult PeakFitter::fitGaussian(const SamplePoint& p1, const SamplePoint& p2, const SamplePoint& p3) noexcept
{
    PeakFitResult result { };
    result.vertex = p2.pos;
    result.peakScore = static_cast<float>(p2.score.metric);
    result.isValid = false;

    const auto m1 = static_cast<float>(p1.score.metric);
    const auto m2 = static_cast<float>(p2.score.metric);
    const auto m3 = static_cast<float>(p3.score.metric);

    // Gaussian fit requires positive metric values
    if (m1 <= 0.0F || m2 <= 0.0F || m3 <= 0.0F) {
        return fitParabolic(p1, p2, p3);
    }

    if (m2 <= m1 || m2 <= m3) {
        return result;
    }

    SamplePoint logP1 = p1;
    SamplePoint logP2 = p2;
    SamplePoint logP3 = p3;

    // Scale ln() results to avoid precision underflow
    logP1.score.metric = static_cast<std::uint32_t>(std::round(std::log(m1) * 1000.0F));
    logP2.score.metric = static_cast<std::uint32_t>(std::round(std::log(m2) * 1000.0F));
    logP3.score.metric = static_cast<std::uint32_t>(std::round(std::log(m3) * 1000.0F));

    const PeakFitResult logFit = fitParabolic(logP1, logP2, logP3);
    if (!logFit.isValid) {
        return fitParabolic(p1, p2, p3);
    }

    result.vertex = logFit.vertex;
    result.subStepOffset = logFit.subStepOffset;
    result.curvature = logFit.curvature / 1000.0F;

    const float logM2 = std::log(m2);
    const float b = logFit.subStepOffset != 0.0F ? -2.0F * result.curvature * logFit.subStepOffset : 0.0F;
    const float fittedLogScore
        = (result.curvature * logFit.subStepOffset * logFit.subStepOffset) + (b * logFit.subStepOffset) + logM2;

    result.peakScore = std::max(m2, std::exp(fittedLogScore));
    result.rSquared = 1.0F;
    result.isValid = true;

    return result;
}

PeakFitResult PeakFitter::fitMultiPoint(const SamplePoint* samples, std::size_t count) noexcept
{
    PeakFitResult bestResult { };
    bestResult.isValid = false;

    if (samples == nullptr || count == 0U) {
        return bestResult;
    }

    if (count < 3U) {
        // Return highest available sample point as discrete fallback
        std::size_t maxIdx = 0U;
        for (std::size_t i = 1U; i < count; ++i) {
            if (samples[i].score.metric > samples[maxIdx].score.metric) {
                maxIdx = i;
            }
        }
        bestResult.vertex = samples[maxIdx].pos;
        bestResult.peakScore = static_cast<float>(samples[maxIdx].score.metric);
        bestResult.isValid = false;
        return bestResult;
    }

    // Spatially sort sample indices without dynamic heap allocation
    constexpr std::size_t kMaxIndexSort = 256U;
    const std::size_t sortCount = std::min(count, kMaxIndexSort);
    std::array<std::size_t, kMaxIndexSort> indices { };
    for (std::size_t i = 0U; i < sortCount; ++i) {
        indices[i] = i;
    }

    std::sort(indices.begin(), indices.begin() + static_cast<std::ptrdiff_t>(sortCount),
        [samples](std::size_t a, std::size_t b) noexcept { return samples[a].pos.value() < samples[b].pos.value(); });

    float highestFittedScore = -1.0F;
    std::size_t globalMaxIdx = 0U;

    for (std::size_t i = 0U; i < sortCount; ++i) {
        const std::size_t curIdx = indices[i];
        if (samples[curIdx].score.metric > samples[globalMaxIdx].score.metric) {
            globalMaxIdx = curIdx;
        }

        if (i >= 1U && i + 1U < sortCount) {
            const std::size_t prevIdx = indices[i - 1U];
            const std::size_t nextIdx = indices[i + 1U];
            const PeakFitResult candidate = fitParabolic(samples[prevIdx], samples[curIdx], samples[nextIdx]);
            if (candidate.isValid && candidate.peakScore > highestFittedScore) {
                highestFittedScore = candidate.peakScore;
                bestResult = candidate;
            }
        }
    }

    if (!bestResult.isValid) {
        bestResult.vertex = samples[globalMaxIdx].pos;
        bestResult.peakScore = static_cast<float>(samples[globalMaxIdx].score.metric);
        bestResult.isValid = false;
    }

    return bestResult;
}

} // namespace LensControl
