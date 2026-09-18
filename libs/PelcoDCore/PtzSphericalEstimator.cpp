/// @file PtzSphericalEstimator.cpp
/// @brief Implementation of domain-specific spherical kinematic estimator.

#include "PtzSphericalEstimator.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace PelcoD {

PtzSphericalEstimator::PtzSphericalEstimator(SphericalEstimatorConfig config)
    : m_config(config)
    , m_cameraModel(config.intrinsics)
{
    setConfig(config);
}

void PtzSphericalEstimator::setConfig(const SphericalEstimatorConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    m_cameraModel.setIntrinsics(config.intrinsics);

    m_ekf.setProcessNoise(config.qAngularPos, config.qAngularVel, config.qAngularAcc);
    m_ukf.setProcessNoise(config.qAngularPos, config.qAngularVel, config.qAngularAcc);
}

const SphericalEstimatorConfig& PtzSphericalEstimator::getConfig() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void PtzSphericalEstimator::setType(EstimatorType type)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.type = type;
}

void PtzSphericalEstimator::reset() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ekf.reset();
    m_ukf.reset();
    m_locked = false;
    m_lastOutlierGated = false;
    m_lastCamPanRad = 0.0;
    m_lastCamTiltRad = 0.0;
}

bool PtzSphericalEstimator::isLocked() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_locked;
}

void PtzSphericalEstimator::init(double targetAzimuthRad, double targetElevationRad)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Math::Vector<6> x0;
    x0[0] = targetAzimuthRad;
    x0[1] = targetElevationRad;
    x0[2] = 0.0;
    x0[3] = 0.0;
    x0[4] = 0.0;
    x0[5] = 0.0;

    Math::Matrix<6, 6> P0 = Math::Matrix<6, 6>::identity();
    P0(0, 0) = 1e-3;
    P0(1, 1) = 1e-3;
    P0(2, 2) = 1e-1;
    P0(3, 3) = 1e-1;
    P0(4, 4) = 1.0;
    P0(5, 5) = 1.0;

    m_ekf.init(x0, P0);
    m_ukf.init(x0, P0);
    m_locked = true;
    m_lastOutlierGated = false;
}

void PtzSphericalEstimator::initFromPixel(double u, double v, double camPanRad, double camTiltRad, double zoom)
{
    double targetAzimuthRad = 0.0;
    double targetElevationRad = 0.0;
    if (m_cameraModel.unproject(u, v, camPanRad, camTiltRad, zoom, targetAzimuthRad, targetElevationRad)) {
        init(targetAzimuthRad, targetElevationRad);
        m_lastCamPanRad = camPanRad;
        m_lastCamTiltRad = camTiltRad;
    }
}

void PtzSphericalEstimator::predict(double dt)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_locked) {
        return;
    }

    if (m_config.type == EstimatorType::EKF) {
        m_ekf.predict(dt);
    } else {
        m_ukf.predict(dt);
    }
}

void PtzSphericalEstimator::update(double u, double v, double camPanRad, double camTiltRad, double zoom, double dt)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastCamPanRad = camPanRad;
    m_lastCamTiltRad = camTiltRad;

    if (!m_locked) {
        initFromPixel(u, v, camPanRad, camTiltRad, zoom);
        return;
    }

    // 1. Prediction step
    if (m_config.type == EstimatorType::EKF) {
        m_ekf.predict(dt);
    } else {
        m_ukf.predict(dt);
    }

    // 2. Non-linear measurement function mapping 6-state [theta, phi, dtheta, dphi, ddtheta, ddphi] to 2D pixel [u, v]
    auto h_func = [this, camPanRad, camTiltRad, zoom](const Math::Vector<6>& state) -> Math::Vector<2> {
        return m_cameraModel.project(state[0], state[1], camPanRad, camTiltRad, zoom);
    };

    // Measurement noise covariance R (2x2)
    const double varPix = m_config.pixelNoiseStd * m_config.pixelNoiseStd;
    Math::Matrix<2, 2> R = Math::Matrix<2, 2>::identity() * varPix;

    const Math::Vector<2> z { u, v };

    if (m_config.type == EstimatorType::EKF) {
        auto H_func = [this, camPanRad, camTiltRad, zoom](const Math::Vector<6>& state) -> Math::Matrix<2, 6> {
            const Math::Matrix<2, 2> J = m_cameraModel.computeJacobian(
                state[0], state[1], camPanRad, camTiltRad, zoom);
            Math::Matrix<2, 6> H;
            H(0, 0) = J(0, 0);
            H(0, 1) = J(0, 1);
            H(1, 0) = J(1, 0);
            H(1, 1) = J(1, 1);
            return H;
        };

        // Pre-check Mahalanobis distance before accepting update
        const Math::Vector<2> z_pred = h_func(m_ekf.getState());
        const Math::Vector<2> y = z - z_pred;
        const Math::Matrix<2, 6> H = H_func(m_ekf.getState());
        const Math::Matrix<2, 2> S = H * m_ekf.getCovariance() * H.transpose() + R;
        const Math::Matrix<2, 2> S_inv = S.inverse();

        const double mahalSq = y[0] * (S_inv(0, 0) * y[0] + S_inv(0, 1) * y[1])
            + y[1] * (S_inv(1, 0) * y[0] + S_inv(1, 1) * y[1]);

        if (mahalSq > (m_config.mahalanobisGateThreshold * m_config.mahalanobisGateThreshold)) {
            // Outlier rejected; coast on prediction
            m_lastOutlierGated = true;
            return;
        }

        m_lastOutlierGated = false;
        m_ekf.update(z, h_func, H_func, R);
    } else {
        // UKF measurement update
        m_ukf.update(z, h_func, R);
        m_lastOutlierGated = false;
    }
}

SphericalTargetState PtzSphericalEstimator::getState(double lookaheadLatencySeconds,
    double camPanRad, double camTiltRad) const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    SphericalTargetState out {};
    if (!m_locked) {
        return out;
    }

    const Math::Vector<6>& x = (m_config.type == EstimatorType::EKF) ? m_ekf.getState() : m_ukf.getState();

    out.azimuthRad = x[0];
    out.elevationRad = x[1];
    out.azimuthVelocityRadPerSec = x[2];
    out.elevationVelocityRadPerSec = x[3];

    // Angular errors relative to camera pan/tilt in degrees
    // Shortest angular difference for azimuth
    double deltaAz = x[0] - camPanRad;
    while (deltaAz > M_PI) {
        deltaAz -= 2.0 * M_PI;
    }
    while (deltaAz < -M_PI) {
        deltaAz += 2.0 * M_PI;
    }

    const double deltaEl = x[1] - camTiltRad;

    out.errorAzimuthDeg = deltaAz * (180.0 / M_PI);
    out.errorElevationDeg = deltaEl * (180.0 / M_PI);
    out.omegaAzimuthDegPerSec = x[2] * (180.0 / M_PI);
    out.omegaElevationDegPerSec = x[3] * (180.0 / M_PI);

    // Lookahead prediction
    if (lookaheadLatencySeconds > 0.0) {
        const double tau = lookaheadLatencySeconds;
        const double predAz = x[0] + x[2] * tau + 0.5 * x[4] * tau * tau;
        const double predEl = x[1] + x[3] * tau + 0.5 * x[5] * tau * tau;

        double predDeltaAz = predAz - camPanRad;
        while (predDeltaAz > M_PI) {
            predDeltaAz -= 2.0 * M_PI;
        }
        while (predDeltaAz < -M_PI) {
            predDeltaAz += 2.0 * M_PI;
        }

        out.predictedErrorAzimuthDeg = predDeltaAz * (180.0 / M_PI);
        out.predictedErrorElevationDeg = (predEl - camTiltRad) * (180.0 / M_PI);
    } else {
        out.predictedErrorAzimuthDeg = out.errorAzimuthDeg;
        out.predictedErrorElevationDeg = out.errorElevationDeg;
    }

    out.mahalanobisDistance = (m_config.type == EstimatorType::EKF)
        ? m_ekf.getMahalanobisDistance()
        : m_ukf.getMahalanobisDistance();
    out.locked = m_locked;
    out.isOutlierGated = m_lastOutlierGated;
    return out;
}

} // namespace PelcoD
