#pragma once

/// @file UnscentedKalmanFilter.h
/// @brief 6-State Unscented Kalman Filter (UKF) with scaled unscented transform.

#include "MatrixMath.h"

#include <functional>

namespace Tracking {

/// @class UnscentedKalmanFilter
/// @brief 6-State, 2-measurement Unscented Kalman Filter propagating 13 sigma points through exact non-linear functions.
class UnscentedKalmanFilter {
public:
    using MeasurementFn = std::function<Math::Vector<2>(const Math::Vector<6>& state)>;

    UnscentedKalmanFilter();

    /// @brief Initializes state vector and error covariance matrix.
    /// @param[in] initState Initial 6-state vector [x, y, vx, vy, ax, ay]^T.
    /// @param[in] initCov Initial 6x6 error covariance matrix P.
    void init(const Math::Vector<6>& initState, const Math::Matrix<6, 6>& initCov);

    /// @brief Configures process noise covariances.
    void setProcessNoise(double qPos, double qVel, double qAcc) noexcept;

    /// @brief Configures unscented transform scaling parameters.
    /// @param[in] alpha Primary spread factor (typically 1e-3 to 1.0).
    /// @param[in] beta Prior distribution tuning parameter (2.0 optimal for Gaussian).
    /// @param[in] kappa Secondary scaling parameter (typically 0.0).
    void setScalingParameters(double alpha, double beta = 2.0, double kappa = 0.0) noexcept;

    /// @brief Executes state and covariance prediction step over elapsed time dt.
    /// @param[in] dt Elapsed time in seconds.
    void predict(double dt);

    /// @brief Executes non-linear measurement update step via sigma point propagation.
    /// @param[in] z Actual 2D sensor measurement [u, v]^T.
    /// @param[in] h Exact non-linear measurement function mapping state to 2D measurement.
    /// @param[in] R Measurement noise covariance matrix (2x2).
    void update(const Math::Vector<2>& z, const MeasurementFn& h, const Math::Matrix<2, 2>& R);

    /// @brief Retrieves the estimated 6-state vector.
    [[nodiscard]] const Math::Vector<6>& getState() const noexcept;

    /// @brief Retrieves the 6x6 error covariance matrix.
    [[nodiscard]] const Math::Matrix<6, 6>& getCovariance() const noexcept;

    /// @brief Retrieves the latest 2D measurement innovation residual (z - z_pred).
    [[nodiscard]] const Math::Vector<2>& getInnovation() const noexcept;

    /// @brief Retrieves the 2x2 innovation covariance matrix S.
    [[nodiscard]] const Math::Matrix<2, 2>& getInnovationCovariance() const noexcept;

    /// @brief Computes squared Mahalanobis distance y^T * S^-1 * y of the latest innovation.
    [[nodiscard]] double getMahalanobisDistance() const noexcept;

    /// @brief Query if filter has been initialized.
    [[nodiscard]] bool isInitialized() const noexcept;

    /// @brief Resets filter state.
    void reset() noexcept;

private:
    void calculateWeights() noexcept;

    bool m_initialized { false };
    Math::Vector<6> m_x {};
    Math::Matrix<6, 6> m_P {};

    Math::Vector<2> m_y {};
    Math::Matrix<2, 2> m_S {};
    double m_mahalanobisSq { 0.0 };

    double m_qPos { 1e-4 };
    double m_qVel { 1e-3 };
    double m_qAcc { 1e-2 };

    // UKF scaling parameters
    double m_alpha { 0.5 };
    double m_beta { 2.0 };
    double m_kappa { 0.0 };
    double m_lambda { 0.0 };

    // Sigma weights (2*N + 1 = 13 weights)
    static constexpr std::size_t SigmaCount = 13U;
    std::array<double, SigmaCount> m_wm {};
    std::array<double, SigmaCount> m_wc {};
};

} // namespace Tracking

namespace PelcoD {
namespace Tracking = ::Tracking;
using UnscentedKalmanFilter = ::Tracking::UnscentedKalmanFilter;
} // namespace PelcoD
