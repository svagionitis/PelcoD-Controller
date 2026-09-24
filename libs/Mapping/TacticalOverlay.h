#pragma once

/// @file TacticalOverlay.h
/// @brief Tactical symbology, optical footprint projection, and line-of-sight geometry.

#include "GeoTypes.h"
#include "KlvTypes.h"
#include "MapViewport.h"

#include <array>
#include <string>
#include <utility>

namespace Mapping {

/// @struct ScreenFrustum
/// @brief 4-corner optical sensor footprint quadrilateral projected onto viewport screen pixels.
struct ScreenFrustum {
    std::array<ScreenPoint, 4> corners {}; ///< [0]=TL, [1]=TR, [2]=BR, [3]=BL
    bool valid { false };
};

/// @struct ScreenVector
/// @brief Directed 2D line segment on viewport in pixels.
struct ScreenVector {
    ScreenPoint origin {};
    ScreenPoint tip {};
    bool valid { false };
};

/// @class TacticalOverlay
/// @brief Transforms camera platform geodetic telemetry and STANAG 4609 footprint polygons
///        into pixel-accurate screen symbology.
class TacticalOverlay {
public:
    /// @brief Projects the 4-corner geodetic footprint frustum into viewport screen coordinates.
    /// @param[in] viewport Current active map viewport.
    /// @param[in] frustum Geodetic frustum corners (from KlvGeodesy / KLV Tag 26-33).
    /// @return Projected ScreenFrustum in screen pixels.
    [[nodiscard]] static ScreenFrustum projectFrustum(const MapViewport& viewport,
                                                      const Klv::FrustumCorners& frustum) noexcept;

    /// @brief Projects a directional heading vector (pointer) originating from the platform position.
    /// @param[in] viewport Current active map viewport.
    /// @param[in] platformPos Geographic location of the platform.
    /// @param[in] headingDeg Compass heading in degrees [0, 360) (0 = North, 90 = East).
    /// @param[in] lengthPixels Vector arrow length in screen pixels (default 40.0).
    /// @return Projected ScreenVector.
    [[nodiscard]] static ScreenVector projectHeadingVector(const MapViewport& viewport,
                                                           const Klv::GeoPoint2D& platformPos,
                                                           double headingDeg,
                                                           double lengthPixels = 40.0) noexcept;

    /// @brief Projects the optical boresight line-of-sight from camera platform to target center.
    /// @param[in] viewport Current active map viewport.
    /// @param[in] platformPos Platform geographic location.
    /// @param[in] targetPos Target / frame center ground location.
    /// @return Projected ScreenVector from platform origin to target ground center.
    [[nodiscard]] static ScreenVector projectLineOfSight(const MapViewport& viewport,
                                                         const Klv::GeoPoint2D& platformPos,
                                                         const Klv::GeoPoint2D& targetPos) noexcept;

    /// @brief Checks whether any part of the frustum polygon intersects or falls within the viewport.
    /// @param[in] frustum ScreenFrustum to test.
    /// @param[in] viewportRect Viewport pixel boundary.
    /// @return True if visible on screen, false if completely off-screen.
    [[nodiscard]] static bool isFrustumVisible(const ScreenFrustum& frustum,
                                              const ScreenRect& viewportRect) noexcept;
};

} // namespace Mapping
