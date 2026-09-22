#pragma once

/// @file PtzCameraModel.h
/// @brief Pinhole camera projective geometry with dynamic optical zoom, lens distortion, and gimbal rotations.

#include "MatrixMath.h"

namespace Tracking {

/// @struct CameraIntrinsics
/// @brief Optical and sensor parameters defining camera projection.
struct CameraIntrinsics {
    int imageWidth { 1920 }; ///< Frame sensor width in pixels.
    int imageHeight { 1080 }; ///< Frame sensor height in pixels.
    double cx { 960.0 }; ///< Horizontal principal point in pixels.
    double cy { 540.0 }; ///< Vertical principal point in pixels.
    double fx0 { 1200.0 }; ///< Base focal length along X axis at 1.0x optical zoom.
    double fy0 { 1200.0 }; ///< Base focal length along Y axis at 1.0x optical zoom.
    double k1 { 0.0 }; ///< First-order radial lens distortion coefficient.
    double k2 { 0.0 }; ///< Second-order radial lens distortion coefficient.
};

/// @class PtzCameraModel
/// @brief Models the non-linear transformation between 3D world target angles and 2D sensor pixel coordinates.
class PtzCameraModel {
public:
    /// @brief Constructs camera model with given intrinsics.
    /// @param[in] intrinsics Sensor resolution, focal lengths, and distortion parameters.
    explicit PtzCameraModel(CameraIntrinsics intrinsics = {});

    /// @brief Projects target spherical angles onto 2D image sensor coordinates.
    /// @param[in] targetAzimuthRad Target true azimuth angle in radians [0, 2*pi).
    /// @param[in] targetElevationRad Target true elevation angle in radians [-pi/2, +pi/2].
    /// @param[in] camPanRad Camera physical pan angle in radians.
    /// @param[in] camTiltRad Camera physical tilt angle in radians.
    /// @param[in] zoom Optical zoom multiplier factor (>= 1.0).
    /// @return 2D vector of pixel coordinates [u, v]^T.
    [[nodiscard]] Math::Vector<2> project(double targetAzimuthRad, double targetElevationRad, double camPanRad,
        double camTiltRad, double zoom = 1.0) const noexcept;

    /// @brief Inversely unprojects 2D pixel coordinates into true target spherical angles.
    /// @param[in] u Pixel horizontal coordinate.
    /// @param[in] v Pixel vertical coordinate.
    /// @param[in] camPanRad Camera physical pan angle in radians.
    /// @param[in] camTiltRad Camera physical tilt angle in radians.
    /// @param[in] zoom Optical zoom multiplier factor (>= 1.0).
    /// @param[out] targetAzimuthRad Output target true azimuth angle in radians.
    /// @param[out] targetElevationRad Output target true elevation angle in radians.
    /// @return True if unprojection succeeded; false if point is outside valid geometry.
    bool unproject(double u, double v, double camPanRad, double camTiltRad, double zoom, double& targetAzimuthRad,
        double& targetElevationRad) const noexcept;

    /// @brief Computes 2x2 measurement Jacobian matrix H = d[u, v] / d[azimuth, elevation].
    /// @param[in] targetAzimuthRad Target azimuth in radians.
    /// @param[in] targetElevationRad Target elevation in radians.
    /// @param[in] camPanRad Camera pan in radians.
    /// @param[in] camTiltRad Camera tilt in radians.
    /// @param[in] zoom Optical zoom factor.
    /// @return 2x2 Jacobian matrix.
    [[nodiscard]] Math::Matrix<2, 2> computeJacobian(double targetAzimuthRad, double targetElevationRad,
        double camPanRad, double camTiltRad, double zoom = 1.0) const noexcept;

    /// @brief Updates camera intrinsics.
    void setIntrinsics(const CameraIntrinsics& intrinsics) noexcept;

    /// @brief Retrieves current camera intrinsics.
    [[nodiscard]] const CameraIntrinsics& getIntrinsics() const noexcept;

private:
    CameraIntrinsics m_intrinsics {};
};

} // namespace Tracking
