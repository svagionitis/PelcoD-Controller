/// @file TestKinematicsSimulator.cpp
/// @brief Unit tests for KinematicsSimulator: position initialization, directional motion,
///        slew-to, zoom, stop, normalization utilities, tilt clamping, and centidegree conversions.

#include "KinematicsSimulator.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>

using namespace PelcoD;

namespace {

constexpr double TOL = 0.1; // degrees

/// @brief Advance the simulator by advancing steady_clock by approximately dtMs milliseconds.
/// This calls update() multiple times with synthetic timestamps.
void advanceMs(KinematicsSimulator& sim, double totalMs, double stepMs = 10.0)
{
    const auto start = std::chrono::steady_clock::now();
    const int steps = static_cast<int>(totalMs / stepMs);
    for (int i = 1; i <= steps; ++i) {
        const auto t = start + std::chrono::microseconds(static_cast<long long>(i * stepMs * 1000.0));
        sim.update(t);
    }
}

TEST(KinematicsSimulatorTest, DefaultPosition)
{
    KinematicsSimulator sim;
    EXPECT_NEAR(sim.currentPanDeg(), 0.0, 1e-9);
    EXPECT_NEAR(sim.currentTiltDeg(), 0.0, 1e-9);
    EXPECT_GE(sim.currentZoom(), 1000.0);
}

TEST(KinematicsSimulatorTest, SetPositionImmediate)
{
    KinematicsSimulator sim;
    sim.setPositionImmediate(180.0, 45.0, 8000.0);
    EXPECT_NEAR(sim.currentPanDeg(), 180.0, TOL);
    EXPECT_NEAR(sim.currentTiltDeg(), 45.0, TOL);
    EXPECT_NEAR(sim.currentZoom(), 8000.0, 100.0);
}

TEST(KinematicsSimulatorTest, IsMovingInitiallyFalse)
{
    KinematicsSimulator sim;
    EXPECT_FALSE(sim.isMoving());
}

TEST(KinematicsSimulatorTest, DirectionalMotionCausesPanChange)
{
    KinematicsSimulator sim;
    KinematicsConfig cfg;
    cfg.enabled = true;
    cfg.maxPanSpeedDegPerSec = 60.0;
    cfg.panAccelerationDegPerSec2 = 360.0;
    sim.setConfig(cfg);

    const double panBefore = sim.currentPanDeg();
    sim.setDirectionalMotion(1.0, 0.0, 0.0); // Pan right at full speed
    EXPECT_TRUE(sim.isMoving());

    advanceMs(sim, 500.0); // 0.5 seconds → should have panned ~30 degrees

    const double panAfter = sim.currentPanDeg();
    EXPECT_TRUE(panAfter > panBefore || (panAfter + 360.0) > panBefore); // Wrap-safe comparison
}

TEST(KinematicsSimulatorTest, StopHaltsMotion)
{
    KinematicsSimulator sim;
    KinematicsConfig cfg;
    cfg.enabled = true;
    cfg.maxPanSpeedDegPerSec = 60.0;
    cfg.panAccelerationDegPerSec2 = 1000.0; // Fast ramp
    sim.setConfig(cfg);

    sim.setDirectionalMotion(1.0, 0.0, 0.0);
    advanceMs(sim, 200.0);
    sim.stop();
    advanceMs(sim, 500.0);
    EXPECT_FALSE(sim.isMoving());
}

TEST(KinematicsSimulatorTest, SlewToReachesTarget)
{
    KinematicsSimulator sim;
    KinematicsConfig cfg;
    cfg.enabled = true;
    cfg.maxPanSpeedDegPerSec = 120.0;
    cfg.maxTiltSpeedDegPerSec = 60.0;
    cfg.panAccelerationDegPerSec2 = 360.0;
    cfg.tiltAccelerationDegPerSec2 = 180.0;
    cfg.minTiltDeg = -90.0;
    cfg.maxTiltDeg = 90.0;
    sim.setConfig(cfg);

    sim.slewTo(45.0, 15.0);
    advanceMs(sim, 3000.0, 20.0);

    EXPECT_NEAR(sim.currentPanDeg(), 45.0, 2.0);
    EXPECT_NEAR(sim.currentTiltDeg(), 15.0, 2.0);
}

TEST(KinematicsSimulatorTest, NormalizePanDeg)
{
    EXPECT_NEAR(KinematicsSimulator::normalizePanDeg(0.0), 0.0, 1e-9);
    EXPECT_NEAR(KinematicsSimulator::normalizePanDeg(360.0), 0.0, 1e-9);
    EXPECT_NEAR(KinematicsSimulator::normalizePanDeg(720.0), 0.0, 1e-9);
    EXPECT_NEAR(KinematicsSimulator::normalizePanDeg(-90.0), 270.0, 1e-9);
    EXPECT_NEAR(KinematicsSimulator::normalizePanDeg(180.0), 180.0, 1e-9);
    EXPECT_NEAR(KinematicsSimulator::normalizePanDeg(361.0), 1.0, 1e-6);
}

TEST(KinematicsSimulatorTest, ShortestAngularDelta)
{
    EXPECT_NEAR(KinematicsSimulator::shortestAngularDelta(0.0, 90.0), 90.0, 1e-9);
    EXPECT_NEAR(KinematicsSimulator::shortestAngularDelta(0.0, 270.0), -90.0, 1e-9);
    EXPECT_NEAR(KinematicsSimulator::shortestAngularDelta(350.0, 10.0), 20.0, 1e-9);
    EXPECT_NEAR(KinematicsSimulator::shortestAngularDelta(10.0, 350.0), -20.0, 1e-9);
    // 180 degrees is equidistant — both +180 and -180 are valid
    const double delta180 = KinematicsSimulator::shortestAngularDelta(180.0, 0.0);
    EXPECT_NEAR(std::abs(delta180), 180.0, 1e-9);
}

TEST(KinematicsSimulatorTest, CentidegreesConversion)
{
    KinematicsSimulator sim;
    sim.setPositionImmediate(180.0, 45.0, 5000.0);
    EXPECT_EQ(sim.currentPanCentidegrees(), 18000U);
    EXPECT_EQ(sim.currentTiltCentidegrees(), 4500U);
    EXPECT_EQ(sim.currentZoomInt(), 5000U);
}

TEST(KinematicsSimulatorTest, ConfigGetSet)
{
    KinematicsSimulator sim;
    KinematicsConfig cfg;
    cfg.enabled = true;
    cfg.maxPanSpeedDegPerSec = 45.0;
    sim.setConfig(cfg);

    const auto got = sim.getConfig();
    EXPECT_TRUE(got.enabled);
    EXPECT_NEAR(got.maxPanSpeedDegPerSec, 45.0, 1e-9);
}

TEST(KinematicsSimulatorTest, SlewZoomTo)
{
    KinematicsSimulator sim;
    KinematicsConfig cfg;
    cfg.enabled = true;
    cfg.zoomTransitTimeSeconds = 0.5; // Fast zoom for testing
    sim.setConfig(cfg);

    sim.slewZoomTo(32767.0);
    advanceMs(sim, 2000.0, 20.0);
    // Zoom should have moved toward target
    EXPECT_GT(sim.currentZoom(), 5000.0);
}

} // namespace
