/// @file TestVideoPlayerController.cpp
/// @brief Tests for VideoPlayerController timeline seeking, scrubbing, and KLV telemetry.

#include <QGuiApplication>
#include <gtest/gtest.h>

#include "VideoPlayerController.h"

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
    const std::vector<std::string> prefixes {
        "",
        "sample-videos/",
        "../sample-videos/",
        "../../sample-videos/",
        "../../../sample-videos/",
        "../../../../sample-videos/"
    };

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

} // namespace VideoApp
