/// @file MultiZoneFocusEvaluator.cpp
#include "MultiZoneFocusEvaluator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace LensControl {

MultiZoneFocusEvaluator::MultiZoneFocusEvaluator(FocusEvaluator& evaluator) noexcept
    : m_evaluator { evaluator }
    , m_mode { MultiZoneArbitrationMode::CenterPriority }
    , m_hasTarget { false }
    , m_targetX { 0.5F }
    , m_targetY { 0.5F }
    , m_targetW { 0.2F }
    , m_targetH { 0.2F }
    , m_prevSubsample { }
    , m_hasPrevFrame { false }
    , m_prevZoneMetric { }
    , m_maxZoneMetric { }
    , m_peakPositions { }
    , m_peakConfirmed { }
{
    reset();
}

void MultiZoneFocusEvaluator::reset() noexcept
{
    m_hasPrevFrame = false;
    for (std::size_t i = 0U; i < kNumZones; ++i) {
        m_prevZoneMetric[i] = 0U;
        m_maxZoneMetric[i] = 0U;
        m_prevPositions[i] = MotorPosition { 0U };
        m_peakPositions[i] = MotorPosition { 0U };
        m_peakConfirmed[i] = false;
    }
}

void MultiZoneFocusEvaluator::setTargetRoi(float normX, float normY, float normW, float normH) noexcept
{
    m_targetX = std::clamp(normX, 0.0F, 1.0F);
    m_targetY = std::clamp(normY, 0.0F, 1.0F);
    m_targetW = std::clamp(normW, 0.01F, 1.0F);
    m_targetH = std::clamp(normH, 0.01F, 1.0F);
    m_hasTarget = true;
}

void MultiZoneFocusEvaluator::clearTargetRoi() noexcept
{
    m_hasTarget = false;
}

void MultiZoneFocusEvaluator::setArbitrationMode(MultiZoneArbitrationMode mode) noexcept
{
    m_mode = mode;
}

MultiZoneArbitrationMode MultiZoneFocusEvaluator::arbitrationMode() const noexcept
{
    return m_mode;
}

PixelRoi MultiZoneFocusEvaluator::zonePixelRoi(
    std::size_t zoneIdx, std::size_t frameWidth, std::size_t frameHeight) noexcept
{
    const std::size_t idx = zoneIdx < kNumZones ? zoneIdx : 4U;
    const std::size_t r = idx / 3U;
    const std::size_t c = idx % 3U;

    const std::size_t x0 = (c * frameWidth) / 3U;
    const std::size_t x1 = ((c + 1U) * frameWidth) / 3U;
    const std::size_t y0 = (r * frameHeight) / 3U;
    const std::size_t y1 = ((r + 1U) * frameHeight) / 3U;

    PixelRoi roi { };
    roi.x = x0;
    roi.y = y0;
    roi.width = x1 > x0 ? (x1 - x0) : 0U;
    roi.height = y1 > y0 ? (y1 - y0) : 0U;
    return roi;
}

NormalizedRoi MultiZoneFocusEvaluator::zoneNormalizedRoi(std::size_t zoneIdx) noexcept
{
    const std::size_t idx = zoneIdx < kNumZones ? zoneIdx : 4U;
    const std::size_t r = idx / 3U;
    const std::size_t c = idx % 3U;

    NormalizedRoi roi { };
    roi.colFrac = static_cast<std::uint8_t>((c * 255U) / 3U);
    roi.rowFrac = static_cast<std::uint8_t>((r * 255U) / 3U);
    roi.widthFrac = static_cast<std::uint8_t>(255U / 3U);
    roi.highFrac = static_cast<std::uint8_t>(255U / 3U);
    return roi;
}

void MultiZoneFocusEvaluator::computeSaliency(const std::uint8_t* grayPixels, std::size_t width, std::size_t height,
    std::size_t stride, std::array<float, kNumZones>& outSaliency) noexcept
{
    for (std::size_t i = 0U; i < kNumZones; ++i) {
        outSaliency[i] = 0.0F;
    }

    if (grayPixels == nullptr || width < 3U || height < 3U) {
        return;
    }

    const std::size_t actualStride = stride > 0U ? stride : width;
    std::array<std::uint64_t, kNumZones> zoneDiffSum { };
    std::array<std::uint32_t, kNumZones> zoneCount { };

    for (std::size_t i = 0U; i < kNumZones; ++i) {
        zoneDiffSum[i] = 0ULL;
        zoneCount[i] = 0U;
    }

    for (std::size_t r = 0U; r < kDownsampleGrid; ++r) {
        const std::size_t py = (r * height) / kDownsampleGrid;
        const std::size_t zoneRow = (r * 3U) / kDownsampleGrid;

        for (std::size_t c = 0U; c < kDownsampleGrid; ++c) {
            const std::size_t px = (c * width) / kDownsampleGrid;
            const std::size_t zoneCol = (c * 3U) / kDownsampleGrid;
            const std::size_t zoneIdx = (zoneRow * 3U) + zoneCol;

            const std::uint8_t currentPixel = grayPixels[(py * actualStride) + px];
            const std::size_t subIdx = (r * kDownsampleGrid) + c;

            if (m_hasPrevFrame) {
                const std::uint8_t prevPixel = m_prevSubsample[subIdx];
                const std::uint32_t diff = currentPixel > prevPixel
                    ? static_cast<std::uint32_t>(currentPixel - prevPixel)
                    : static_cast<std::uint32_t>(prevPixel - currentPixel);

                zoneDiffSum[zoneIdx] += diff;
                zoneCount[zoneIdx] += 1U;
            }

            m_prevSubsample[subIdx] = currentPixel;
        }
    }

    m_hasPrevFrame = true;

    for (std::size_t i = 0U; i < kNumZones; ++i) {
        if (zoneCount[i] > 0U) {
            const float meanDiff = static_cast<float>(zoneDiffSum[i]) / static_cast<float>(zoneCount[i]);
            outSaliency[i] = meanDiff;
        }
    }
}

float MultiZoneFocusEvaluator::computeTargetWeight(std::size_t zoneIdx) const noexcept
{
    if (!m_hasTarget) {
        return 1.0F;
    }

    const std::size_t idx = zoneIdx < kNumZones ? zoneIdx : 4U;
    const std::size_t r = idx / 3U;
    const std::size_t c = idx % 3U;

    const float zx0 = static_cast<float>(c) / 3.0F;
    const float zx1 = static_cast<float>(c + 1U) / 3.0F;
    const float zy0 = static_cast<float>(r) / 3.0F;
    const float zy1 = static_cast<float>(r + 1U) / 3.0F;

    const float tx0 = std::max(0.0F, m_targetX - (m_targetW * 0.5F));
    const float tx1 = std::min(1.0F, m_targetX + (m_targetW * 0.5F));
    const float ty0 = std::max(0.0F, m_targetY - (m_targetH * 0.5F));
    const float ty1 = std::min(1.0F, m_targetY + (m_targetH * 0.5F));

    const float ix0 = std::max(zx0, tx0);
    const float ix1 = std::min(zx1, tx1);
    const float iy0 = std::max(zy0, ty0);
    const float iy1 = std::min(zy1, ty1);

    if (ix1 > ix0 && iy1 > iy0) {
        const float interArea = (ix1 - ix0) * (iy1 - iy0);
        constexpr float zoneArea = (1.0F / 3.0F) * (1.0F / 3.0F);
        const float overlapRatio = interArea / zoneArea;
        return 1.0F + (3.0F * std::clamp(overlapRatio, 0.0F, 1.0F));
    }

    return 0.2F;
}

MultiZoneResult MultiZoneFocusEvaluator::evaluateGrid(const std::uint8_t* grayPixels, std::size_t width,
    std::size_t height, std::size_t stride, MotorPosition currentFocusPos) noexcept
{
    MultiZoneResult result { };
    result.selectedZoneIndex = 4U;
    result.compositeMetric = 0U;
    result.hasForegroundPeak = false;
    result.recommendedPosition = currentFocusPos;

    if (grayPixels == nullptr || width < 3U || height < 3U) {
        return result;
    }

    std::array<float, kNumZones> saliencyValues { };
    computeSaliency(grayPixels, width, height, stride, saliencyValues);

    const NormalizedRoi savedRoi = m_evaluator.roi();

    constexpr std::array<float, kNumZones> spatialWeights = { 0.8F, 1.2F, 0.8F, 1.2F, 2.0F, 1.2F, 0.8F, 1.2F, 0.8F };

    float totalWeight = 0.0F;
    float weightedMetricSum = 0.0F;

    for (std::size_t i = 0U; i < kNumZones; ++i) {
        const NormalizedRoi zoneRoi = zoneNormalizedRoi(i);
        m_evaluator.setRoi(zoneRoi);

        const FocusMetricScore score = m_evaluator.evaluate(grayPixels, width, height, stride);
        result.zones[i].metric = score;

        const float saliency = saliencyValues[i];
        result.zones[i].motionSaliency = saliency;
        const float motionWeight = 1.0F + std::clamp(saliency / 20.0F, 0.0F, 2.0F);

        const float targetWeight = computeTargetWeight(i);
        result.zones[i].targetWeight = targetWeight;

        const float combinedWeight = spatialWeights[i] * motionWeight * targetWeight;
        const float zoneMetricFloat = static_cast<float>(score.metric);
        result.zones[i].compositeScore = zoneMetricFloat * combinedWeight;

        totalWeight += combinedWeight;
        weightedMetricSum += zoneMetricFloat * combinedWeight;

        // Foreground peak detection per zone
        if (score.metric > m_maxZoneMetric[i]) {
            m_maxZoneMetric[i] = score.metric;
        }

        constexpr std::uint32_t kMinSummitMetric = 50U;
        if (!m_peakConfirmed[i] && m_maxZoneMetric[i] >= kMinSummitMetric) {
            if (score.metric < m_prevZoneMetric[i]) {
                m_peakConfirmed[i] = true;
                m_peakPositions[i] = m_prevPositions[i];
            }
        }
        m_prevZoneMetric[i] = score.metric;
        m_prevPositions[i] = currentFocusPos;

        result.zones[i].peakConfirmed = m_peakConfirmed[i];
        result.zones[i].peakPosition = m_peakPositions[i];
    }

    m_evaluator.setRoi(savedRoi);

    if (totalWeight > 0.0F) {
        result.compositeMetric = static_cast<std::uint32_t>(weightedMetricSum / totalWeight);
    }

    // Arbitration
    switch (m_mode) {
    case MultiZoneArbitrationMode::CenterPriority:
        result.selectedZoneIndex = 4U;
        break;

    case MultiZoneArbitrationMode::ClosestSubject: {
        bool foundPeak = false;
        std::size_t bestZone = 4U;
        MotorPosition lowestPeakPos { 65535U };

        for (std::size_t i = 0U; i < kNumZones; ++i) {
            if (m_peakConfirmed[i]) {
                if (!foundPeak || m_peakPositions[i] < lowestPeakPos) {
                    lowestPeakPos = m_peakPositions[i];
                    bestZone = i;
                    foundPeak = true;
                }
            }
        }

        if (foundPeak) {
            result.selectedZoneIndex = bestZone;
            result.hasForegroundPeak = true;
            result.recommendedPosition = lowestPeakPos;
        } else {
            // Fallback to highest composite score
            std::size_t maxZone = 4U;
            float maxScore = -1.0F;
            for (std::size_t i = 0U; i < kNumZones; ++i) {
                if (result.zones[i].compositeScore > maxScore) {
                    maxScore = result.zones[i].compositeScore;
                    maxZone = i;
                }
            }
            result.selectedZoneIndex = maxZone;
        }
        break;
    }

    case MultiZoneArbitrationMode::MotionSaliency: {
        std::size_t maxZone = 4U;
        float maxSaliency = 0.0F;
        for (std::size_t i = 0U; i < kNumZones; ++i) {
            if (result.zones[i].motionSaliency > maxSaliency) {
                maxSaliency = result.zones[i].motionSaliency;
                maxZone = i;
            }
        }
        result.selectedZoneIndex = maxZone;
        break;
    }

    case MultiZoneArbitrationMode::TargetCentering: {
        std::size_t maxZone = 4U;
        float maxTargetWeight = -1.0F;
        for (std::size_t i = 0U; i < kNumZones; ++i) {
            if (result.zones[i].targetWeight > maxTargetWeight) {
                maxTargetWeight = result.zones[i].targetWeight;
                maxZone = i;
            }
        }
        result.selectedZoneIndex = maxZone;
        break;
    }

    case MultiZoneArbitrationMode::FusedWeighted:
    default: {
        std::size_t maxZone = 4U;
        float maxScore = -1.0F;
        for (std::size_t i = 0U; i < kNumZones; ++i) {
            if (result.zones[i].compositeScore > maxScore) {
                maxScore = result.zones[i].compositeScore;
                maxZone = i;
            }
        }
        result.selectedZoneIndex = maxZone;
        break;
    }
    }

    return result;
}

} // namespace LensControl
