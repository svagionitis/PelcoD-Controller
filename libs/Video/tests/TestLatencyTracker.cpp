/// @file TestLatencyTracker.cpp
/// @brief GoogleTest unit tests for the Qt-free LatencyTracker and decoder frame timestamping.

#include "LatencyTracker.h"
#include "MockVideoDecoder.h"

#include <gtest/gtest.h>

#include <cstdint>

namespace {

constexpr std::int64_t kNsPerMs { 1'000'000 };

} // namespace

TEST(TestLatencyTracker, EmptyTrackerReportsZero)
{
    const Video::LatencyTracker tracker {};
    const Video::LatencyStats stats { tracker.snapshot() };

    EXPECT_EQ(stats.count, 0U);
    EXPECT_DOUBLE_EQ(stats.lastMs, 0.0);
    EXPECT_DOUBLE_EQ(stats.avgMs, 0.0);
    EXPECT_DOUBLE_EQ(stats.minMs, 0.0);
    EXPECT_DOUBLE_EQ(stats.maxMs, 0.0);
}

TEST(TestLatencyTracker, ComputesLastAvgMinMax)
{
    Video::LatencyTracker tracker { 8U };
    tracker.addSample(10.0);
    tracker.addSample(30.0);
    tracker.addSample(20.0);

    const Video::LatencyStats stats { tracker.snapshot() };
    EXPECT_EQ(stats.count, 3U);
    EXPECT_DOUBLE_EQ(stats.lastMs, 20.0);
    EXPECT_DOUBLE_EQ(stats.avgMs, 20.0);
    EXPECT_DOUBLE_EQ(stats.minMs, 10.0);
    EXPECT_DOUBLE_EQ(stats.maxMs, 30.0);
}

TEST(TestLatencyTracker, RollingWindowEvictsOldestSamples)
{
    Video::LatencyTracker tracker { 2U };
    tracker.addSample(100.0);
    tracker.addSample(4.0);
    tracker.addSample(6.0);

    const Video::LatencyStats stats { tracker.snapshot() };
    EXPECT_EQ(stats.count, 3U);
    EXPECT_DOUBLE_EQ(stats.avgMs, 5.0);
    EXPECT_DOUBLE_EQ(stats.minMs, 4.0);
    EXPECT_DOUBLE_EQ(stats.maxMs, 6.0);
}

TEST(TestLatencyTracker, ZeroWindowIsClampedToOne)
{
    Video::LatencyTracker tracker { 0U };
    tracker.addSample(7.0);
    tracker.addSample(9.0);

    const Video::LatencyStats stats { tracker.snapshot() };
    EXPECT_DOUBLE_EQ(stats.avgMs, 9.0);
    EXPECT_DOUBLE_EQ(stats.minMs, 9.0);
}

TEST(TestLatencyTracker, RejectsInvalidSamples)
{
    Video::LatencyTracker tracker {};
    tracker.addSample(-1.0);

    EXPECT_FALSE(tracker.addInterval(0, 5 * kNsPerMs));
    EXPECT_FALSE(tracker.addInterval(10 * kNsPerMs, 5 * kNsPerMs));
    EXPECT_EQ(tracker.snapshot().count, 0U);
}

TEST(TestLatencyTracker, AddIntervalConvertsNanosecondsToMs)
{
    Video::LatencyTracker tracker {};
    EXPECT_TRUE(tracker.addInterval(10 * kNsPerMs, 35 * kNsPerMs));

    const Video::LatencyStats stats { tracker.snapshot() };
    EXPECT_EQ(stats.count, 1U);
    EXPECT_DOUBLE_EQ(stats.lastMs, 25.0);
}

TEST(TestLatencyTracker, ResetClearsHistory)
{
    Video::LatencyTracker tracker {};
    tracker.addSample(12.0);
    tracker.reset();

    EXPECT_EQ(tracker.snapshot().count, 0U);
    EXPECT_DOUBLE_EQ(tracker.snapshot().maxMs, 0.0);
}

TEST(TestLatencyTracker, SteadyClockIsMonotonic)
{
    const std::int64_t t0 { Video::steadyNowNs() };
    const std::int64_t t1 { Video::steadyNowNs() };

    EXPECT_GT(t0, 0);
    EXPECT_GE(t1, t0);
}

TEST(TestLatencyTracker, DecoderStampsDecodedFrames)
{
    Video::MockVideoDecoder decoder {};
    ASSERT_TRUE(decoder.initialize("mock://test"));

    const std::int64_t before { Video::steadyNowNs() };
    ASSERT_TRUE(decoder.decodeNextFrame());
    const std::int64_t after { Video::steadyNowNs() };

    const Video::FrameInfo frame { decoder.getRawFrameData() };
    EXPECT_GE(frame.decodedAtNs, before);
    EXPECT_LE(frame.decodedAtNs, after);
}
