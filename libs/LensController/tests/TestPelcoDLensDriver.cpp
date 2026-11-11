/// @file TestPelcoDLensDriver.cpp
/// @brief GoogleTest suite for Pelco-D motorized lens driver adapter.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include <vector>

#include "LensTypes.h"
#include "PelcoDLensDriver.h"

namespace LensControl {

class TestPelcoDLensDriver : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_driver.setTransmitCallback(&TestPelcoDLensDriver::onTransmit, this);
    }

    static void onTransmit(const std::uint8_t* data, std::size_t len, void* userCtx)
    {
        auto* self = static_cast<TestPelcoDLensDriver*>(userCtx);
        self->m_captured.assign(data, data + len);
    }

    LensRanges m_ranges { 100U, 2000U, 200U, 10000U };
    PelcoDLensDriver m_driver { 1U, m_ranges };
    std::vector<std::uint8_t> m_captured { };
};

TEST_F(TestPelcoDLensDriver, AddressAndInit)
{
    EXPECT_EQ(m_driver.cameraAddress(), 1U);
    m_driver.setCameraAddress(42U);
    EXPECT_EQ(m_driver.cameraAddress(), 42U);
    m_driver.setCameraAddress(0U); // clamps to 1
    EXPECT_EQ(m_driver.cameraAddress(), 1U);

    EXPECT_TRUE(m_driver.initialize(1U, 9600U));
}

TEST_F(TestPelcoDLensDriver, RelativeZoomCommands)
{
    // Zoom Tele: Cmd2 bit 5 (0x20)
    // 0xFF, 0x01, 0x00, 0x20, 0x00, 0x00, Checksum = (1 + 0x20) = 0x21
    m_driver.moveZoom(100, MotorSpeed { 128U });
    const std::vector<std::uint8_t> expectedTele { 0xFFU, 0x01U, 0x00U, 0x20U, 0x00U, 0x00U, 0x21U };
    EXPECT_EQ(m_captured, expectedTele);

    // Zoom Wide: Cmd2 bit 6 (0x40)
    // 0xFF, 0x01, 0x00, 0x40, 0x00, 0x00, Checksum = (1 + 0x40) = 0x41
    m_driver.moveZoom(-100, MotorSpeed { 128U });
    const std::vector<std::uint8_t> expectedWide { 0xFFU, 0x01U, 0x00U, 0x40U, 0x00U, 0x00U, 0x41U };
    EXPECT_EQ(m_captured, expectedWide);

    // Zoom Stop
    m_driver.moveZoom(0, MotorSpeed { 128U });
    const std::vector<std::uint8_t> expectedStop { 0xFFU, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U };
    EXPECT_EQ(m_captured, expectedStop);
}

TEST_F(TestPelcoDLensDriver, RelativeFocusCommands)
{
    // Focus Far: Cmd2 bit 0 (0x01)
    // 0xFF, 0x01, 0x00, 0x01, 0x00, 0x00, Checksum = (1 + 1) = 0x02
    m_driver.moveFocus(50, MotorSpeed { 128U });
    const std::vector<std::uint8_t> expectedFar { 0xFFU, 0x01U, 0x00U, 0x01U, 0x00U, 0x00U, 0x02U };
    EXPECT_EQ(m_captured, expectedFar);

    // Focus Near: Cmd1 bit 0 (0x01)
    // 0xFF, 0x01, 0x01, 0x00, 0x00, 0x00, Checksum = (1 + 1) = 0x02
    m_driver.moveFocus(-50, MotorSpeed { 128U });
    const std::vector<std::uint8_t> expectedNear { 0xFFU, 0x01U, 0x01U, 0x00U, 0x00U, 0x00U, 0x02U };
    EXPECT_EQ(m_captured, expectedNear);

    // Focus Stop
    m_driver.moveFocus(0, MotorSpeed { 128U });
    const std::vector<std::uint8_t> expectedStop { 0xFFU, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U };
    EXPECT_EQ(m_captured, expectedStop);
}

TEST_F(TestPelcoDLensDriver, StopMotionCommand)
{
    m_driver.stopMotion();
    const std::vector<std::uint8_t> expected { 0xFFU, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U };
    EXPECT_EQ(m_captured, expected);
}

TEST_F(TestPelcoDLensDriver, AbsolutePositioningAndClamping)
{
    // Set Zoom Position Opcode 0x4F: pos = 1200 (0x04B0)
    // Checksum = (1 + 0x4F + 0x04 + 0xB0) % 256 = (1 + 79 + 4 + 176) = 260 % 256 = 4
    m_driver.setZoomPosition(MotorPosition { 1200U }, MotorSpeed { 128U });
    const std::vector<std::uint8_t> expectedZoom { 0xFFU, 0x01U, 0x00U, 0x4FU, 0x04U, 0xB0U, 0x04U };
    EXPECT_EQ(m_captured, expectedZoom);
    EXPECT_EQ(m_driver.currentZoom().value(), 1200U);

    // Set Focus Position Opcode 0x4B: pos = 600 (0x0258)
    // Checksum = (1 + 0x4B + 0x02 + 0x58) % 256 = (1 + 75 + 2 + 88) = 166 (0xA6)
    m_driver.setFocusPosition(MotorPosition { 600U }, MotorSpeed { 128U });
    const std::vector<std::uint8_t> expectedFocus { 0xFFU, 0x01U, 0x00U, 0x4BU, 0x02U, 0x58U, 0xA6U };
    EXPECT_EQ(m_captured, expectedFocus);
    EXPECT_EQ(m_driver.currentFocus().value(), 600U);

    // Clamping on bounds
    m_driver.setZoomPosition(MotorPosition { 50000U }, MotorSpeed { 128U });
    EXPECT_EQ(m_driver.currentZoom().value(), 10000U);
    EXPECT_TRUE(hasFlag(m_driver.limitFlags(), LensLimitFlags::TeleLimit));

    m_driver.setFocusPosition(MotorPosition { 10U }, MotorSpeed { 128U });
    EXPECT_EQ(m_driver.currentFocus().value(), 100U);
    EXPECT_TRUE(hasFlag(m_driver.limitFlags(), LensLimitFlags::NearLimit));
}

TEST_F(TestPelcoDLensDriver, QueryAndResponseParsing)
{
    // Query Zoom (Opcode 0x5D)
    // Checksum = (1 + 0x5D) = 0x5E
    m_driver.queryZoomPosition();
    const std::vector<std::uint8_t> expectedZoomQry { 0xFFU, 0x01U, 0x00U, 0x5DU, 0x00U, 0x00U, 0x5EU };
    EXPECT_EQ(m_captured, expectedZoomQry);

    // Response for Zoom: 1200 (0x04B0), Checksum = (1 + 0x5D + 0x04 + 0xB0) % 256 = 0x12
    const std::uint8_t zoomRespCheck = static_cast<std::uint8_t>((1U + 0x5DU + 0x04U + 0xB0U) & 0xFFU);
    const std::vector<std::uint8_t> zoomResp { 0xFFU, 0x01U, 0x00U, 0x5DU, 0x04U, 0xB0U, zoomRespCheck };
    m_driver.processIncomingBytes(zoomResp.data(), zoomResp.size());
    EXPECT_EQ(m_driver.currentZoom().value(), 1200U);

    // Query Focus (Opcode 0x57)
    // Checksum = (1 + 0x57) = 0x58
    m_driver.queryFocusPosition();
    const std::vector<std::uint8_t> expectedFocusQry { 0xFFU, 0x01U, 0x00U, 0x57U, 0x00U, 0x00U, 0x58U };
    EXPECT_EQ(m_captured, expectedFocusQry);

    // Response for Focus: 600 (0x0258)
    const std::uint8_t focusRespCheck = static_cast<std::uint8_t>((1U + 0x57U + 0x02U + 0x58U) & 0xFFU);
    const std::vector<std::uint8_t> focusResp { 0xFFU, 0x01U, 0x00U, 0x57U, 0x02U, 0x58U, focusRespCheck };
    m_driver.processIncomingBytes(focusResp.data(), focusResp.size());
    EXPECT_EQ(m_driver.currentFocus().value(), 600U);
}

TEST_F(TestPelcoDLensDriver, BuiltinAfAndTrackingDefaults)
{
    EXPECT_FALSE(m_driver.hasBuiltinAutofocus());
    EXPECT_FALSE(m_driver.triggerBuiltinAutofocus());
    EXPECT_FALSE(m_driver.hasBuiltinZoomTracking());
    EXPECT_FALSE(m_driver.setBuiltinZoomTracking(true));
}

TEST_F(TestPelcoDLensDriver, RawCommandPassthrough)
{
    const std::vector<std::uint8_t> raw { 0x01U, 0x02U, 0x03U, 0x04U };
    m_driver.sendRawCommand(raw.data(), raw.size());
    EXPECT_EQ(m_captured, raw);
}

} // namespace LensControl
