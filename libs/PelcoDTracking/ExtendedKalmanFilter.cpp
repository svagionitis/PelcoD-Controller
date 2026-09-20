/// @file ExtendedKalmanFilter.cpp
/// @brief Implementation of 6-state Extended Kalman Filter.

#include "ExtendedKalmanFilter.h"

#include <algorithm>

namespace PelcoD {

ExtendedKalmanFilter::ExtendedKalmanFilter()
{
    reset();
}

void ExtendedKalmanFilter::reset() noexcept
{
    m_initialized = false;
    m_x = Math::Vector<6>();
    m_P = Math::Matrix<6, 6>::identity();
    m_Q = Math::Matrix<6, 6>::identity();
    m_y = Math::Vector<2>();
    m_S = Math::Matrix<2, 2>::identity();
    m_mahalanobisSq = 0.0;
}

bool ExtendedKalmanFilter::isInitialized() const noexcept
{
    return m_initialized;
}

void ExtendedKalmanFilter::init(const Math::Vector<6>& initState, const Math::Matrix<6, 6>& initCov)
{
    m_x = initState;
    m_P = initCov;
    m_initialized = true;
    m_mahalanobisSq = 0.0;
}

void ExtendedKalmanFilter::setProcessNoise(double qPos, double qVel, double qAcc) noexcept
{
    m_qPos = std::max(1e-9, qPos);
    m_qVel = std::max(1e-9, qVel);
    m_qAcc = std::max(1e-9, qAcc);
}

void ExtendedKalmanFilter::predict(double dt)
{
    if (!m_initialized) {
        return;
    }

    const double d = std::max(1e-4, dt);
    const double d2 = 0.5 * d * d;

    // Transition matrix F (Constant acceleration kinematics)
    Math::Matrix<6, 6> F = Math::Matrix<6, 6>::identity();
    F(0, 2) = d;
    F(0, 4) = d2;
    F(1, 3) = d;
    F(1, 5) = d2;
    F(2, 4) = d;
    F(3, 5) = d;

    // State extrapolation: x_pred = F * x
    m_x = F * m_x;

    // Process noise covariance Q
    Math::Matrix<6, 6> Q;
    Q(0, 0) = m_qPos * d;
    Q(1, 1) = m_qPos * d;
    Q(2, 2) = m_qVel * d;
    Q(3, 3) = m_qVel * d;
    Q(4, 4) = m_qAcc * d;
    Q(5, 5) = m_qAcc * d;

    // Covariance extrapolation: P_pred = F * P * F^T + Q
    m_P = F * m_P * F.transpose() + Q;
}

void ExtendedKalmanFilter::update(const Math::Vector<2>& z, const MeasurementFn& h,
    const JacobianFn& H_fn, const Math::Matrix<2, 2>& R)
{
    if (!m_initialized) {
        return;
    }

    // Predicted measurement: z_pred = h(x)
    const Math::Vector<2> z_pred = h(m_x);

    // Measurement residual innovation: y = z - z_pred
    m_y = z - z_pred;

    // Measurement Jacobian: H (2x6)
    const Math::Matrix<2, 6> H = H_fn(m_x);

    // Innovation covariance: S = H * P * H^T + R (2x2)
    m_S = H * m_P * H.transpose() + R;

    // Invert S (2x2 matrix)
    const Math::Matrix<2, 2> S_inv = m_S.inverse();

    // Compute squared Mahalanobis distance: y^T * S^-1 * y
    m_mahalanobisSq = m_y[0] * (S_inv(0, 0) * m_y[0] + S_inv(0, 1) * m_y[1])
        + m_y[1] * (S_inv(1, 0) * m_y[0] + S_inv(1, 1) * m_y[1]);

    // Near-optimal Kalman gain: K = P * H^T * S^-1 (6x2)
    const Math::Matrix<6, 2> K = m_P * H.transpose() * S_inv;

    // State update: x = x + K * y
    m_x = m_x + (K * m_y);

    // Joseph form stabilized covariance update: P = (I - K*H) * P * (I - K*H)^T + K * R * K^T
    const Math::Matrix<6, 6> I = Math::Matrix<6, 6>::identity();
    const Math::Matrix<6, 6> I_KH = I - (K * H);
    m_P = I_KH * m_P * I_KH.transpose() + K * R * K.transpose();
}

const Math::Vector<6>& ExtendedKalmanFilter::getState() const noexcept
{
    return m_x;
}

const Math::Matrix<6, 6>& ExtendedKalmanFilter::getCovariance() const noexcept
{
    return m_P;
}

const Math::Vector<2>& ExtendedKalmanFilter::getInnovation() const noexcept
{
    return m_y;
}

const Math::Matrix<2, 2>& ExtendedKalmanFilter::getInnovationCovariance() const noexcept
{
    return m_S;
}

double ExtendedKalmanFilter::getMahalanobisDistance() const noexcept
{
    return (m_mahalanobisSq > 0.0) ? std::sqrt(m_mahalanobisSq) : 0.0;
}

} // namespace PelcoD
