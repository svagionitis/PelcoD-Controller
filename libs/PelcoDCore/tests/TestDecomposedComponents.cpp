/// @file TestDecomposedComponents.cpp
/// @brief Unit tests for decomposed PelcoDCore subsystems (Finding A1).

#include "CallbackDispatcher.h"
#include "CapturingTransport.h"
#include "PelcoDDevice.h"
#include "PelcoDFrame.h"
#include "PelcoDSession.h"
#include "ProtocolMetrics.h"
#include "PtzCommands.h"
#include "TelemetryCoordinator.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <vector>

using namespace PelcoD;
using namespace std::chrono_literals;

TEST(TelemetryCoordinatorTest, InitialStateAndSetters)
{
    TelemetryCoordinator coordinator {};
    EXPECT_FALSE(coordinator.getStatus().connected);
    EXPECT_EQ(coordinator.getStatus().address, 1U);

    coordinator.setConnected(true);
    EXPECT_TRUE(coordinator.getStatus().connected);

    coordinator.setAddress(5U);
    EXPECT_EQ(coordinator.getStatus().address, 5U);

    coordinator.setSpeeds(2U, 3U);
    EXPECT_EQ(coordinator.getStatus().zoomSpeed, 2U);
    EXPECT_EQ(coordinator.getStatus().focusSpeed, 3U);

    coordinator.setActivePreset(12U);
    EXPECT_EQ(coordinator.getStatus().activePreset, 12U);

    coordinator.setFocusPosition(1500U);
    EXPECT_EQ(coordinator.getStatus().focusPosition, 1500U);

    coordinator.setIrisPosition(300U);
    EXPECT_EQ(coordinator.getStatus().irisPosition, 300U);

    coordinator.setPanLimit(true, 18000U);
    EXPECT_EQ(coordinator.getStatus().manualLeftLimitCentidegrees, 18000U);
    EXPECT_EQ(coordinator.getStatus().lastLimitCentidegrees, 18000U);

    coordinator.setScanLimit(false, 27000U);
    EXPECT_EQ(coordinator.getStatus().scanRightLimitCentidegrees, 27000U);

    coordinator.setAutoFocus(AutoMode::Auto);
    EXPECT_EQ(coordinator.getStatus().autoFocus, AutoMode::Auto);

    coordinator.setAutoIris(AutoMode::On);
    EXPECT_EQ(coordinator.getStatus().autoIris, AutoMode::On);

    coordinator.setAgc(AutoMode::Off);
    EXPECT_EQ(coordinator.getStatus().agc, AutoMode::Off);

    coordinator.setBacklightComp(SwitchState::On);
    EXPECT_EQ(coordinator.getStatus().backlightComp, SwitchState::On);

    coordinator.setAutoWhiteBalance(SwitchState::On);
    EXPECT_EQ(coordinator.getStatus().autoWhiteBalance, SwitchState::On);

    coordinator.setSerialNumber("SN12345");
    EXPECT_EQ(coordinator.getInfo().serialNumber, "SN12345");
}

TEST(TelemetryCoordinatorTest, FrameFilteringByAddress)
{
    TelemetryCoordinator coordinator {};
    coordinator.setAddress(1U);

    // Response for address 1 (pan response 0x1234 = 4660 centidegrees)
    const auto frameAddr1 = PelcoDFrame::createFrame(1U, 0x00U, 0x59U, 0x12U, 0x34U);
    EXPECT_TRUE(coordinator.updateFromFrame(frameAddr1));
    EXPECT_EQ(coordinator.getStatus().panCentidegrees, 0x1234U);

    // Response for address 2 must be rejected and not mutate address 1
    const auto frameAddr2 = PelcoDFrame::createFrame(2U, 0x00U, 0x59U, 0x56U, 0x78U);
    EXPECT_FALSE(coordinator.updateFromFrame(frameAddr2));
    EXPECT_EQ(coordinator.getStatus().panCentidegrees, 0x1234U);
    EXPECT_EQ(coordinator.getStatus().address, 1U);
}

TEST(TelemetryCoordinatorTest, PollingConfiguration)
{
    TelemetryCoordinator coordinator {};
    EXPECT_FALSE(coordinator.isPolling());
    EXPECT_EQ(coordinator.getPollIntervalMs(), 1000U);

    coordinator.setPolling(true, 500U);
    EXPECT_TRUE(coordinator.isPolling());
    EXPECT_EQ(coordinator.getPollIntervalMs(), 500U);
}

TEST(ProtocolMetricsTest, CounterAndLatencyRecording)
{
    ProtocolMetrics metrics {};
    const auto initialStats = metrics.getStats();
    EXPECT_EQ(initialStats.queriesSent, 0U);
    EXPECT_EQ(initialStats.queriesCompleted, 0U);
    EXPECT_EQ(initialStats.queryTimeouts, 0U);
    EXPECT_EQ(initialStats.queryRetries, 0U);
    EXPECT_DOUBLE_EQ(initialStats.avgRttMs, 0.0);

    metrics.recordQuerySent();
    metrics.recordQuerySent();
    metrics.recordQuerySuccess(10000us); // 10 ms
    metrics.recordQuerySuccess(20000us); // 20 ms
    metrics.recordQueryTimeout();
    metrics.recordQueryRetry();

    const auto stats = metrics.getStats();
    EXPECT_EQ(stats.queriesSent, 2U);
    EXPECT_EQ(stats.queriesCompleted, 2U);
    EXPECT_EQ(stats.queryTimeouts, 1U);
    EXPECT_EQ(stats.queryRetries, 1U);
    EXPECT_DOUBLE_EQ(stats.lastRttMs, 20.0);
    EXPECT_DOUBLE_EQ(stats.minRttMs, 10.0);
    EXPECT_DOUBLE_EQ(stats.maxRttMs, 20.0);
    EXPECT_DOUBLE_EQ(stats.avgRttMs, 15.0);

    metrics.resetStats();
    const auto reset = metrics.getStats();
    EXPECT_EQ(reset.queriesSent, 0U);
    EXPECT_EQ(reset.queriesCompleted, 0U);
    EXPECT_DOUBLE_EQ(reset.avgRttMs, 0.0);
}

TEST(CallbackDispatcherTest, DispatchAndLifecycle)
{
    CallbackDispatcher dispatcher {};

    std::atomic<std::uint32_t> statusFired { 0U };
    std::atomic<std::uint32_t> trafficFired { 0U };
    std::atomic<std::uint32_t> timeoutFired { 0U };
    std::atomic<std::uint32_t> completedFired { 0U };
    std::atomic<std::uint32_t> retryFired { 0U };
    std::atomic<std::uint32_t> latencyFired { 0U };

    auto connStatus = dispatcher.addStatusCallback([&](const DeviceStatus&) { statusFired.fetch_add(1U); });

    auto connTraffic
        = dispatcher.addTrafficCallback([&](bool, const std::vector<std::uint8_t>&) { trafficFired.fetch_add(1U); });

    auto connTimeout = dispatcher.addTimeoutCallback([&](const std::string&) { timeoutFired.fetch_add(1U); });

    auto connCompleted = dispatcher.addQueryCompletedCallback(
        [&](const std::string&, bool, const DeviceStatus&) { completedFired.fetch_add(1U); });

    auto connRetry = dispatcher.addRetryCallback(
        [&](const std::string&, std::uint32_t, std::uint32_t, std::chrono::milliseconds) { retryFired.fetch_add(1U); });

    auto connLatency = dispatcher.addQueryLatencyCallback(
        [&](const std::string&, std::chrono::microseconds, bool) { latencyFired.fetch_add(1U); });

    DeviceStatus dummyStatus {};
    dispatcher.notifyStatus(dummyStatus);
    dispatcher.notifyTraffic(true, { 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 });
    dispatcher.notifyTimeout("QueryPan");
    dispatcher.notifyQueryCompleted("QueryPan", true, dummyStatus);
    dispatcher.notifyRetry("QueryPan", 1U, 2U, 50ms);
    dispatcher.notifyQueryLatency("QueryPan", 5000us, true);

    EXPECT_EQ(statusFired.load(), 1U);
    EXPECT_EQ(trafficFired.load(), 1U);
    EXPECT_EQ(timeoutFired.load(), 1U);
    EXPECT_EQ(completedFired.load(), 1U);
    EXPECT_EQ(retryFired.load(), 1U);
    EXPECT_EQ(latencyFired.load(), 1U);

    connStatus.disconnect();
    connTraffic.disconnect();
    connTimeout.disconnect();
    connCompleted.disconnect();
    connRetry.disconnect();
    connLatency.disconnect();

    dispatcher.notifyStatus(dummyStatus);
    dispatcher.notifyTraffic(true, { 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 });
    dispatcher.notifyTimeout("QueryPan");
    dispatcher.notifyQueryCompleted("QueryPan", true, dummyStatus);
    dispatcher.notifyRetry("QueryPan", 1U, 2U, 50ms);
    dispatcher.notifyQueryLatency("QueryPan", 5000us, true);

    // Counts must not have increased
    EXPECT_EQ(statusFired.load(), 1U);
    EXPECT_EQ(trafficFired.load(), 1U);
    EXPECT_EQ(timeoutFired.load(), 1U);
    EXPECT_EQ(completedFired.load(), 1U);
    EXPECT_EQ(retryFired.load(), 1U);
    EXPECT_EQ(latencyFired.load(), 1U);
}

TEST(PelcoDSessionAndCommandsTest, PtzCommandDispatchThroughSession)
{
    auto transport = std::make_shared<PelcoD::Test::CapturingTransport>();
    auto session = std::make_shared<PelcoDSession>(transport, 1U);
    auto telemetry = std::make_shared<TelemetryCoordinator>();
    auto guard = std::make_shared<MotionSafetyGuard>(session);

    PtzCommands commands { session, guard, telemetry };

    ASSERT_TRUE(session->start());

    EXPECT_EQ(commands.panLeft(0x20U), SendResult::Success);
    std::this_thread::sleep_for(30ms);
    EXPECT_EQ(commands.tiltUp(0x15U), SendResult::Success);
    std::this_thread::sleep_for(30ms);
    EXPECT_EQ(commands.stopMotion(), SendResult::Success);

    std::this_thread::sleep_for(60ms);
    session->stop();

    const auto frames = transport->sentFrames();
    ASSERT_GE(frames.size(), 3U);
    EXPECT_EQ(frames[0][3], 0x04U); // Pan Left
    EXPECT_EQ(frames[1][3], 0x08U); // Tilt Up
    EXPECT_EQ(frames[2][3], 0x00U); // Stop
}
