#pragma once

/// @file SensorParallaxCompensator.h
/// @brief Physical baseline modeling, range-dependent angular parallax, and cross-spectrum reticle/bounding-box alignment.

#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace PayloadHal {

/// @struct SensorOffset3D
/// @brief Physical 3D baseline offset of a secondary sensor or laser relative to the reference sensor nodal point.
struct SensorOffset3D {
    double lateralOffsetM { 0.0 };      ///< bx: Right (+) / Left (-) in meters along gimbal horizontal axis
    double verticalOffsetM { 0.0 };     ///< by: Up (+) / Down (-) in meters along gimbal vertical axis
    double longitudinalOffsetM { 0.0 }; ///< bz: Forward (+) / Back (-) in meters along optical line-of-sight
};

/// @struct BoresightCalibration
/// @brief Static mechanical angular alignment calibration offsets at optical infinity.
struct BoresightCalibration {
    double azimuthOffsetDeg { 0.0 };    ///< Horizontal optical axis angular divergence at infinity
    double elevationOffsetDeg { 0.0 };  ///< Vertical optical axis angular divergence at infinity
    double rollOffsetDeg { 0.0 };       ///< Sensor roll/rotation divergence about line-of-sight
};

/// @struct ParallaxDisparity
/// @brief Computed angular, normalized screen, and pixel displacement at a designated slant range.
struct ParallaxDisparity {
    double rangeMeters { 0.0 };         ///< Slant range evaluated in meters
    double azimuthDisparityDeg { 0.0 };   ///< Net angular azimuth shift in degrees
    double elevationDisparityDeg { 0.0 }; ///< Net angular elevation shift in degrees
    double normalizedScreenDeltaX { 0.0 };///< Normalized screen offset X [-1.0, +1.0]
    double normalizedScreenDeltaY { 0.0 };///< Normalized screen offset Y [-1.0, +1.0]
    double pixelDeltaX { 0.0 };           ///< Pixel disparity X in image coordinates
    double pixelDeltaY { 0.0 };           ///< Pixel disparity Y in image coordinates
};

/// @struct ScreenRect2D
/// @brief 2D bounding box representing an object detection or tracking gate.
struct ScreenRect2D {
    double x { 0.0 };      ///< Top-left or center X
    double y { 0.0 };      ///< Top-left or center Y
    double width { 0.0 };  ///< Width in pixels or normalized units
    double height { 0.0 }; ///< Height in pixels or normalized units
};

/// @class SensorParallaxCompensator
/// @brief Computes range-dependent boresight divergence, reticle convergence offsets, and cross-spectrum tracking box transfers.
class SensorParallaxCompensator {
public:
    SensorParallaxCompensator() = default;
    explicit SensorParallaxCompensator(const SensorOffset3D& baseline,
                                       const BoresightCalibration& boresight = {});
    virtual ~SensorParallaxCompensator() = default;

    // --- Configuration ---
    void setBaselineOffset(const SensorOffset3D& baseline) noexcept;
    [[nodiscard]] SensorOffset3D baselineOffset() const noexcept;

    void setBoresightCalibration(const BoresightCalibration& boresight) noexcept;
    [[nodiscard]] BoresightCalibration boresightCalibration() const noexcept;

    // --- Core Parallax Computation ---

    /// @brief Computes angular disparity and image pixel displacement at a given slant range.
    /// @param[in] slantRangeMeters Distance to target along line-of-sight in meters.
    /// @param[in] primaryHfovDeg Horizontal field of view of the camera in degrees.
    /// @param[in] primaryVfovDeg Vertical field of view of the camera in degrees.
    /// @param[in] imageWidth Display/sensor width in pixels (default 1920).
    /// @param[in] imageHeight Display/sensor height in pixels (default 1080).
    /// @return ParallaxDisparity record containing angles and pixel shifts.
    [[nodiscard]] ParallaxDisparity computeDisparity(
        double slantRangeMeters,
        double primaryHfovDeg,
        double primaryVfovDeg,
        std::uint32_t imageWidth = 1920U,
        std::uint32_t imageHeight = 1080U) const;

    /// @brief Computes normalized screen displacement required to converge the reticle onto the aimpoint at range.
    /// @param[in] slantRangeMeters Slant range in meters.
    /// @param[in] hfovDeg Active camera horizontal field of view in degrees.
    /// @param[in] vfovDeg Active camera vertical field of view in degrees.
    /// @return Pair of normalized screen offsets {deltaX, deltaY} in range [-1.0, 1.0].
    [[nodiscard]] std::pair<double, double> computeReticleOffset(
        double slantRangeMeters,
        double hfovDeg,
        double vfovDeg) const;

    /// @brief Transforms an object detection bounding box from the primary camera frame into the secondary camera frame.
    /// @param[in] primaryBox Bounding box on primary camera (pixel or normalized).
    /// @param[in] slantRangeMeters Target distance in meters.
    /// @param[in] primaryHfovDeg Primary camera horizontal FOV.
    /// @param[in] primaryVfovDeg Primary camera vertical FOV.
    /// @param[in] secondaryHfovDeg Secondary camera horizontal FOV.
    /// @param[in] secondaryVfovDeg Secondary camera vertical FOV.
    /// @return Transformed and scaled bounding box for secondary camera.
    [[nodiscard]] ScreenRect2D transformBoundingBox(
        const ScreenRect2D& primaryBox,
        double slantRangeMeters,
        double primaryHfovDeg,
        double primaryVfovDeg,
        double secondaryHfovDeg,
        double secondaryVfovDeg) const;

    /// @brief Maps a normalized screen coordinate from primary sensor space to secondary sensor space.
    /// @param[in] xNorm Normalized X [-1.0, 1.0].
    /// @param[in] yNorm Normalized Y [-1.0, 1.0].
    /// @param[in] slantRangeMeters Target distance in meters.
    /// @param[in] primaryHfovDeg Primary camera horizontal FOV.
    /// @param[in] primaryVfovDeg Primary camera vertical FOV.
    /// @param[in] secondaryHfovDeg Secondary camera horizontal FOV.
    /// @param[in] secondaryVfovDeg Secondary camera vertical FOV.
    /// @return Mapped point in secondary sensor normalized coordinates.
    [[nodiscard]] std::pair<double, double> mapPointPrimaryToSecondary(
        double xNorm,
        double yNorm,
        double slantRangeMeters,
        double primaryHfovDeg,
        double primaryVfovDeg,
        double secondaryHfovDeg,
        double secondaryVfovDeg) const;

    /// @brief Computes convergence look-angle corrections for an auxiliary Laser Rangefinder or Illuminator.
    /// @param[in] slantRangeMeters Target distance in meters.
    /// @param[in] lrfOffset 3D offset of laser relative to camera (uses default internal offset if all zero).
    /// @return Pair of angular corrections {deltaAzDeg, deltaElDeg}.
    [[nodiscard]] std::pair<double, double> computeLrfConvergenceAngles(
        double slantRangeMeters,
        const SensorOffset3D& lrfOffset = {}) const;

private:
    mutable std::mutex m_mutex {};
    SensorOffset3D m_baseline {};
    BoresightCalibration m_boresight {};
};

} // namespace PayloadHal
