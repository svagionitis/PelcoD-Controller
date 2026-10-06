/// @file TestDjiSubtitleParser.cpp
/// @brief Unit tests for the DJI tx3g/SRT telemetry text parser.

#include "DjiSubtitleParser.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

using Dji::DjiTelemetrySample;
using Dji::parseDjiText;

namespace {

/// @brief Exact sample 0 captured from a DJI Matrice 4T thermal recording.
constexpr const char* kM4tSample {
    "FrameCnt: 0 2026-09-24 15:20:55.713\n"
    "[focal_len: 52.70] [dzoom_ratio: 1.00], [latitude: 38.375988] [longitude: 23.257121] "
    "[rel_alt: 20.160 abs_alt: 169.523] [gb_yaw: -77.4 gb_pitch: 7.4 gb_roll: 0.0] [ir_gain_mode: 0] "
};

/// @brief 2026-09-24T15:20:55.713 expressed as microseconds since 1970-01-01 (no TZ applied).
constexpr std::int64_t kM4tLocalUs { 1790263255713000 };

} // namespace

TEST(DjiSubtitleParserTest, ParsesMatrice4tSample)
{
    const auto parsed { parseDjiText(kM4tSample) };
    ASSERT_TRUE(parsed.has_value());
    const DjiTelemetrySample& s { *parsed };

    ASSERT_TRUE(s.frameCount.has_value());
    EXPECT_EQ(*s.frameCount, 0U);
    ASSERT_TRUE(s.localTimeUs.has_value());
    EXPECT_EQ(*s.localTimeUs, kM4tLocalUs);
    ASSERT_TRUE(s.focalLenMm.has_value());
    EXPECT_DOUBLE_EQ(*s.focalLenMm, 52.70);
    ASSERT_TRUE(s.digitalZoom.has_value());
    EXPECT_DOUBLE_EQ(*s.digitalZoom, 1.00);
    ASSERT_TRUE(s.latitudeDeg.has_value());
    EXPECT_DOUBLE_EQ(*s.latitudeDeg, 38.375988);
    ASSERT_TRUE(s.longitudeDeg.has_value());
    EXPECT_DOUBLE_EQ(*s.longitudeDeg, 23.257121);
    ASSERT_TRUE(s.relAltM.has_value());
    EXPECT_DOUBLE_EQ(*s.relAltM, 20.160);
    ASSERT_TRUE(s.absAltM.has_value());
    EXPECT_DOUBLE_EQ(*s.absAltM, 169.523);
    ASSERT_TRUE(s.gimbalYawDeg.has_value());
    EXPECT_DOUBLE_EQ(*s.gimbalYawDeg, -77.4);
    ASSERT_TRUE(s.gimbalPitchDeg.has_value());
    EXPECT_DOUBLE_EQ(*s.gimbalPitchDeg, 7.4);
    ASSERT_TRUE(s.gimbalRollDeg.has_value());
    EXPECT_DOUBLE_EQ(*s.gimbalRollDeg, 0.0);

    ASSERT_EQ(s.extra.count("ir_gain_mode"), 1U);
    EXPECT_EQ(s.extra.at("ir_gain_mode"), "0");
}

TEST(DjiSubtitleParserTest, HandlesCrLfAndLargeFrameCount)
{
    const std::string text { "FrameCnt: 30006 2026-09-24 15:37:35.679\r\n"
                             "[latitude: 38.376228] [longitude: 23.257436] [rel_alt: 9.734 abs_alt: 159.096]\r\n" };
    const auto parsed { parseDjiText(text) };
    ASSERT_TRUE(parsed.has_value());
    ASSERT_TRUE(parsed->frameCount.has_value());
    EXPECT_EQ(*parsed->frameCount, 30006U);
    ASSERT_TRUE(parsed->latitudeDeg.has_value());
    EXPECT_DOUBLE_EQ(*parsed->latitudeDeg, 38.376228);
    ASSERT_TRUE(parsed->absAltM.has_value());
    EXPECT_DOUBLE_EQ(*parsed->absAltM, 159.096);
}

TEST(DjiSubtitleParserTest, MissingOptionalFieldsStayEmpty)
{
    const auto parsed { parseDjiText("[latitude: 10.5] [longitude: -20.25]") };
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FALSE(parsed->frameCount.has_value());
    EXPECT_FALSE(parsed->localTimeUs.has_value());
    EXPECT_FALSE(parsed->gimbalYawDeg.has_value());
    EXPECT_FALSE(parsed->absAltM.has_value());
    ASSERT_TRUE(parsed->longitudeDeg.has_value());
    EXPECT_DOUBLE_EQ(*parsed->longitudeDeg, -20.25);
}

TEST(DjiSubtitleParserTest, ConsumerDroneFormatWithCameraKeys)
{
    // Mini/Air series layout: counters on line 1, date on line 2, camera exposure keys.
    const std::string text {
        "SrtCnt : 1, DiffTime : 33ms\n"
        "2024-05-01 09:15:30.050\n"
        "[iso : 100] [shutter : 1/1000.0] [fnum : 1.7] [ev : 0] [focal_len : 24.00] "
        "[latitude: 37.9715] [longtitude: 23.7267] [rel_alt: 50.000 abs_alt: 120.500] [ct : 5500]"
    };
    const auto parsed { parseDjiText(text) };
    ASSERT_TRUE(parsed.has_value());
    ASSERT_TRUE(parsed->localTimeUs.has_value());
    EXPECT_EQ(*parsed->localTimeUs, 1714554930050000);
    ASSERT_TRUE(parsed->longitudeDeg.has_value()); // DJI firmware misspelling "longtitude"
    EXPECT_DOUBLE_EQ(*parsed->longitudeDeg, 23.7267);
    ASSERT_TRUE(parsed->focalLenMm.has_value());
    EXPECT_DOUBLE_EQ(*parsed->focalLenMm, 24.0);
    EXPECT_EQ(parsed->extra.at("iso"), "100");
    EXPECT_EQ(parsed->extra.at("shutter"), "1/1000.0");
    EXPECT_EQ(parsed->extra.at("ct"), "5500");
}

TEST(DjiSubtitleParserTest, RejectsTextWithoutPosition)
{
    EXPECT_FALSE(parseDjiText("").has_value());
    EXPECT_FALSE(parseDjiText("hello world").has_value());
    EXPECT_FALSE(parseDjiText("FrameCnt: 1 2026-09-24 15:20:55.713\n[gb_yaw: 1.0]").has_value());
}

TEST(DjiSubtitleParserTest, MalformedNumbersAreIgnored)
{
    const auto parsed { parseDjiText("[latitude: abc] [longitude: 23.5] [gb_yaw: 1.2.3]") };
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FALSE(parsed->latitudeDeg.has_value());
    EXPECT_FALSE(parsed->gimbalYawDeg.has_value());
    ASSERT_TRUE(parsed->longitudeDeg.has_value());
    EXPECT_DOUBLE_EQ(*parsed->longitudeDeg, 23.5);
}

TEST(DjiSubtitleParserTest, UnterminatedBracketDoesNotOverrun)
{
    const auto parsed { parseDjiText("[latitude: 1.0] [longitude: 2.0] [gb_yaw: 3.0") };
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FALSE(parsed->gimbalYawDeg.has_value());
}

TEST(DjiSubtitleParserTest, InvalidDateIsIgnored)
{
    const auto parsed { parseDjiText("FrameCnt: 3 2026-13-40 25:61:61.000\n[latitude: 1.0] [longitude: 2.0]") };
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FALSE(parsed->localTimeUs.has_value());
    ASSERT_TRUE(parsed->frameCount.has_value());
    EXPECT_EQ(*parsed->frameCount, 3U);
}

TEST(DjiSubtitleParserTest, FractionalSecondsOfVaryingPrecision)
{
    const auto noFrac { parseDjiText("2026-09-24 15:20:55\n[latitude: 1.0] [longitude: 2.0]") };
    ASSERT_TRUE(noFrac.has_value());
    ASSERT_TRUE(noFrac->localTimeUs.has_value());
    EXPECT_EQ(*noFrac->localTimeUs, 1790263255000000);

    const auto micro { parseDjiText("2026-09-24 15:20:55.123456\n[latitude: 1.0] [longitude: 2.0]") };
    ASSERT_TRUE(micro.has_value());
    ASSERT_TRUE(micro->localTimeUs.has_value());
    EXPECT_EQ(*micro->localTimeUs, 1790263255123456);
}
