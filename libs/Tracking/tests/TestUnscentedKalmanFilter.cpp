/// @file TestUnscentedKalmanFilter.cpp
/// @brief Unit tests for UnscentedKalmanFilter: initialization, sigma-point predict/update,
///        scaling parameters, covariance reduction, Mahalanobis distance, reset, and convergence.

#include "MatrixMath.h"
#include "UnscentedKalmanFilter.h"

#include <cmath>
#include <gtest/gtest.h>
#include <iostream>

using namespace Tracking;
using namespace Math;

namespace {

constexpr double TOL = 1e-6;

bool near(double a, double b, double tol = TOL)
{
    return std::abs(a - b) <= tol;
}

/// @brief Identity measurement: h(x) = [x0, x1]^T
Vector<2> identityH(const Vector<6>& x)
{
    return { x[0], x[1] };
}

// ---------------------------------------------------------------------------

TEST(UnscentedKalmanFilterTest, NotInitializedByDefault)
{
    std::cout << "[Test] testNotInitializedByDefault\n";
    UnscentedKalmanFilter ukf;
    EXPECT_TRUE(!ukf.isInitialized());
    std::cout << "  -> PASSED\n";
}

TEST(UnscentedKalmanFilterTest, InitMarksInitialized)
{
    std::cout << "[Test] testInitMarksInitialized\n";
    UnscentedKalmanFilter ukf;
    Vector<6> x0 {};
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 1.0;
    }
    ukf.init(x0, P0);
    EXPECT_TRUE(ukf.isInitialized());

    const auto& s = ukf.getState();
    for (std::size_t i = 0U; i < 6U; ++i) {
        EXPECT_TRUE(near(s[i], 0.0));
    }
    std::cout << "  -> PASSED\n";
}

TEST(UnscentedKalmanFilterTest, PredictAdvancesPosition)
{
    std::cout << "[Test] testPredictAdvancesPosition\n";
    UnscentedKalmanFilter ukf;
    Vector<6> x0 {};
    x0[0] = 1.0; // position x
    x0[2] = 0.5; // velocity x
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 0.1;
    }
    ukf.init(x0, P0);
    ukf.setProcessNoise(1e-4, 1e-3, 1e-2);

    ukf.predict(0.1);

    const auto& s = ukf.getState();
    // Position advances by vx*dt = 0.05, so x > 1.0
    EXPECT_TRUE(s[0] > 1.0);
    std::cout << "  -> PASSED\n";
}

TEST(UnscentedKalmanFilterTest, UpdateReducesCovariance)
{
    std::cout << "[Test] testUpdateReducesCovariance\n";
    UnscentedKalmanFilter ukf;
    Vector<6> x0 {};
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 1.0;
    }
    ukf.init(x0, P0);

    double traceBefore = 0.0;
    for (std::size_t i = 0U; i < 6U; ++i) {
        traceBefore += ukf.getCovariance()(i, i);
    }

    Vector<2> z { 0.0, 0.0 };
    Matrix<2, 2> R {};
    R(0, 0) = 0.01;
    R(1, 1) = 0.01;
    ukf.update(z, identityH, R);

    double traceAfter = 0.0;
    for (std::size_t i = 0U; i < 6U; ++i) {
        traceAfter += ukf.getCovariance()(i, i);
    }

    EXPECT_TRUE(traceAfter < traceBefore);
    std::cout << "  -> PASSED\n";
}

TEST(UnscentedKalmanFilterTest, InnovationAfterUpdate)
{
    std::cout << "[Test] testInnovationAfterUpdate\n";
    UnscentedKalmanFilter ukf;
    Vector<6> x0 {};
    x0[0] = 2.0;
    x0[1] = 3.0;
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 0.5;
    }
    ukf.init(x0, P0);

    Vector<2> z { 2.5, 3.5 };
    Matrix<2, 2> R {};
    R(0, 0) = 0.1;
    R(1, 1) = 0.1;
    ukf.update(z, identityH, R);

    const auto& innov = ukf.getInnovation();
    EXPECT_TRUE(near(innov[0], 0.5, 0.05));
    EXPECT_TRUE(near(innov[1], 0.5, 0.05));
    std::cout << "  -> PASSED\n";
}

TEST(UnscentedKalmanFilterTest, MahalanobisDistanceNonNegative)
{
    std::cout << "[Test] testMahalanobisDistanceNonNegative\n";
    UnscentedKalmanFilter ukf;
    Vector<6> x0 {};
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 1.0;
    }
    ukf.init(x0, P0);

    Vector<2> z { 1.0, 1.0 };
    Matrix<2, 2> R {};
    R(0, 0) = 0.1;
    R(1, 1) = 0.1;
    ukf.update(z, identityH, R);

    EXPECT_TRUE(ukf.getMahalanobisDistance() >= 0.0);
    std::cout << "  -> PASSED\n";
}

TEST(UnscentedKalmanFilterTest, SetScalingParameters)
{
    std::cout << "[Test] testSetScalingParameters\n";
    UnscentedKalmanFilter ukf;
    Vector<6> x0 {};
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 0.5;
    }
    ukf.init(x0, P0);

    ukf.setScalingParameters(1e-3, 2.0, 0.0);
    ukf.predict(0.01);
    const auto& s = ukf.getState();
    for (std::size_t i = 0U; i < 6U; ++i) {
        EXPECT_TRUE(std::isfinite(s[i]));
    }
    std::cout << "  -> PASSED\n";
}

TEST(UnscentedKalmanFilterTest, ResetClearsState)
{
    std::cout << "[Test] testResetClearsState\n";
    UnscentedKalmanFilter ukf;
    Vector<6> x0 {};
    x0[0] = 5.0;
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 1.0;
    }
    ukf.init(x0, P0);
    EXPECT_TRUE(ukf.isInitialized());

    ukf.reset();
    EXPECT_TRUE(!ukf.isInitialized());
    std::cout << "  -> PASSED\n";
}

TEST(UnscentedKalmanFilterTest, PredictUpdateCycleConverges)
{
    std::cout << "[Test] testPredictUpdateCycleConverges\n";
    UnscentedKalmanFilter ukf;
    Vector<6> x0 {};
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 1.0;
    }
    ukf.init(x0, P0);
    ukf.setProcessNoise(1e-4, 1e-3, 1e-2);

    Matrix<2, 2> R {};
    R(0, 0) = 0.01;
    R(1, 1) = 0.01;

    for (int k = 0; k < 50; ++k) {
        ukf.predict(0.05);
        Vector<2> z { 1.0, 2.0 };
        ukf.update(z, identityH, R);
    }

    const auto& s = ukf.getState();
    EXPECT_TRUE(near(s[0], 1.0, 0.1));
    EXPECT_TRUE(near(s[1], 2.0, 0.1));
    std::cout << "  State after 50 cycles: (" << s[0] << ", " << s[1] << ")\n";
    std::cout << "  -> PASSED\n";
}

} // namespace
