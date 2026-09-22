/// @file TestExtendedKalmanFilter.cpp
/// @brief Unit tests for ExtendedKalmanFilter: initialization, predict/update cycle,
///        state getters, innovation, Mahalanobis distance, and reset.

#include "ExtendedKalmanFilter.h"
#include "MatrixMath.h"

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

/// @brief Simple linear measurement function: h(x) = [x0, x1]^T
Vector<2> linearH(const Vector<6>& x)
{
    return { x[0], x[1] };
}

/// @brief Jacobian of the linear measurement function: H = [[1,0,0,0,0,0],[0,1,0,0,0,0]]
Matrix<2, 6> linearJacobian(const Vector<6>&)
{
    Matrix<2, 6> H {};
    H(0, 0) = 1.0;
    H(1, 1) = 1.0;
    return H;
}

// ---------------------------------------------------------------------------

TEST(ExtendedKalmanFilterTest, NotInitializedByDefault)
{
    std::cout << "[Test] testNotInitializedByDefault\n";
    ExtendedKalmanFilter ekf;
    EXPECT_TRUE(!ekf.isInitialized());
    std::cout << "  -> PASSED\n";
}

TEST(ExtendedKalmanFilterTest, InitMarksInitialized)
{
    std::cout << "[Test] testInitMarksInitialized\n";
    ExtendedKalmanFilter ekf;
    Vector<6> x0 {};
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 1.0;
    }
    ekf.init(x0, P0);
    EXPECT_TRUE(ekf.isInitialized());
    const auto& state = ekf.getState();
    for (int i = 0; i < 6; ++i) {
        EXPECT_TRUE(near(state[static_cast<std::size_t>(i)], 0.0));
    }
    std::cout << "  -> PASSED\n";
}

TEST(ExtendedKalmanFilterTest, PredictAdvancesState)
{
    std::cout << "[Test] testPredictAdvancesState\n";
    ExtendedKalmanFilter ekf;
    Vector<6> x0 {};
    x0[0] = 1.0; // position x
    x0[2] = 0.5; // velocity x
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 0.1;
    }

    ekf.init(x0, P0);
    ekf.setProcessNoise(1e-4, 1e-3, 1e-2);

    const double dt = 0.1;
    ekf.predict(dt);

    const auto& s = ekf.getState();
    // Position should advance: x0 + vx*dt
    EXPECT_TRUE(s[0] > 1.0);
    std::cout << "  -> PASSED\n";
}

TEST(ExtendedKalmanFilterTest, UpdateReducesCovariance)
{
    std::cout << "[Test] testUpdateReducesCovariance\n";
    ExtendedKalmanFilter ekf;
    Vector<6> x0 {};
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 1.0;
    }
    ekf.init(x0, P0);

    double traceBefore = 0.0;
    for (std::size_t i = 0U; i < 6U; ++i) {
        traceBefore += ekf.getCovariance()(i, i);
    }

    Vector<2> z { 0.0, 0.0 };
    Matrix<2, 2> R {};
    R(0, 0) = 0.01;
    R(1, 1) = 0.01;

    ekf.update(z, linearH, linearJacobian, R);

    double traceAfter = 0.0;
    for (std::size_t i = 0U; i < 6U; ++i) {
        traceAfter += ekf.getCovariance()(i, i);
    }

    EXPECT_TRUE(traceAfter < traceBefore);
    std::cout << "  -> PASSED\n";
}

TEST(ExtendedKalmanFilterTest, GetInnovationAfterUpdate)
{
    std::cout << "[Test] testGetInnovationAfterUpdate\n";
    ExtendedKalmanFilter ekf;
    Vector<6> x0 {};
    x0[0] = 2.0;
    x0[1] = 3.0;
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 0.5;
    }
    ekf.init(x0, P0);

    Vector<2> z { 2.5, 3.5 };
    Matrix<2, 2> R {};
    R(0, 0) = 0.1;
    R(1, 1) = 0.1;

    ekf.update(z, linearH, linearJacobian, R);

    const auto& innov = ekf.getInnovation();
    // Innovation = z - h(x_prior) = [0.5, 0.5]
    EXPECT_TRUE(near(innov[0], 0.5, 0.05));
    EXPECT_TRUE(near(innov[1], 0.5, 0.05));
    std::cout << "  -> PASSED\n";
}

TEST(ExtendedKalmanFilterTest, MahalanobisDistanceIsPositive)
{
    std::cout << "[Test] testMahalanobisDistanceIsPositive\n";
    ExtendedKalmanFilter ekf;
    Vector<6> x0 {};
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 1.0;
    }
    ekf.init(x0, P0);

    Vector<2> z { 1.0, 2.0 };
    Matrix<2, 2> R {};
    R(0, 0) = 0.1;
    R(1, 1) = 0.1;
    ekf.update(z, linearH, linearJacobian, R);

    EXPECT_TRUE(ekf.getMahalanobisDistance() >= 0.0);
    std::cout << "  -> PASSED\n";
}

TEST(ExtendedKalmanFilterTest, ResetClearsState)
{
    std::cout << "[Test] testResetClearsState\n";
    ExtendedKalmanFilter ekf;
    Vector<6> x0 {};
    x0[0] = 5.0;
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 1.0;
    }
    ekf.init(x0, P0);
    EXPECT_TRUE(ekf.isInitialized());

    ekf.reset();
    EXPECT_TRUE(!ekf.isInitialized());
    std::cout << "  -> PASSED\n";
}

TEST(ExtendedKalmanFilterTest, SetProcessNoise)
{
    std::cout << "[Test] testSetProcessNoise\n";
    ExtendedKalmanFilter ekf;
    Vector<6> x0 {};
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 1.0;
    }
    ekf.init(x0, P0);

    ekf.setProcessNoise(1.0, 2.0, 3.0);
    ekf.predict(0.1);
    const auto& s = ekf.getState();
    for (int i = 0; i < 6; ++i) {
        EXPECT_TRUE(std::isfinite(s[static_cast<std::size_t>(i)]));
    }
    std::cout << "  -> PASSED\n";
}

TEST(ExtendedKalmanFilterTest, PredictUpdateCycleConverges)
{
    std::cout << "[Test] testPredictUpdateCycleConverges\n";
    ExtendedKalmanFilter ekf;
    Vector<6> x0 {};
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 1.0;
    }
    ekf.init(x0, P0);
    ekf.setProcessNoise(1e-4, 1e-3, 1e-2);

    Matrix<2, 2> R {};
    R(0, 0) = 0.01;
    R(1, 1) = 0.01;

    for (int k = 0; k < 50; ++k) {
        ekf.predict(0.05);
        Vector<2> z { 1.0, 2.0 };
        ekf.update(z, linearH, linearJacobian, R);
    }

    const auto& s = ekf.getState();
    EXPECT_TRUE(near(s[0], 1.0, 0.05));
    EXPECT_TRUE(near(s[1], 2.0, 0.05));
    std::cout << "  State after 50 cycles: (" << s[0] << ", " << s[1] << ")\n";
    std::cout << "  -> PASSED\n";
}

} // namespace
