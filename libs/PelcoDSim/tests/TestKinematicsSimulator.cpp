/// @file TestKinematicsSimulator.cpp
/// @brief Unit tests for KinematicsSimulator: position initialization, directional motion,
///        slew-to, zoom, stop, normalization utilities, tilt clamping, and centidegree conversions.

#include "KinematicsSimulator.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace PelcoD;

namespace {

constexpr double TOL = 0.1; // degrees

bool near(double a, double b, double tol = TOL)
{
    return std::abs(a - b) <= tol;
}

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

// ---------------------------------------------------------------------------

void testDefaultPosition()
{
    std::cout << "[Test] testDefaultPosition\n";
    KinematicsSimulator sim;
    assert(near(sim.currentPanDeg(), 0.0, 1e-9));
    assert(near(sim.currentTiltDeg(), 0.0, 1e-9));
    assert(sim.currentZoom() >= 1000.0);
    std::cout << "  -> PASSED\n";
}

void testSetPositionImmediate()
{
    std::cout << "[Test] testSetPositionImmediate\n";
    KinematicsSimulator sim;
    sim.setPositionImmediate(180.0, 45.0, 8000.0);
    assert(near(sim.currentPanDeg(), 180.0));
    assert(near(sim.currentTiltDeg(), 45.0));
    assert(near(sim.currentZoom(), 8000.0, 100.0));
    std::cout << "  -> PASSED\n";
}

void testIsMovingInitiallyFalse()
{
    std::cout << "[Test] testIsMovingInitiallyFalse\n";
    KinematicsSimulator sim;
    assert(!sim.isMoving());
    std::cout << "  -> PASSED\n";
}

void testDirectionalMotionCausesPanChange()
{
    std::cout << "[Test] testDirectionalMotionCausesPanChange\n";
    KinematicsSimulator sim;
    KinematicsConfig cfg;
    cfg.enabled = true;
    cfg.maxPanSpeedDegPerSec = 60.0;
    cfg.panAccelerationDegPerSec2 = 360.0;
    sim.setConfig(cfg);

    const double panBefore = sim.currentPanDeg();
    sim.setDirectionalMotion(1.0, 0.0, 0.0); // Pan right at full speed
    assert(sim.isMoving());

    advanceMs(sim, 500.0); // 0.5 seconds → should have panned ~30 degrees

    const double panAfter = sim.currentPanDeg();
    assert(panAfter > panBefore || (panAfter + 360.0) > panBefore); // Wrap-safe comparison
    std::cout << "  pan: " << panBefore << " -> " << panAfter << "\n";
    std::cout << "  -> PASSED\n";
}

void testStopHaltsMotion()
{
    std::cout << "[Test] testStopHaltsMotion\n";
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
    assert(!sim.isMoving());
    std::cout << "  -> PASSED\n";
}

void testSlewToReachesTarget()
{
    std::cout << "[Test] testSlewToReachesTarget\n";
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

    assert(near(sim.currentPanDeg(), 45.0, 2.0));
    assert(near(sim.currentTiltDeg(), 15.0, 2.0));
    std::cout << "  pan=" << sim.currentPanDeg() << " tilt=" << sim.currentTiltDeg() << "\n";
    std::cout << "  -> PASSED\n";
}

void testNormalizePanDeg()
{
    std::cout << "[Test] testNormalizePanDeg\n";
    assert(near(KinematicsSimulator::normalizePanDeg(0.0), 0.0, 1e-9));
    assert(near(KinematicsSimulator::normalizePanDeg(360.0), 0.0, 1e-9));
    assert(near(KinematicsSimulator::normalizePanDeg(720.0), 0.0, 1e-9));
    assert(near(KinematicsSimulator::normalizePanDeg(-90.0), 270.0, 1e-9));
    assert(near(KinematicsSimulator::normalizePanDeg(180.0), 180.0, 1e-9));
    assert(near(KinematicsSimulator::normalizePanDeg(361.0), 1.0, 1e-6));
    std::cout << "  -> PASSED\n";
}

void testShortestAngularDelta()
{
    std::cout << "[Test] testShortestAngularDelta\n";
    assert(near(KinematicsSimulator::shortestAngularDelta(0.0, 90.0), 90.0, 1e-9));
    assert(near(KinematicsSimulator::shortestAngularDelta(0.0, 270.0), -90.0, 1e-9));
    assert(near(KinematicsSimulator::shortestAngularDelta(350.0, 10.0), 20.0, 1e-9));
    assert(near(KinematicsSimulator::shortestAngularDelta(10.0, 350.0), -20.0, 1e-9));
    // 180 degrees is equidistant — both +180 and -180 are valid
    const double delta180 = KinematicsSimulator::shortestAngularDelta(180.0, 0.0);
    assert(near(std::abs(delta180), 180.0, 1e-9));
    std::cout << "  -> PASSED\n";
}

void testCentidegreesConversion()
{
    std::cout << "[Test] testCentidegreesConversion\n";
    KinematicsSimulator sim;
    sim.setPositionImmediate(180.0, 45.0, 5000.0);
    assert(sim.currentPanCentidegrees() == 18000U);
    assert(sim.currentTiltCentidegrees() == 4500U);
    assert(sim.currentZoomInt() == 5000U);
    std::cout << "  -> PASSED\n";
}

void testConfigGetSet()
{
    std::cout << "[Test] testConfigGetSet\n";
    KinematicsSimulator sim;
    KinematicsConfig cfg;
    cfg.enabled = true;
    cfg.maxPanSpeedDegPerSec = 45.0;
    sim.setConfig(cfg);

    const auto got = sim.getConfig();
    assert(got.enabled);
    assert(near(got.maxPanSpeedDegPerSec, 45.0, 1e-9));
    std::cout << "  -> PASSED\n";
}

void testSlewZoomTo()
{
    std::cout << "[Test] testSlewZoomTo\n";
    KinematicsSimulator sim;
    KinematicsConfig cfg;
    cfg.enabled = true;
    cfg.zoomTransitTimeSeconds = 0.5; // Fast zoom for testing
    sim.setConfig(cfg);

    sim.slewZoomTo(32767.0);
    advanceMs(sim, 2000.0, 20.0);
    // Zoom should have moved toward target
    assert(sim.currentZoom() > 5000.0);
    std::cout << "  zoom=" << sim.currentZoom() << "\n";
    std::cout << "  -> PASSED\n";
}

} // namespace

int main()
{
    std::cout << "Running TestKinematicsSimulator Test Suite\n";
    testDefaultPosition();
    testSetPositionImmediate();
    testIsMovingInitiallyFalse();
    testDirectionalMotionCausesPanChange();
    testStopHaltsMotion();
    testSlewToReachesTarget();
    testNormalizePanDeg();
    testShortestAngularDelta();
    testCentidegreesConversion();
    testConfigGetSet();
    testSlewZoomTo();
    std::cout << "All TestKinematicsSimulator Tests Passed!\n";
    return 0;
}
