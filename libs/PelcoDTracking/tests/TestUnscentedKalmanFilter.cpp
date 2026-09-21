/// @file TestUnscentedKalmanFilter.cpp
/// @brief Unit tests for UnscentedKalmanFilter: initialization, sigma-point predict/update,
///        scaling parameters, covariance reduction, Mahalanobis distance, reset, and convergence.

#include "MatrixMath.h"
#include "UnscentedKalmanFilter.h"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace PelcoD;
using namespace PelcoD::Math;

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

void testNotInitializedByDefault()
{
    std::cout << "[Test] testNotInitializedByDefault\n";
    UnscentedKalmanFilter ukf;
    assert(!ukf.isInitialized());
    std::cout << "  -> PASSED\n";
}

void testInitMarksInitialized()
{
    std::cout << "[Test] testInitMarksInitialized\n";
    UnscentedKalmanFilter ukf;
    Vector<6> x0 {};
    Matrix<6, 6> P0 {};
    for (std::size_t i = 0U; i < 6U; ++i) {
        P0(i, i) = 1.0;
    }
    ukf.init(x0, P0);
    assert(ukf.isInitialized());

    const auto& s = ukf.getState();
    for (std::size_t i = 0U; i < 6U; ++i) {
        assert(near(s[i], 0.0));
    }
    std::cout << "  -> PASSED\n";
}

void testPredictAdvancesPosition()
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
    assert(s[0] > 1.0);
    std::cout << "  -> PASSED\n";
}

void testUpdateReducesCovariance()
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

    assert(traceAfter < traceBefore);
    std::cout << "  -> PASSED\n";
}

void testInnovationAfterUpdate()
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
    assert(near(innov[0], 0.5, 0.05));
    assert(near(innov[1], 0.5, 0.05));
    std::cout << "  -> PASSED\n";
}

void testMahalanobisDistanceNonNegative()
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

    assert(ukf.getMahalanobisDistance() >= 0.0);
    std::cout << "  -> PASSED\n";
}

void testSetScalingParameters()
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
        assert(std::isfinite(s[i]));
    }
    std::cout << "  -> PASSED\n";
}

void testResetClearsState()
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
    assert(ukf.isInitialized());

    ukf.reset();
    assert(!ukf.isInitialized());
    std::cout << "  -> PASSED\n";
}

void testPredictUpdateCycleConverges()
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
    assert(near(s[0], 1.0, 0.1));
    assert(near(s[1], 2.0, 0.1));
    std::cout << "  State after 50 cycles: (" << s[0] << ", " << s[1] << ")\n";
    std::cout << "  -> PASSED\n";
}

} // namespace

int main()
{
    std::cout << "Running TestUnscentedKalmanFilter Test Suite\n";
    testNotInitializedByDefault();
    testInitMarksInitialized();
    testPredictAdvancesPosition();
    testUpdateReducesCovariance();
    testInnovationAfterUpdate();
    testMahalanobisDistanceNonNegative();
    testSetScalingParameters();
    testResetClearsState();
    testPredictUpdateCycleConverges();
    std::cout << "All TestUnscentedKalmanFilter Tests Passed!\n";
    return 0;
}
