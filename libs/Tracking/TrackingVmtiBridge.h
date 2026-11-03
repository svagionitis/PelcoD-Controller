#pragma once

/// @file TrackingVmtiBridge.h
/// @brief Interoperability bridge between Tracking estimation algorithms and MISB ST 0903 VMTI metadata.

#include <cstdint>

#include <optional>
#include <string_view>
#include <vector>

#include "Klv/KlvGeodesy.h"
#include "Klv/VmtiTypes.h"
#include "PtzCameraModel.h"
#include "PtzSphericalEstimator.h"

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
    [[nodiscard]] static Klv::VTargetPack buildTargetPack(const SphericalTargetState& state,
        const CameraIntrinsics& intrinsics, std::uint32_t trackId, double camPanRad = 0.0, double camTiltRad = 0.0,
        double zoom = 1.0, double slantRangeMeters = 0.0,
        const std::optional<Klv::GeoPoint3D>& platformPos = std::nullopt) noexcept;

    /// @brief Extracts 2D continuous pixel coordinate from a MISB ST 0903 VTargetPack.
    /// @param[in] pack VTargetPack detection.
    /// @param[out] outU Horizontal pixel coordinate.
    /// @param[out] outV Vertical pixel coordinate.
    /// @return True if a valid centroid or bounding box center was found.
    [[nodiscard]] static bool extractPixelDetection(const Klv::VTargetPack& pack, double& outU, double& outV) noexcept;

    /// @brief Packages an estimated kinematic state and history into a standard MISB ST 0903 VTrackPack.
    /// @param[in] state Kinematic state from PtzSphericalEstimator.
    /// @param[in] trackId Unique persistent track identifier.
    /// @param[in] status Current track lifecycle status.
    /// @param[in] slantRangeMeters Target slant range in meters for linear velocity computation.
    /// @param[in] history Trajectory waypoints trail.
    /// @param[in] startTimeUs Precision timestamp when track was initiated.
    /// @param[in] durationUs Active tracking duration in microseconds.
    /// @return Serialized VTrackPack ready for VMTI Tag 102 series encoding.
    [[nodiscard]] static Klv::VTrackPack buildTrackPack(const SphericalTargetState& state, std::uint32_t trackId,
        Klv::TrackStatus status, double slantRangeMeters = 0.0, const std::vector<Klv::TrackHistoryPoint>& history = {},
        std::optional<std::uint64_t> startTimeUs = std::nullopt,
        std::optional<std::uint32_t> durationUs = std::nullopt) noexcept;

    /// @brief Packages classification attributes into a standard MISB ST 0903 OntologyLocalSet.
    /// @param[in] trackId Track identifier matching target.
    /// @param[in] category NATO STANAG 1241 / 4676 classification category.
    /// @param[in] label Human-readable taxonomy class name.
    /// @param[in] confidence Classification confidence percentage [0, 100]%.
    /// @param[in] sidc MIL-STD-2525 / APP-6 symbol identification code.
    /// @param[in] uri Taxonomy schema URI / URN.
    /// @return Serialized OntologyLocalSet ready for VMTI Tag 103 encoding.
    [[nodiscard]] static Klv::OntologyLocalSet buildOntology(std::uint32_t trackId, Klv::StanagCategory category,
        std::string_view label, std::uint8_t confidence, std::string_view sidc = "",
        std::string_view uri = "urn:nato:stanag:1241") noexcept;
};

} // namespace Tracking
