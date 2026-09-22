/// @file TestMockSonyCamera.cpp
/// @brief Unit tests verifying MockSonyCamera simulation realism and error injection.

#include "MockSonyCamera.h"
#include "SonyViscaBuilder.h"
#include <ViscaBuilder.h>
#include <ViscaParser.h>
#include <gtest/gtest.h>

using namespace Visca;
using namespace Visca::Sony;

/// @brief Tests MockSonyCamera ITransport lifecycle methods.
/// @details Verifies open(), close(), isOpen(), and baud rate modification.
TEST(TestMockSonyCamera, TransportLifecycle)
{
    MockSonyCamera mock;
    EXPECT_TRUE(mock.isOpen());

    mock.setBaudRate(38400);
    EXPECT_EQ(mock.getBaudRate(), 38400U);

    mock.close();
    EXPECT_FALSE(mock.isOpen());

    EXPECT_TRUE(mock.open());
    EXPECT_TRUE(mock.isOpen());
}

/// @brief Tests direct register read and write operations.
/// @details Verifies setting and querying camera register banks 0x00 to 0x7F.
TEST(TestMockSonyCamera, RegisterManipulation)
{
    MockSonyCamera mock(SonyCameraModelType::FCB_EV9520L, 1);

    mock.setRegisterValue(0x57, 0x01); // Distortion comp
    EXPECT_EQ(mock.registerValue(0x57), 0x01);

    std::vector<ViscaFrame> rxFrames;
    mock.setDataCallback([&](const std::vector<uint8_t>& data) {
        ViscaRxAccumulator acc;
        acc.setFrameCallback([&](const ViscaFrame& f) { rxFrames.push_back(f); });
        acc.addData(data);
    });

    // Send register write via VISCA: 81 01 04 24 47 00 01 FF
    const ViscaFrame writeReg = SonyViscaBuilder::writeRegister(1, 0x47, 0x01);
    EXPECT_TRUE(mock.sendData(writeReg.bytes()));

    EXPECT_EQ(mock.registerValue(0x47), 0x01);
}

/// @brief Tests error injection into MockSonyCamera.
/// @details Injects a CommandNotExecutable error and verifies that the camera returns y0 60 41 FF.
TEST(TestMockSonyCamera, InjectedErrors)
{
    MockSonyCamera mock(SonyCameraModelType::FCB_EV9520L, 1);

    std::vector<ViscaFrame> rxFrames;
    ViscaRxAccumulator acc;
    acc.setFrameCallback([&](const ViscaFrame& f) { rxFrames.push_back(f); });

    mock.setDataCallback([&](const std::vector<uint8_t>& data) { acc.addData(data); });

    mock.injectNextError(ViscaErrorCode::CommandNotExecutable);

    const ViscaFrame zoomCmd = SonyViscaBuilder::zoomTele(1);
    mock.sendData(zoomCmd.bytes());

    ASSERT_EQ(rxFrames.size(), 1U);
    EXPECT_TRUE(rxFrames[0].isError());
    EXPECT_EQ(rxFrames[0].errorCode(), ViscaErrorCode::CommandNotExecutable);
}

/// @brief Tests Command Cancel handling.
/// @details Sends cancel command (81 21 FF) and verifies cancellation response (90 61 04 FF).
TEST(TestMockSonyCamera, CommandCancelHandling)
{
    MockSonyCamera mock(SonyCameraModelType::FCB_EV9520L, 1);

    std::vector<ViscaFrame> rxFrames;
    ViscaRxAccumulator acc;
    acc.setFrameCallback([&](const ViscaFrame& f) { rxFrames.push_back(f); });

    mock.setDataCallback([&](const std::vector<uint8_t>& data) { acc.addData(data); });

    const ViscaFrame cancelCmd = ViscaBuilder::commandCancel(1, ViscaSocket::Socket1);
    mock.sendData(cancelCmd.bytes());

    ASSERT_EQ(rxFrames.size(), 1U);
    EXPECT_TRUE(rxFrames[0].isError());
    EXPECT_EQ(rxFrames[0].socket(), ViscaSocket::Socket1);
    EXPECT_EQ(rxFrames[0].errorCode(), ViscaErrorCode::CommandCanceled);
}
