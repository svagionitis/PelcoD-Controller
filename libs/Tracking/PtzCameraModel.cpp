/// @file PtzCameraModel.cpp
/// @brief Implementation of camera projection geometry, radial distortion, and gimbal kinematics.

#include "PtzCameraModel.h"

#include <algorithm>
#include <cmath>

namespace Tracking {

PtzCameraModel::PtzCameraModel(CameraIntrinsics intrinsics)
    : m_intrinsics(intrinsics)
{
}

void PtzCameraModel::setIntrinsics(const CameraIntrinsics& intrinsics) noexcept
{
    m_intrinsics = intrinsics;
    m_intrinsics.fx0 = std::max(10.0, m_intrinsics.fx0);
    m_intrinsics.fy0 = std::max(10.0, m_intrinsics.fy0);
}

const CameraIntrinsics& PtzCameraModel::getIntrinsics() const noexcept
{
    return m_intrinsics;
}

Math::Vector<2> PtzCameraModel::project(double targetAzimuthRad, double targetElevationRad, double camPanRad,
    double camTiltRad, double zoom) const noexcept
{
    const double z = std::max(1.0, zoom);
    const double fx = m_intrinsics.fx0 * z;
    const double fy = m_intrinsics.fy0 * z;

    // Target direction vector in world coordinates
    // Azimuth theta: 0 is North/forward (+Z), pi/2 is East (+X)
    // Elevation phi: positive up (+Y), negative down (-Y)
    const double cosEl = std::cos(targetElevationRad);
    const double sinEl = std::sin(targetElevationRad);
    const double cosAz = std::cos(targetAzimuthRad);
    const double sinAz = std::sin(targetAzimuthRad);

    const double Xw = cosEl * sinAz;
    const double Yw = -sinEl; // Camera Y is downward in image space
    const double Zw = cosEl * cosAz;

    // Apply inverse camera pan rotation (around Y axis)
    const double cosPan = std::cos(camPanRad);
    const double sinPan = std::sin(camPanRad);

    const double X1 = cosPan * Xw - sinPan * Zw;
    const double Y1 = Yw;
    const double Z1 = sinPan * Xw + cosPan * Zw;

    // Apply inverse camera tilt rotation (around X axis)
    const double cosTilt = std::cos(camTiltRad);
    const double sinTilt = std::sin(camTiltRad);

    const double Xc = X1;
    const double Yc = cosTilt * Y1 + sinTilt * Z1;
    const double Zc = -sinTilt * Y1 + cosTilt * Z1;

    Math::Vector<2> pixel;
    if (Zc <= 1e-4) {
        // Target is behind camera plane
        pixel[0] = m_intrinsics.cx;
        pixel[1] = m_intrinsics.cy;
        return pixel;
    }

    const double xn = Xc / Zc;
    const double yn = Yc / Zc;

    // Radial distortion
    const double r2 = xn * xn + yn * yn;
    const double radial = 1.0 + m_intrinsics.k1 * r2 + m_intrinsics.k2 * r2 * r2;
    const double xd = xn * radial;
    const double yd = yn * radial;

    pixel[0] = m_intrinsics.cx + fx * xd;
    pixel[1] = m_intrinsics.cy + fy * yd;
    return pixel;
}

bool PtzCameraModel::unproject(double u, double v, double camPanRad, double camTiltRad, double zoom,
    double& targetAzimuthRad, double& targetElevationRad) const noexcept
{
    const double z = std::max(1.0, zoom);
    const double fx = m_intrinsics.fx0 * z;
    const double fy = m_intrinsics.fy0 * z;

    double xd = (u - m_intrinsics.cx) / fx;
    double yd = (v - m_intrinsics.cy) / fy;

    // Iterative undistortion (fixed-point iteration)
    double xn = xd;
    double yn = yd;
    for (int iter = 0; iter < 5; ++iter) {
        const double r2 = xn * xn + yn * yn;
        const double radial = 1.0 + m_intrinsics.k1 * r2 + m_intrinsics.k2 * r2 * r2;
        if (std::abs(radial) > 1e-6) {
            xn = xd / radial;
            yn = yd / radial;
        }
    }

    // Ray in camera coordinates
    const double norm = std::sqrt(xn * xn + yn * yn + 1.0);
    const double Xc = xn / norm;
    const double Yc = yn / norm;
    const double Zc = 1.0 / norm;

    // Rotate back by camera tilt (+phi around X)
    const double cosTilt = std::cos(camTiltRad);
    const double sinTilt = std::sin(camTiltRad);

    const double X1 = Xc;
    const double Y1 = cosTilt * Yc - sinTilt * Zc;
    const double Z1 = sinTilt * Yc + cosTilt * Zc;

    // Rotate back by camera pan (+theta around Y)
    const double cosPan = std::cos(camPanRad);
    const double sinPan = std::sin(camPanRad);

    const double Xw = cosPan * X1 + sinPan * Z1;
    const double Yw = Y1;
    const double Zw = -sinPan * X1 + cosPan * Z1;

    targetElevationRad = -std::asin(std::clamp(Yw, -1.0, 1.0));
    targetAzimuthRad = std::atan2(Xw, Zw);
    return true;
}

Math::Matrix<2, 2> PtzCameraModel::computeJacobian(double targetAzimuthRad, double targetElevationRad,
    double camPanRad, double camTiltRad, double zoom) const noexcept
{
    // High-precision central differences for analytical robustness across all distortion combinations
    constexpr double eps = 1e-6;

    const auto p_az_plus
        = project(targetAzimuthRad + eps, targetElevationRad, camPanRad, camTiltRad, zoom);
    const auto p_az_minus
        = project(targetAzimuthRad - eps, targetElevationRad, camPanRad, camTiltRad, zoom);

    const auto p_el_plus
        = project(targetAzimuthRad, targetElevationRad + eps, camPanRad, camTiltRad, zoom);
    const auto p_el_minus
        = project(targetAzimuthRad, targetElevationRad - eps, camPanRad, camTiltRad, zoom);

    Math::Matrix<2, 2> J;
    J(0, 0) = (p_az_plus[0] - p_az_minus[0]) / (2.0 * eps); // du / d_azimuth
    J(0, 1) = (p_el_plus[0] - p_el_minus[0]) / (2.0 * eps); // du / d_elevation
    J(1, 0) = (p_az_plus[1] - p_az_minus[1]) / (2.0 * eps); // dv / d_azimuth
    J(1, 1) = (p_el_plus[1] - p_el_minus[1]) / (2.0 * eps); // dv / d_elevation
    return J;
}

} // namespace Tracking
