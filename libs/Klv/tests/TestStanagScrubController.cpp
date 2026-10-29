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
        VideoIndexEntry v0 {};
        v0.ptsTicks = 90000U;
        v0.isKeyframe = true;
        VideoIndexEntry v1 {};
        v1.ptsTicks = 93000U;
        v1.isKeyframe = false;
        VideoIndexEntry v2 {};
        v2.ptsTicks = 96000U;
        v2.isKeyframe = false;
        VideoIndexEntry v3 {};
        v3.ptsTicks = 99000U;
        v3.isKeyframe = true;

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

    TEST(TestStanagScrubController, SeekModeFastKeyframeVsExact)
    {
        StanagScrubController controller {};

        TelemetryTimeIndex timeIndex {};
        // Keyframe at 0.0s (0 ticks)
        VideoIndexEntry v0 {};
        v0.ptsTicks = 0U;
        v0.isKeyframe = true;
        // P-frame at 0.5s (45000 ticks)
        VideoIndexEntry v1 {};
        v1.ptsTicks = 45000U;
        v1.isKeyframe = false;
        // P-frame at 1.0s (90000 ticks)
        VideoIndexEntry v2 {};
        v2.ptsTicks = 90000U;
        v2.isKeyframe = false;
        // Keyframe at 2.0s (180000 ticks)
        VideoIndexEntry v3 {};
        v3.ptsTicks = 180000U;
        v3.isKeyframe = true;

        timeIndex.addVideoEntry(v0);
        timeIndex.addVideoEntry(v1);
        timeIndex.addVideoEntry(v2);
        timeIndex.addVideoEntry(v3);
        timeIndex.finalize();

        controller.setTimeIndex(timeIndex);

        double lastSeekSec = -1.0;
        bool lastKeyframeOnly = false;
        controller.setSeekCallback([&](double sec, bool kfOnly) {
            lastSeekSec = sec;
            lastKeyframeOnly = kfOnly;
            return true;
        });

        // 1. FastKeyframeOnly seek to 1.0s (90000 ticks) -> should seek to keyframe at 0.0s
        controller.setSeekMode(ScrubSeekMode::FastKeyframeOnly);
        EXPECT_TRUE(controller.scrubToSeconds(1.0));
        EXPECT_NEAR(lastSeekSec, 0.0, 1e-4);
        EXPECT_TRUE(lastKeyframeOnly);
        EXPECT_EQ(controller.stats().keyframeSeekCount, 1U);

        // 2. ExactFrameAccurate seek to 1.0s -> should seek to exact frame at 1.0s
        controller.setSeekMode(ScrubSeekMode::ExactFrameAccurate);
        EXPECT_TRUE(controller.scrubToSeconds(1.0));
        EXPECT_NEAR(lastSeekSec, 1.0, 1e-4);
        EXPECT_FALSE(lastKeyframeOnly);
        EXPECT_EQ(controller.stats().exactSeekCount, 1U);
    }

    TEST(TestStanagScrubController, VfrStepFramesAccuracy)
    {
        StanagScrubController controller {};

        TelemetryTimeIndex timeIndex {};
        // Non-linear VFR frame distribution
        VideoIndexEntry v0 {};
        v0.ptsTicks = 0U;
        v0.isKeyframe = true;
        VideoIndexEntry v1 {};
        v1.ptsTicks = 9000U;
        v1.isKeyframe = false; // +100ms
        VideoIndexEntry v2 {};
        v2.ptsTicks = 90000U;
        v2.isKeyframe = false; // +900ms
        VideoIndexEntry v3 {};
        v3.ptsTicks = 94500U;
        v3.isKeyframe = false; // +50ms

        timeIndex.addVideoEntry(v0);
        timeIndex.addVideoEntry(v1);
        timeIndex.addVideoEntry(v2);
        timeIndex.addVideoEntry(v3);
        timeIndex.finalize();

        controller.setTimeIndex(timeIndex);

        // Initial position at 0
        EXPECT_TRUE(controller.scrubToPts(0U));
        EXPECT_EQ(controller.currentFrame().ptsTicks, 0U);

        // Step 1: should land exactly on frame 1 (9000 ticks)
        EXPECT_TRUE(controller.stepFrames(1));
        EXPECT_EQ(controller.currentFrame().ptsTicks, 9000U);

        // Step 2: should land exactly on frame 2 (90000 ticks)
        EXPECT_TRUE(controller.stepFrames(1));
        EXPECT_EQ(controller.currentFrame().ptsTicks, 90000U);

        // Step 3: should land exactly on frame 3 (94500 ticks)
        EXPECT_TRUE(controller.stepFrames(1));
        EXPECT_EQ(controller.currentFrame().ptsTicks, 94500U);

        // Step beyond last frame clamps to frame 3
        EXPECT_TRUE(controller.stepFrames(1));
        EXPECT_EQ(controller.currentFrame().ptsTicks, 94500U);

        // Step back 2 frames -> frame 1 (9000 ticks)
        EXPECT_TRUE(controller.stepFrames(-2));
        EXPECT_EQ(controller.currentFrame().ptsTicks, 9000U);
    }

} // namespace
} // namespace Klv
