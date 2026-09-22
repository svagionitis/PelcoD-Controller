/// @file UnscentedKalmanFilter.cpp
/// @brief Implementation of 6-state Unscented Kalman Filter.

#include "UnscentedKalmanFilter.h"

#include <algorithm>
#include <cmath>

namespace Tracking {

UnscentedKalmanFilter::UnscentedKalmanFilter()
{
    calculateWeights();
    reset();
}

void UnscentedKalmanFilter::reset() noexcept
{
    m_initialized = false;
    m_x = Math::Vector<6>();
    m_P = Math::Matrix<6, 6>::identity();
    m_y = Math::Vector<2>();
    m_S = Math::Matrix<2, 2>::identity();
    m_mahalanobisSq = 0.0;
}

bool UnscentedKalmanFilter::isInitialized() const noexcept
{
    return m_initialized;
}

void UnscentedKalmanFilter::setScalingParameters(double alpha, double beta, double kappa) noexcept
{
    m_alpha = std::clamp(alpha, 1e-4, 1.0);
    m_beta = std::max(0.0, beta);
    m_kappa = std::max(0.0, kappa);
    calculateWeights();
}

void UnscentedKalmanFilter::calculateWeights() noexcept
{
    constexpr double N = 6.0;
    m_lambda = m_alpha * m_alpha * (N + m_kappa) - N;
    const double denom = N + m_lambda;

    m_wm[0] = m_lambda / denom;
    m_wc[0] = m_wm[0] + (1.0 - (m_alpha * m_alpha) + m_beta);

    const double commonWeight = 0.5 / denom;
    for (std::size_t i = 1; i < SigmaCount; ++i) {
        m_wm[i] = commonWeight;
        m_wc[i] = commonWeight;
    }
}

void UnscentedKalmanFilter::init(const Math::Vector<6>& initState, const Math::Matrix<6, 6>& initCov)
{
    m_x = initState;
    m_P = initCov;
    m_initialized = true;
    m_mahalanobisSq = 0.0;
}

void UnscentedKalmanFilter::setProcessNoise(double qPos, double qVel, double qAcc) noexcept
{
    m_qPos = std::max(1e-9, qPos);
    m_qVel = std::max(1e-9, qVel);
    m_qAcc = std::max(1e-9, qAcc);
}

void UnscentedKalmanFilter::predict(double dt)
{
    if (!m_initialized) {
        return;
    }

    const double d = std::max(1e-4, dt);
    const double d2 = 0.5 * d * d;

    // Linear state kinematics F
    Math::Matrix<6, 6> F = Math::Matrix<6, 6>::identity();
    F(0, 2) = d;
    F(0, 4) = d2;
    F(1, 3) = d;
    F(1, 5) = d2;
    F(2, 4) = d;
    F(3, 5) = d;

    // Extrapolate state: x_pred = F * x
    m_x = F * m_x;

    // Process noise covariance Q
    Math::Matrix<6, 6> Q;
    Q(0, 0) = m_qPos * d;
    Q(1, 1) = m_qPos * d;
    Q(2, 2) = m_qVel * d;
    Q(3, 3) = m_qVel * d;
    Q(4, 4) = m_qAcc * d;
    Q(5, 5) = m_qAcc * d;

    // Covariance extrapolation
    m_P = F * m_P * F.transpose() + Q;
}

void UnscentedKalmanFilter::update(const Math::Vector<2>& z, const MeasurementFn& h,
    const Math::Matrix<2, 2>& R)
{
    if (!m_initialized) {
        return;
    }

    constexpr std::size_t N = 6;
    const double c = std::sqrt(std::max(1e-9, static_cast<double>(N) + m_lambda));

    // Matrix square root via Cholesky decomposition of P: P = L * L^T
    const Math::Matrix<6, 6> L = m_P.cholesky();

    // Generate 13 Sigma Points: chi_0, chi_1..6, chi_7..12
    std::array<Math::Vector<6>, SigmaCount> chi;
    chi[0] = m_x;

    for (std::size_t i = 0; i < N; ++i) {
        Math::Vector<6> col;
        for (std::size_t r = 0; r < N; ++r) {
            col[r] = L(r, i) * c;
        }
        chi[1 + i] = m_x + col;
        chi[1 + N + i] = m_x - col;
    }

    // Propagate each sigma point through exact non-linear measurement function h(chi_i)
    std::array<Math::Vector<2>, SigmaCount> gamma;
    Math::Vector<2> z_pred;

    for (std::size_t i = 0; i < SigmaCount; ++i) {
        gamma[i] = h(chi[i]);
        z_pred[0] += m_wm[i] * gamma[i][0];
        z_pred[1] += m_wm[i] * gamma[i][1];
    }

    // Measurement residual innovation
    m_y = z - z_pred;

    // Measurement covariance P_zz = R + sum(wc_i * (gamma_i - z_pred) * (gamma_i - z_pred)^T)
    Math::Matrix<2, 2> P_zz = R;
    // Cross-covariance P_xz = sum(wc_i * (chi_i - x) * (gamma_i - z_pred)^T)
    Math::Matrix<6, 2> P_xz;

    for (std::size_t i = 0; i < SigmaCount; ++i) {
        const Math::Vector<2> diffZ = gamma[i] - z_pred;
        const Math::Vector<6> diffX = chi[i] - m_x;

        P_zz = P_zz + (diffZ.outer(diffZ) * m_wc[i]);
        P_xz = P_xz + (diffX.outer(diffZ) * m_wc[i]);
    }

    m_S = P_zz;
    const Math::Matrix<2, 2> S_inv = m_S.inverse();

    // Squared Mahalanobis distance
    m_mahalanobisSq = m_y[0] * (S_inv(0, 0) * m_y[0] + S_inv(0, 1) * m_y[1])
        + m_y[1] * (S_inv(1, 0) * m_y[0] + S_inv(1, 1) * m_y[1]);

    // Kalman gain: K = P_xz * P_zz^-1
    const Math::Matrix<6, 2> K = P_xz * S_inv;

    // State update
    m_x = m_x + (K * m_y);

    // Covariance update: P = P - K * P_zz * K^T
    m_P = m_P - (K * P_zz * K.transpose());
}

const Math::Vector<6>& UnscentedKalmanFilter::getState() const noexcept
{
    return m_x;
}

const Math::Matrix<6, 6>& UnscentedKalmanFilter::getCovariance() const noexcept
{
    return m_P;
}

const Math::Vector<2>& UnscentedKalmanFilter::getInnovation() const noexcept
{
    return m_y;
}

const Math::Matrix<2, 2>& UnscentedKalmanFilter::getInnovationCovariance() const noexcept
{
    return m_S;
}

double UnscentedKalmanFilter::getMahalanobisDistance() const noexcept
{
    return (m_mahalanobisSq > 0.0) ? std::sqrt(m_mahalanobisSq) : 0.0;
}

} // namespace Tracking
