/// @file TestDjiTelemetrySource.cpp
/// @brief End-to-end tests: synthetic DJI MP4 -> timed ST 0601 messages.

#include "DjiTelemetrySource.h"
#include "Mp4TestBuilder.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

using Dji::DjiLoadOptions;
using Dji::isMp4File;
using Dji::loadDjiTrack;
using Dji::TimedTelemetry;
using DjiTest::buildMp4;
using DjiTest::Mp4BuildOptions;
using DjiTest::TempFile;

namespace {

/// @brief Builds DJI M4T-style sample text for frame @p i.
/// @param[in] i Frame index.
/// @return Sample text.
std::string djiText(std::size_t i)
{
    return "FrameCnt: " + std::to_string(i) + " 2026-09-24 15:20:55.713\n"
        + "[focal_len: 52.70] [dzoom_ratio: 1.00], [latitude: 38.37598" + std::to_string(i)
        + "] [longitude: 23.257121] [rel_alt: 20.160 abs_alt: 169.523] "
          "[gb_yaw: -77.4 gb_pitch: 7.4 gb_roll: 0.0] [ir_gain_mode: 0] ";
}

} // namespace

TEST(DjiTelemetrySourceTest, LoadsEverySampleAlignedToFrames)
{
    Mp4BuildOptions o {};
    for (std::size_t i { 0U }; i < 6U; ++i) {
        o.texts.push_back(djiText(i));
    }
    const TempFile file { buildMp4(o), "source_e2e" };

    std::vector<TimedTelemetry> out {};
    ASSERT_TRUE(loadDjiTrack(file.path(), out));
    ASSERT_EQ(out.size(), 6U);

    for (std::size_t i { 0U }; i < out.size(); ++i) {
        EXPECT_NEAR(out[i].timeSec, static_cast<double>(i) / 30.0, 1e-9);
        ASSERT_TRUE(out[i].message.sensorLatitudeDeg.has_value());
        EXPECT_NEAR(*out[i].message.sensorLatitudeDeg, 38.37598 + static_cast<double>(i) * 1e-6, 1e-9);
    }

    const auto& first { out.front().message };
    // UTC offset auto-derived from mvhd creation time (+3 h).
    ASSERT_TRUE(first.precisionTimeStampUs.has_value());
    EXPECT_EQ(*first.precisionTimeStampUs, 1790252455713000ULL);
    ASSERT_TRUE(first.platformDesignation.has_value());
    EXPECT_EQ(*first.platformDesignation, "DJI DJI Matrice 4T");
    ASSERT_TRUE(first.sensorHfovDeg.has_value()); // aspect from video tkhd 1280x1024
    EXPECT_NEAR(*first.sensorHfovDeg, 35.546, 1e-2);
    ASSERT_TRUE(first.sensorRelAzimuthDeg.has_value());
    EXPECT_NEAR(*first.sensorRelAzimuthDeg, 282.6, 1e-9);
}

TEST(DjiTelemetrySourceTest, ManualUtcOffsetOverridesDerived)
{
    Mp4BuildOptions o {};
    o.texts = { djiText(0U) };
    const TempFile file { buildMp4(o), "source_override" };

    DjiLoadOptions opts {};
    opts.utcOffsetMin = 0;
    std::vector<TimedTelemetry> out {};
    ASSERT_TRUE(loadDjiTrack(file.path(), out, opts));
    ASSERT_EQ(out.size(), 1U);
    ASSERT_TRUE(out[0].message.precisionTimeStampUs.has_value());
    EXPECT_EQ(*out[0].message.precisionTimeStampUs, 1790263255713000ULL);
}

TEST(DjiTelemetrySourceTest, UnparseableSamplesAreSkipped)
{
    Mp4BuildOptions o {};
    o.texts = { djiText(0U), "garbage", djiText(2U) };
    const TempFile file { buildMp4(o), "source_skip" };

    std::vector<TimedTelemetry> out {};
    ASSERT_TRUE(loadDjiTrack(file.path(), out));
    ASSERT_EQ(out.size(), 2U);
    EXPECT_NEAR(out[1].timeSec, 2.0 / 30.0, 1e-9);
}

TEST(DjiTelemetrySourceTest, FailsWithoutTextTrackAndClearsOutput)
{
    Mp4BuildOptions o {};
    o.texts = { djiText(0U) };
    o.includeTextTrack = false;
    const TempFile file { buildMp4(o), "source_no_track" };

    std::vector<TimedTelemetry> out(3U);
    EXPECT_FALSE(loadDjiTrack(file.path(), out));
    EXPECT_TRUE(out.empty());
}

TEST(DjiTelemetrySourceTest, DetectsMp4ByFtyp)
{
    Mp4BuildOptions o {};
    o.texts = { djiText(0U) };
    const TempFile mp4 { buildMp4(o), "source_is_mp4" };
    EXPECT_TRUE(isMp4File(mp4.path()));

    const DjiTest::Bytes ts(376U, 0x47U);
    const TempFile notMp4 { ts, "source_is_ts" };
    EXPECT_FALSE(isMp4File(notMp4.path()));
    EXPECT_FALSE(isMp4File("/nonexistent/file.mp4"));
}
