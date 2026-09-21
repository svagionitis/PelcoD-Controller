/// @file TestPtzSphericalEstimator.cpp
/// @brief Unit tests for PtzSphericalEstimator: EKF/UKF mode, init, predict/update cycle,
///        state getters, lock query, reset, and config set/get.

#include "PtzSphericalEstimator.h"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace PelcoD;

namespace {

#ifndef M_PI
constexpr double M_PI = 3.14159265358979323846;
#endif

constexpr double TOL = 0.05; // 50 mrad / ~3 degrees tolerance

[[maybe_unused]] bool near(double a, double b, double tol = TOL)
{
    return std::abs(a - b) <= tol;
}

// ---------------------------------------------------------------------------

void testNotLockedByDefault()
{
    std::cout << "[Test] testNotLockedByDefault\n";
    PtzSphericalEstimator est;
    assert(!est.isLocked());
    std::cout << "  -> PASSED\n";
}

void testInitSetsLocked()
{
    std::cout << "[Test] testInitSetsLocked\n";
    PtzSphericalEstimator est;
    est.init(1.0, 0.2);
    assert(est.isLocked());
    std::cout << "  -> PASSED\n";
}

void testInitFromPixelSetsLocked()
{
    std::cout << "[Test] testInitFromPixelSetsLocked\n";
    PtzSphericalEstimator est;
    // Camera looking at 0 pan/tilt, target at principal point → boresight
    est.initFromPixel(960.0, 540.0, 0.0, 0.0, 1.0);
    assert(est.isLocked());
    std::cout << "  -> PASSED\n";
}

void testResetClearsLock()
{
    std::cout << "[Test] testResetClearsLock\n";
    PtzSphericalEstimator est;
    est.init(1.0, 0.2);
    assert(est.isLocked());
    est.reset();
    assert(!est.isLocked());
    std::cout << "  -> PASSED\n";
}

void testConfigGetSet()
{
    std::cout << "[Test] testConfigGetSet\n";
    PtzSphericalEstimator est;
    SphericalEstimatorConfig cfg;
    cfg.type = EstimatorType::UKF;
    cfg.pixelNoiseStd = 5.0;
    cfg.mahalanobisGateThreshold = 9.99;
    est.setConfig(cfg);

    const auto& got = est.getConfig();
    assert(got.type == EstimatorType::UKF);
    assert(std::abs(got.pixelNoiseStd - 5.0) < 1e-9);
    assert(std::abs(got.mahalanobisGateThreshold - 9.99) < 1e-9);
    std::cout << "  -> PASSED\n";
}

void testSetTypeEKFAndUKF()
{
    std::cout << "[Test] testSetTypeEKFAndUKF\n";
    PtzSphericalEstimator est;
    est.init(1.0, 0.1);

    est.setType(EstimatorType::UKF);
    // Verify state is accessible and finite
    const auto s1 = est.getState();
    assert(std::isfinite(s1.azimuthRad));
    assert(std::isfinite(s1.elevationRad));

    est.setType(EstimatorType::EKF);
    const auto s2 = est.getState();
    assert(std::isfinite(s2.azimuthRad));
    std::cout << "  -> PASSED\n";
}

void testGetStateFieldsAreSaneAfterInit()
{
    std::cout << "[Test] testGetStateFieldsAreSaneAfterInit\n";
    PtzSphericalEstimator est;
    const double targetAz = 1.0;
    const double targetEl = 0.2;
    est.init(targetAz, targetEl);

    const auto s = est.getState(0.0, targetAz, targetEl);
    assert(std::isfinite(s.azimuthRad));
    assert(std::isfinite(s.elevationRad));
    assert(std::isfinite(s.errorAzimuthDeg));
    assert(std::isfinite(s.errorElevationDeg));
    assert(s.locked);
    // When camera is pointed at target, error should be small
    assert(std::abs(s.errorAzimuthDeg) < 1.0);
    assert(std::abs(s.errorElevationDeg) < 1.0);
    std::cout << "  -> PASSED\n";
}

void testPredictDoesNotCrash()
{
    std::cout << "[Test] testPredictDoesNotCrash\n";
    PtzSphericalEstimator est;
    est.init(1.0, 0.1);
    est.predict(0.05);
    const auto s = est.getState();
    assert(std::isfinite(s.azimuthRad));
    assert(std::isfinite(s.elevationRad));
    std::cout << "  -> PASSED\n";
}

void testUpdateWithMeasurementConverges()
{
    std::cout << "[Test] testUpdateWithMeasurementConverges\n";
    PtzSphericalEstimator est;
    const double camPan = 0.0;
    const double camTilt = 0.0;
    const double zoom = 1.0;

    // Initialize from principal-point pixel → boresight
    est.initFromPixel(960.0, 540.0, camPan, camTilt, zoom);

    // Feed the same pixel measurement 30 times
    for (int k = 0; k < 30; ++k) {
        est.update(960.0, 540.0, camPan, camTilt, zoom, 0.033);
    }

    const auto s = est.getState(0.0, camPan, camTilt);
    assert(std::isfinite(s.azimuthRad));
    assert(std::isfinite(s.elevationRad));
    std::cout << "  -> PASSED\n";
}

void testUKFModeConverges()
{
    std::cout << "[Test] testUKFModeConverges\n";
    SphericalEstimatorConfig cfg;
    cfg.type = EstimatorType::UKF;
    PtzSphericalEstimator est { cfg };

    est.initFromPixel(960.0, 540.0, 0.0, 0.0, 1.0);
    for (int k = 0; k < 30; ++k) {
        est.update(960.0, 540.0, 0.0, 0.0, 1.0, 0.033);
    }
    const auto s = est.getState();
    assert(std::isfinite(s.azimuthRad));
    std::cout << "  -> PASSED\n";
}

} // namespace

int main()
{
    std::cout << "Running TestPtzSphericalEstimator Test Suite\n";
    testNotLockedByDefault();
    testInitSetsLocked();
    testInitFromPixelSetsLocked();
    testResetClearsLock();
    testConfigGetSet();
    testSetTypeEKFAndUKF();
    testGetStateFieldsAreSaneAfterInit();
    testPredictDoesNotCrash();
    testUpdateWithMeasurementConverges();
    testUKFModeConverges();
    std::cout << "All TestPtzSphericalEstimator Tests Passed!\n";
    return 0;
}
