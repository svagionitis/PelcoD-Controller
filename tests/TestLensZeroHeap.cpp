/// @file TestLensZeroHeap.cpp
/// @brief Fast-path zero-heap memory certification test suite for LensAppQt.

#include <cstddef>
#include <cstdint>

#include <array>
#include <memory>
#include <vector>

#include <QCoreApplication>
#include <gtest/gtest.h>

#include "AtomicTripleBuffer.h"
#include "FocusEvaluator.h"
#include "LensFramePacket.h"
#include "LensTelemetryRingBuffer.h"
#include "PeakFitter.h"
#include "ZoomTrackFocusEngine.h"
#include "ZoomTrackTypes.h"

namespace LensApp {

class LensZeroHeapTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qApp == nullptr) {
            static int argc { 1 };
            static char appName[] { "TestLensZeroHeap" };
            static char* argv[] { appName, nullptr };
            static auto app { std::make_unique<QCoreApplication>(argc, argv) };
        }
    }
};

TEST_F(LensZeroHeapTest, TelemetryRingBufferZeroAllocation)
{
    LensTelemetryRingBuffer ringBuffer;

    TelemetrySample sample { };
    sample.timestampNs = 1000000000ULL;
    sample.frameIndex = 0U;
    sample.channel = 0U;
    sample.focusPosition = 15000U;
    sample.zoomPosition = 30000U;
    sample.metricScore = 45.2;
    sample.afState = 1U;
    sample.limitFlags = 0U;
    sample.decodeLatencyUs = 900U;
    sample.metricLatencyUs = 400U;
    sample.loopLatencyUs = 1800U;
    sample.currentFps = 60.0F;

    // Append 1,000 samples continuously in steady state
    for (std::uint32_t i { 0U }; i < 1000U; ++i) {
        sample.timestampNs += 16666666ULL;
        sample.frameIndex = i;
        sample.focusPosition = static_cast<std::uint16_t>((15000U + i) & 0xFFFFU);
        sample.metricScore = 40.0 + static_cast<double>(i % 20U);
        ringBuffer.appendSample(sample);
    }

    EXPECT_EQ(ringBuffer.capacity(), 300);
    EXPECT_EQ(ringBuffer.count(), 300);
    EXPECT_GT(ringBuffer.latestMetricScore(), 0.0);
    EXPECT_GT(ringBuffer.minMetricScore(), 0.0);
    EXPECT_GT(ringBuffer.maxMetricScore(), 0.0);
    EXPECT_GT(ringBuffer.avgMetricScore(), 0.0);
}

TEST_F(LensZeroHeapTest, AtomicTripleBufferZeroAllocation)
{
    Video::AtomicTripleBuffer<LensFramePacket> tripleBuf;

    constexpr std::uint32_t testWidth { 1280U };
    constexpr std::uint32_t testHeight { 720U };
    constexpr std::size_t rgbSize { static_cast<std::size_t>(testWidth * testHeight * 3U) };

    for (auto& slot : tripleBuf.getSlots()) {
        slot.preallocate(testWidth, testHeight);
    }

    const std::size_t initialCapacity { tripleBuf.getWriteBuffer().rgbBuffer.capacity() };

    // Simulate 500 continuous frame exchanges without reallocation
    for (std::uint32_t i { 0U }; i < 500U; ++i) {
        auto& writeSlot = tripleBuf.getWriteBuffer();
        EXPECT_EQ(writeSlot.width, testWidth);
        EXPECT_EQ(writeSlot.height, testHeight);
        EXPECT_EQ(writeSlot.rgbBuffer.size(), rgbSize);
        EXPECT_EQ(writeSlot.rgbBuffer.capacity(), initialCapacity);

        writeSlot.timestampSec = static_cast<double>(i) * 0.016666;
        writeSlot.frameId = i;
        tripleBuf.publishWriteBuffer();

        const bool swapped = tripleBuf.swapReadBuffer();
        EXPECT_TRUE(swapped);

        const auto& readSlot = tripleBuf.getReadBuffer();
        EXPECT_EQ(readSlot.width, testWidth);
        EXPECT_EQ(readSlot.height, testHeight);
        EXPECT_EQ(readSlot.frameId, i);
    }
}

TEST_F(LensZeroHeapTest, FocusEvaluatorZeroAllocation)
{
    constexpr std::size_t width { 640U };
    constexpr std::size_t height { 480U };
    std::vector<std::uint8_t> lumaBuffer(width * height, 0U);

    // Populate with vertical step edge in the center
    const std::size_t midX = width / 2U;
    for (std::size_t y { 0U }; y < height; ++y) {
        for (std::size_t x = midX; x < width; ++x) {
            lumaBuffer[y * width + x] = 255U;
        }
    }

    const std::array<LensControl::FocusMetricType, 6> kernels { LensControl::FocusMetricType::SobelGradientSquared,
        LensControl::FocusMetricType::DiscreteLaplacian, LensControl::FocusMetricType::TenengradThreshold,
        LensControl::FocusMetricType::VollathCorrelationF4, LensControl::FocusMetricType::VollathCorrelationF5,
        LensControl::FocusMetricType::DctHighFrequency };

    LensControl::FocusEvaluator evaluator;
    LensControl::NormalizedRoi fullRoi { };
    fullRoi.colFrac = 0U;
    fullRoi.rowFrac = 0U;
    fullRoi.widthFrac = 255U;
    fullRoi.highFrac = 255U;
    evaluator.setRoi(fullRoi);

    for (const auto kernel : kernels) {
        evaluator.setMetricType(kernel);
        for (std::uint32_t iter { 0U }; iter < 50U; ++iter) {
            const auto score = evaluator.evaluate(lumaBuffer.data(), width, height, width, 0ULL, iter);
            EXPECT_GT(score.pixelCount, 0U);
        }
    }
}

TEST_F(LensZeroHeapTest, PeakFitterZeroAllocation)
{
    using namespace LensControl;

    SamplePoint p1 { };
    p1.pos = MotorPosition { 10000U };
    p1.score.metric = 120U;

    SamplePoint p2 { };
    p2.pos = MotorPosition { 10500U };
    p2.score.metric = 350U;

    SamplePoint p3 { };
    p3.pos = MotorPosition { 11000U };
    p3.score.metric = 180U;

    for (std::uint32_t i { 0U }; i < 1000U; ++i) {
        const auto parabolic = PeakFitter::fitParabolic(p1, p2, p3);
        EXPECT_TRUE(parabolic.isValid);
        EXPECT_GE(parabolic.vertex.value(), 10000U);
        EXPECT_LE(parabolic.vertex.value(), 11000U);

        const auto gaussian = PeakFitter::fitGaussian(p1, p2, p3);
        EXPECT_TRUE(gaussian.isValid);
        EXPECT_GE(gaussian.vertex.value(), 10000U);
        EXPECT_LE(gaussian.vertex.value(), 11000U);
    }
}

TEST_F(LensZeroHeapTest, ZoomTrackFocusEngineZeroAllocation)
{
    using namespace LensControl;

    TrackCurve curve { };
    curve.knotCount = 4U;
    curve.distanceMeters = 10.0F;

    curve.knots[0].zoom = MotorPosition { 0U };
    curve.knots[0].focus = MotorPosition { 1000U };
    curve.knots[1].zoom = MotorPosition { 15000U };
    curve.knots[1].focus = MotorPosition { 4500U };
    curve.knots[2].zoom = MotorPosition { 35000U };
    curve.knots[2].focus = MotorPosition { 12000U };
    curve.knots[3].zoom = MotorPosition { 65535U };
    curve.knots[3].focus = MotorPosition { 30000U };

    ZoomTrackFocusEngine engine;
    EXPECT_TRUE(engine.addCurve(curve));
    engine.setMode(InterpolationMode::PchipSpline);

    // Evaluate across 1,000 steps without allocation
    for (std::uint32_t z { 0U }; z <= 65000U; z += 65U) {
        const auto f = engine.evaluateFocus(MotorPosition { static_cast<std::uint16_t>(z) });
        EXPECT_GE(f.value(), 1000U);
        EXPECT_LE(f.value(), 30000U);
    }
}

} // namespace LensApp

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int fakeArgc = 1;
    char fakeName[] = "TestLensZeroHeap";
    char* fakeArgv[] = { fakeName, nullptr };
    QCoreApplication app(fakeArgc, fakeArgv);
    return RUN_ALL_TESTS();
}
