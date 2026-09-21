/// @file TestMockKinematics.cpp
/// @brief Unit tests for Mock Device Kinematics and Latency Simulator.

#include "KinematicsSimulator.h"
#include "LatencyPipeline.h"
#include "MockPelcoDDevice.h"
#include "PelcoDDevice.h"
#include "TestHelpers.h"

#include <atomic>
#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using namespace PelcoDTest;

namespace {

TEST(MockKinematicsTest, InstantaneousDefault)
{
    std::cout << "[Test] testInstantaneousDefault...\n";
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    EXPECT_TRUE(!mock->getKinematicsConfig().enabled);
    EXPECT_TRUE(!mock->getLatencyConfig().enabled);
    EXPECT_TRUE(!mock->isMoving());

    PelcoD::PelcoDDevice device(mock, 1U);
    EXPECT_TRUE(device.start());

    // Instantaneous pan motion
    const auto st0 = mock->getInternalState();
    device.panRight(0x20U);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto st1 = mock->getInternalState();
    EXPECT_TRUE(st1.panCentidegrees > st0.panCentidegrees);
    EXPECT_TRUE(!mock->isMoving()); // In instantaneous mode, isMoving remains false

    // Instantaneous preset setting and recall
    device.setPreset(1U);
    device.zeroPan();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(mock->getInternalState().panCentidegrees == 0U);

    device.goToPreset(1U);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto st2 = mock->getInternalState();
    EXPECT_TRUE(st2.panCentidegrees == st1.panCentidegrees);
    EXPECT_TRUE(!mock->isMoving());

    device.stop();
    std::cout << "  -> PASSED\n";
}

TEST(MockKinematicsTest, KinematicsShortestArcMath)
{
    std::cout << "[Test] testKinematicsShortestArcMath...\n";
    // 350 deg to 10 deg -> +20 deg (clockwise)
    double d1 = PelcoD::KinematicsSimulator::shortestAngularDelta(350.0, 10.0);
    EXPECT_TRUE(std::abs(d1 - 20.0) < 1e-6);

    // 10 deg to 350 deg -> -20 deg (counter-clockwise)
    double d2 = PelcoD::KinematicsSimulator::shortestAngularDelta(10.0, 350.0);
    EXPECT_TRUE(std::abs(d2 - (-20.0)) < 1e-6);

    // 0 deg to 180 deg -> +180 deg
    double d3 = PelcoD::KinematicsSimulator::shortestAngularDelta(0.0, 180.0);
    EXPECT_TRUE(std::abs(std::abs(d3) - 180.0) < 1e-6);

    // 100 deg to 120 deg -> +20 deg
    double d4 = PelcoD::KinematicsSimulator::shortestAngularDelta(100.0, 120.0);
    EXPECT_TRUE(std::abs(d4 - 20.0) < 1e-6);

    // 120 deg to 100 deg -> -20 deg
    double d5 = PelcoD::KinematicsSimulator::shortestAngularDelta(120.0, 100.0);
    EXPECT_TRUE(std::abs(d5 - (-20.0)) < 1e-6);

    std::cout << "  -> PASSED\n";
}

TEST(MockKinematicsTest, ContinuousVelocityMotion)
{
    std::cout << "[Test] testContinuousVelocityMotion...\n";
    PelcoD::KinematicsSimulator sim;
    PelcoD::KinematicsConfig cfg;
    cfg.enabled = true;
    cfg.maxPanSpeedDegPerSec = 60.0;
    cfg.panAccelerationDegPerSec2 = 600.0; // Quick ramp to speed
    sim.setConfig(cfg);
    sim.setPositionImmediate(0.0, 0.0, 1000.0);

    // Start moving pan right at full speed
    sim.setDirectionalMotion(1.0, 0.0);
    EXPECT_TRUE(sim.isMoving());

    // Advance 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    sim.update();
    EXPECT_TRUE(sim.currentPanDeg() > 0.5); // Should have progressed several degrees
    EXPECT_TRUE(sim.isMoving());

    // Stop motion
    sim.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    sim.update();
    // Should decelerate to a stop
    EXPECT_TRUE(!sim.isMoving());
    const double finalPan = sim.currentPanDeg();

    // Further time advances should not change pan
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sim.update();
    EXPECT_TRUE(std::abs(sim.currentPanDeg() - finalPan) < 1e-4);

    std::cout << "  -> PASSED\n";
}

TEST(MockKinematicsTest, PresetSlewShortestArc)
{
    std::cout << "[Test] testPresetSlewShortestArc...\n";
    PelcoD::KinematicsSimulator sim;
    PelcoD::KinematicsConfig cfg;
    cfg.enabled = true;
    cfg.maxPanSpeedDegPerSec = 180.0; // fast slew for test
    cfg.panAccelerationDegPerSec2 = 1800.0;
    sim.setConfig(cfg);

    // Initial position: 350 deg
    sim.setPositionImmediate(350.0, 0.0, 1000.0);

    // Target: 10 deg (across zero boundary)
    sim.slewTo(10.0, 0.0);
    EXPECT_TRUE(sim.isMoving());

    // Wait until slew finishes (should take ~0.15s)
    const auto startWait = std::chrono::steady_clock::now();
    bool reached = false;
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startWait).count()
        < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        sim.update();
        if (!sim.isMoving()) {
            // Slew finished; target should be near 10 deg
            EXPECT_TRUE(std::abs(sim.currentPanDeg() - 10.0) < 0.5);
            reached = true;
            break;
        }
    }
    EXPECT_TRUE(reached);

    std::cout << "  -> PASSED\n";
}

TEST(MockKinematicsTest, ZoomTransitSimulation)
{
    std::cout << "[Test] testZoomTransitSimulation...\n";
    PelcoD::KinematicsSimulator sim;
    PelcoD::KinematicsConfig cfg;
    cfg.enabled = true;
    cfg.zoomTransitTimeSeconds = 0.5; // 0.5s full transit
    sim.setConfig(cfg);
    sim.setPositionImmediate(0.0, 0.0, 1000.0);

    sim.slewZoomTo(30000.0);
    EXPECT_TRUE(sim.isMoving());

    // Advance 100ms -> zoom should be moving up towards 30000
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    sim.update();
    EXPECT_TRUE(sim.currentZoom() > 1000.0);

    // Wait for zoom to reach target
    const auto startWait = std::chrono::steady_clock::now();
    bool reached = false;
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startWait).count()
        < 1500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        sim.update();
        if (!sim.isMoving()) {
            EXPECT_TRUE(std::abs(sim.currentZoom() - 30000.0) < 1.0);
            reached = true;
            break;
        }
    }
    EXPECT_TRUE(reached);

    std::cout << "  -> PASSED\n";
}

TEST(MockKinematicsTest, LatencyQueueTiming)
{
    std::cout << "[Test] testLatencyQueueTiming...\n";
    PelcoD::LatencyPipeline pipe;
    EXPECT_TRUE(!pipe.isWorkerActive() && "LatencyPipeline thread started eagerly in constructor!");

    PelcoD::LatencyConfig cfg;
    cfg.enabled = true;
    cfg.baseLatencyMs = 60U;
    cfg.jitterMs = 0U;
    cfg.packetDropPercent = 0.0;
    pipe.setConfig(cfg);
    EXPECT_TRUE(pipe.isWorkerActive() && "LatencyPipeline thread not started when enabled!");

    std::atomic<bool> received { false };
    std::chrono::steady_clock::time_point sendTime;
    std::chrono::steady_clock::time_point recvTime;

    const auto deliveryCb = [&]([[maybe_unused]] const std::vector<std::uint8_t>& data) {
        recvTime = std::chrono::steady_clock::now();
        received.store(true);
    };

    sendTime = std::chrono::steady_clock::now();
    pipe.enqueue({ 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 }, deliveryCb);

    // Wait for delivery
    const auto startWait = std::chrono::steady_clock::now();
    while (!received.load()
        && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startWait).count()
            < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_TRUE(received.load());
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(recvTime - sendTime).count();
    // Verify delayed by at least ~45ms
    EXPECT_TRUE(elapsedMs >= 45);

    pipe.stop();
    std::cout << "  -> PASSED (elapsed: " << elapsedMs << " ms)\n";
}

TEST(MockKinematicsTest, PacketDropSimulation)
{
    std::cout << "[Test] testPacketDropSimulation...\n";
    PelcoD::LatencyPipeline pipe;
    PelcoD::LatencyConfig cfg;
    cfg.enabled = true;
    cfg.baseLatencyMs = 5U;
    cfg.jitterMs = 0U;
    cfg.packetDropPercent = 100.0; // 100% loss
    pipe.setConfig(cfg);

    std::atomic<int> receivedCount { 0 };
    const auto deliveryCb = [&]([[maybe_unused]] const std::vector<std::uint8_t>& data) { receivedCount.fetch_add(1); };

    for (int i = 0; i < 10; ++i) {
        pipe.enqueue({ 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 }, deliveryCb);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(receivedCount.load() == 0); // All dropped

    // Switch drop rate to 0%
    cfg.packetDropPercent = 0.0;
    pipe.setConfig(cfg);

    for (int i = 0; i < 10; ++i) {
        pipe.enqueue({ 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 }, deliveryCb);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(receivedCount.load() == 10); // All 10 delivered

    pipe.stop();
    std::cout << "  -> PASSED\n";
}

TEST(MockKinematicsTest, MockDeviceWithKinematicsAndLatency)
{
    std::cout << "[Test] testMockDeviceWithKinematicsAndLatency...\n";
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);

    // Initial state: at 90 deg, preset 1 at 30 deg, preset 2 at 90 deg
    PelcoD::MockDeviceState initState;
    initState.panCentidegrees = 9000U;
    initState.presets[1U] = PelcoD::PresetPosition { 3000U, 0U, 1000U };
    initState.presets[2U] = PelcoD::PresetPosition { 9000U, 0U, 1000U };
    mock->setInternalState(initState);

    PelcoD::KinematicsConfig kCfg;
    kCfg.enabled = true;
    kCfg.maxPanSpeedDegPerSec = 120.0;
    kCfg.panAccelerationDegPerSec2 = 1200.0;
    mock->setKinematicsConfig(kCfg);

    PelcoD::LatencyConfig lCfg;
    lCfg.enabled = true;
    lCfg.baseLatencyMs = 20U;
    lCfg.jitterMs = 0U;
    lCfg.packetDropPercent = 0.0;
    mock->setLatencyConfig(lCfg);

    PelcoD::PelcoDDevice device(mock, 1U);
    EXPECT_TRUE(device.start());

    // Command GoToPreset 1 (slew from 90 deg to 30 deg)
    device.goToPreset(1U);

    // Wait until slewing starts (device worker loop dispatches command asynchronously)
    const auto startWaitMove = std::chrono::steady_clock::now();
    bool startedMoving = false;
    while (
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startWaitMove).count()
        < 1000) {
        if (mock->isMoving()) {
            startedMoving = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(startedMoving);

    // Wait until slew reaches preset 1
    const auto startWaitSettle = std::chrono::steady_clock::now();
    bool settled = false;
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startWaitSettle)
               .count()
        < 2000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (!mock->isMoving()) {
            settled = true;
            break;
        }
    }
    EXPECT_TRUE(settled);
    const auto endState = mock->getInternalState();
    EXPECT_TRUE(std::abs(static_cast<int>(endState.panCentidegrees) - 3000) < 100);

    device.stop();
    std::cout << "  -> PASSED\n";
}

} // namespace

