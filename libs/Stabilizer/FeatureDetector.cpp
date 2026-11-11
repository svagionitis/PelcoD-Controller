/// @file FeatureDetector.cpp
/// @brief Implementation of Shi-Tomasi corner detector with spatial grid bucketing.
#include "FeatureDetector.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FrameView.h"
#include "StabilizerArena.h"
#include "StabilizerTypes.h"

namespace Stabilizer {

namespace {

    struct CellBucket {
        std::uint16_t count { 0U };
        FeaturePoint points[64] { };
    };

} // namespace

FeatureDetector::FeatureDetector(const FeatureDetectorConfig& config) noexcept
{
    if (config.isValid()) {
        config_ = config;
    }
}

bool FeatureDetector::setConfig(const FeatureDetectorConfig& config) noexcept
{
    if (!config.isValid()) {
        return false;
    }
    config_ = config;
    return true;
}

std::size_t FeatureDetector::detectFeatures(
    const FrameView& frame, StabilizerArena& arena, FeaturePoint* outFeatures, std::size_t maxFeatures) noexcept
{
    if (frame.isEmpty() || (outFeatures == nullptr) || (maxFeatures == 0U) || !config_.isValid()) {
        return 0U;
    }

    const FrameDimensions& dims = frame.dimensions();
    const std::uint32_t width = dims.width();
    const std::uint32_t height = dims.height();
    const std::uint16_t margin = config_.borderMargin;

    if ((width <= static_cast<std::uint32_t>(margin) * 2U) || (height <= static_cast<std::uint32_t>(margin) * 2U)) {
        return 0U;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    auto* gradX = static_cast<std::int16_t*>(arena.allocate(pixelCount * sizeof(std::int16_t), 64U));
    auto* gradY = static_cast<std::int16_t*>(arena.allocate(pixelCount * sizeof(std::int16_t), 64U));
    auto* scores = static_cast<float*>(arena.allocate(pixelCount * sizeof(float), 64U));
    auto* suppressed = static_cast<float*>(arena.allocate(pixelCount * sizeof(float), 64U));

    if ((gradX == nullptr) || (gradY == nullptr) || (scores == nullptr) || (suppressed == nullptr)) {
        return 0U;
    }

    computeGradients(frame, gradX, gradY);

    float maxScore = 0.0F;
    computeCornerScores(frame, gradX, gradY, scores, maxScore);

    if (maxScore <= 0.0F) {
        return 0U;
    }

    const float minThreshold = std::max(1.0F, config_.quality.value() * maxScore);

    suppressNonMax(dims, scores, minThreshold, suppressed);

    return bucketFeatures(dims, suppressed, outFeatures, maxFeatures, arena);
}

void FeatureDetector::computeGradients(const FrameView& frame, std::int16_t* gradX, std::int16_t* gradY) noexcept
{
    const std::uint32_t width = frame.width();
    const std::uint32_t height = frame.height();

    std::memset(gradX, 0, static_cast<std::size_t>(width) * sizeof(std::int16_t));
    std::memset(gradX + (static_cast<std::size_t>(height - 1U) * width), 0,
        static_cast<std::size_t>(width) * sizeof(std::int16_t));
    std::memset(gradY, 0, static_cast<std::size_t>(width) * sizeof(std::int16_t));
    std::memset(gradY + (static_cast<std::size_t>(height - 1U) * width), 0,
        static_cast<std::size_t>(width) * sizeof(std::int16_t));

    for (std::uint32_t y = 1U; y + 1U < height; ++y) {
        const std::uint8_t* prevRow = frame.rowPtr(y - 1U);
        const std::uint8_t* currRow = frame.rowPtr(y);
        const std::uint8_t* nextRow = frame.rowPtr(y + 1U);
        std::int16_t* rowGx = gradX + (static_cast<std::size_t>(y) * width);
        std::int16_t* rowGy = gradY + (static_cast<std::size_t>(y) * width);

        rowGx[0] = 0;
        rowGy[0] = 0;
        rowGx[width - 1U] = 0;
        rowGy[width - 1U] = 0;

        for (std::uint32_t x = 1U; x + 1U < width; ++x) {
            const std::int32_t p00 = static_cast<std::int32_t>(prevRow[x - 1U]);
            const std::int32_t p02 = static_cast<std::int32_t>(prevRow[x + 1U]);

            const std::int32_t p10 = static_cast<std::int32_t>(currRow[x - 1U]);
            const std::int32_t p12 = static_cast<std::int32_t>(currRow[x + 1U]);

            const std::int32_t p20 = static_cast<std::int32_t>(nextRow[x - 1U]);
            const std::int32_t p21 = static_cast<std::int32_t>(nextRow[x]);
            const std::int32_t p22 = static_cast<std::int32_t>(nextRow[x + 1U]);
            const std::int32_t p01 = static_cast<std::int32_t>(prevRow[x]);

            const std::int32_t gx = (p02 - p00) + ((p12 - p10) * 2) + (p22 - p20);
            const std::int32_t gy = (p20 - p00) + ((p21 - p01) * 2) + (p22 - p02);

            rowGx[x] = static_cast<std::int16_t>(gx);
            rowGy[x] = static_cast<std::int16_t>(gy);
        }
    }
}

void FeatureDetector::computeCornerScores(const FrameView& frame, const std::int16_t* gradX, const std::int16_t* gradY,
    float* outScores, float& outMaxScore) noexcept
{
    const std::uint32_t width = frame.width();
    const std::uint32_t height = frame.height();
    const std::size_t totalPixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::memset(outScores, 0, totalPixels * sizeof(float));

    const std::uint16_t margin = config_.borderMargin;
    float peak = 0.0F;

    for (std::uint32_t y = margin; y + margin < height; ++y) {
        const std::int16_t* gXPrev = gradX + (static_cast<std::size_t>(y - 1U) * width);
        const std::int16_t* gXCurr = gradX + (static_cast<std::size_t>(y) * width);
        const std::int16_t* gXNext = gradX + (static_cast<std::size_t>(y + 1U) * width);

        const std::int16_t* gYPrev = gradY + (static_cast<std::size_t>(y - 1U) * width);
        const std::int16_t* gYCurr = gradY + (static_cast<std::size_t>(y) * width);
        const std::int16_t* gYNext = gradY + (static_cast<std::size_t>(y + 1U) * width);

        float* rowScores = outScores + (static_cast<std::size_t>(y) * width);

        for (std::uint32_t x = margin; x + margin < width; ++x) {
            float sumIxx = 0.0F;
            float sumIyy = 0.0F;
            float sumIxy = 0.0F;

            const std::int16_t* const xRows[3] = { gXPrev, gXCurr, gXNext };
            const std::int16_t* const yRows[3] = { gYPrev, gYCurr, gYNext };

            for (std::size_t r = 0U; r < 3U; ++r) {
                for (std::size_t c = 0U; c < 3U; ++c) {
                    const std::size_t px = (x - 1U) + c;
                    const float gx = static_cast<float>(xRows[r][px]);
                    const float gy = static_cast<float>(yRows[r][px]);
                    sumIxx += gx * gx;
                    sumIyy += gy * gy;
                    sumIxy += gx * gy;
                }
            }

            const float trace = sumIxx + sumIyy;
            const float diff = sumIxx - sumIyy;
            const float disc = (diff * diff) + (4.0F * sumIxy * sumIxy);
            const float sqrtDisc = std::sqrt(disc);
            const float lambdaMin = 0.5F * (trace - sqrtDisc);

            if (lambdaMin > 0.0F) {
                rowScores[x] = lambdaMin;
                if (lambdaMin > peak) {
                    peak = lambdaMin;
                }
            }
        }
    }
    outMaxScore = peak;
}

void FeatureDetector::suppressNonMax(
    const FrameDimensions& dims, const float* scores, float minThreshold, float* suppressedScores) noexcept
{
    const std::uint32_t width = dims.width();
    const std::uint32_t height = dims.height();
    const std::size_t totalPixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::memset(suppressedScores, 0, totalPixels * sizeof(float));

    const std::uint16_t margin = config_.borderMargin;

    for (std::uint32_t y = margin; y + margin < height; ++y) {
        const float* prevRow = scores + (static_cast<std::size_t>(y - 1U) * width);
        const float* currRow = scores + (static_cast<std::size_t>(y) * width);
        const float* nextRow = scores + (static_cast<std::size_t>(y + 1U) * width);
        float* outRow = suppressedScores + (static_cast<std::size_t>(y) * width);

        for (std::uint32_t x = margin; x + margin < width; ++x) {
            const float val = currRow[x];
            if (val < minThreshold) {
                continue;
            }

            if ((val < prevRow[x - 1U]) || (val < prevRow[x]) || (val < prevRow[x + 1U]) || (val < currRow[x - 1U])
                || (val <= currRow[x + 1U]) || (val <= nextRow[x - 1U]) || (val <= nextRow[x])
                || (val <= nextRow[x + 1U])) {
                continue;
            }

            outRow[x] = val;
        }
    }
}

std::size_t FeatureDetector::bucketFeatures(const FrameDimensions& dims, const float* suppressedScores,
    FeaturePoint* outFeatures, std::size_t maxFeatures, StabilizerArena& arena) noexcept
{
    const std::uint16_t cols = config_.bucketing.gridCols;
    const std::uint16_t rows = config_.bucketing.gridRows;
    const std::uint16_t maxPerCell = std::min<std::uint16_t>(config_.bucketing.maxFeaturesPerCell, 64U);
    const float minDist = config_.bucketing.minDistance;
    const float minDistSq = minDist * minDist;
    const std::size_t totalCells = static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);

    auto* buckets = static_cast<CellBucket*>(arena.allocate(totalCells * sizeof(CellBucket), 64U));
    if (buckets == nullptr) {
        return 0U;
    }
    std::memset(buckets, 0, totalCells * sizeof(CellBucket));

    const float cellW = static_cast<float>(dims.width()) / static_cast<float>(cols);
    const float cellH = static_cast<float>(dims.height()) / static_cast<float>(rows);
    const std::uint16_t margin = config_.borderMargin;
    const std::uint32_t width = dims.width();
    const std::uint32_t height = dims.height();

    for (std::uint32_t y = margin; y + margin < height; ++y) {
        const float* currRow = suppressedScores + (static_cast<std::size_t>(y) * width);
        const float* prevRow = suppressedScores + (static_cast<std::size_t>(y - 1U) * width);
        const float* nextRow = suppressedScores + (static_cast<std::size_t>(y + 1U) * width);

        for (std::uint32_t x = margin; x + margin < width; ++x) {
            const float center = currRow[x];
            if (center <= 0.0F) {
                continue;
            }

            const float left = currRow[x - 1U];
            const float right = currRow[x + 1U];
            const float top = prevRow[x];
            const float bottom = nextRow[x];

            float subX = static_cast<float>(x);
            float subY = static_cast<float>(y);

            const float denomX = 2.0F * (left - (2.0F * center) + right);
            if (std::abs(denomX) > 1e-4F) {
                const float dx = (left - right) / denomX;
                subX += std::clamp(dx, -0.5F, 0.5F);
            }

            const float denomY = 2.0F * (top - (2.0F * center) + bottom);
            if (std::abs(denomY) > 1e-4F) {
                const float dy = (top - bottom) / denomY;
                subY += std::clamp(dy, -0.5F, 0.5F);
            }

            const std::uint16_t cx = std::min<std::uint16_t>(
                static_cast<std::uint16_t>(subX / cellW), static_cast<std::uint16_t>(cols - 1U));
            const std::uint16_t cy = std::min<std::uint16_t>(
                static_cast<std::uint16_t>(subY / cellH), static_cast<std::uint16_t>(rows - 1U));
            const std::uint16_t cellId = static_cast<std::uint16_t>((cy * cols) + cx);

            FeaturePoint cand { SubPixelPoint { subX, subY }, center, cellId };
            CellBucket& bucket = buckets[cellId];

            bool tooClose = false;
            for (std::uint16_t i = 0U; i < bucket.count; ++i) {
                if (cand.pt.distanceSquared(bucket.points[i].pt) < minDistSq) {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) {
                continue;
            }

            if (bucket.count < maxPerCell) {
                std::size_t insertIdx = bucket.count;
                while ((insertIdx > 0U) && (cand.score > bucket.points[insertIdx - 1U].score)) {
                    bucket.points[insertIdx] = bucket.points[insertIdx - 1U];
                    --insertIdx;
                }
                bucket.points[insertIdx] = cand;
                ++bucket.count;
            } else if (cand.score > bucket.points[bucket.count - 1U].score) {
                std::size_t insertIdx = bucket.count - 1U;
                while ((insertIdx > 0U) && (cand.score > bucket.points[insertIdx - 1U].score)) {
                    bucket.points[insertIdx] = bucket.points[insertIdx - 1U];
                    --insertIdx;
                }
                bucket.points[insertIdx] = cand;
            }
        }
    }

    std::size_t totalOutput = 0U;
    const std::size_t limit = std::min({ maxFeatures, static_cast<std::size_t>(config_.maxTotalFeatures) });

    for (std::uint16_t rank = 0U; rank < maxPerCell; ++rank) {
        for (std::size_t c = 0U; c < totalCells; ++c) {
            if (totalOutput >= limit) {
                break;
            }
            if (rank < buckets[c].count) {
                outFeatures[totalOutput] = buckets[c].points[rank];
                ++totalOutput;
            }
        }
        if (totalOutput >= limit) {
            break;
        }
    }

    return totalOutput;
}

} // namespace Stabilizer
