#pragma once

/// @file TrackingVmtiBridge.h
/// @brief Interoperability bridge between Tracking estimation algorithms and MISB ST 0903 VMTI metadata.

#include "PtzCameraModel.h"
#include "PtzSphericalEstimator.h"
#include "Klv/KlvGeodesy.h"
#include "Klv/VmtiTypes.h"

#include <cstdint>
#include <optional>

namespace Tracking {

/// @class TrackingVmtiBridge
/// @brief Converts estimated kinematic target states into standard MISB ST 0903 VTargetPack elements,
///        and extracts visual detection measurements from incoming VMTI packets.
class TrackingVmtiBridge {
public:
    /// @brief Packages an estimated spherical target state into a standard MISB ST 0903 VTargetPack.
    /// @param[in] state Kinematic state from PtzSphericalEstimator.
    /// @param[in] intrinsics Camera projection intrinsics.
    /// @param[in] trackId Unique track identifier (BER-OID).
    /// @param[in] camPanRad Camera physical pan angle in radians.
    /// @param[in] camTiltRad Camera physical tilt angle in radians.
    /// @param[in] zoom Optical magnification factor (>= 1.0).
    /// @param[in] slantRangeMeters Target slant range in meters (optional, for geolocation).
    /// @param[in] platformPos Platform 3D geodetic location (optional, for target geolocation).
    /// @return Serialized VTargetPack ready for VMTI Local Set encoding.
    [[nodiscard]] static Klv::VTargetPack buildTargetPack(
        const SphericalTargetState& state,
        const CameraIntrinsics& intrinsics,
        std::uint32_t trackId,
        double camPanRad = 0.0,
        double camTiltRad = 0.0,
        double zoom = 1.0,
        double slantRangeMeters = 0.0,
        const std::optional<Klv::GeoPoint3D>& platformPos = std::nullopt) noexcept;

    /// @brief Extracts 2D continuous pixel coordinate from a MISB ST 0903 VTargetPack.
    /// @param[in] pack VTargetPack detection.
    /// @param[out] outU Horizontal pixel coordinate.
    /// @param[out] outV Vertical pixel coordinate.
    /// @return True if a valid centroid or bounding box center was found.
    [[nodiscard]] static bool extractPixelDetection(
        const Klv::VTargetPack& pack,
        double& outU,
        double& outV) noexcept;
};

} // namespace Tracking
