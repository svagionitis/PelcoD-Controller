#include "StanagScrubController.h"
#include <gtest/gtest.h>

namespace Klv {
namespace {

TEST(TestStanagScrubController, EmptyControllerFailsGracefully)
{
    StanagScrubController controller {};
    EXPECT_FALSE(controller.scrubToSeconds(10.0));
    EXPECT_FALSE(controller.scrubToPts(90000U));
    EXPECT_FALSE(controller.stepFrames(1));
}

TEST(TestStanagScrubController, BidirectionalScrubbingAndInterpolation)
{
    StanagScrubController controller {};

    TelemetryTimeIndex timeIndex {};

    // 4 video frames at 30 fps (3000 ticks = 33.33ms apart)
    VideoIndexEntry v0 {}; v0.ptsTicks = 90000U; v0.isKeyframe = true;
    VideoIndexEntry v1 {}; v1.ptsTicks = 93000U; v1.isKeyframe = false;
    VideoIndexEntry v2 {}; v2.ptsTicks = 96000U; v2.isKeyframe = false;
    VideoIndexEntry v3 {}; v3.ptsTicks = 99000U; v3.isKeyframe = true;

    timeIndex.addVideoEntry(v0);
    timeIndex.addVideoEntry(v1);
    timeIndex.addVideoEntry(v2);
    timeIndex.addVideoEntry(v3);

    // 2 KLV packets: at t=0s (90000 ticks) and t=0.1s (99000 ticks)
    UasDatalinkMessage m0 {};
    m0.platformHeadingDeg = 100.0;
    m0.sensorLatitudeDeg = 34.0;
    m0.sensorLongitudeDeg = -118.0;

    KlvIndexEntry k0 {};
    k0.ptsTicks = 90000U;
    k0.utcTimestampUs = 1000000U;
    k0.message = m0;

    UasDatalinkMessage m1 {};
    m1.platformHeadingDeg = 110.0;
    m1.sensorLatitudeDeg = 35.0;
    m1.sensorLongitudeDeg = -117.0;

    KlvIndexEntry k1 {};
    k1.ptsTicks = 99000U;
    k1.utcTimestampUs = 1100000U;
    k1.message = m1;

    timeIndex.addKlvEntry(k0);
    timeIndex.addKlvEntry(k1);
    timeIndex.finalize();

    controller.setTimeIndex(timeIndex);

    bool callbackInvoked = false;
    SynchronizedScrubFrame lastFrame {};
    controller.setScrubCallback([&](const SynchronizedScrubFrame& f) {
        callbackInvoked = true;
        lastFrame = f;
    });

    // 1. Scrub to t = 0.0s (PTS 90000)
    EXPECT_TRUE(controller.scrubToSeconds(0.0));
    EXPECT_TRUE(callbackInvoked);
    EXPECT_EQ(lastFrame.ptsTicks, 90000U);
    EXPECT_TRUE(lastFrame.isKeyframe);
    EXPECT_FALSE(lastFrame.isInterpolated);
    ASSERT_TRUE(lastFrame.telemetry.platformHeadingDeg.has_value());
    EXPECT_NEAR(*lastFrame.telemetry.platformHeadingDeg, 100.0, 1e-4);

    // 2. Scrub forward to intermediate frame (PTS ~94500, midway)
    callbackInvoked = false;
    EXPECT_TRUE(controller.scrubToSeconds(0.05)); // 50ms in -> midpoint between 90000 and 99000
    EXPECT_TRUE(callbackInvoked);
    ASSERT_TRUE(lastFrame.telemetry.platformHeadingDeg.has_value());
    // Heading should be interpolated between 100 and 110 (around 105)
    EXPECT_GT(*lastFrame.telemetry.platformHeadingDeg, 100.0);
    EXPECT_LT(*lastFrame.telemetry.platformHeadingDeg, 110.0);

    // 3. Backward scrub back to 0.0s
    callbackInvoked = false;
    EXPECT_TRUE(controller.scrubToSeconds(0.0));
    EXPECT_TRUE(callbackInvoked);
    EXPECT_EQ(lastFrame.ptsTicks, 90000U);
    ASSERT_TRUE(lastFrame.telemetry.platformHeadingDeg.has_value());
    EXPECT_NEAR(*lastFrame.telemetry.platformHeadingDeg, 100.0, 1e-4);

    // 4. Frame stepping: step forward 1 frame
    callbackInvoked = false;
    EXPECT_TRUE(controller.stepFrames(1));
    EXPECT_TRUE(callbackInvoked);

    // 5. Check stats
    auto st = controller.stats();
    EXPECT_EQ(st.totalVideoFrames, 4U);
    EXPECT_EQ(st.totalKeyframes, 2U);
    EXPECT_EQ(st.totalKlvPackets, 2U);
    EXPECT_GT(st.cacheHitCount, 0U);
}

} // namespace
} // namespace Klv
