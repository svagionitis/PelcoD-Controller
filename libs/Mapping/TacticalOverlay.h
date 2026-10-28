#pragma once

/// @file TacticalOverlay.h
/// @brief Tactical symbology, optical footprint projection, and line-of-sight geometry.

#include <array>

#include "GeoTypes.h"
#include "KlvTypes.h"
#include "MapViewport.h"

namespace Mapping {

/// @struct ScreenFrustum
/// @brief 4-corner optical sensor footprint quadrilateral projected onto viewport screen pixels.
/// @details Stores corners in clockwise order: [0]=Top-Left, [1]=Top-Right, [2]=Bottom-Right, [3]=Bottom-Left.
struct ScreenFrustum {
    std::array<ScreenPoint, 4> corners { }; ///< [0]=TL, [1]=TR, [2]=BR, [3]=BL
    bool valid { false }; ///< True if all four footprint corners were projected validly
};

/// @struct ScreenVector
/// @brief Directed 2D line segment on viewport in pixels.
/// @details Used for rendering platform compass heading indicators and optical boresight lines.
struct ScreenVector {
    ScreenPoint origin { }; ///< Start point in viewport screen pixels
    ScreenPoint tip { }; ///< End point in viewport screen pixels
    bool valid { false }; ///< True if the vector geometry is valid
};

/// @class TacticalOverlay
/// @brief Transforms camera platform geodetic telemetry and STANAG 4609 footprint polygons
///        into pixel-accurate screen symbology.
/// @details Unrolls horizontal screen wrapping across the +-180 deg anti-meridian so footprint
///          polygons and boresight rays remain contiguous.
class TacticalOverlay {
public:
    /// @brief Projects the 4-corner geodetic footprint frustum into viewport screen coordinates.
    /// @details Unrolls screen X coordinates of corners 1..3 relative to corner 0 across the anti-meridian.
    /// @param[in] viewport Current active map viewport.
    /// @param[in] frustum Geodetic frustum corners (from KlvGeodesy / KLV Tag 26-33).
    /// @return Projected ScreenFrustum in screen pixels.
    [[nodiscard]] static ScreenFrustum projectFrustum(
        const MapViewport& viewport, const Klv::FrustumCorners& frustum) noexcept;

    /// @brief Projects a directional heading vector (pointer) originating from the platform position.
    /// @details Converts compass heading (0 deg = North = -Y, 90 deg = East = +X) into a 2D pixel segment.
    /// @param[in] viewport Current active map viewport.
    /// @param[in] platformPos Geographic location of the platform.
    /// @param[in] headingDeg Compass heading in degrees [0, 360) (0 = North, 90 = East).
    /// @param[in] lengthPixels Vector arrow length in screen pixels (default 40.0).
    /// @return Projected ScreenVector.
    [[nodiscard]] static ScreenVector projectHeadingVector(const MapViewport& viewport,
        const Klv::GeoPoint2D& platformPos, double headingDeg, double lengthPixels = 40.0) noexcept;

    /// @brief Projects the optical boresight line-of-sight from camera platform to target center.
    /// @details Unrolls the target tip X coordinate relative to the platform origin across the anti-meridian.
    /// @param[in] viewport Current active map viewport.
    /// @param[in] platformPos Platform geographic location.
    /// @param[in] targetPos Target / frame center ground location.
    /// @return Projected ScreenVector from platform origin to target ground center.
    [[nodiscard]] static ScreenVector projectLineOfSight(
        const MapViewport& viewport, const Klv::GeoPoint2D& platformPos, const Klv::GeoPoint2D& targetPos) noexcept;

    /// @brief Checks whether any part of the frustum polygon intersects or falls within the viewport.
    /// @details Performs corner inclusion and axis-aligned bounding box intersection tests against viewportRect.
    /// @param[in] frustum ScreenFrustum to test.
    /// @param[in] viewportRect Viewport pixel boundary.
    /// @return True if visible on screen, false if completely off-screen.
    [[nodiscard]] static bool isFrustumVisible(const ScreenFrustum& frustum, const ScreenRect& viewportRect) noexcept;
};

} // namespace Mapping
