#include "TelemetryTimeIndex.h"
#include <gtest/gtest.h>

namespace Klv {
namespace {

TEST(TestTelemetryTimeIndex, EmptyIndexBehavior)
{
    TelemetryTimeIndex index {};
    EXPECT_TRUE(index.empty());
    EXPECT_EQ(index.videoFrameCount(), 0U);
    EXPECT_EQ(index.keyframeCount(), 0U);
    EXPECT_EQ(index.klvPacketCount(), 0U);
    EXPECT_DOUBLE_EQ(index.durationSeconds(), 0.0);

    EXPECT_FALSE(index.findPrecedingKeyframe(90000U).has_value());
    EXPECT_FALSE(index.findVideoFrame(90000U).has_value());
    EXPECT_FALSE(index.findTelemetry(90000U).has_value());

    KlvIndexEntry before {};
    KlvIndexEntry after {};
    EXPECT_FALSE(index.findTelemetryBounds(90000U, before, after));
    EXPECT_TRUE(index.findWindow(0U, 100000U).empty());
}

TEST(TestTelemetryTimeIndex, VideoIndexingAndKeyframeLookup)
{
    TelemetryTimeIndex index {};

    // 3 video frames at 30 fps (3000 ticks apart): Frame 0 (Key), Frame 1 (P), Frame 2 (P)
    // Frame 3 (Key) at 9000 ticks
    VideoIndexEntry f0 {};
    f0.ptsTicks = 90000U;
    f0.fileByteOffset = 0x1000U;
    f0.isKeyframe = true;
    index.addVideoEntry(f0);

    VideoIndexEntry f1 {};
    f1.ptsTicks = 93000U;
    f1.fileByteOffset = 0x2000U;
    f1.isKeyframe = false;
    index.addVideoEntry(f1);

    VideoIndexEntry f2 {};
    f2.ptsTicks = 96000U;
    f2.fileByteOffset = 0x3000U;
    f2.isKeyframe = false;
    index.addVideoEntry(f2);

    VideoIndexEntry f3 {};
    f3.ptsTicks = 99000U;
    f3.fileByteOffset = 0x4000U;
    f3.isKeyframe = true;
    index.addVideoEntry(f3);

    index.finalize();

    EXPECT_FALSE(index.empty());
    EXPECT_EQ(index.videoFrameCount(), 4U);
    EXPECT_EQ(index.keyframeCount(), 2U);
    EXPECT_DOUBLE_EQ(index.durationSeconds(), 9000.0 / 90000.0); // 0.1s

    // Keyframe lookup at exact keyframe
    auto kf0 = index.findPrecedingKeyframe(90000U);
    ASSERT_TRUE(kf0.has_value());
    EXPECT_EQ(kf0->ptsTicks, 90000U);
    EXPECT_EQ(kf0->fileByteOffset, 0x1000U);

    // Keyframe lookup on intermediate frame (95000 -> should return f0 at 90000)
    auto kfMid = index.findPrecedingKeyframe(95000U);
    ASSERT_TRUE(kfMid.has_value());
    EXPECT_EQ(kfMid->ptsTicks, 90000U);

    // Keyframe lookup at or after f3
    auto kf3 = index.findPrecedingKeyframe(105000U);
    ASSERT_TRUE(kf3.has_value());
    EXPECT_EQ(kf3->ptsTicks, 99000U);
    EXPECT_EQ(kf3->fileByteOffset, 0x4000U);

    // Video frame nearest search
    auto vf1 = index.findVideoFrame(93100U);
    ASSERT_TRUE(vf1.has_value());
    EXPECT_EQ(vf1->ptsTicks, 93000U);
}

TEST(TestTelemetryTimeIndex, KlvIndexingAndSearch)
{
    TelemetryTimeIndex index {};

    KlvIndexEntry k0 {};
    k0.ptsTicks = 90000U;
    k0.utcTimestampUs = 1700000000000000ULL;
    k0.fileByteOffset = 0x500U;
    index.addKlvEntry(k0);

    KlvIndexEntry k1 {};
    k1.ptsTicks = 99000U; // 100 ms later (9000 ticks)
    k1.utcTimestampUs = 1700000000100000ULL;
    k1.fileByteOffset = 0x1500U;
    index.addKlvEntry(k1);

    index.finalize();

    EXPECT_EQ(index.klvPacketCount(), 2U);
    EXPECT_EQ(index.baseUtcUs(), 1700000000000000ULL);

    // Query within 50 ms tolerance
    auto match = index.findTelemetry(90900U, 50000U); // 10 ms skew
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->ptsTicks, 90000U);

    // Query exceeding tolerance
    auto noMatch = index.findTelemetry(94500U, 20000U); // 50 ms skew, 20 ms max
    EXPECT_FALSE(noMatch.has_value());

    // Window query
    auto win = index.findWindow(80000U, 100000U);
    EXPECT_EQ(win.size(), 2U);

    // Bounds query
    KlvIndexEntry before {};
    KlvIndexEntry after {};
    ASSERT_TRUE(index.findTelemetryBounds(94500U, before, after));
    EXPECT_EQ(before.ptsTicks, 90000U);
    EXPECT_EQ(after.ptsTicks, 99000U);
}

TEST(TestTelemetryTimeIndex, TimeConversions)
{
    TelemetryTimeIndex index {};

    VideoIndexEntry v {};
    v.ptsTicks = 90000U;
    index.addVideoEntry(v);

    KlvIndexEntry k {};
    k.ptsTicks = 90000U;
    k.utcTimestampUs = 1000000U;
    index.addKlvEntry(k);

    index.finalize();

    // PTS to seconds
    EXPECT_DOUBLE_EQ(index.ptsToSeconds(90000U), 0.0);
    EXPECT_DOUBLE_EQ(index.ptsToSeconds(180000U), 1.0);

    // Seconds to PTS
    EXPECT_EQ(index.secondsToPts(0.0), 90000U);
    EXPECT_EQ(index.secondsToPts(2.5), 90000U + 225000U);

    // PTS to UTC
    EXPECT_EQ(index.ptsToUtc(90000U), 1000000U);
    EXPECT_EQ(index.ptsToUtc(180000U), 2000000U);

    // UTC to PTS
    EXPECT_EQ(index.utcToPts(1000000U), 90000U);
    EXPECT_EQ(index.utcToPts(2000000U), 180000U);
}

} // namespace
} // namespace Klv
