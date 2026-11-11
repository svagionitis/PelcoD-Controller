/// @file FocusEvaluator.cpp
/// @brief Implementation of the zero-heap optical sharpness and focus score evaluator.

#include "FocusEvaluator.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>

#include "DctSharpnessEvaluator.h"

namespace LensControl {

FocusEvaluator::FocusEvaluator(FocusMetricType metric) noexcept
    : m_metricType { metric }
    , m_roi { }
    , m_noiseThreshold { 0U }
{
}

FocusMetricScore FocusEvaluator::evaluate(const std::uint8_t* grayPixels, std::size_t width, std::size_t height,
    std::size_t stride, std::uint64_t timestampNs, std::uint32_t frameId) const noexcept
{
    FocusMetricScore score { };
    score.timestampNs = timestampNs;
    score.frameId = frameId;

    if (grayPixels == nullptr || width == 0U || height == 0U) {
        return score;
    }

    const std::size_t actualStride = (stride == 0U) ? width : stride;
    if (actualStride < width) {
        return score;
    }

    const PixelRoi pixelRoi = calculatePixelRoi(m_roi, width, height);
    if (!pixelRoi.isValid()) {
        return score;
    }

    std::uint64_t sumLuma = 0U;
    std::uint64_t sumLumaSq = 0U;
    for (std::size_t y = 0U; y < pixelRoi.height; ++y) {
        const std::uint8_t* row = grayPixels + (pixelRoi.y + y) * actualStride + pixelRoi.x;
        for (std::size_t x = 0U; x < pixelRoi.width; ++x) {
            const std::uint64_t val = static_cast<std::uint64_t>(row[x]);
            sumLuma += val;
            sumLumaSq += val * val;
        }
    }

    const std::size_t totalPixels = pixelRoi.width * pixelRoi.height;
    score.pixelCount = static_cast<std::uint32_t>(totalPixels);

    if (totalPixels > 0U) {
        const std::uint64_t totalPix64 = static_cast<std::uint64_t>(totalPixels);
        const std::uint32_t meanVal = static_cast<std::uint32_t>(sumLuma / totalPix64);
        score.mean = meanVal;

        const std::uint64_t meanSq = static_cast<std::uint64_t>(meanVal) * static_cast<std::uint64_t>(meanVal);
        const std::uint64_t meanOfSquares = sumLumaSq / totalPix64;
        score.variance = (meanOfSquares >= meanSq) ? static_cast<std::uint32_t>(meanOfSquares - meanSq) : 0U;
    }

    switch (m_metricType) {
    case FocusMetricType::SobelGradientSquared:
        score.metric = evalSobel(grayPixels, actualStride, pixelRoi);
        break;
    case FocusMetricType::DiscreteLaplacian:
        score.metric = evalLaplacian(grayPixels, actualStride, pixelRoi);
        break;
    case FocusMetricType::TenengradThreshold:
        score.metric = evalTenengrad(grayPixels, actualStride, pixelRoi);
        break;
    case FocusMetricType::VollathCorrelationF4:
        score.metric = evalVollathF4(grayPixels, actualStride, pixelRoi);
        break;
    case FocusMetricType::VollathCorrelationF5:
        score.metric = evalVollathF5(grayPixels, actualStride, pixelRoi, score.mean);
        break;
    case FocusMetricType::DctHighFrequency:
        score.metric = evalDct(grayPixels, width, height, actualStride);
        break;
    }

    return score;
}

void FocusEvaluator::setMetricType(FocusMetricType metric) noexcept
{
    m_metricType = metric;
}

FocusMetricType FocusEvaluator::metricType() const noexcept
{
    return m_metricType;
}

void FocusEvaluator::setRoi(NormalizedRoi roi) noexcept
{
    m_roi = roi;
}

NormalizedRoi FocusEvaluator::roi() const noexcept
{
    return m_roi;
}

void FocusEvaluator::setNoiseThreshold(std::uint32_t threshold) noexcept
{
    m_noiseThreshold = threshold;
}

std::uint32_t FocusEvaluator::noiseThreshold() const noexcept
{
    return m_noiseThreshold;
}

PixelRoi FocusEvaluator::calculatePixelRoi(NormalizedRoi roi, std::size_t frameWidth, std::size_t frameHeight) noexcept
{
    if (frameWidth == 0U || frameHeight == 0U) {
        return PixelRoi { 0U, 0U, 0U, 0U };
    }

    const std::size_t x = (static_cast<std::size_t>(roi.colFrac) * frameWidth) / 256U;
    const std::size_t y = (static_cast<std::size_t>(roi.rowFrac) * frameHeight) / 256U;

    const std::size_t right
        = (static_cast<std::uint16_t>(roi.colFrac) + static_cast<std::uint16_t>(roi.widthFrac) >= 255U)
        ? frameWidth
        : ((static_cast<std::size_t>(roi.colFrac + roi.widthFrac) * frameWidth) / 256U);

    const std::size_t bottom
        = (static_cast<std::uint16_t>(roi.rowFrac) + static_cast<std::uint16_t>(roi.highFrac) >= 255U)
        ? frameHeight
        : ((static_cast<std::size_t>(roi.rowFrac + roi.highFrac) * frameHeight) / 256U);

    if (x >= frameWidth || y >= frameHeight) {
        return PixelRoi { 0U, 0U, 0U, 0U };
    }

    const std::size_t clampedRight = std::min(right, frameWidth);
    const std::size_t clampedBottom = std::min(bottom, frameHeight);

    const std::size_t w = (clampedRight > x) ? (clampedRight - x) : 0U;
    const std::size_t h = (clampedBottom > y) ? (clampedBottom - y) : 0U;

    return PixelRoi { x, y, w, h };
}

std::uint32_t FocusEvaluator::evalSobel(
    const std::uint8_t* pixels, std::size_t stride, const PixelRoi& roi) const noexcept
{
    if (roi.width < 3U || roi.height < 3U) {
        return 0U;
    }

    std::uint64_t acc = 0U;
    const std::size_t endX = roi.x + roi.width - 1U;
    const std::size_t endY = roi.y + roi.height - 1U;

    for (std::size_t y = roi.y + 1U; y < endY; ++y) {
        const std::uint8_t* rowPrev = pixels + (y - 1U) * stride;
        const std::uint8_t* rowCurr = pixels + y * stride;
        const std::uint8_t* rowNext = pixels + (y + 1U) * stride;

        for (std::size_t x = roi.x + 1U; x < endX; ++x) {
            const int p00 = static_cast<int>(rowPrev[x - 1U]);
            const int p01 = static_cast<int>(rowPrev[x]);
            const int p02 = static_cast<int>(rowPrev[x + 1U]);
            const int p10 = static_cast<int>(rowCurr[x - 1U]);
            const int p12 = static_cast<int>(rowCurr[x + 1U]);
            const int p20 = static_cast<int>(rowNext[x - 1U]);
            const int p21 = static_cast<int>(rowNext[x]);
            const int p22 = static_cast<int>(rowNext[x + 1U]);

            const int gx = (p02 + 2 * p12 + p22) - (p00 + 2 * p10 + p20);
            const int gy = (p20 + 2 * p21 + p22) - (p00 + 2 * p01 + p02);
            acc += static_cast<std::uint64_t>(gx * gx + gy * gy);
        }
    }

    const std::uint64_t numEval
        = static_cast<std::uint64_t>(roi.width - 2U) * static_cast<std::uint64_t>(roi.height - 2U);
    if (numEval == 0U) {
        return 0U;
    }

    const std::uint64_t normalized = acc / numEval;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(normalized, 0xFFFFFFFFULL));
}

std::uint32_t FocusEvaluator::evalLaplacian(
    const std::uint8_t* pixels, std::size_t stride, const PixelRoi& roi) const noexcept
{
    if (roi.width < 3U || roi.height < 3U) {
        return 0U;
    }

    std::uint64_t acc = 0U;
    const std::size_t endX = roi.x + roi.width - 1U;
    const std::size_t endY = roi.y + roi.height - 1U;

    for (std::size_t y = roi.y + 1U; y < endY; ++y) {
        const std::uint8_t* rowPrev = pixels + (y - 1U) * stride;
        const std::uint8_t* rowCurr = pixels + y * stride;
        const std::uint8_t* rowNext = pixels + (y + 1U) * stride;

        for (std::size_t x = roi.x + 1U; x < endX; ++x) {
            const int c = static_cast<int>(rowCurr[x]);
            const int l = static_cast<int>(rowCurr[x - 1U]);
            const int r = static_cast<int>(rowCurr[x + 1U]);
            const int u = static_cast<int>(rowPrev[x]);
            const int d = static_cast<int>(rowNext[x]);

            const int lap = 4 * c - l - r - u - d;
            acc += static_cast<std::uint64_t>(lap * lap);
        }
    }

    const std::uint64_t numEval
        = static_cast<std::uint64_t>(roi.width - 2U) * static_cast<std::uint64_t>(roi.height - 2U);
    if (numEval == 0U) {
        return 0U;
    }

    const std::uint64_t normalized = acc / numEval;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(normalized, 0xFFFFFFFFULL));
}

std::uint32_t FocusEvaluator::evalTenengrad(
    const std::uint8_t* pixels, std::size_t stride, const PixelRoi& roi) const noexcept
{
    if (roi.width < 3U || roi.height < 3U) {
        return 0U;
    }

    std::uint64_t acc = 0U;
    const std::size_t endX = roi.x + roi.width - 1U;
    const std::size_t endY = roi.y + roi.height - 1U;
    const std::uint64_t thresh = static_cast<std::uint64_t>(m_noiseThreshold);

    for (std::size_t y = roi.y + 1U; y < endY; ++y) {
        const std::uint8_t* rowPrev = pixels + (y - 1U) * stride;
        const std::uint8_t* rowCurr = pixels + y * stride;
        const std::uint8_t* rowNext = pixels + (y + 1U) * stride;

        for (std::size_t x = roi.x + 1U; x < endX; ++x) {
            const int p00 = static_cast<int>(rowPrev[x - 1U]);
            const int p01 = static_cast<int>(rowPrev[x]);
            const int p02 = static_cast<int>(rowPrev[x + 1U]);
            const int p10 = static_cast<int>(rowCurr[x - 1U]);
            const int p12 = static_cast<int>(rowCurr[x + 1U]);
            const int p20 = static_cast<int>(rowNext[x - 1U]);
            const int p21 = static_cast<int>(rowNext[x]);
            const int p22 = static_cast<int>(rowNext[x + 1U]);

            const int gx = (p02 + 2 * p12 + p22) - (p00 + 2 * p10 + p20);
            const int gy = (p20 + 2 * p21 + p22) - (p00 + 2 * p01 + p02);
            const std::uint64_t g2 = static_cast<std::uint64_t>(gx * gx + gy * gy);

            if (g2 > thresh) {
                acc += g2;
            }
        }
    }

    const std::uint64_t numEval
        = static_cast<std::uint64_t>(roi.width - 2U) * static_cast<std::uint64_t>(roi.height - 2U);
    if (numEval == 0U) {
        return 0U;
    }

    const std::uint64_t normalized = acc / numEval;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(normalized, 0xFFFFFFFFULL));
}

std::uint32_t FocusEvaluator::evalVollathF4(
    const std::uint8_t* pixels, std::size_t stride, const PixelRoi& roi) const noexcept
{
    if (roi.width < 3U || roi.height == 0U) {
        return 0U;
    }

    std::int64_t term1 = 0;
    std::int64_t term2 = 0;
    const std::size_t endX = roi.x + roi.width - 2U;
    const std::size_t endY = roi.y + roi.height;

    for (std::size_t y = roi.y; y < endY; ++y) {
        const std::uint8_t* row = pixels + y * stride;
        for (std::size_t x = roi.x; x < endX; ++x) {
            const std::int64_t p0 = static_cast<std::int64_t>(row[x]);
            const std::int64_t p1 = static_cast<std::int64_t>(row[x + 1U]);
            const std::int64_t p2 = static_cast<std::int64_t>(row[x + 2U]);
            term1 += (p0 * p1);
            term2 += (p0 * p2);
        }
    }

    const std::int64_t diff = term1 - term2;
    if (diff <= 0) {
        return 0U;
    }

    const std::uint64_t numEval = static_cast<std::uint64_t>(roi.width - 2U) * static_cast<std::uint64_t>(roi.height);
    if (numEval == 0U) {
        return 0U;
    }

    const std::uint64_t normalized = static_cast<std::uint64_t>(diff) / numEval;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(normalized, 0xFFFFFFFFULL));
}

std::uint32_t FocusEvaluator::evalVollathF5(
    const std::uint8_t* pixels, std::size_t stride, const PixelRoi& roi, std::uint32_t meanLuma) const noexcept
{
    (void)meanLuma;
    if (roi.width < 2U || roi.height == 0U) {
        return 0U;
    }

    std::int64_t term1 = 0;
    std::int64_t sum0 = 0;
    std::int64_t sum1 = 0;
    const std::size_t endX = roi.x + roi.width - 1U;
    const std::size_t endY = roi.y + roi.height;

    for (std::size_t y = roi.y; y < endY; ++y) {
        const std::uint8_t* row = pixels + y * stride;
        for (std::size_t x = roi.x; x < endX; ++x) {
            const std::int64_t p0 = static_cast<std::int64_t>(row[x]);
            const std::int64_t p1 = static_cast<std::int64_t>(row[x + 1U]);
            term1 += (p0 * p1);
            sum0 += p0;
            sum1 += p1;
        }
    }

    const double numEval = static_cast<double>((roi.width - 1U) * roi.height);
    if (numEval <= 0.0) {
        return 0U;
    }

    const double diff = static_cast<double>(term1) - (static_cast<double>(sum0) * static_cast<double>(sum1)) / numEval;
    if (diff <= 0.0) {
        return 0U;
    }

    const double normalized = diff / numEval;
    return static_cast<std::uint32_t>(std::min<double>(normalized, 4294967295.0));
}

std::uint32_t FocusEvaluator::evalDct(
    const std::uint8_t* pixels, std::size_t width, std::size_t height, std::size_t stride) const noexcept
{
    if (width < 8U || height < 8U) {
        return 0U;
    }

    Math::DctSharpnessConfig config { };
    config.mode = Math::DctMetricMode::HighFrequencyAcSum;
    config.minFrequencySum = 3;
    config.blockStride = 8;
    config.roiNormX = static_cast<double>(m_roi.colFrac) / 256.0;
    config.roiNormY = static_cast<double>(m_roi.rowFrac) / 256.0;
    config.roiNormWidth = static_cast<double>(m_roi.widthFrac) / 256.0;
    config.roiNormHeight = static_cast<double>(m_roi.highFrac) / 256.0;
    config.enableNormalization = true;

    const Math::DctSharpnessEvaluator dctEval { config };
    const Math::SharpnessResult result
        = dctEval.evaluate(pixels, static_cast<int>(width), static_cast<int>(height), static_cast<int>(stride));

    if (result.blocksEvaluated == 0U || result.normalizedScore <= 0.0) {
        return 0U;
    }

    const double scaled = result.normalizedScore * 1000.0;
    return static_cast<std::uint32_t>(std::min<double>(scaled, 4294967295.0));
}

} // namespace LensControl
