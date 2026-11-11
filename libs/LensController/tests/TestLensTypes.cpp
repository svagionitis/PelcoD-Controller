/// @file TestLensTypes.cpp
/// @brief Unit tests for strongly-typed lens domain value objects and enums.

#include <cstdint>

#include <gtest/gtest.h>

#include "LensTypes.h"

using namespace LensControl;

TEST(TestLensTypes, CameraIndexBasic)
{
    constexpr CameraIndex c0 { 0U };
    constexpr CameraIndex c1 { 1U };
    constexpr CameraIndex c0b { 0U };

    EXPECT_EQ(c0.value(), 0U);
    EXPECT_EQ(c1.value(), 1U);
    EXPECT_EQ(c0, c0b);
    EXPECT_NE(c0, c1);
    EXPECT_LT(c0, c1);
}

TEST(TestLensTypes, MotorPositionOrderingAndEquality)
{
    constexpr MotorPosition p100 { 100U };
    constexpr MotorPosition p200 { 200U };
    constexpr MotorPosition p100b { 100U };

    EXPECT_EQ(p100.value(), 100U);
    EXPECT_EQ(p100, p100b);
    EXPECT_NE(p100, p200);
    EXPECT_LT(p100, p200);
    EXPECT_LE(p100, p200);
    EXPECT_LE(p100, p100b);
    EXPECT_GT(p200, p100);
    EXPECT_GE(p200, p100);
}

TEST(TestLensTypes, MotorSpeedThresholds)
{
    constexpr MotorSpeed sMin { MotorSpeed::kMinSpeed };
    constexpr MotorSpeed sDef { MotorSpeed::kDefaultSpeed };
    constexpr MotorSpeed sMax { MotorSpeed::kMaxSpeed };

    EXPECT_EQ(sMin.value(), 0U);
    EXPECT_TRUE(sMin.isZero());
    EXPECT_FALSE(sMin.isMax());

    EXPECT_EQ(sDef.value(), 127U);
    EXPECT_FALSE(sDef.isZero());
    EXPECT_FALSE(sDef.isMax());

    EXPECT_EQ(sMax.value(), 255U);
    EXPECT_FALSE(sMax.isZero());
    EXPECT_TRUE(sMax.isMax());

    EXPECT_LT(sMin, sDef);
    EXPECT_LT(sDef, sMax);
}

TEST(TestLensTypes, StrongSpeedTypesDisallowImplicitMix)
{
    constexpr ZoomSpeed zs { 150U };
    constexpr FocusSpeed fs { 180U };

    EXPECT_EQ(zs.value(), 150U);
    EXPECT_EQ(fs.value(), 180U);

    EXPECT_EQ(zs.toMotorSpeed().value(), 150U);
    EXPECT_EQ(fs.toMotorSpeed().value(), 180U);

    constexpr ZoomSpeed zsFromMotor { MotorSpeed { 200U } };
    EXPECT_EQ(zsFromMotor.value(), 200U);

    constexpr FocusSpeed fsFromMotor { MotorSpeed { 220U } };
    EXPECT_EQ(fsFromMotor.value(), 220U);
}

TEST(TestLensTypes, LensRangesClamping)
{
    constexpr LensRanges ranges { 100U, 1000U, 50U, 500U };
    EXPECT_TRUE(ranges.isValid());

    constexpr LensRanges invalidRanges { 1000U, 100U, 50U, 500U };
    EXPECT_FALSE(invalidRanges.isValid());

    // Below focus minimum
    EXPECT_EQ(ranges.clampFocus(MotorPosition { 20U }), MotorPosition { 100U });
    // Above focus maximum
    EXPECT_EQ(ranges.clampFocus(MotorPosition { 2000U }), MotorPosition { 1000U });
    // Inside focus range
    EXPECT_EQ(ranges.clampFocus(MotorPosition { 450U }), MotorPosition { 450U });

    // Below zoom minimum
    EXPECT_EQ(ranges.clampZoom(MotorPosition { 10U }), MotorPosition { 50U });
    // Above zoom maximum
    EXPECT_EQ(ranges.clampZoom(MotorPosition { 900U }), MotorPosition { 500U });
    // Inside zoom range
    EXPECT_EQ(ranges.clampZoom(MotorPosition { 300U }), MotorPosition { 300U });
}

TEST(TestLensTypes, LensLimitFlagsBitwiseOperations)
{
    const auto flagNear = LensLimitFlags::NearLimit;
    const auto flagFar = LensLimitFlags::FarLimit;

    const auto combined = flagNear | flagFar;
    const auto bitmask = static_cast<std::uint8_t>(combined);

    EXPECT_TRUE(hasFlag(bitmask, LensLimitFlags::NearLimit));
    EXPECT_TRUE(hasFlag(bitmask, LensLimitFlags::FarLimit));
    EXPECT_FALSE(hasFlag(bitmask, LensLimitFlags::WideLimit));
    EXPECT_FALSE(hasFlag(bitmask, LensLimitFlags::TeleLimit));

    const auto masked = combined & LensLimitFlags::NearLimit;
    EXPECT_EQ(masked, LensLimitFlags::NearLimit);
}

TEST(TestLensTypes, NormalizedRoiCoordinateValidation)
{
    // Center 50% box: col=64, row=64, w=128, h=128 -> (64+128=192 <= 256)
    constexpr NormalizedRoi roiCenter { };
    EXPECT_TRUE(roiCenter.isValid());

    // Full frame box: col=0, row=0, w=255, h=255 -> (255 <= 256)
    constexpr NormalizedRoi roiFull { 0U, 0U, 255U, 255U };
    EXPECT_TRUE(roiFull.isValid());

    // Width overflow: col=200, w=100 -> 300 > 256
    constexpr NormalizedRoi roiOverflow { 200U, 64U, 100U, 128U };
    EXPECT_FALSE(roiOverflow.isValid());

    // Zero width
    constexpr NormalizedRoi roiZeroWidth { 64U, 64U, 0U, 128U };
    EXPECT_FALSE(roiZeroWidth.isValid());

    // Zero height
    constexpr NormalizedRoi roiZeroHeight { 64U, 64U, 128U, 0U };
    EXPECT_FALSE(roiZeroHeight.isValid());
}
