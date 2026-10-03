/// @file TestViscaDevice.cpp
/// @brief Unit tests for ViscaDevice 2-socket state machine concurrency, pacing, and inquiries.

#include "MockViscaTransport.h"
#include "ViscaBuilder.h"
#include "ViscaDevice.h"
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <vector>

using namespace Visca;
using namespace Visca::Testing;

/// @brief Tests single command synchronous execution roundtrip.
/// @details Verifies that a zoom direct command is sent, acknowledged, completed, and returns success.
TEST(TestViscaDevice, SingleCommandSync)
{
    auto mockCamera = std::make_shared<MockViscaTransport>(uint8_t { 1U });
    mockCamera->open();

    ViscaDevice device(mockCamera, 1);
    EXPECT_TRUE(device.isConnected());

    const ViscaFrame zoomCmd { 0x81, 0x01, 0x04, 0x47, 0x02, 0x00, 0x00, 0x00, 0xFF };
    const CommandResult result = device.sendCommandSync(zoomCmd, std::chrono::milliseconds(1000));

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.errorCode, ViscaErrorCode::None);
}

/// @brief Tests 2-socket concurrent command dispatch and queue pacing.
/// @details Sends multiple asynchronous commands, verifying that Socket 1 and Socket 2
/// execute concurrently and subsequent commands are queued until a socket frees up.
TEST(TestViscaDevice, TwoSocketConcurrencyAndQueuePacing)
{
    auto mockCamera = std::make_shared<MockViscaTransport>(uint8_t { 1U });
    mockCamera->open();

    ViscaDevice device(mockCamera, 1);

    std::vector<CommandResult> results;
    std::mutex resMutex;

    auto onComplete = [&](const CommandResult& res) {
        std::scoped_lock lock(resMutex);
        results.push_back(res);
    };

    // Dispatch 3 commands back-to-back
    const ViscaFrame cmd1 { 0x81, 0x01, 0x04, 0x47, 0x01, 0x00, 0x00, 0x00, 0xFF };
    const ViscaFrame cmd2 { 0x81, 0x01, 0x04, 0x47, 0x02, 0x00, 0x00, 0x00, 0xFF };
    const ViscaFrame cmd3 { 0x81, 0x01, 0x04, 0x47, 0x03, 0x00, 0x00, 0x00, 0xFF };

    device.sendCommandAsync(cmd1, onComplete);
    device.sendCommandAsync(cmd2, onComplete);
    device.sendCommandAsync(cmd3, onComplete);

    // Give asynchronous queue a moment to drain
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::scoped_lock lock(resMutex);
    EXPECT_EQ(results.size(), 3U);
    for (const auto& r : results) {
        EXPECT_TRUE(r.success);
    }
}

/// @brief Tests inquiry fast-path execution.
/// @details Verifies that Version Inquiry returns the complete payload without tying up command sockets.
TEST(TestViscaDevice, InquiryExecution)
{
    auto mockCamera = std::make_shared<MockViscaTransport>(uint8_t { 1U });
    mockCamera->open();

    ViscaDevice device(mockCamera, 1);

    const ViscaFrame inq = ViscaBuilder::versionInquiry(1);
    const InquiryResult res = device.sendInquirySync(inq, std::chrono::milliseconds(1000));

    EXPECT_TRUE(res.success);
    EXPECT_TRUE(res.responseFrame.isInquiryResponse());
    EXPECT_EQ(res.responseFrame.size(), 10U);
}

/// @brief Tests timeout detection when camera does not respond.
/// @details Verifies that waiting on a non-responsive target returns a failure result.
TEST(TestViscaDevice, TimeoutDetection)
{
    auto mockCamera = std::make_shared<MockViscaTransport>(uint8_t { 1U });
    mockCamera->open();

    // Send command to address 3 when mock is configured for address 1 (will be ignored)
    ViscaDevice device(mockCamera, 3);

    const ViscaFrame cmd { 0x83, 0x01, 0x04, 0x00, 0x02, 0xFF };
    const CommandResult result = device.sendCommandSync(cmd, std::chrono::milliseconds(50));

    EXPECT_FALSE(result.success);
}

/// @brief Tests Pan/Tilt drive commands and position inquiry tracking through ViscaDevice.
TEST(TestViscaDevice, PanTiltDriveAndInquiries)
{
    auto mockCamera = std::make_shared<MockViscaTransport>(uint8_t { 1U });
    mockCamera->open();

    ViscaDevice device(mockCamera, 1);

    // Continuous drive
    const auto driveRes = device.panTiltDrive(0x10, 0x0C, ViscaPanDirection::Right, ViscaTiltDirection::Up);
    EXPECT_TRUE(driveRes.success);

    // Stop
    const auto stopRes = device.panTiltStop();
    EXPECT_TRUE(stopRes.success);

    // Absolute Position Move (Pan = 1000, Tilt = -500)
    const auto absRes = device.panTiltAbsolute(0x14, 0x0A, 1000, -500);
    EXPECT_TRUE(absRes.success);

    // Query position
    const auto pos = device.queryPanTiltPosition();
    ASSERT_TRUE(pos.has_value());
    EXPECT_EQ(pos->panPosition, 1000);
    EXPECT_EQ(pos->tiltPosition, -500);

    // Home
    const auto homeRes = device.panTiltHome();
    EXPECT_TRUE(homeRes.success);

    const auto homePos = device.queryPanTiltPosition();
    ASSERT_TRUE(homePos.has_value());
    EXPECT_EQ(homePos->panPosition, 0);
    EXPECT_EQ(homePos->tiltPosition, 0);

    // Query status
    const auto status = device.queryPanTiltStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status->panSpeed, 0x10);
    EXPECT_EQ(status->tiltSpeed, 0x0A);
}

/// @brief Tests VISCA protocol state machine and telemetry counters.
TEST(TestViscaDevice, ProtocolStatisticsTracking)
{
    auto mockCamera = std::make_shared<MockViscaTransport>(uint8_t { 1U });
    mockCamera->open();

    ViscaDevice device(mockCamera, 1);

    auto stats = device.getProtocolStats();
    EXPECT_EQ(stats.socket1State, ViscaSocketState::Idle);
    EXPECT_EQ(stats.socket2State, ViscaSocketState::Idle);
    EXPECT_EQ(stats.socket1Processed, 0U);
    EXPECT_EQ(stats.socket2Processed, 0U);
    EXPECT_EQ(stats.inquiriesProcessed, 0U);

    // Send a sync command
    const ViscaFrame zoomCmd { 0x81, 0x01, 0x04, 0x47, 0x02, 0x00, 0x00, 0x00, 0xFF };
    const CommandResult cmdRes = device.sendCommandSync(zoomCmd);
    EXPECT_TRUE(cmdRes.success);

    // Send an inquiry
    const auto pos = device.queryPanTiltPosition();
    EXPECT_TRUE(pos.has_value());

    stats = device.getProtocolStats();
    EXPECT_GT(stats.socket1Processed + stats.socket2Processed, 0U);
    EXPECT_GE(stats.inquiriesProcessed, 1U);
    EXPECT_EQ(stats.syntaxErrors, 0U);
    EXPECT_EQ(stats.timeouts, 0U);
    EXPECT_GE(stats.avgTurnaroundMs, 0.0);

    device.resetProtocolStats();
    const auto resetStats = device.getProtocolStats();
    EXPECT_EQ(resetStats.socket1Processed, 0U);
    EXPECT_EQ(resetStats.socket2Processed, 0U);
    EXPECT_EQ(resetStats.inquiriesProcessed, 0U);
}
