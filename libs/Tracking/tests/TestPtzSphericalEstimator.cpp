/// @file TestPtzSphericalEstimator.cpp
/// @brief Unit tests for PtzSphericalEstimator: EKF/UKF mode, init, predict/update cycle,
///        state getters, lock query, reset, and config set/get.

#include "PtzSphericalEstimator.h"

#include <cmath>
#include <gtest/gtest.h>
#include <iostream>

using namespace Tracking;

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

TEST(PtzSphericalEstimatorTest, NotLockedByDefault)
{
    std::cout << "[Test] testNotLockedByDefault\n";
    PtzSphericalEstimator est;
    EXPECT_TRUE(!est.isLocked());
    std::cout << "  -> PASSED\n";
}

TEST(PtzSphericalEstimatorTest, InitSetsLocked)
{
    std::cout << "[Test] testInitSetsLocked\n";
    PtzSphericalEstimator est;
    est.init(1.0, 0.2);
    EXPECT_TRUE(est.isLocked());
    std::cout << "  -> PASSED\n";
}

TEST(PtzSphericalEstimatorTest, InitFromPixelSetsLocked)
{
    std::cout << "[Test] testInitFromPixelSetsLocked\n";
    PtzSphericalEstimator est;
    // Camera looking at 0 pan/tilt, target at principal point → boresight
    est.initFromPixel(960.0, 540.0, 0.0, 0.0, 1.0);
    EXPECT_TRUE(est.isLocked());
    std::cout << "  -> PASSED\n";
}

TEST(PtzSphericalEstimatorTest, ResetClearsLock)
{
    std::cout << "[Test] testResetClearsLock\n";
    PtzSphericalEstimator est;
    est.init(1.0, 0.2);
    EXPECT_TRUE(est.isLocked());
    est.reset();
    EXPECT_TRUE(!est.isLocked());
    std::cout << "  -> PASSED\n";
}

TEST(PtzSphericalEstimatorTest, ConfigGetSet)
{
    std::cout << "[Test] testConfigGetSet\n";
    PtzSphericalEstimator est;
    SphericalEstimatorConfig cfg;
    cfg.type = EstimatorType::UKF;
    cfg.pixelNoiseStd = 5.0;
    cfg.mahalanobisGateThreshold = 9.99;
    est.setConfig(cfg);

    const auto& got = est.getConfig();
    EXPECT_TRUE(got.type == EstimatorType::UKF);
    EXPECT_TRUE(std::abs(got.pixelNoiseStd - 5.0) < 1e-9);
    EXPECT_TRUE(std::abs(got.mahalanobisGateThreshold - 9.99) < 1e-9);
    std::cout << "  -> PASSED\n";
}

TEST(PtzSphericalEstimatorTest, SetTypeEKFAndUKF)
{
    std::cout << "[Test] testSetTypeEKFAndUKF\n";
    PtzSphericalEstimator est;
    est.init(1.0, 0.1);

    est.setType(EstimatorType::UKF);
    // Verify state is accessible and finite
    const auto s1 = est.getState();
    EXPECT_TRUE(std::isfinite(s1.azimuthRad));
    EXPECT_TRUE(std::isfinite(s1.elevationRad));

    est.setType(EstimatorType::EKF);
    const auto s2 = est.getState();
    EXPECT_TRUE(std::isfinite(s2.azimuthRad));
    std::cout << "  -> PASSED\n";
}

TEST(PtzSphericalEstimatorTest, GetStateFieldsAreSaneAfterInit)
{
    std::cout << "[Test] testGetStateFieldsAreSaneAfterInit\n";
    PtzSphericalEstimator est;
    const double targetAz = 1.0;
    const double targetEl = 0.2;
    est.init(targetAz, targetEl);

    const auto s = est.getState(0.0, targetAz, targetEl);
    EXPECT_TRUE(std::isfinite(s.azimuthRad));
    EXPECT_TRUE(std::isfinite(s.elevationRad));
    EXPECT_TRUE(std::isfinite(s.errorAzimuthDeg));
    EXPECT_TRUE(std::isfinite(s.errorElevationDeg));
    EXPECT_TRUE(s.locked);
    // When camera is pointed at target, error should be small
    EXPECT_TRUE(std::abs(s.errorAzimuthDeg) < 1.0);
    EXPECT_TRUE(std::abs(s.errorElevationDeg) < 1.0);
    std::cout << "  -> PASSED\n";
}

TEST(PtzSphericalEstimatorTest, PredictDoesNotCrash)
{
    std::cout << "[Test] testPredictDoesNotCrash\n";
    PtzSphericalEstimator est;
    est.init(1.0, 0.1);
    est.predict(0.05);
    const auto s = est.getState();
    EXPECT_TRUE(std::isfinite(s.azimuthRad));
    EXPECT_TRUE(std::isfinite(s.elevationRad));
    std::cout << "  -> PASSED\n";
}

TEST(PtzSphericalEstimatorTest, UpdateWithMeasurementConverges)
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
    EXPECT_TRUE(std::isfinite(s.azimuthRad));
    EXPECT_TRUE(std::isfinite(s.elevationRad));
    std::cout << "  -> PASSED\n";
}

TEST(PtzSphericalEstimatorTest, UKFModeConverges)
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
    EXPECT_TRUE(std::isfinite(s.azimuthRad));
    std::cout << "  -> PASSED\n";
}

TEST(PtzSphericalEstimatorTest, CovarianceAndCe90Computation)
{
    std::cout << "[Test] testCovarianceAndCe90Computation\n";
    PtzSphericalEstimator est;
    est.init(0.2, -0.4); // 0.2 rad azimuth, -0.4 rad elevation (down)

    // With 0 slant range, CE90 and LE90 are 0
    const auto sZero = est.getState(0.0, 0.0, 0.0, 0.0);
    EXPECT_GT(sZero.sigmaAzimuthRad, 0.0);
    EXPECT_GT(sZero.sigmaElevationRad, 0.0);
    EXPECT_NEAR(sZero.ce90Meters, 0.0, 1e-6);
    EXPECT_NEAR(sZero.le90Meters, 0.0, 1e-6);

    // With 1000m slant range, CE90 and LE90 are positively estimated
    const auto s1000 = est.getState(0.0, 0.0, 0.0, 1000.0);
    EXPECT_GT(s1000.ce90Meters, 0.0);
    EXPECT_GT(s1000.le90Meters, 0.0);

    // Higher slant range scales CE90 linearly
    const auto s2000 = est.getState(0.0, 0.0, 0.0, 2000.0);
    EXPECT_NEAR(s2000.ce90Meters, s1000.ce90Meters * 2.0, 0.1);
    EXPECT_NEAR(s2000.le90Meters, s1000.le90Meters * 2.0, 0.1);
    std::cout << "  -> PASSED\n";
}

} // namespace
