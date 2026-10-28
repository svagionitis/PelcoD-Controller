#pragma once

/// @file VmtiOverlay.h
/// @brief Tactical display symbology for MISB ST 0903 Video Moving Target Indicator (VMTI) tracks.

#include <cstdint>

#include <optional>
#include <string>
#include <vector>

#include "GeoTypes.h"
#include "KlvTypes.h"
#include "MapViewport.h"
#include "VmtiTypes.h"

namespace Mapping {

/// @struct ScreenVmtiTarget
/// @brief Projected MISB ST 0903 target symbology on the viewport screen.
/// @details Contains the 2D screen pixel position, CE90 horizontal uncertainty radius in pixels,
///          formatted callsign label, confidence percentage, and motion state.
struct ScreenVmtiTarget {
    ScreenPoint screenPos { }; ///< Projected target ground location in pixels
    double ce90Pixels { 0.0 }; ///< CE90 horizontal uncertainty circle radius in pixels
    std::uint32_t targetId { 0U }; ///< Unique track ID
    std::string callsign { }; ///< Formatted tactical track callsign (e.g., "TRK-001")
    double confidence { 0.0 }; ///< Target detection confidence [0, 100]%
    bool isMoving { false }; ///< True if detection status indicates moving target
    bool valid { false }; ///< True if target location was successfully projected
};

/// @class VmtiOverlay
/// @brief Transforms MISB ST 0903 target series into viewport screen coordinates with uncertainty ellipses.
/// @details Supports both absolute WGS-84 target coordinates (Tag 17) and frame-center relative offsets (Tag 19/20).
class VmtiOverlay {
public:
    /// @brief Projects an individual MISB ST 0903 VTargetPack onto viewport screen coordinates.
    /// @details Resolves absolute target coordinates or frame-center relative offsets and computes
    ///          the horizontal uncertainty radius in pixels.
    /// @param[in] viewport Current active map viewport.
    /// @param[in] pack VTargetPack containing target location or frame center offsets.
    /// @param[in] frameCenter Optional frame center geodetic coordinate if pack uses relative offsets.
    /// @return Projected ScreenVmtiTarget symbology.
    [[nodiscard]] static ScreenVmtiTarget projectTarget(const MapViewport& viewport, const Klv::VTargetPack& pack,
        const std::optional<Klv::GeoPoint2D>& frameCenter = std::nullopt);

    /// @brief Projects all valid targets in a VMTI Local Set onto viewport screen coordinates.
    /// @details Iterates through vmti.targets and retains only targets with valid resolved coordinates.
    /// @param[in] viewport Current active map viewport.
    /// @param[in] vmti VmtiLocalSet containing target series.
    /// @param[in] frameCenter Optional frame center for offset resolution.
    /// @return Vector of projected ScreenVmtiTarget items.
    [[nodiscard]] static std::vector<ScreenVmtiTarget> projectTargetSeries(const MapViewport& viewport,
        const Klv::VmtiLocalSet& vmti, const std::optional<Klv::GeoPoint2D>& frameCenter = std::nullopt);

    /// @brief Tests if a projected target falls inside or intersects the viewport rect.
    /// @details Expands the target position by max(ce90Pixels, 8.0) before testing rectangle intersection.
    /// @param[in] target Projected ScreenVmtiTarget.
    /// @param[in] viewportRect Viewport pixel boundary.
    /// @return True if visible on screen.
    [[nodiscard]] static bool isTargetVisible(const ScreenVmtiTarget& target, const ScreenRect& viewportRect) noexcept;
};

} // namespace Mapping
