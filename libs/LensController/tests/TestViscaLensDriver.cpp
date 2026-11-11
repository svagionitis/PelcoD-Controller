/// @file TestViscaLensDriver.cpp
/// @brief GoogleTest suite for Sony VISCA optical camera block driver adapter.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include <vector>

#include "LensTypes.h"
#include "ViscaLensDriver.h"

namespace LensControl {

class TestViscaLensDriver : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_driver.setTransmitCallback(&TestViscaLensDriver::onTransmit, this);
    }

    static void onTransmit(const std::uint8_t* data, std::size_t len, void* userCtx)
    {
        auto* self = static_cast<TestViscaLensDriver*>(userCtx);
        self->m_captured.assign(data, data + len);
    }

    LensRanges m_ranges { 100U, 1000U, 50U, 5000U };
    ViscaLensDriver m_driver { 1U, m_ranges };
    std::vector<std::uint8_t> m_captured { };
};

TEST_F(TestViscaLensDriver, AddressAndInit)
{
    EXPECT_EQ(m_driver.cameraAddress(), 1U);
    m_driver.setCameraAddress(5U);
    EXPECT_EQ(m_driver.cameraAddress(), 5U);
    m_driver.setCameraAddress(10U); // clamps to 7
    EXPECT_EQ(m_driver.cameraAddress(), 7U);
    m_driver.setCameraAddress(0U); // clamps to 1
    EXPECT_EQ(m_driver.cameraAddress(), 1U);

    EXPECT_TRUE(m_driver.initialize(1U, 9600U));
}

TEST_F(TestViscaLensDriver, RelativeZoomCommands)
{
    // Zoom Tele at max speed 255 -> VISCA speed 7
    m_driver.moveZoom(100, MotorSpeed { 255U });
    const std::vector<std::uint8_t> expectedTele { 0x81U, 0x01U, 0x04U, 0x07U, 0x27U, 0xFFU };
    EXPECT_EQ(m_captured, expectedTele);

    // Zoom Wide at min speed 0 -> VISCA speed 0
    m_driver.moveZoom(-100, MotorSpeed { 0U });
    const std::vector<std::uint8_t> expectedWide { 0x81U, 0x01U, 0x04U, 0x07U, 0x30U, 0xFFU };
    EXPECT_EQ(m_captured, expectedWide);

    // Zoom Stop (step 0)
    m_driver.moveZoom(0, MotorSpeed { 128U });
    const std::vector<std::uint8_t> expectedStop { 0x81U, 0x01U, 0x04U, 0x07U, 0x00U, 0xFFU };
    EXPECT_EQ(m_captured, expectedStop);
}

TEST_F(TestViscaLensDriver, RelativeFocusCommands)
{
    // Focus Far at speed 255 -> VISCA speed 7
    m_driver.moveFocus(50, MotorSpeed { 255U });
    const std::vector<std::uint8_t> expectedFar { 0x81U, 0x01U, 0x04U, 0x08U, 0x27U, 0xFFU };
    EXPECT_EQ(m_captured, expectedFar);

    // Focus Near at speed 0 -> VISCA speed 0
    m_driver.moveFocus(-50, MotorSpeed { 0U });
    const std::vector<std::uint8_t> expectedNear { 0x81U, 0x01U, 0x04U, 0x08U, 0x30U, 0xFFU };
    EXPECT_EQ(m_captured, expectedNear);

    // Focus Stop
    m_driver.moveFocus(0, MotorSpeed { 128U });
    const std::vector<std::uint8_t> expectedStop { 0x81U, 0x01U, 0x04U, 0x08U, 0x00U, 0xFFU };
    EXPECT_EQ(m_captured, expectedStop);
}

TEST_F(TestViscaLensDriver, StopMotionCommands)
{
    m_driver.stopMotion();
    // Last transmitted is focus stop: 81 01 04 08 00 FF
    const std::vector<std::uint8_t> expected { 0x81U, 0x01U, 0x04U, 0x08U, 0x00U, 0xFFU };
    EXPECT_EQ(m_captured, expected);
}

TEST_F(TestViscaLensDriver, AbsolutePositioningAndClamping)
{
    // Zoom Direct: 0x0800 (2048) -> nibbles 0, 8, 0, 0
    m_driver.setZoomPosition(MotorPosition { 2048U }, MotorSpeed { 128U });
    const std::vector<std::uint8_t> expectedZoom { 0x81U, 0x01U, 0x04U, 0x47U, 0x00U, 0x08U, 0x00U, 0x00U, 0xFFU };
    EXPECT_EQ(m_captured, expectedZoom);
    EXPECT_EQ(m_driver.currentZoom().value(), 2048U);

    // Focus Direct: 0x0258 (600) -> nibbles 0, 2, 5, 8
    m_driver.setFocusPosition(MotorPosition { 600U }, MotorSpeed { 128U });
    const std::vector<std::uint8_t> expectedFocus { 0x81U, 0x01U, 0x04U, 0x48U, 0x00U, 0x02U, 0x05U, 0x08U, 0xFFU };
    EXPECT_EQ(m_captured, expectedFocus);
    EXPECT_EQ(m_driver.currentFocus().value(), 600U);

    // Zoom clamped to max (5000)
    m_driver.setZoomPosition(MotorPosition { 60000U }, MotorSpeed { 128U });
    EXPECT_EQ(m_driver.currentZoom().value(), 5000U);
    EXPECT_TRUE(hasFlag(m_driver.limitFlags(), LensLimitFlags::TeleLimit));

    // Focus clamped to min (100)
    m_driver.setFocusPosition(MotorPosition { 10U }, MotorSpeed { 128U });
    EXPECT_EQ(m_driver.currentFocus().value(), 100U);
    EXPECT_TRUE(hasFlag(m_driver.limitFlags(), LensLimitFlags::NearLimit));
}

TEST_F(TestViscaLensDriver, BuiltinAutofocusAndTracking)
{
    EXPECT_TRUE(m_driver.hasBuiltinAutofocus());
    EXPECT_TRUE(m_driver.triggerBuiltinAutofocus());
    const std::vector<std::uint8_t> expectedAf { 0x81U, 0x01U, 0x04U, 0x18U, 0x01U, 0xFFU };
    EXPECT_EQ(m_captured, expectedAf);

    EXPECT_TRUE(m_driver.hasBuiltinZoomTracking());
    EXPECT_TRUE(m_driver.setBuiltinZoomTracking(true));
    const std::vector<std::uint8_t> expectedAfAuto { 0x81U, 0x01U, 0x04U, 0x38U, 0x02U, 0xFFU };
    EXPECT_EQ(m_captured, expectedAfAuto);

    EXPECT_TRUE(m_driver.setBuiltinZoomTracking(false));
    const std::vector<std::uint8_t> expectedAfManual { 0x81U, 0x01U, 0x04U, 0x38U, 0x03U, 0xFFU };
    EXPECT_EQ(m_captured, expectedAfManual);
}

TEST_F(TestViscaLensDriver, InquiriesAndResponseParsing)
{
    // Zoom inquiry
    m_driver.queryZoomPosition();
    const std::vector<std::uint8_t> expectedZoomInq { 0x81U, 0x09U, 0x04U, 0x47U, 0xFFU };
    EXPECT_EQ(m_captured, expectedZoomInq);

    // Incoming reply for Zoom: 0x04B0 (1200)
    const std::vector<std::uint8_t> zoomReply { 0x90U, 0x50U, 0x00U, 0x04U, 0x0BU, 0x00U, 0xFFU };
    m_driver.processIncomingBytes(zoomReply.data(), zoomReply.size());
    EXPECT_EQ(m_driver.currentZoom().value(), 1200U);

    // Focus inquiry
    m_driver.queryFocusPosition();
    const std::vector<std::uint8_t> expectedFocusInq { 0x81U, 0x09U, 0x04U, 0x48U, 0xFFU };
    EXPECT_EQ(m_captured, expectedFocusInq);

    // Incoming reply for Focus: 0x01F4 (500)
    const std::vector<std::uint8_t> focusReply { 0x90U, 0x50U, 0x00U, 0x01U, 0x0FU, 0x04U, 0xFFU };
    m_driver.processIncomingBytes(focusReply.data(), focusReply.size());
    EXPECT_EQ(m_driver.currentFocus().value(), 500U);
}

TEST_F(TestViscaLensDriver, RawCommandPassthrough)
{
    const std::vector<std::uint8_t> raw { 0x81U, 0x01U, 0x00U, 0x01U, 0xFFU };
    m_driver.sendRawCommand(raw.data(), raw.size());
    EXPECT_EQ(m_captured, raw);
}

} // namespace LensControl
