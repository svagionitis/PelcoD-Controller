/// @file TestNonlinearKalman.cpp
/// @brief Comprehensive verification suite for MatrixMath, PtzCameraModel, EKF, UKF, and PtzSphericalEstimator.

#include "ExtendedKalmanFilter.h"
#include "MatrixMath.h"
#include "PtzCameraModel.h"
#include "PtzSphericalEstimator.h"
#include "UnscentedKalmanFilter.h"

#include <cmath>
#include <gtest/gtest.h>
#include <iostream>
#include <vector>

namespace {

using namespace Tracking;
using namespace Math;

constexpr double TEST_EPSILON = 1e-4;
constexpr double PI = 3.14159265358979323846;

bool approxEqual(double a, double b, double eps = TEST_EPSILON)
{
    return std::abs(a - b) <= eps;
}

double degToRad(double deg)
{
    return deg * (PI / 180.0);
}

double radToDeg(double rad)
{
    return rad * (180.0 / PI);
}

// ============================================================================
// 1. Matrix Math Tests
// ============================================================================
TEST(NonlinearKalmanTest, MatrixBasicOperations)
{
    std::cout << "[Test] Matrix basic operations (addition, multiplication, transpose)..." << std::endl;

    Matrix<2, 2> a {};
    a(0, 0) = 1.0;
    a(0, 1) = 2.0;
    a(1, 0) = 3.0;
    a(1, 1) = 4.0;

    Matrix<2, 2> b {};
    b(0, 0) = 5.0;
    b(0, 1) = 6.0;
    b(1, 0) = 7.0;
    b(1, 1) = 8.0;

    auto c = a + b;
    EXPECT_TRUE(approxEqual(c(0, 0), 6.0));
    EXPECT_TRUE(approxEqual(c(0, 1), 8.0));
    EXPECT_TRUE(approxEqual(c(1, 0), 10.0));
    EXPECT_TRUE(approxEqual(c(1, 1), 12.0));

    auto prod = a * b;
    // [1*5 + 2*7, 1*6 + 2*8] = [19, 22]
    // [3*5 + 4*7, 3*6 + 4*8] = [43, 50]
    EXPECT_TRUE(approxEqual(prod(0, 0), 19.0));
    EXPECT_TRUE(approxEqual(prod(0, 1), 22.0));
    EXPECT_TRUE(approxEqual(prod(1, 0), 43.0));
    EXPECT_TRUE(approxEqual(prod(1, 1), 50.0));

    auto at = a.transpose();
    EXPECT_TRUE(approxEqual(at(0, 0), 1.0));
    EXPECT_TRUE(approxEqual(at(0, 1), 3.0));
    EXPECT_TRUE(approxEqual(at(1, 0), 2.0));
    EXPECT_TRUE(approxEqual(at(1, 1), 4.0));

    // Vector operations
    Vector<3> v1 { 1.0, 2.0, 3.0 };
    Vector<2> v2 { 4.0, 5.0 };
    auto outerMat = v1.outer(v2); // 3x2 matrix
    EXPECT_TRUE(approxEqual(outerMat(0, 0), 4.0));
    EXPECT_TRUE(approxEqual(outerMat(0, 1), 5.0));
    EXPECT_TRUE(approxEqual(outerMat(2, 0), 12.0));
    EXPECT_TRUE(approxEqual(outerMat(2, 1), 15.0));

    std::cout << "  -> Passed!" << std::endl;
}

TEST(NonlinearKalmanTest, MatrixInversion)
{
    std::cout << "[Test] Matrix inversion with partial pivoting..." << std::endl;

    Matrix<3, 3> m {};
    m(0, 0) = 2.0;
    m(0, 1) = 1.0;
    m(0, 2) = 1.0;
    m(1, 0) = 1.0;
    m(1, 1) = 3.0;
    m(1, 2) = 2.0;
    m(2, 0) = 1.0;
    m(2, 1) = 0.0;
    m(2, 2) = 0.0;

    auto inv = m.inverse();
    auto identity = m * inv;

    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 3; ++c) {
            double expected = (r == c) ? 1.0 : 0.0;
            EXPECT_TRUE(approxEqual(identity(r, c), expected, 1e-5));
        }
    }

    std::cout << "  -> Passed!" << std::endl;
}

TEST(NonlinearKalmanTest, CholeskyDecomposition)
{
    std::cout << "[Test] Lower-triangular Cholesky decomposition..." << std::endl;

    // Symmetric positive-definite matrix
    Matrix<3, 3> a {};
    a(0, 0) = 4.0;
    a(0, 1) = 12.0;
    a(0, 2) = -16.0;
    a(1, 0) = 12.0;
    a(1, 1) = 37.0;
    a(1, 2) = -43.0;
    a(2, 0) = -16.0;
    a(2, 1) = -43.0;
    a(2, 2) = 98.0;

    auto l = a.cholesky();
    // Upper triangle must be 0
    EXPECT_TRUE(approxEqual(l(0, 1), 0.0));
    EXPECT_TRUE(approxEqual(l(0, 2), 0.0));
    EXPECT_TRUE(approxEqual(l(1, 2), 0.0));

    // Reconstruct A = L * L^T
    auto recon = l * l.transpose();
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 3; ++c) {
            EXPECT_TRUE(approxEqual(recon(r, c), a(r, c), 1e-5));
        }
    }

    std::cout << "  -> Passed!" << std::endl;
}

// ============================================================================
// 2. PTZ Camera Model Tests
// ============================================================================
TEST(NonlinearKalmanTest, CameraModelProjectionAndUnprojection)
{
    std::cout << "[Test] Camera model projection and unprojection round-trip..." << std::endl;

    CameraIntrinsics intr {};
    intr.imageWidth = 1920;
    intr.imageHeight = 1080;
    intr.cx = 960.0;
    intr.cy = 540.0;
    intr.fx0 = 1200.0;
    intr.fy0 = 1200.0;
    intr.k1 = -0.10; // Mild barrel distortion
    intr.k2 = 0.02;

    PtzCameraModel model(intr);

    // 1. Center boresight should project exactly to principal point (960, 540)
    auto centerProj = model.project(0.0, 0.0, 0.0, 0.0, 1.0);
    EXPECT_TRUE(approxEqual(centerProj[0], 960.0));
    EXPECT_TRUE(approxEqual(centerProj[1], 540.0));

    // 2. Off-axis angle round-trip
    const std::vector<std::pair<double, double>> testAnglesDeg
        = { { 5.0, 3.0 }, { -8.0, 6.0 }, { 12.0, -9.0 }, { -15.0, -10.0 } };

    for (const auto& [azDeg, elDeg] : testAnglesDeg) {
        const double azRad = degToRad(azDeg);
        const double elRad = degToRad(elDeg);

        auto proj = model.project(azRad, elRad, 0.0, 0.0, 1.0);
        // Ensure projection falls inside frame
        EXPECT_TRUE(proj[0] > 0.0 && proj[0] < 1920.0);
        EXPECT_TRUE(proj[1] > 0.0 && proj[1] < 1080.0);

        double recAzRad = 0.0;
        double recElRad = 0.0;
        bool ok = model.unproject(proj[0], proj[1], 0.0, 0.0, 1.0, recAzRad, recElRad);
        EXPECT_TRUE(ok);

        EXPECT_TRUE(approxEqual(radToDeg(recAzRad), azDeg, 1e-3));
        EXPECT_TRUE(approxEqual(radToDeg(recElRad), elDeg, 1e-3));
    }

    std::cout << "  -> Passed!" << std::endl;
}

TEST(NonlinearKalmanTest, CameraModelOpticalZoom)
{
    std::cout << "[Test] Camera model dynamic optical zoom scaling..." << std::endl;

    CameraIntrinsics intr {};
    intr.fx0 = 1000.0;
    intr.fy0 = 1000.0;
    intr.cx = 960.0;
    intr.cy = 540.0;

    PtzCameraModel model(intr);

    const double azRad = degToRad(2.0); // 2 degrees off boresight
    auto p1x = model.project(azRad, 0.0, 0.0, 0.0, 1.0);
    auto p5x = model.project(azRad, 0.0, 0.0, 0.0, 5.0); // 5x optical zoom

    const double offset1x = p1x[0] - 960.0;
    const double offset5x = p5x[0] - 960.0;
    EXPECT_TRUE(approxEqual(offset5x / offset1x, 5.0, 0.1));

    std::cout << "  -> Passed!" << std::endl;
}

TEST(NonlinearKalmanTest, CameraModelJacobian)
{
    std::cout << "[Test] Camera model Jacobian matrix computation..." << std::endl;

    CameraIntrinsics intr {};
    PtzCameraModel model(intr);

    auto H = model.computeJacobian(degToRad(4.0), degToRad(-3.0), 0.0, 0.0, 1.0);

    // H should be 2x2 with non-zero diagonal entries
    EXPECT_TRUE(std::abs(H(0, 0)) > 10.0); // du/dtheta
    EXPECT_TRUE(std::abs(H(1, 1)) > 10.0); // dv/dphi

    // Check determinant non-zero (full rank)
    double det = H(0, 0) * H(1, 1) - H(0, 1) * H(1, 0);
    EXPECT_TRUE(std::abs(det) > 1.0);

    std::cout << "  -> Passed!" << std::endl;
}

// ============================================================================
// 3. Extended Kalman Filter (EKF) Tests
// ============================================================================
TEST(NonlinearKalmanTest, EkfCircularTrajectoryHighElevation)
{
    std::cout << "[Test] EKF tracking high-elevation circular trajectory (70 deg elevation)..." << std::endl;

    SphericalEstimatorConfig config {};
    config.type = EstimatorType::EKF;
    config.pixelNoiseStd = 1.0;
    config.intrinsics.imageWidth = 1920;
    config.intrinsics.imageHeight = 1080;
    config.intrinsics.fx0 = 1200.0;
    config.intrinsics.fy0 = 1200.0;

    PtzSphericalEstimator estimator(config);
    PtzCameraModel model(config.intrinsics);

    // Target orbits at elevation 70 degrees with radius 2 degrees in azimuth
    const double baseElDeg = 70.0;
    const double radiusAzDeg = 2.0;
    const double dt = 0.033; // ~30 fps
    const double omega = 2.0 * PI * 0.5; // 0.5 Hz orbit

    // Camera aimed at pan 0, tilt 70 deg
    const double camPanRad = 0.0;
    const double camTiltRad = degToRad(baseElDeg);

    // Initialize state
    estimator.init(degToRad(0.0), degToRad(baseElDeg));

    double maxAzError = 0.0;
    double maxElError = 0.0;

    for (int step = 0; step < 120; ++step) {
        double t = step * dt;
        double trueAzDeg = radiusAzDeg * std::sin(omega * t);
        double trueElDeg = baseElDeg + 0.5 * std::cos(omega * t);

        auto proj = model.project(degToRad(trueAzDeg), degToRad(trueElDeg), camPanRad, camTiltRad, 1.0);

        estimator.update(proj[0], proj[1], camPanRad, camTiltRad, 1.0, dt);

        if (step > 30) { // After convergence
            auto state = estimator.getState(0.0, camPanRad, camTiltRad);
            double estAzDeg = radToDeg(state.azimuthRad);
            double estElDeg = radToDeg(state.elevationRad);

            maxAzError = std::max(maxAzError, std::abs(estAzDeg - trueAzDeg));
            maxElError = std::max(maxElError, std::abs(estElDeg - trueElDeg));
        }
    }

    EXPECT_TRUE(maxAzError < 0.20); // Within 0.20 deg tracking accuracy
    EXPECT_TRUE(maxElError < 0.20);

    std::cout << "  -> Passed! (Max error: az=" << maxAzError << " deg, el=" << maxElError << " deg)" << std::endl;
}

TEST(NonlinearKalmanTest, EkfMahalanobisOutlierGating)
{
    std::cout << "[Test] EKF Mahalanobis distance outlier rejection..." << std::endl;

    SphericalEstimatorConfig config {};
    config.type = EstimatorType::EKF;
    config.mahalanobisGateThreshold = 9.21; // 99% Chi-square threshold for 2 DOF

    PtzSphericalEstimator estimator(config);
    estimator.initFromPixel(960.0, 540.0, 0.0, 0.0, 1.0);

    // Warm-up with normal measurements
    for (int i = 0; i < 20; ++i) {
        estimator.update(960.0, 540.0, 0.0, 0.0, 1.0, 0.04);
    }

    // Inject massive outlier measurement (e.g. glitch to corner pixel 10, 10)
    estimator.update(10.0, 10.0, 0.0, 0.0, 1.0, 0.04);

    auto state = estimator.getState(0.0, 0.0, 0.0);
    // Outlier must be detected and rejected!
    EXPECT_TRUE(state.isOutlierGated);

    // Filter state should not be corrupted by glitch
    EXPECT_TRUE(std::abs(state.errorAzimuthDeg) < 0.5);
    EXPECT_TRUE(std::abs(state.errorElevationDeg) < 0.5);

    std::cout << "  -> Passed!" << std::endl;
}

// ============================================================================
// 4. Unscented Kalman Filter (UKF) Tests
// ============================================================================
TEST(NonlinearKalmanTest, UkfTrackingUnderSevereRadialDistortion)
{
    std::cout << "[Test] UKF non-linear sigma-point tracking under severe lens distortion..." << std::endl;

    SphericalEstimatorConfig config {};
    config.type = EstimatorType::UKF;
    config.intrinsics.k1 = -0.35; // Severe wide-angle barrel distortion
    config.intrinsics.k2 = 0.12;

    PtzSphericalEstimator ukf(config);
    PtzCameraModel model(config.intrinsics);

    ukf.init(degToRad(-5.0), degToRad(2.0));

    const double dt = 0.04;
    double trueAzDeg = -5.0;
    double trueElDeg = 2.0;
    double trueVelAz = 1.5; // deg/s
    double trueVelEl = -0.5; // deg/s

    for (int step = 0; step < 60; ++step) {
        trueAzDeg += trueVelAz * dt;
        trueElDeg += trueVelEl * dt;

        auto proj = model.project(degToRad(trueAzDeg), degToRad(trueElDeg), 0.0, 0.0, 1.0);

        ukf.update(proj[0], proj[1], 0.0, 0.0, 1.0, dt);
    }

    auto finalState = ukf.getState(0.0, 0.0, 0.0);
    double finalAzErr = std::abs(radToDeg(finalState.azimuthRad) - trueAzDeg);
    double finalElErr = std::abs(radToDeg(finalState.elevationRad) - trueElDeg);
    double finalVelAzErr = std::abs(finalState.omegaAzimuthDegPerSec - trueVelAz);

    EXPECT_TRUE(finalAzErr < 0.15);
    EXPECT_TRUE(finalElErr < 0.15);
    EXPECT_TRUE(finalVelAzErr < 0.35);

    std::cout << "  -> Passed! (Final UKF error: az=" << finalAzErr << " deg, el=" << finalElErr << " deg)"
              << std::endl;
}

// ============================================================================
// 5. PtzSphericalEstimator System Tests
// ============================================================================
TEST(NonlinearKalmanTest, PtzSphericalEstimatorWorkflow)
{
    std::cout << "[Test] PtzSphericalEstimator complete workflow (lock, lookahead, algo switch)..." << std::endl;

    SphericalEstimatorConfig config {};
    config.type = EstimatorType::EKF;

    PtzSphericalEstimator estimator(config);
    PtzCameraModel model(config.intrinsics);

    EXPECT_TRUE(!estimator.isLocked());

    // 1. Target acquisition
    estimator.initFromPixel(960.0, 540.0, 0.0, 0.0, 1.0);
    EXPECT_TRUE(estimator.isLocked());

    // 2. Feed moving target (pan velocity ~10 deg/s)
    const double dt = 0.04;
    double currentAzDeg = 0.0;
    for (int i = 0; i < 25; ++i) {
        currentAzDeg += 10.0 * dt;
        auto proj = model.project(degToRad(currentAzDeg), 0.0, 0.0, 0.0, 1.0);
        estimator.update(proj[0], proj[1], 0.0, 0.0, 1.0, dt);
    }

    auto state = estimator.getState(0.0, 0.0, 0.0);
    EXPECT_TRUE(approxEqual(state.errorAzimuthDeg, currentAzDeg, 0.6));
    EXPECT_TRUE(approxEqual(state.errorElevationDeg, 0.0, 0.3));
    EXPECT_TRUE(approxEqual(state.omegaAzimuthDegPerSec, 10.0, 1.8));

    // 3. Test lookahead latency prediction
    auto lookaheadState = estimator.getState(0.20, 0.0, 0.0); // 200 ms forward lookahead
    // Predicted azimuth should be ~ currentAz + 10 deg/s * 0.20s = currentAz + 2.0 deg
    EXPECT_TRUE(approxEqual(lookaheadState.predictedErrorAzimuthDeg, currentAzDeg + 2.0, 0.8));

    // 4. Switch algorithm to UKF on the fly
    estimator.setType(EstimatorType::UKF);

    for (int i = 0; i < 15; ++i) {
        currentAzDeg += 10.0 * dt;
        auto proj = model.project(degToRad(currentAzDeg), 0.0, 0.0, 0.0, 1.0);
        estimator.update(proj[0], proj[1], 0.0, 0.0, 1.0, dt);
    }

    auto ukfState = estimator.getState(0.0, 0.0, 0.0);
    EXPECT_TRUE(approxEqual(ukfState.errorAzimuthDeg, currentAzDeg, 0.6));

    // 5. Target release
    estimator.reset();
    EXPECT_TRUE(!estimator.isLocked());

    std::cout << "  -> Passed!" << std::endl;
}

} // namespace
