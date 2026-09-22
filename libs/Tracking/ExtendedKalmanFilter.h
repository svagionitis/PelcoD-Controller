#pragma once

/// @file ExtendedKalmanFilter.h
/// @brief Non-linear 6-state Extended Kalman Filter with Joseph stabilized covariance updates.

#include "MatrixMath.h"

#include <functional>

namespace Tracking {

/// @class ExtendedKalmanFilter
/// @brief 6-State (pos, vel, acc in 2 axes), 2-measurement Extended Kalman Filter.
class ExtendedKalmanFilter {
public:
    using MeasurementFn = std::function<Math::Vector<2>(const Math::Vector<6>& state)>;
    using JacobianFn = std::function<Math::Matrix<2, 6>(const Math::Vector<6>& state)>;

    ExtendedKalmanFilter();

    /// @brief Initializes state vector and error covariance matrix.
    /// @param[in] initState Initial 6-state vector [x, y, vx, vy, ax, ay]^T.
    /// @param[in] initCov Initial 6x6 error covariance matrix P.
    void init(const Math::Vector<6>& initState, const Math::Matrix<6, 6>& initCov);

    /// @brief Configures process noise covariances for position, velocity, and acceleration.
    void setProcessNoise(double qPos, double qVel, double qAcc) noexcept;

    /// @brief Executes non-linear state and covariance prediction step over elapsed time dt.
    /// @param[in] dt Elapsed time in seconds.
    void predict(double dt);

    /// @brief Executes non-linear measurement update step with Joseph stabilized covariance.
    /// @param[in] z Actual 2D sensor measurement [u, v]^T.
    /// @param[in] h Measurement function converting state vector to predicted measurement.
    /// @param[in] H Measurement Jacobian matrix dh/dx (2x6).
    /// @param[in] R Measurement noise covariance matrix (2x2).
    void update(const Math::Vector<2>& z, const MeasurementFn& h, const JacobianFn& H, const Math::Matrix<2, 2>& R);

    /// @brief Retrieves the estimated 6-state vector.
    [[nodiscard]] const Math::Vector<6>& getState() const noexcept;

    /// @brief Retrieves the 6x6 error covariance matrix.
    [[nodiscard]] const Math::Matrix<6, 6>& getCovariance() const noexcept;

    /// @brief Retrieves the latest 2D measurement innovation residual (z - h(x)).
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
    bool m_initialized { false };
    Math::Vector<6> m_x {};
    Math::Matrix<6, 6> m_P {};
    Math::Matrix<6, 6> m_Q {};

    Math::Vector<2> m_y {};
    Math::Matrix<2, 2> m_S {};
    double m_mahalanobisSq { 0.0 };

    double m_qPos { 1e-4 };
    double m_qVel { 1e-3 };
    double m_qAcc { 1e-2 };
};

} // namespace Tracking
