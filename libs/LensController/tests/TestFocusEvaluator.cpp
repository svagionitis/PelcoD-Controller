/// @file TestFocusEvaluator.cpp
/// @brief Automated unit test suite for zero-heap FocusEvaluator sharpness metrics.

#include "FocusEvaluator.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "LensTypes.h"

using namespace LensControl;

namespace {

std::vector<std::uint8_t> createFlatBuffer(std::size_t width, std::size_t height, std::uint8_t value)
{
    return std::vector<std::uint8_t>(width * height, value);
}

std::vector<std::uint8_t> createStepEdgeBuffer(std::size_t width, std::size_t height)
{
    std::vector<std::uint8_t> buf(width * height, 0U);
    const std::size_t midX = width / 2U;
    for (std::size_t y = 0U; y < height; ++y) {
        for (std::size_t x = midX; x < width; ++x) {
            buf[y * width + x] = 255U;
        }
    }
    return buf;
}

std::vector<std::uint8_t> createBarBuffer(std::size_t width, std::size_t height)
{
    std::vector<std::uint8_t> buf(width * height, 0U);
    const std::size_t startX = width / 4U;
    const std::size_t endX = (3U * width) / 4U;
    for (std::size_t y = 0U; y < height; ++y) {
        for (std::size_t x = startX; x < endX; ++x) {
            buf[y * width + x] = 255U;
        }
    }
    return buf;
}

std::vector<std::uint8_t> boxBlur(
    const std::vector<std::uint8_t>& src, std::size_t width, std::size_t height, int radius)
{
    std::vector<std::uint8_t> dst = src;
    const int w = static_cast<int>(width);
    const int h = static_cast<int>(height);

    for (int y = radius; y < h - radius; ++y) {
        for (int x = radius; x < w - radius; ++x) {
            int sum = 0;
            int count = 0;
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    sum += static_cast<int>(src[static_cast<std::size_t>((y + dy) * w + (x + dx))]);
                    ++count;
                }
            }
            dst[static_cast<std::size_t>(y * w + x)] = static_cast<std::uint8_t>(sum / count);
        }
    }
    return dst;
}

std::vector<std::uint8_t> createCheckerboard(std::size_t width, std::size_t height, std::size_t blockSize)
{
    std::vector<std::uint8_t> buf(width * height, 0U);
    for (std::size_t y = 0U; y < height; ++y) {
        for (std::size_t x = 0U; x < width; ++x) {
            const bool isWhite = ((x / blockSize) + (y / blockSize)) % 2U == 0U;
            buf[y * width + x] = isWhite ? 255U : 0U;
        }
    }
    return buf;
}

} // namespace

TEST(TestFocusEvaluator, ConfigurationGettersSetters)
{
    FocusEvaluator eval;
    EXPECT_EQ(eval.metricType(), FocusMetricType::SobelGradientSquared);

    eval.setMetricType(FocusMetricType::DiscreteLaplacian);
    EXPECT_EQ(eval.metricType(), FocusMetricType::DiscreteLaplacian);

    eval.setMetricType(FocusMetricType::TenengradThreshold);
    EXPECT_EQ(eval.metricType(), FocusMetricType::TenengradThreshold);

    eval.setNoiseThreshold(120U);
    EXPECT_EQ(eval.noiseThreshold(), 120U);

    NormalizedRoi customRoi { };
    customRoi.colFrac = 10U;
    customRoi.rowFrac = 20U;
    customRoi.widthFrac = 200U;
    customRoi.highFrac = 210U;
    eval.setRoi(customRoi);
    EXPECT_EQ(eval.roi().colFrac, 10U);
    EXPECT_EQ(eval.roi().rowFrac, 20U);
    EXPECT_EQ(eval.roi().widthFrac, 200U);
    EXPECT_EQ(eval.roi().highFrac, 210U);
}

TEST(TestFocusEvaluator, NullOrEmptyBufferHandling)
{
    FocusEvaluator eval;
    const std::vector<std::uint8_t> buf = createFlatBuffer(64U, 64U, 128U);

    const FocusMetricScore s1 = eval.evaluate(nullptr, 64U, 64U, 64U, 1000ULL, 1U);
    EXPECT_EQ(s1.metric, 0U);
    EXPECT_EQ(s1.mean, 0U);
    EXPECT_EQ(s1.variance, 0U);
    EXPECT_EQ(s1.pixelCount, 0U);
    EXPECT_EQ(s1.timestampNs, 1000ULL);
    EXPECT_EQ(s1.frameId, 1U);

    const FocusMetricScore s2 = eval.evaluate(buf.data(), 0U, 64U, 64U);
    EXPECT_EQ(s2.metric, 0U);

    const FocusMetricScore s3 = eval.evaluate(buf.data(), 64U, 0U, 64U);
    EXPECT_EQ(s3.metric, 0U);

    const FocusMetricScore s4 = eval.evaluate(buf.data(), 64U, 64U, 32U);
    EXPECT_EQ(s4.metric, 0U);
}

TEST(TestFocusEvaluator, TinySubWindowHandling)
{
    FocusEvaluator eval;
    const std::vector<std::uint8_t> buf = createFlatBuffer(64U, 64U, 128U);

    NormalizedRoi tinyRoi { };
    tinyRoi.colFrac = 0U;
    tinyRoi.rowFrac = 0U;
    tinyRoi.widthFrac = 1U;
    tinyRoi.highFrac = 1U;
    eval.setRoi(tinyRoi);

    const FocusMetricScore s = eval.evaluate(buf.data(), 64U, 64U);
    EXPECT_EQ(s.metric, 0U);
}

TEST(TestFocusEvaluator, UniformFlatImageMetricZero)
{
    FocusEvaluator eval;
    const std::vector<std::uint8_t> buf = createFlatBuffer(64U, 64U, 128U);

    NormalizedRoi fullRoi { };
    fullRoi.colFrac = 0U;
    fullRoi.rowFrac = 0U;
    fullRoi.widthFrac = 255U;
    fullRoi.highFrac = 255U;
    eval.setRoi(fullRoi);

    eval.setMetricType(FocusMetricType::SobelGradientSquared);
    const FocusMetricScore sobelScore = eval.evaluate(buf.data(), 64U, 64U);
    EXPECT_EQ(sobelScore.metric, 0U);
    EXPECT_EQ(sobelScore.mean, 128U);
    EXPECT_EQ(sobelScore.variance, 0U);

    eval.setMetricType(FocusMetricType::DiscreteLaplacian);
    const FocusMetricScore lapScore = eval.evaluate(buf.data(), 64U, 64U);
    EXPECT_EQ(lapScore.metric, 0U);

    eval.setMetricType(FocusMetricType::TenengradThreshold);
    const FocusMetricScore tenScore = eval.evaluate(buf.data(), 64U, 64U);
    EXPECT_EQ(tenScore.metric, 0U);

    eval.setMetricType(FocusMetricType::VollathCorrelationF4);
    const FocusMetricScore f4Score = eval.evaluate(buf.data(), 64U, 64U);
    EXPECT_EQ(f4Score.metric, 0U);
}

TEST(TestFocusEvaluator, SharpStepEdgeHighMetric)
{
    FocusEvaluator eval;
    const std::vector<std::uint8_t> stepBuf = createStepEdgeBuffer(64U, 64U);
    const std::vector<std::uint8_t> barBuf = createBarBuffer(64U, 64U);

    NormalizedRoi fullRoi { };
    fullRoi.colFrac = 0U;
    fullRoi.rowFrac = 0U;
    fullRoi.widthFrac = 255U;
    fullRoi.highFrac = 255U;
    eval.setRoi(fullRoi);

    eval.setMetricType(FocusMetricType::SobelGradientSquared);
    const FocusMetricScore sobelScore = eval.evaluate(stepBuf.data(), 64U, 64U);
    EXPECT_GT(sobelScore.metric, 0U);

    eval.setMetricType(FocusMetricType::DiscreteLaplacian);
    const FocusMetricScore lapScore = eval.evaluate(stepBuf.data(), 64U, 64U);
    EXPECT_GT(lapScore.metric, 0U);

    eval.setMetricType(FocusMetricType::TenengradThreshold);
    const FocusMetricScore tenScore = eval.evaluate(stepBuf.data(), 64U, 64U);
    EXPECT_GT(tenScore.metric, 0U);

    eval.setMetricType(FocusMetricType::VollathCorrelationF4);
    const FocusMetricScore f4Score = eval.evaluate(barBuf.data(), 64U, 64U);
    EXPECT_GT(f4Score.metric, 0U);
}

TEST(TestFocusEvaluator, ProgressiveBlurMonotonicity)
{
    FocusEvaluator eval;
    NormalizedRoi fullRoi { };
    fullRoi.colFrac = 0U;
    fullRoi.rowFrac = 0U;
    fullRoi.widthFrac = 255U;
    fullRoi.highFrac = 255U;
    eval.setRoi(fullRoi);

    const std::vector<std::uint8_t> sharp = createStepEdgeBuffer(64U, 64U);
    const std::vector<std::uint8_t> blur1 = boxBlur(sharp, 64U, 64U, 1);
    const std::vector<std::uint8_t> blur2 = boxBlur(sharp, 64U, 64U, 2);

    eval.setMetricType(FocusMetricType::SobelGradientSquared);
    const std::uint32_t sSharp = eval.evaluate(sharp.data(), 64U, 64U).metric;
    const std::uint32_t sBlur1 = eval.evaluate(blur1.data(), 64U, 64U).metric;
    const std::uint32_t sBlur2 = eval.evaluate(blur2.data(), 64U, 64U).metric;

    EXPECT_GT(sSharp, sBlur1);
    EXPECT_GT(sBlur1, sBlur2);

    eval.setMetricType(FocusMetricType::DiscreteLaplacian);
    const std::uint32_t lSharp = eval.evaluate(sharp.data(), 64U, 64U).metric;
    const std::uint32_t lBlur1 = eval.evaluate(blur1.data(), 64U, 64U).metric;
    const std::uint32_t lBlur2 = eval.evaluate(blur2.data(), 64U, 64U).metric;

    EXPECT_GT(lSharp, lBlur1);
    EXPECT_GT(lBlur1, lBlur2);
}

TEST(TestFocusEvaluator, TenengradNoiseGateRejection)
{
    FocusEvaluator eval(FocusMetricType::TenengradThreshold);
    NormalizedRoi fullRoi { };
    fullRoi.colFrac = 0U;
    fullRoi.rowFrac = 0U;
    fullRoi.widthFrac = 255U;
    fullRoi.highFrac = 255U;
    eval.setRoi(fullRoi);

    std::vector<std::uint8_t> noisyBuf(64U * 64U, 128U);
    for (std::size_t y = 0U; y < 64U; ++y) {
        for (std::size_t x = 0U; x < 64U; ++x) {
            const std::uint8_t noise = static_cast<std::uint8_t>((x * 13U + y * 29U) % 7U);
            noisyBuf[y * 64U + x] = static_cast<std::uint8_t>(125U + noise);
        }
    }

    eval.setNoiseThreshold(0U);
    const std::uint32_t ungated = eval.evaluate(noisyBuf.data(), 64U, 64U).metric;
    EXPECT_GT(ungated, 0U);

    eval.setNoiseThreshold(500U);
    const std::uint32_t gated = eval.evaluate(noisyBuf.data(), 64U, 64U).metric;
    EXPECT_EQ(gated, 0U);
}

TEST(TestFocusEvaluator, VollathF4ThermalNoiseImmunity)
{
    FocusEvaluator eval(FocusMetricType::VollathCorrelationF4);
    NormalizedRoi fullRoi { };
    fullRoi.colFrac = 0U;
    fullRoi.rowFrac = 0U;
    fullRoi.widthFrac = 255U;
    fullRoi.highFrac = 255U;
    eval.setRoi(fullRoi);

    std::vector<std::uint8_t> noisyFlat(64U * 64U, 128U);
    for (std::size_t i = 0U; i < noisyFlat.size(); ++i) {
        noisyFlat[i] = (i % 2U == 0U) ? 130U : 126U;
    }

    const FocusMetricScore score = eval.evaluate(noisyFlat.data(), 64U, 64U);
    EXPECT_EQ(score.metric, 0U);
}

TEST(TestFocusEvaluator, VollathF5DcOffsetSubtraction)
{
    FocusEvaluator eval(FocusMetricType::VollathCorrelationF5);
    NormalizedRoi fullRoi { };
    fullRoi.colFrac = 0U;
    fullRoi.rowFrac = 0U;
    fullRoi.widthFrac = 255U;
    fullRoi.highFrac = 255U;
    eval.setRoi(fullRoi);

    std::vector<std::uint8_t> buf1(64U * 64U, 50U);
    std::vector<std::uint8_t> buf2(64U * 64U, 150U);

    for (std::size_t y = 0U; y < 64U; ++y) {
        for (std::size_t x = 32U; x < 64U; ++x) {
            buf1[y * 64U + x] = 70U;
            buf2[y * 64U + x] = 170U;
        }
    }

    const std::uint32_t m1 = eval.evaluate(buf1.data(), 64U, 64U).metric;
    const std::uint32_t m2 = eval.evaluate(buf2.data(), 64U, 64U).metric;

    EXPECT_GT(m1, 0U);
    EXPECT_GT(m2, 0U);
    EXPECT_NEAR(static_cast<double>(m1), static_cast<double>(m2), 5.0);
}

TEST(TestFocusEvaluator, DctFrequencyHighEnergy)
{
    FocusEvaluator eval(FocusMetricType::DctHighFrequency);
    NormalizedRoi fullRoi { };
    fullRoi.colFrac = 0U;
    fullRoi.rowFrac = 0U;
    fullRoi.widthFrac = 255U;
    fullRoi.highFrac = 255U;
    eval.setRoi(fullRoi);

    const std::vector<std::uint8_t> flat = createFlatBuffer(64U, 64U, 128U);
    const std::vector<std::uint8_t> checker = createCheckerboard(64U, 64U, 4U);

    const std::uint32_t mFlat = eval.evaluate(flat.data(), 64U, 64U).metric;
    const std::uint32_t mChecker = eval.evaluate(checker.data(), 64U, 64U).metric;

    EXPECT_EQ(mFlat, 0U);
    EXPECT_GT(mChecker, 0U);
}

TEST(TestFocusEvaluator, RoiCoordinateClamping)
{
    FocusEvaluator eval;
    NormalizedRoi outOfBoundsRoi { };
    outOfBoundsRoi.colFrac = 240U;
    outOfBoundsRoi.rowFrac = 240U;
    outOfBoundsRoi.widthFrac = 100U;
    outOfBoundsRoi.highFrac = 100U;
    eval.setRoi(outOfBoundsRoi);

    const PixelRoi pixelRoi = FocusEvaluator::calculatePixelRoi(outOfBoundsRoi, 64U, 64U);
    EXPECT_LT(pixelRoi.x, 64U);
    EXPECT_LT(pixelRoi.y, 64U);
    EXPECT_LE(pixelRoi.x + pixelRoi.width, 64U);
    EXPECT_LE(pixelRoi.y + pixelRoi.height, 64U);

    const std::vector<std::uint8_t> buf = createStepEdgeBuffer(64U, 64U);
    const FocusMetricScore score = eval.evaluate(buf.data(), 64U, 64U);
    EXPECT_GT(score.pixelCount, 0U);
}

TEST(TestFocusEvaluator, RowStrideSupport)
{
    FocusEvaluator eval;
    NormalizedRoi fullRoi { };
    fullRoi.colFrac = 0U;
    fullRoi.rowFrac = 0U;
    fullRoi.widthFrac = 255U;
    fullRoi.highFrac = 255U;
    eval.setRoi(fullRoi);

    const std::vector<std::uint8_t> src64 = createStepEdgeBuffer(64U, 64U);

    std::vector<std::uint8_t> strided(128U * 64U, 0U);
    for (std::size_t y = 0U; y < 64U; ++y) {
        for (std::size_t x = 0U; x < 64U; ++x) {
            strided[y * 128U + x] = src64[y * 64U + x];
        }
    }

    const FocusMetricScore scoreContiguous = eval.evaluate(src64.data(), 64U, 64U, 64U);
    const FocusMetricScore scoreStrided = eval.evaluate(strided.data(), 64U, 64U, 128U);

    EXPECT_EQ(scoreContiguous.metric, scoreStrided.metric);
    EXPECT_EQ(scoreContiguous.mean, scoreStrided.mean);
    EXPECT_EQ(scoreContiguous.variance, scoreStrided.variance);
    EXPECT_EQ(scoreContiguous.pixelCount, scoreStrided.pixelCount);
}

TEST(TestFocusEvaluator, MeanAndVarianceAccuracy)
{
    FocusEvaluator eval;
    NormalizedRoi fullRoi { };
    fullRoi.colFrac = 0U;
    fullRoi.rowFrac = 0U;
    fullRoi.widthFrac = 255U;
    fullRoi.highFrac = 255U;
    eval.setRoi(fullRoi);

    std::vector<std::uint8_t> binary(64U * 64U, 0U);
    for (std::size_t y = 0U; y < 64U; ++y) {
        for (std::size_t x = 32U; x < 64U; ++x) {
            binary[y * 64U + x] = 200U;
        }
    }

    const FocusMetricScore score = eval.evaluate(binary.data(), 64U, 64U);
    EXPECT_EQ(score.mean, 100U);
    EXPECT_NEAR(static_cast<double>(score.variance), 10000.0, 1.0);
}
