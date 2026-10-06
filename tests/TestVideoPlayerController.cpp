/// @file TestVideoPlayerController.cpp
/// @brief Tests for VideoPlayerController timeline seeking, scrubbing, and KLV telemetry.

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>
#include <gtest/gtest.h>

#include "DecoderFactory.h"
#include "LatencyTracker.h"
#include "VideoPlayerController.h"
#include "VideoQuickItem.h"

#if defined(PELCOD_HAS_DJI)
#include "Mp4TestBuilder.h"
#endif

#include <algorithm>
#include <iterator>

namespace VideoApp {

class VideoPlayerControllerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qApp == nullptr) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
            static int argc = 1;
            static char appName[] = "TestVideoPlayerController";
            static char* argv[] = { appName, nullptr };
            static auto app = std::make_unique<QGuiApplication>(argc, argv);
        }
    }
};

TEST_F(VideoPlayerControllerTest, BackwardScrubbingRewinds)
{
    VideoPlayerController controller {};

    // Set up a mock KLV timeline with two distinct waypoints
    VideoPlayerController::TimedKlv p1 {};
    p1.timeSeconds = 0.0;
    p1.message.platformHeadingDeg = 86.0;
    p1.message.sensorLatitudeDeg = 54.0;
    p1.message.sensorLongitudeDeg = -110.0;

    VideoPlayerController::TimedKlv p2 {};
    p2.timeSeconds = 88.0;
    p2.message.platformHeadingDeg = 82.0;
    p2.message.sensorLatitudeDeg = 55.0;
    p2.message.sensorLongitudeDeg = -111.0;

    controller.addKlvTimelineEntry(p1);
    controller.addKlvTimelineEntry(p2);

    // Initial state: t = 0.0
    controller.updateKlvTelemetry(0.0);
    EXPECT_DOUBLE_EQ(controller.platformHeading(), 86.0);
    EXPECT_EQ(controller.lastKlvIndex(), 0U);

    // Scrub forward past 88.0s -> telemetry advances to waypoint 2
    controller.updateKlvTelemetry(90.0);
    EXPECT_DOUBLE_EQ(controller.platformHeading(), 82.0);
    EXPECT_EQ(controller.lastKlvIndex(), 1U);

    // Scrub backward to 10.0s -> telemetry MUST rewind to waypoint 1
    controller.updateKlvTelemetry(10.0);
    EXPECT_DOUBLE_EQ(controller.platformHeading(), 86.0);
    EXPECT_EQ(controller.lastKlvIndex(), 0U);
}

TEST_F(VideoPlayerControllerTest, ScrubbingBoundaryConditions)
{
    VideoPlayerController controller {};

    VideoPlayerController::TimedKlv p1 {};
    p1.timeSeconds = 10.0;
    p1.message.platformHeadingDeg = 10.0;

    VideoPlayerController::TimedKlv p2 {};
    p2.timeSeconds = 20.0;
    p2.message.platformHeadingDeg = 20.0;

    VideoPlayerController::TimedKlv p3 {};
    p3.timeSeconds = 30.0;
    p3.message.platformHeadingDeg = 30.0;

    controller.addKlvTimelineEntry(p1);
    controller.addKlvTimelineEntry(p2);
    controller.addKlvTimelineEntry(p3);

    // Before first message
    controller.updateKlvTelemetry(5.0);
    EXPECT_DOUBLE_EQ(controller.platformHeading(), 10.0);
    EXPECT_EQ(controller.lastKlvIndex(), 0U);

    // Exact match on middle waypoint
    controller.updateKlvTelemetry(20.0);
    EXPECT_DOUBLE_EQ(controller.platformHeading(), 20.0);
    EXPECT_EQ(controller.lastKlvIndex(), 1U);

    // Past last waypoint
    controller.updateKlvTelemetry(50.0);
    EXPECT_DOUBLE_EQ(controller.platformHeading(), 30.0);
    EXPECT_EQ(controller.lastKlvIndex(), 2U);

    // Scrub back to middle
    controller.updateKlvTelemetry(25.0);
    EXPECT_DOUBLE_EQ(controller.platformHeading(), 20.0);
    EXPECT_EQ(controller.lastKlvIndex(), 1U);

    // Scrub back to start
    controller.updateKlvTelemetry(0.0);
    EXPECT_DOUBLE_EQ(controller.platformHeading(), 10.0);
    EXPECT_EQ(controller.lastKlvIndex(), 0U);
}

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
            std::fclose(fp);
            return candidate;
        }
    }
    return {};
}

TEST_F(VideoPlayerControllerTest, LoadKlvSampleAndScrub)
{
    const std::string samplePath { findSampleVideo("mpegts-klv-day-flight.ts") };
    if (samplePath.empty()) {
        GTEST_SKIP() << "mpegts-klv-day-flight.ts not found";
    }

    VideoPlayerController controller {};
    controller.loadKlvTrack(QString::fromStdString(samplePath));

    ASSERT_EQ(controller.klvTimelineSize(), 6U);
    EXPECT_EQ(controller.lastKlvIndex(), 0U);

    // Initial heading at start of flight video (~86.8 degrees)
    const double initialHeading { controller.platformHeading() };
    EXPECT_GT(initialHeading, 80.0);

    // Scrub past the 88.75s dead gap to 90.0s -> waypoint 1 (t=88.751s)
    controller.updateKlvTelemetry(90.0);
    EXPECT_EQ(controller.lastKlvIndex(), 1U);
    const double headingAt90 { controller.platformHeading() };
    EXPECT_NE(headingAt90, initialHeading);

    // Scrub forward to 100.0s -> waypoint 2 (t=94.859s)
    controller.updateKlvTelemetry(100.0);
    EXPECT_EQ(controller.lastKlvIndex(), 2U);

    // Scrub backward into the 88.75s dead gap to 10.0s -> MUST rewind to waypoint 0 (t=0.0s)
    controller.updateKlvTelemetry(10.0);
    EXPECT_EQ(controller.lastKlvIndex(), 0U);
    EXPECT_DOUBLE_EQ(controller.platformHeading(), initialHeading);

    // Scrub back to 0.0s
    controller.updateKlvTelemetry(0.0);
    EXPECT_EQ(controller.lastKlvIndex(), 0U);
    EXPECT_DOUBLE_EQ(controller.platformHeading(), initialHeading);
}

#if defined(PELCOD_HAS_DJI)
TEST_F(VideoPlayerControllerTest, LoadDjiMp4TelemetryAndScrub)
{
    DjiTest::Mp4BuildOptions opts {};
    opts.texts = {
        "FrameCnt: 0 2026-09-24 15:20:55.713\n[latitude: 38.375988] [longitude: 23.257121] [abs_alt: 169.5]",
        "FrameCnt: 1 2026-09-24 15:20:55.746\n[latitude: 38.376000] [longitude: 23.257200] [abs_alt: 169.6]",
        "FrameCnt: 2 2026-09-24 15:20:55.779\n[latitude: 38.376100] [longitude: 23.257300] [abs_alt: 169.7]",
    };
    const DjiTest::TempFile file { DjiTest::buildMp4(opts), "controller_dji" };

    VideoPlayerController controller {};
    controller.loadKlvTrack(QString::fromStdString(file.path()));

    ASSERT_EQ(controller.klvTimelineSize(), 3U);
    EXPECT_NEAR(controller.klvTimeAt(2U), 2.0 / 30.0, 1e-9);
    EXPECT_NEAR(controller.platformLatitude(), 38.375988, 1e-9);

    controller.updateKlvTelemetry(0.05); // between frame 1 (0.033) and frame 2 (0.067)
    EXPECT_EQ(controller.lastKlvIndex(), 1U);
    EXPECT_NEAR(controller.platformLongitude(), 23.2572, 1e-9);
}

TEST_F(VideoPlayerControllerTest, Mp4WithoutDjiTrackLeavesTimelineEmpty)
{
    DjiTest::Mp4BuildOptions opts {};
    opts.texts = { "[latitude: 1.0] [longitude: 2.0]" };
    opts.includeTextTrack = false;
    const DjiTest::TempFile file { DjiTest::buildMp4(opts), "controller_plain_mp4" };

    VideoPlayerController controller {};
    controller.loadKlvTrack(QString::fromStdString(file.path()));
    EXPECT_EQ(controller.klvTimelineSize(), 0U);
}
#endif

namespace {

    /// @brief Paints the item onto an offscreen canvas and simulates a scene graph swap.
    /// @param[in,out] item Video item under test.
    void presentFrame(VideoQuickItem& item)
    {
        QImage canvas(320, 240, QImage::Format_RGB32);
        QPainter painter(&canvas);
        item.paint(&painter);
        painter.end();
        item.notifyFrameSwapped();
        item.publishLatency();
    }

} // namespace

TEST_F(VideoPlayerControllerTest, VideoItemMeasuresDisplayLatency)
{
    VideoQuickItem item {};
    item.setSize(QSizeF(320.0, 240.0));
    EXPECT_DOUBLE_EQ(item.displayLatencyMs(), 0.0);
    EXPECT_EQ(item.presentedFrames(), 0U);

    QImage testFrame(320, 240, QImage::Format_RGB888);
    testFrame.fill(Qt::green);

    constexpr qint64 kFiveMsNs { 5'000'000 };
    item.updateTimedFrame(testFrame, static_cast<qint64>(Video::steadyNowNs()) - kFiveMsNs);

    QSignalSpy latSpy(&item, &VideoQuickItem::displayLatencyChanged);
    presentFrame(item);

    EXPECT_EQ(latSpy.count(), 1);
    EXPECT_EQ(item.presentedFrames(), 1U);
    EXPECT_GE(item.displayLatencyLastMs(), 5.0);
    EXPECT_GE(item.displayLatencyMs(), 5.0);
    EXPECT_LE(item.displayLatencyMinMs(), item.displayLatencyMaxMs());

    // Repainting the same frame must not be counted again
    presentFrame(item);
    EXPECT_EQ(item.presentedFrames(), 1U);

    // Unstamped frames are displayed but not measured
    item.updateFrame(testFrame);
    presentFrame(item);
    EXPECT_EQ(item.presentedFrames(), 1U);

    item.resetLatency();
    EXPECT_EQ(item.presentedFrames(), 0U);
    EXPECT_DOUBLE_EQ(item.displayLatencyMs(), 0.0);
}

TEST_F(VideoPlayerControllerTest, ControllerFeedsDisplayLatency)
{
    VideoPlayerController controller {};
    VideoQuickItem item {};
    item.setSize(QSizeF(320.0, 240.0));
    controller.attachVideoItem(&item);

    const auto backends { Video::DecoderFactory::availableBackends() };
    const auto mockIt { std::find(backends.cbegin(), backends.cend(), Video::BackendType::Mock) };
    ASSERT_NE(mockIt, backends.cend());
    controller.setBackendIndex(static_cast<int>(std::distance(backends.cbegin(), mockIt)));
    controller.startPlayback();

    for (int i = 0; (i < 40) && (item.presentedFrames() == 0U); ++i) {
        QTest::qWait(30);
        presentFrame(item);
    }
    controller.stopPlayback();

    EXPECT_GT(item.presentedFrames(), 0U);
    EXPECT_GT(item.displayLatencyLastMs(), 0.0);
}

} // namespace VideoApp
