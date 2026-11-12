/// @file TestStabilizerZeroHeap.cpp
/// @brief Fast-path zero-heap memory certification test suite for StabilizerAppQt.

#include <cstddef>
#include <cstdint>

#include <memory>
#include <vector>

#include <QCoreApplication>
#include <gtest/gtest.h>

#include "AtomicTripleBuffer.h"
#include "StabilizerFramePacket.h"
#include "StabilizerWorker.h"
#include "TelemetryRingBuffer.h"

namespace StabilizerApp {

class StabilizerZeroHeapTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qApp == nullptr) {
            static int argc { 1 };
            static char appName[] { "TestStabilizerZeroHeap" };
            static char* argv[] { appName, nullptr };
            static auto app { std::make_unique<QCoreApplication>(argc, argv) };
        }
    }
};

TEST_F(StabilizerZeroHeapTest, TelemetryRingBufferZeroAllocation)
{
    TelemetryRingBuffer ringBuffer;

    Stabilizer::StabilizerDiagnostics sample { };
    sample.latencyUs.pyramidGenUs = 800U;
    sample.latencyUs.featureDetectUs = 1500U;
    sample.latencyUs.opticalFlowUs = 2500U;
    sample.latencyUs.motionModelUs = 900U;
    sample.latencyUs.trajectoryFilterUs = 300U;
    sample.latencyUs.warpSynthesisUs = 3200U;
    sample.latencyUs.totalFrameUs = 9200U;
    sample.trackedFeatures = 120U;
    sample.inlierCount = 95U;

    // Append 500 samples in steady state
    for (std::uint32_t i { 0U }; i < 500U; ++i) {
        sample.rawTransform.tx = static_cast<float>(i % 50U);
        sample.rawTransform.ty = static_cast<float>((i * 2U) % 50U);
        sample.smoothedTransform.tx = sample.rawTransform.tx * 0.5F;
        sample.smoothedTransform.ty = sample.rawTransform.ty * 0.5F;
        ringBuffer.appendSample(sample);
    }

    EXPECT_EQ(ringBuffer.capacity(), 150);
    EXPECT_EQ(ringBuffer.count(), 150);
    EXPECT_GE(ringBuffer.latestJitter(), 0.0);
    EXPECT_GE(ringBuffer.latestMarginUtilization(), 0.0);

    // Verify snapshot retrieval operates cleanly
    const auto rawX = ringBuffer.getRawTrajectory(0);
    const auto smoothX = ringBuffer.getSmoothTrajectory(0);
    EXPECT_EQ(rawX.size(), 150);
    EXPECT_EQ(smoothX.size(), 150);
}

TEST_F(StabilizerZeroHeapTest, AtomicTripleBufferZeroAllocation)
{
    Video::AtomicTripleBuffer<StabilizerFramePacket> tripleBuf;

    // Preallocate slots for 720p
    constexpr std::uint32_t testWidth { 1280U };
    constexpr std::uint32_t testHeight { 720U };
    for (auto& slot : tripleBuf.getSlots()) {
        slot.preallocate(testWidth, testHeight);
    }

    // Simulate 500 continuous frame exchanges without reallocation
    for (std::uint32_t i { 0U }; i < 500U; ++i) {
        auto& writeSlot = tripleBuf.getWriteBuffer();
        EXPECT_EQ(writeSlot.width, testWidth);
        EXPECT_EQ(writeSlot.height, testHeight);
        EXPECT_EQ(writeSlot.rawRgb.size(), static_cast<std::size_t>(testWidth * testHeight * 3U));
        EXPECT_EQ(writeSlot.stabilizedRgb.size(), static_cast<std::size_t>(testWidth * testHeight * 3U));

        writeSlot.timestampSec = static_cast<double>(i) * 0.033;
        tripleBuf.publishWriteBuffer();

        const bool swapped = tripleBuf.swapReadBuffer();
        EXPECT_TRUE(swapped);

        const auto& readSlot = tripleBuf.getReadBuffer();
        EXPECT_EQ(readSlot.width, testWidth);
        EXPECT_EQ(readSlot.height, testHeight);
        EXPECT_DOUBLE_EQ(readSlot.timestampSec, static_cast<double>(i) * 0.033);
    }
}

TEST_F(StabilizerZeroHeapTest, WorkerSteadyStateExecution)
{
    Video::AtomicTripleBuffer<StabilizerFramePacket> tripleBuf;
    StabilizerWorker worker { tripleBuf };

    worker.openSource(QStringLiteral("synthetic://shaker"));
    EXPECT_EQ(worker.frameWidth(), 1280U);
    EXPECT_EQ(worker.frameHeight(), 720U);

    // Warm-up pipeline
    for (std::uint32_t i { 0U }; i < 10U; ++i) {
        worker.processTick();
        static_cast<void>(tripleBuf.swapReadBuffer());
    }

    // Execute 100 steady-state ticks
    for (std::uint32_t i { 0U }; i < 100U; ++i) {
        worker.processTick();
        const bool swapped = tripleBuf.swapReadBuffer();
        EXPECT_TRUE(swapped);
        const auto& readSlot = tripleBuf.getReadBuffer();
        EXPECT_EQ(readSlot.width, 1280U);
        EXPECT_EQ(readSlot.height, 720U);
    }

    worker.stop();
}

} // namespace StabilizerApp