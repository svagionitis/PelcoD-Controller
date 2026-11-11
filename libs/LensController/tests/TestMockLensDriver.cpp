/// @file TestMockLensDriver.cpp
/// @brief Unit tests for MockLensDriver actuator hardware abstraction.

#include <cstddef>
#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "LensTypes.h"
#include "MockLensDriver.h"

using namespace LensControl;

TEST(TestMockLensDriver, InitializePortAndBaud)
{
    MockLensDriver driver { };
    EXPECT_TRUE(driver.initialize(2U, 9600U));
    EXPECT_EQ(driver.boundPort(), 2U);
    EXPECT_EQ(driver.baudRate(), 9600U);
    EXPECT_TRUE(driver.isMotionStopped());
}

TEST(TestMockLensDriver, RelativeFocusMovementAndLimits)
{
    constexpr LensRanges ranges { 100U, 1000U, 50U, 500U };
    MockLensDriver driver { ranges };

    // Initial position starts at focusMin (100) -> NearLimit set
    EXPECT_EQ(driver.currentFocus().value(), 100U);
    EXPECT_TRUE(hasFlag(driver.limitFlags(), LensLimitFlags::NearLimit));
    EXPECT_FALSE(hasFlag(driver.limitFlags(), LensLimitFlags::FarLimit));

    // Move focus forward +200 steps
    driver.moveFocus(200, MotorSpeed { 150U });
    EXPECT_EQ(driver.currentFocus().value(), 300U);
    EXPECT_EQ(driver.lastFocusSpeed().value(), 150U);
    EXPECT_FALSE(hasFlag(driver.limitFlags(), LensLimitFlags::NearLimit));
    EXPECT_FALSE(hasFlag(driver.limitFlags(), LensLimitFlags::FarLimit));
    EXPECT_FALSE(driver.isMotionStopped());

    // Move focus past maximum (+1500) -> clamps to focusMax (1000) and trips FarLimit
    driver.moveFocus(1500, MotorSpeed { 200U });
    EXPECT_EQ(driver.currentFocus().value(), 1000U);
    EXPECT_TRUE(hasFlag(driver.limitFlags(), LensLimitFlags::FarLimit));
    EXPECT_FALSE(hasFlag(driver.limitFlags(), LensLimitFlags::NearLimit));

    // Move focus backwards past minimum (-1500) -> clamps to focusMin (100) and trips NearLimit
    driver.moveFocus(-1500, MotorSpeed { 100U });
    EXPECT_EQ(driver.currentFocus().value(), 100U);
    EXPECT_TRUE(hasFlag(driver.limitFlags(), LensLimitFlags::NearLimit));
    EXPECT_FALSE(hasFlag(driver.limitFlags(), LensLimitFlags::FarLimit));
}

TEST(TestMockLensDriver, RelativeZoomMovementAndLimits)
{
    constexpr LensRanges ranges { 100U, 1000U, 50U, 500U };
    MockLensDriver driver { ranges };

    // Initial position starts at zoomMin (50) -> WideLimit set
    EXPECT_EQ(driver.currentZoom().value(), 50U);
    EXPECT_TRUE(hasFlag(driver.limitFlags(), LensLimitFlags::WideLimit));
    EXPECT_FALSE(hasFlag(driver.limitFlags(), LensLimitFlags::TeleLimit));

    // Move zoom forward +100 steps
    driver.moveZoom(100, MotorSpeed { 120U });
    EXPECT_EQ(driver.currentZoom().value(), 150U);
    EXPECT_EQ(driver.lastZoomSpeed().value(), 120U);
    EXPECT_FALSE(hasFlag(driver.limitFlags(), LensLimitFlags::WideLimit));
    EXPECT_FALSE(hasFlag(driver.limitFlags(), LensLimitFlags::TeleLimit));

    // Move zoom past maximum (+1000) -> clamps to zoomMax (500) and trips TeleLimit
    driver.moveZoom(1000, MotorSpeed { 255U });
    EXPECT_EQ(driver.currentZoom().value(), 500U);
    EXPECT_TRUE(hasFlag(driver.limitFlags(), LensLimitFlags::TeleLimit));
    EXPECT_FALSE(hasFlag(driver.limitFlags(), LensLimitFlags::WideLimit));

    // Move zoom backwards past minimum (-1000) -> clamps to zoomMin (50) and trips WideLimit
    driver.moveZoom(-1000, MotorSpeed { 80U });
    EXPECT_EQ(driver.currentZoom().value(), 50U);
    EXPECT_TRUE(hasFlag(driver.limitFlags(), LensLimitFlags::WideLimit));
    EXPECT_FALSE(hasFlag(driver.limitFlags(), LensLimitFlags::TeleLimit));
}

TEST(TestMockLensDriver, AbsolutePositionCommands)
{
    constexpr LensRanges ranges { 100U, 1000U, 50U, 500U };
    MockLensDriver driver { ranges };

    // Drive focus to 450
    driver.setFocusPosition(MotorPosition { 450U }, MotorSpeed { 180U });
    EXPECT_EQ(driver.currentFocus().value(), 450U);
    EXPECT_FALSE(hasFlag(driver.limitFlags(), LensLimitFlags::NearLimit));
    EXPECT_FALSE(hasFlag(driver.limitFlags(), LensLimitFlags::FarLimit));

    // Drive zoom to 320
    driver.setZoomPosition(MotorPosition { 320U }, MotorSpeed { 160U });
    EXPECT_EQ(driver.currentZoom().value(), 320U);
    EXPECT_FALSE(hasFlag(driver.limitFlags(), LensLimitFlags::WideLimit));
    EXPECT_FALSE(hasFlag(driver.limitFlags(), LensLimitFlags::TeleLimit));

    // Command beyond bounds -> clamped
    driver.setFocusPosition(MotorPosition { 5000U }, MotorSpeed { 200U });
    EXPECT_EQ(driver.currentFocus().value(), 1000U);
    EXPECT_TRUE(hasFlag(driver.limitFlags(), LensLimitFlags::FarLimit));

    driver.setZoomPosition(MotorPosition { 0U }, MotorSpeed { 200U });
    EXPECT_EQ(driver.currentZoom().value(), 50U);
    EXPECT_TRUE(hasFlag(driver.limitFlags(), LensLimitFlags::WideLimit));
}

TEST(TestMockLensDriver, StopMotionImmediateHalt)
{
    MockLensDriver driver { };
    driver.moveFocus(100, MotorSpeed { 200U });
    EXPECT_FALSE(driver.isMotionStopped());

    driver.stopMotion();
    EXPECT_TRUE(driver.isMotionStopped());
    EXPECT_TRUE(driver.lastFocusSpeed().isZero());
    EXPECT_TRUE(driver.lastZoomSpeed().isZero());
}

TEST(TestMockLensDriver, RawSerialCommandBuffering)
{
    MockLensDriver driver { };
    const std::vector<std::uint8_t> packet1 { 0x81U, 0x01U, 0x04U, 0x07U, 0x02U, 0xFFU };
    driver.sendRawCommand(packet1.data(), packet1.size());

    EXPECT_EQ(driver.rawCommands(), packet1);

    const std::vector<std::uint8_t> packet2 { 0x81U, 0x01U, 0x04U, 0x00U, 0x02U, 0xFFU };
    driver.sendRawCommand(packet2.data(), packet2.size());

    std::vector<std::uint8_t> expected = packet1;
    expected.insert(expected.end(), packet2.begin(), packet2.end());
    EXPECT_EQ(driver.rawCommands(), expected);

    driver.clearRawCommands();
    EXPECT_TRUE(driver.rawCommands().empty());
}

TEST(TestMockLensDriver, BuiltinAutofocusCapabilities)
{
    MockLensDriver driver { };
    EXPECT_FALSE(driver.hasBuiltinAutofocus());
    EXPECT_FALSE(driver.triggerBuiltinAutofocus());

    driver.setBuiltinAfSupported(true);
    EXPECT_TRUE(driver.hasBuiltinAutofocus());
    EXPECT_TRUE(driver.triggerBuiltinAutofocus());
}

TEST(TestMockLensDriver, BuiltinZoomTrackingCapabilities)
{
    MockLensDriver driver { };
    EXPECT_FALSE(driver.hasBuiltinZoomTracking());

    driver.setBuiltinZoomSupported(true);
    EXPECT_TRUE(driver.hasBuiltinZoomTracking());

    EXPECT_TRUE(driver.setBuiltinZoomTracking(false));
    EXPECT_FALSE(driver.hasBuiltinZoomTracking());
}
