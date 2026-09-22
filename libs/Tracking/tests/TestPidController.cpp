/// @file TestPidController.cpp
/// @brief Automated unit test suite for PidController and PtzAutoTracker in PelcoDCore.

#include "PidController.h"
#include "PtzAutoTracker.h"

#include <gtest/gtest.h>
#include <cmath>
#include <iostream>

using namespace PelcoD;

namespace {

TEST(PidControllerTest, ProportionalResponse)
{
    std::cout << "[Test] testProportionalResponse...\n";
    PidController pid(10.0, 0.0, 0.0, 0.0, 0.0, -100.0, 100.0);
    EXPECT_TRUE(pid.getKp() == 10.0);
    EXPECT_TRUE(pid.getKi() == 0.0);
    EXPECT_TRUE(pid.getKd() == 0.0);

    const double out1 = pid.update(2.0, 0.1);
    EXPECT_TRUE(std::abs(out1 - 20.0) < 1e-6);

    const double out2 = pid.update(-3.5, 0.1);
    EXPECT_TRUE(std::abs(out2 - (-35.0)) < 1e-6);

    std::cout << "  -> PASSED\n";
}

TEST(PidControllerTest, IntegralElimination)
{
    std::cout << "[Test] testIntegralElimination...\n";
    // Pure integral controller
    PidController pid(0.0, 5.0, 0.0, 0.0, 0.0, -100.0, 100.0);

    // Step 1: error = 1.0, dt = 0.1 -> integral becomes 0.1 -> out = 0.5
    const double out1 = pid.update(1.0, 0.1);
    EXPECT_TRUE(std::abs(out1 - 0.5) < 1e-6);
    EXPECT_TRUE(std::abs(pid.getIntegral() - 0.1) < 1e-6);

    // Step 2: error = 1.0, dt = 0.1 -> integral becomes 0.2 -> out = 1.0
    const double out2 = pid.update(1.0, 0.1);
    EXPECT_TRUE(std::abs(out2 - 1.0) < 1e-6);
    EXPECT_TRUE(std::abs(pid.getIntegral() - 0.2) < 1e-6);

    pid.reset();
    EXPECT_TRUE(std::abs(pid.getIntegral() - 0.0) < 1e-6);

    std::cout << "  -> PASSED\n";
}

TEST(PidControllerTest, DerivativeDamping)
{
    std::cout << "[Test] testDerivativeDamping...\n";
    PidController pid(0.0, 0.0, 2.0, 0.0, 0.0, -500.0, 500.0);
    pid.setDerivativeFilterAlpha(1.0); // Raw derivative for exact math check

    // Initial step initializes previous error
    const double out1 = pid.update(0.0, 0.1);
    EXPECT_TRUE(std::abs(out1) < 1e-6);

    // Error step from 0.0 to 10.0 in 0.1 sec -> dError/dt = 100.0 -> out = 200.0
    const double out2 = pid.update(10.0, 0.1);
    EXPECT_TRUE(std::abs(out2 - 200.0) < 1e-6);

    // Error remains constant at 10.0 -> dError/dt = 0.0 -> out = 0.0
    const double out3 = pid.update(10.0, 0.1);
    EXPECT_TRUE(std::abs(out3) < 1e-6);

    std::cout << "  -> PASSED\n";
}

TEST(PidControllerTest, Deadband)
{
    std::cout << "[Test] testDeadband...\n";
    PidController pid(50.0, 1.0, 2.0, 0.0, 0.05, -63.0, 63.0);
    EXPECT_TRUE(pid.getDeadband() == 0.05);

    // Error within deadband (+/- 0.05) must produce zero output
    const double outZero1 = pid.update(0.02, 0.04);
    EXPECT_TRUE(std::abs(outZero1) < 1e-6);

    const double outZero2 = pid.update(-0.049, 0.04);
    EXPECT_TRUE(std::abs(outZero2) < 1e-6);

    // Error exceeding deadband produces non-zero output
    const double outActive = pid.update(0.10, 0.04);
    EXPECT_TRUE(outActive > 0.0);

    std::cout << "  -> PASSED\n";
}

TEST(PidControllerTest, AntiWindup)
{
    std::cout << "[Test] testAntiWindup...\n";
    PidController pid(10.0, 10.0, 0.0, 0.0, 0.0, -63.0, 63.0);
    EXPECT_TRUE(pid.getMinOutput() == -63.0);
    EXPECT_TRUE(pid.getMaxOutput() == 63.0);

    // Push large error repeatedly to saturate output at 63.0
    for (int i = 0; i < 20; ++i) {
        const double out = pid.update(10.0, 0.1);
        EXPECT_TRUE(out == 63.0);
    }

    // Now reverse error to negative: anti-windup prevents delayed recovery
    const double outReverse = pid.update(-10.0, 0.1);
    EXPECT_TRUE(outReverse < 63.0);

    std::cout << "  -> PASSED\n";
}

TEST(PidControllerTest, VelocityFeedforward)
{
    std::cout << "[Test] testVelocityFeedforward...\n";
    PidController pid(10.0, 0.0, 0.0, 5.0, 0.0, -100.0, 100.0);
    EXPECT_TRUE(pid.getKff() == 5.0);

    // Error is zero, but target has velocity 4.0 -> feedforward output = 20.0
    const double out = pid.update(0.0, 0.1, 4.0);
    EXPECT_TRUE(std::abs(out - 20.0) < 1e-6);

    std::cout << "  -> PASSED\n";
}

TEST(PidControllerTest, PtzAutoTrackerTracking)
{
    std::cout << "[Test] testPtzAutoTrackerTracking...\n";
    PtzAutoTracker tracker;
    tracker.setMaxSpeeds(63, 63);

    // Target to the right (errorX = 0.5) and above center (errorY = -0.4)
    const auto cmd = tracker.update(0.5, -0.4, 0.0, 0.0, true, false, 0.04);

    EXPECT_TRUE(cmd.state == PtzAutoTracker::TrackingState::Tracking);
    EXPECT_TRUE(cmd.shouldMove);

    // Pan Right (+1)
    EXPECT_TRUE(cmd.panDirection == 1);
    EXPECT_TRUE(cmd.panSpeed > 0 && cmd.panSpeed <= 63);

    // Tilt Up (+1)
    EXPECT_TRUE(cmd.tiltDirection == 1);
    EXPECT_TRUE(cmd.tiltSpeed > 0 && cmd.tiltSpeed <= 63);

    std::cout << "  -> PASSED\n";
}

TEST(PidControllerTest, PtzAutoTrackerDeadband)
{
    std::cout << "[Test] testPtzAutoTrackerDeadband...\n";
    PtzAutoTracker tracker;
    tracker.setDeadbands(0.03, 0.03);

    // Target within 3% deadband of optical boresight
    const auto cmd = tracker.update(0.015, -0.01, 0.0, 0.0, true, false, 0.04);

    EXPECT_TRUE(cmd.state == PtzAutoTracker::TrackingState::Tracking);
    EXPECT_TRUE(!cmd.shouldMove);
    EXPECT_TRUE(cmd.panSpeed == 0);
    EXPECT_TRUE(cmd.tiltSpeed == 0);

    std::cout << "  -> PASSED\n";
}

TEST(PidControllerTest, PtzAutoTrackerCoastingAndLost)
{
    std::cout << "[Test] testPtzAutoTrackerCoastingAndLost...\n";
    PtzAutoTracker tracker;

    // 1. Initial lock tracking
    const auto cmdTrack = tracker.update(0.4, 0.3, 1.0, 0.5, true, false, 0.04);
    EXPECT_TRUE(cmdTrack.state == PtzAutoTracker::TrackingState::Tracking);
    EXPECT_TRUE(cmdTrack.shouldMove);

    // 2. Target occluded (coasting on prediction)
    const auto cmdCoast = tracker.update(0.45, 0.35, 1.0, 0.5, true, true, 0.04);
    EXPECT_TRUE(cmdCoast.state == PtzAutoTracker::TrackingState::Coasting);
    EXPECT_TRUE(cmdCoast.shouldMove);

    // 3. Target lost: initial deceleration step
    const auto cmdDecel = tracker.update(0.0, 0.0, 0.0, 0.0, false, false, 0.1);
    EXPECT_TRUE(cmdDecel.state == PtzAutoTracker::TrackingState::Lost);

    // 4. Target lost past deceleration threshold (0.6s)
    const auto cmdStopped = tracker.update(0.0, 0.0, 0.0, 0.0, false, false, 0.7);
    EXPECT_TRUE(cmdStopped.state == PtzAutoTracker::TrackingState::Lost);
    EXPECT_TRUE(!cmdStopped.shouldMove);
    EXPECT_TRUE(cmdStopped.panSpeed == 0);
    EXPECT_TRUE(cmdStopped.tiltSpeed == 0);

    std::cout << "  -> PASSED\n";
}

TEST(PidControllerTest, AutoZoomFraming)
{
    std::cout << "[Test] testAutoZoomFraming...\n";
    PtzAutoTracker tracker;
    tracker.setAutoZoomEnabled(true);
    tracker.setTargetFramingHeight(0.20, 0.04);
    tracker.setZoomCenteringThreshold(0.25);

    EXPECT_TRUE(tracker.isAutoZoomEnabled());
    EXPECT_TRUE(std::abs(tracker.getTargetFramingHeight() - 0.20) < 1e-6);
    EXPECT_TRUE(std::abs(tracker.getFramingDeadband() - 0.04) < 1e-6);
    EXPECT_TRUE(std::abs(tracker.getZoomCenteringThreshold() - 0.25) < 1e-6);

    // 1. Target too small (height = 0.12 < 0.16) and centered (errorX = 0.05, errorY = 0.05) -> Tele (+1)
    const auto cmdTele = tracker.update(0.05, 0.05, 0.0, 0.0, true, false, 0.04, 0.12);
    EXPECT_TRUE(cmdTele.zoomDirection == 1);
    EXPECT_TRUE(cmdTele.shouldZoom);
    EXPECT_TRUE(cmdTele.zoomSpeed > 0);

    // 2. Target within deadband (height = 0.20) -> Stop (0)
    const auto cmdDeadband = tracker.update(0.05, 0.05, 0.0, 0.0, true, false, 0.04, 0.20);
    EXPECT_TRUE(cmdDeadband.zoomDirection == 0);
    EXPECT_TRUE(!cmdDeadband.shouldZoom);

    // 3. Target too large (height = 0.30 > 0.24) -> Wide (-1)
    const auto cmdWide = tracker.update(0.05, 0.05, 0.0, 0.0, true, false, 0.04, 0.30);
    EXPECT_TRUE(cmdWide.zoomDirection == -1);
    EXPECT_TRUE(cmdWide.shouldZoom);
    EXPECT_TRUE(cmdWide.zoomSpeed > 0);

    // 4. Centering Interlock: Target too small (0.12) but off-center (errorX = 0.40 > 0.25) -> Tele inhibited (0)
    const auto cmdInhibited = tracker.update(0.40, 0.05, 0.0, 0.0, true, false, 0.04, 0.12);
    EXPECT_TRUE(cmdInhibited.zoomDirection == 0);
    EXPECT_TRUE(!cmdInhibited.shouldZoom);

    // 5. Target too large (0.30) and off-center (errorX = 0.40) -> Wide still operates for safety (-1)
    const auto cmdWideOffCenter = tracker.update(0.40, 0.05, 0.0, 0.0, true, false, 0.04, 0.30);
    EXPECT_TRUE(cmdWideOffCenter.zoomDirection == -1);
    EXPECT_TRUE(cmdWideOffCenter.shouldZoom);

    // 6. Target lost -> Zoom immediately stops
    const auto cmdLost = tracker.update(0.0, 0.0, 0.0, 0.0, false, false, 0.04);
    EXPECT_TRUE(cmdLost.zoomDirection == 0);
    EXPECT_TRUE(!cmdLost.shouldZoom);

    std::cout << "  -> PASSED\n";
}

TEST(PidControllerTest, PredictiveLeadBoresight)
{
    std::cout << "[Test] testPredictiveLeadBoresight...\n";
    PtzAutoTracker tracker;
    tracker.setPredictiveLeadEnabled(false);
    EXPECT_TRUE(!tracker.isPredictiveLeadEnabled());

    tracker.setPanGains(40.0, 0.0, 0.0, 0.0); // Pure P-controller for clean math

    // Stationary at errorX = 0.10 -> out = 4.0
    const auto cmdNoLead = tracker.update(0.10, 0.0, 0.0, 0.0, true, false, 0.04);

    // Enable predictive lead
    tracker.setPredictiveLeadEnabled(true);
    tracker.setLeadGain(0.10, 0.25); // kLead = 0.10s, maxLead = 0.25
    EXPECT_TRUE(tracker.isPredictiveLeadEnabled());
    EXPECT_TRUE(std::abs(tracker.getLeadGain() - 0.10) < 1e-6);
    EXPECT_TRUE(std::abs(tracker.getMaxLead() - 0.25) < 1e-6);

    // Moving right at vx = 1.0 -> lead deflection = 0.10 -> effective error = 0.20 -> out should double
    const auto cmdWithLead = tracker.update(0.10, 0.0, 1.0, 0.0, true, false, 0.04);
    EXPECT_TRUE(cmdWithLead.panDirection == 1);
    EXPECT_TRUE(cmdWithLead.panSpeed > cmdNoLead.panSpeed);
    EXPECT_TRUE(std::abs(tracker.getLastLeadOffsetX() - 0.10) < 1e-6);
    EXPECT_TRUE(std::abs(tracker.getLastLeadOffsetY()) < 1e-6);

    // Test clamp: vx = 10.0 -> lead = 1.0 clamped to maxLead = 0.25 -> effective error = 0.10 + 0.25 = 0.35
    const auto cmdClamped = tracker.update(0.10, 0.0, 10.0, 0.0, true, false, 0.04);
    const auto cmdClamped2 = tracker.update(0.10, 0.0, 100.0, 0.0, true, false, 0.04);
    EXPECT_TRUE(cmdClamped.panSpeed == cmdClamped2.panSpeed);
    EXPECT_TRUE(std::abs(tracker.getLastLeadOffsetX() - 0.25) < 1e-6);

    tracker.reset();
    EXPECT_TRUE(std::abs(tracker.getLastLeadOffsetX()) < 1e-6);
    EXPECT_TRUE(std::abs(tracker.getLastLeadOffsetY()) < 1e-6);

    std::cout << "  -> PASSED\n";
}

TEST(PidControllerTest, ZoomAwareGainScheduling)
{
    std::cout << "[Test] testZoomAwareGainScheduling...\n";
    PtzAutoTracker tracker;
    tracker.setPanGains(40.0, 0.0, 0.0, 0.0); // Pure P-controller

    // Base zoom = 1.0x at errorX = 0.20 -> out = 8.0
    const auto cmdWide = tracker.update(0.20, 0.0, 0.0, 0.0, true, false, 0.04, 0.0, 1.0);
    EXPECT_TRUE(cmdWide.panSpeed == 8);

    // Magnification = 16.0x -> gain scaled down by sqrt(16) = 4 -> out should be 2
    const auto cmdTele = tracker.update(0.20, 0.0, 0.0, 0.0, true, false, 0.04, 0.0, 16.0);
    EXPECT_TRUE(cmdTele.panSpeed == 2);

    // Disable gain scheduling -> out returns to 8 even at 16x zoom
    tracker.setZoomGainSchedulingEnabled(false);
    EXPECT_TRUE(!tracker.isZoomGainSchedulingEnabled());
    const auto cmdNoSched = tracker.update(0.20, 0.0, 0.0, 0.0, true, false, 0.04, 0.0, 16.0);
    EXPECT_TRUE(cmdNoSched.panSpeed == 8);

    std::cout << "  -> PASSED\n";
}

} // namespace

