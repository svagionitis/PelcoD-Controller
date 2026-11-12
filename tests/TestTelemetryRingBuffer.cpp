/// @file TestTelemetryRingBuffer.cpp
/// @brief Unit tests for TelemetryRingBuffer circular storage and trajectory extraction.

#include <cstdint>
#include <memory>

#include <QCoreApplication>
#include <QSignalSpy>
#include <gtest/gtest.h>

#include "TelemetryRingBuffer.h"
#include "VideoStabilizerEngine.h"

namespace StabilizerApp {

class TelemetryRingBufferTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qApp == nullptr) {
            static int argc = 1;
            static char appName[] = "TestTelemetryRingBuffer";
            static char* argv[] = { appName, nullptr };
            static auto app = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
};

TEST_F(TelemetryRingBufferTest, InitialStateEmpty)
{
    TelemetryRingBuffer ring;
    EXPECT_EQ(ring.capacity(), 150);
    EXPECT_EQ(ring.count(), 0);
    EXPECT_EQ(ring.latestJitter(), 0.0);
    EXPECT_FALSE(ring.isSoftMarginEngaged());
    EXPECT_TRUE(ring.getRawTrajectory(0).isEmpty());
}

TEST_F(TelemetryRingBufferTest, AppendSamplesAndCircularWrap)
{
    TelemetryRingBuffer ring;
    QSignalSpy spy(&ring, &TelemetryRingBuffer::historyUpdated);

    Stabilizer::StabilizerDiagnostics diag { };
    diag.frameIndex = 1U;
    diag.rawTransform = Stabilizer::AffineTransform2D::translation(10.0F, -5.0F);
    diag.smoothedTransform = Stabilizer::AffineTransform2D::translation(8.0F, -4.0F);
    diag.correctionTransform = Stabilizer::AffineTransform2D::translation(-2.0F, 1.0F);

    ring.appendSample(diag);

    EXPECT_EQ(ring.count(), 1);
    EXPECT_EQ(spy.count(), 1);
    // dx = 10 - 8 = 2, dy = -5 - (-4) = -1, jitter = 4 + 1 = 5
    EXPECT_NEAR(ring.latestJitter(), 5.0, 1e-3);

    // Append 200 samples to verify circular wrap at 150
    for (std::uint32_t i = 2U; i <= 200U; ++i) {
        diag.frameIndex = i;
        diag.rawTransform.tx = static_cast<float>(i);
        ring.appendSample(diag);
    }

    EXPECT_EQ(ring.count(), 150);
    const auto rawX = ring.getRawTrajectory(0);
    EXPECT_EQ(rawX.size(), 150);

    const auto jitterHist = ring.getJitterHistory();
    EXPECT_EQ(jitterHist.size(), 150);
}

TEST_F(TelemetryRingBufferTest, SoftMarginEngagement)
{
    TelemetryRingBuffer ring;

    Stabilizer::StabilizerDiagnostics diag { };
    // Large displacement > 80 * 0.8 = 64px
    diag.correctionTransform = Stabilizer::AffineTransform2D::translation(70.0F, 0.0F);

    ring.appendSample(diag);

    EXPECT_TRUE(ring.isSoftMarginEngaged());
    EXPECT_GE(ring.latestMarginUtilization(), 0.80);

    ring.clear();
    EXPECT_EQ(ring.count(), 0);
    EXPECT_FALSE(ring.isSoftMarginEngaged());
}

} // namespace StabilizerApp
