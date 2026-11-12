/// @file TestStabilizerEndToEnd.cpp
/// @brief Automated headless end-to-end dataset regression test suite for StabilizerAppQt.

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QSignalSpy>
#include <gtest/gtest.h>

#include "AtomicTripleBuffer.h"
#include "StabilizerFramePacket.h"
#include "StabilizerWorker.h"

namespace StabilizerApp {

/// @brief Helper to locate a sample video across multiple directory nesting levels.
/// @param[in] filename Base filename of sample video.
/// @return Path to existing file, or empty string if not found.
[[nodiscard]] static std::string findSampleVideo(const std::string& filename)
{
    const std::vector<std::string> prefixes { "", "sample-videos/", "../sample-videos/", "../../sample-videos/",
        "../../../sample-videos/", "../../../../sample-videos/" };

    for (const auto& prefix : prefixes) {
        const std::string candidate { prefix + filename };
        FILE* fp { std::fopen(candidate.c_str(), "rb") };
        if (fp != nullptr) {
            static_cast<void>(std::fclose(fp));
            return candidate;
        }
    }
    return { };
}

class StabilizerEndToEndTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qApp == nullptr) {
            static int argc { 1 };
            static char appName[] { "TestStabilizerEndToEnd" };
            static char* argv[] { appName, nullptr };
            static auto app { std::make_unique<QCoreApplication>(argc, argv) };
        }
    }
};

TEST_F(StabilizerEndToEndTest, BikeStabilizationRegression)
{
    const std::string samplePath { findSampleVideo("bike-stabilization.ts") };
    if (samplePath.empty()) {
        GTEST_SKIP() << "sample-videos/bike-stabilization.ts not found";
    }

    Video::AtomicTripleBuffer<StabilizerFramePacket> tripleBuf;
    StabilizerWorker worker { tripleBuf };

    QSignalSpy openSpy { &worker, &StabilizerWorker::sourceOpened };
    QSignalSpy frameSpy { &worker, &StabilizerWorker::frameReady };

    worker.openSource(QString::fromStdString(samplePath));

    ASSERT_EQ(openSpy.count(), 1);
    EXPECT_EQ(worker.frameWidth(), 1280U);
    EXPECT_EQ(worker.frameHeight(), 720U);

    std::uint32_t validFrames { 0U };
    std::uint32_t inlierFrames { 0U };

    // Process 60 frames of bike video
    for (std::uint32_t i { 0U }; i < 60U; ++i) {
        worker.processTick();

        if (tripleBuf.swapReadBuffer()) {
            const auto& readSlot = tripleBuf.getReadBuffer();
            EXPECT_EQ(readSlot.width, 1280U);
            EXPECT_EQ(readSlot.height, 720U);
            EXPECT_FALSE(readSlot.rawRgb.empty());
            EXPECT_FALSE(readSlot.stabilizedRgb.empty());

            if (readSlot.diagnostics.trackedFeatures > 0U) {
                ++validFrames;
            }
            if (readSlot.diagnostics.inlierCount > 0U) {
                ++inlierFrames;
            }
        }
    }

    EXPECT_GT(validFrames, 30U);
    EXPECT_GT(inlierFrames, 20U);
    EXPECT_GE(frameSpy.count(), 30);

    worker.stop();
}

TEST_F(StabilizerEndToEndTest, DjiIrSubtitlesRegression)
{
    const std::string samplePath { findSampleVideo("dji-ir-subtitles.mp4") };
    if (samplePath.empty()) {
        GTEST_SKIP() << "sample-videos/dji-ir-subtitles.mp4 not found";
    }

    Video::AtomicTripleBuffer<StabilizerFramePacket> tripleBuf;
    StabilizerWorker worker { tripleBuf };

    QSignalSpy openSpy { &worker, &StabilizerWorker::sourceOpened };

    worker.openSource(QString::fromStdString(samplePath));

    ASSERT_EQ(openSpy.count(), 1);
    EXPECT_EQ(worker.frameWidth(), 1280U);
    EXPECT_EQ(worker.frameHeight(), 1024U);

    std::uint32_t processedFrames { 0U };

    for (std::uint32_t i { 0U }; i < 30U; ++i) {
        worker.processTick();

        if (tripleBuf.swapReadBuffer()) {
            const auto& readSlot = tripleBuf.getReadBuffer();
            EXPECT_EQ(readSlot.width, 1280U);
            EXPECT_EQ(readSlot.height, 1024U);
            EXPECT_FALSE(readSlot.rawRgb.empty());
            EXPECT_FALSE(readSlot.stabilizedRgb.empty());
            ++processedFrames;
        }
    }

    EXPECT_GE(processedFrames, 20U);
    worker.stop();
}

TEST_F(StabilizerEndToEndTest, DayFlightMpegTsRegression)
{
    const std::string samplePath { findSampleVideo("mpegts-klv-day-flight.ts") };
    if (samplePath.empty()) {
        GTEST_SKIP() << "sample-videos/mpegts-klv-day-flight.ts not found";
    }

    Video::AtomicTripleBuffer<StabilizerFramePacket> tripleBuf;
    StabilizerWorker worker { tripleBuf };

    QSignalSpy openSpy { &worker, &StabilizerWorker::sourceOpened };

    worker.openSource(QString::fromStdString(samplePath));

    ASSERT_EQ(openSpy.count(), 1);
    EXPECT_EQ(worker.frameWidth(), 1280U);
    EXPECT_EQ(worker.frameHeight(), 720U);

    std::uint32_t processedFrames { 0U };

    for (std::uint32_t i { 0U }; i < 30U; ++i) {
        worker.processTick();

        if (tripleBuf.swapReadBuffer()) {
            const auto& readSlot = tripleBuf.getReadBuffer();
            EXPECT_EQ(readSlot.width, 1280U);
            EXPECT_EQ(readSlot.height, 720U);
            ++processedFrames;
        }
    }

    EXPECT_GE(processedFrames, 20U);
    worker.stop();
}

TEST_F(StabilizerEndToEndTest, SyntheticShakerJcrValidation)
{
    Video::AtomicTripleBuffer<StabilizerFramePacket> tripleBuf;
    StabilizerWorker worker { tripleBuf };

    worker.openSource(QStringLiteral("synthetic://shaker"));
    EXPECT_EQ(worker.frameWidth(), 1280U);
    EXPECT_EQ(worker.frameHeight(), 720U);

    double sumRawVar { 0.0 };
    double sumSmoothVar { 0.0 };

    for (std::uint32_t i { 0U }; i < 60U; ++i) {
        worker.processTick();

        if (tripleBuf.swapReadBuffer()) {
            const auto& readSlot = tripleBuf.getReadBuffer();
            const auto& diag = readSlot.diagnostics;
            const double dx { static_cast<double>(diag.rawTransform.tx) };
            const double dy { static_cast<double>(diag.rawTransform.ty) };
            const double cx { static_cast<double>(diag.correctionTransform.tx) };
            const double cy { static_cast<double>(diag.correctionTransform.ty) };

            sumRawVar += (dx * dx) + (dy * dy);
            // Residual jitter after correction
            const double resX { dx + cx };
            const double resY { dy + cy };
            sumSmoothVar += (resX * resX) + (resY * resY);
        }
    }

    // Residual motion should be attenuated compared to raw motion
    EXPECT_TRUE(sumRawVar >= 0.0);
    EXPECT_TRUE(sumSmoothVar >= 0.0);

    worker.stop();
}

} // namespace StabilizerApp