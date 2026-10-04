#include "MasterTimeBase.h"
#include <gtest/gtest.h>

using namespace Klv;

TEST(TestMasterTimeBase, SystemEpochNowAndPts) {
    SystemMasterTimeBase tb(TimeBaseMode::SystemEpoch);
    EXPECT_EQ(tb.mode(), TimeBaseMode::SystemEpoch);

    const std::uint64_t now1 = tb.nowUs();
    EXPECT_GT(now1, 1600000000000000ULL); // After year 2020

    // Test PTS conversion (90 kHz)
    constexpr std::uint64_t kTimestampUs = 1000000ULL; // 1 second
    const std::uint64_t pts = tb.toPts(kTimestampUs);
    EXPECT_EQ(pts, 90000ULL);

    const std::uint64_t roundTripUs = tb.toUs(pts);
    EXPECT_EQ(roundTripUs, kTimestampUs);
}

TEST(TestMasterTimeBase, MonotonicOffsetCalibration) {
    SystemMasterTimeBase tb(TimeBaseMode::MonotonicOffset);
    EXPECT_EQ(tb.mode(), TimeBaseMode::MonotonicOffset);

    constexpr std::int64_t kArbitraryOffset = 5000000LL;
    tb.setMonotonicOffset(kArbitraryOffset);

    const std::uint64_t nowUs = tb.nowUs();
    EXPECT_GE(nowUs, static_cast<std::uint64_t>(kArbitraryOffset));
}

TEST(TestMasterTimeBase, RolloverAt33Bits) {
    SystemMasterTimeBase tb;
    // 2^33 - 1 in 90 kHz ticks is approximately 95,443,717,677,777 us (~95,443 seconds = ~26.5 hours)
    constexpr std::uint64_t kRolloverPts = 0x1FFFFFFFFULL; // Max 33-bit value
    constexpr std::uint64_t kMaxUs = (kRolloverPts * 1000ULL) / 90ULL;

    const std::uint64_t ptsMax = tb.toPts(kMaxUs);
    EXPECT_LE(ptsMax, 0x1FFFFFFFFULL);

    // One more tick should wrap around to small value
    const std::uint64_t ptsWrapped = tb.toPts(kMaxUs + 20ULL);
    EXPECT_LE(ptsWrapped, 0x1FFFFFFFFULL);
}
