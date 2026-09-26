#pragma once

/// @file PlatformLeverArmCompensator.h
/// @brief Kinematic coordinate frame transformations compensating for host platform lever arms,
///        dynamic attitude (pitch, roll, yaw), and gimbal mounting orientations.

#include "GeoreferenceUtils.h"
#include "Klv/KlvTypes.h"
#include "PayloadTypes.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

namespace PayloadHal {

/// @enum GimbalMountingType
/// @brief Gimbal installation orientation relative to the host platform body frame.
enum class GimbalMountingType : std::uint8_t {
    Upright,  ///< Standard pedestal/mast mount (Z down, Az=0 fwd, El=0 level)
    Inverted, ///< Underslung aircraft/drone belly mount (180° roll inversion)
    Custom    ///< Arbitrary 3D mounting Euler angles relative to platform body
};

/// @struct Vector3D
/// @brief Generic 3-axis Cartesian vector in meters.
struct Vector3D {
    double x { 0.0 }; ///< Forward (+) / Back (-) in body frame or North in NED
    double y { 0.0 }; ///< Right (+) / Left (-) in body frame or East in NED
    double z { 0.0 }; ///< Down (+) / Up (-) in body frame or Down in NED
};

/// @struct MountingOrientation
/// @brief 3D Euler angles describing gimbal mount attitude relative to the platform body frame.
struct MountingOrientation {
    double yawDeg { 0.0 };   ///< Azimuth alignment offset relative to platform nose/bow [0, 360)
    double pitchDeg { 0.0 }; ///< Down/up tilt offset of mount base plate [-90, +90]
    double rollDeg { 0.0 };  ///< Bank/roll offset of mount base plate [-180, +180]
};

/// @struct PlatformLeverArmConfig
/// @brief Rigid-body geometric offsets between platform navigation reference, gimbal, and sensor.
struct PlatformLeverArmConfig {
    Vector3D gpsToGimbalBodyM { 0.0, 0.0, 0.0 };    ///< Physical vector from GPS antenna to gimbal base
    Vector3D gimbalToSensorM { 0.0, 0.0, 0.0 };     ///< Physical vector from gimbal pivot to sensor nodal point
    GimbalMountingType mountingType { GimbalMountingType::Upright };
    MountingOrientation mountOrientation {};         ///< Used when mountingType == Custom
};

/// @struct PlatformPose
/// @brief Dynamic host vehicle navigation state.
struct PlatformPose {
    Klv::GeoPoint3D gpsPosition {}; ///< Latitude, Longitude, Altitude MSL/HAE in meters
    double headingDeg { 0.0 };      ///< True compass heading [0, 360)
    double pitchDeg { 0.0 };        ///< Platform pitch [-90, +90]
    double rollDeg { 0.0 };         ///< Platform roll [-180, +180]
};

/// @class PlatformLeverArmCompensator
/// @brief Computes rigid-body coordinate transformations eliminating navigation lever-arm errors.
class PlatformLeverArmCompensator {
public:
    PlatformLeverArmCompensator() = default;
    explicit PlatformLeverArmCompensator(const PlatformLeverArmConfig& config);
    virtual ~PlatformLeverArmCompensator() = default;

    // --- Configuration ---
    void setConfig(const PlatformLeverArmConfig& config) noexcept;
    [[nodiscard]] PlatformLeverArmConfig config() const noexcept;

    // --- Forward Kinematics: Sensor Position & Target Georeferencing ---

    /// @brief Computes the exact 3D geodetic coordinates of the sensor optical nodal point.
    /// @param[in] pose Current platform navigation state.
    /// @param[in] panAngleDeg Gimbal pan angle in degrees.
    /// @param[in] tiltAngleDeg Gimbal tilt angle in degrees.
    /// @return 3D geodetic coordinate of sensor optical center.
    [[nodiscard]] Klv::GeoPoint3D computeSensorPosition(
        const PlatformPose& pose,
        double panAngleDeg = 0.0,
        double tiltAngleDeg = 0.0) const;

    /// @brief Computes the 3D position vector of the sensor optical nodal point in local NED frame
    ///        relative to the host platform GPS/navigation antenna.
    /// @param[in] pose Current platform navigation state.
    /// @param[in] panAngleDeg Gimbal pan angle in degrees.
    /// @param[in] tiltAngleDeg Gimbal tilt angle in degrees.
    /// @return 3D Cartesian vector [x=North, y=East, z=Down] in meters.
    [[nodiscard]] Vector3D computeSensorPositionNed(
        const PlatformPose& pose,
        double panAngleDeg = 0.0,
        double tiltAngleDeg = 0.0) const;

    /// @brief Computes the unit line-of-sight vector in local NED frame.
    /// @param[in] pose Current platform navigation state.
    /// @param[in] panAngleDeg Gimbal pan angle in degrees.
    /// @param[in] tiltAngleDeg Gimbal tilt angle in degrees.
    /// @return Normalized 3D line-of-sight vector in local NED frame.
    [[nodiscard]] Vector3D computeLineOfSightNed(
        const PlatformPose& pose,
        double panAngleDeg,
        double tiltAngleDeg) const;

    /// @brief Projects 3D target coordinates given slant range, compensating for all lever arms.
    /// @param[in] pose Current platform navigation state.
    /// @param[in] panAngleDeg Gimbal pan angle in degrees.
    /// @param[in] tiltAngleDeg Gimbal tilt angle in degrees.
    /// @param[in] slantRangeMeters Target distance along line of sight.
    /// @return Target geodetic 3D coordinate if calculation succeeds.
    [[nodiscard]] std::optional<Klv::GeoPoint3D> computeTargetFromSlantRange(
        const PlatformPose& pose,
        double panAngleDeg,
        double tiltAngleDeg,
        double slantRangeMeters) const;

    /// @brief Projects target coordinates by intersecting terrain ground plane.
    /// @param[in] pose Current platform navigation state.
    /// @param[in] panAngleDeg Gimbal pan angle in degrees.
    /// @param[in] tiltAngleDeg Gimbal tilt angle in degrees.
    /// @param[in] groundElevationM Target ground elevation MSL in meters.
    /// @return Target geodetic 3D coordinate if line of sight intersects the ground.
    [[nodiscard]] std::optional<Klv::GeoPoint3D> computeTargetFromGroundIntersection(
        const PlatformPose& pose,
        double panAngleDeg,
        double tiltAngleDeg,
        double groundElevationM = 0.0) const;

    // --- Inverse Kinematics: Look-Angles from True Sensor Position ---

    /// @brief Computes required gimbal pan and tilt angles to acquire a 3D target coordinate,
    ///        taking into account platform attitude, mounting orientation, and lever arm.
    /// @param[in] pose Current platform navigation state.
    /// @param[in] targetPos Target 3D geodetic position.
    /// @return GimbalLookAngles structure with required pan and tilt angles in degrees.
    [[nodiscard]] GimbalLookAngles computeLookAnglesToTarget(
        const PlatformPose& pose,
        const Klv::GeoPoint3D& targetPos) const;

private:
    mutable std::mutex m_mutex {};
    PlatformLeverArmConfig m_config {};
};

} // namespace PayloadHal
