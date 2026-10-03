#include "TacticalOverlay.h"

#include <algorithm>
#include <cmath>

namespace Mapping {

namespace {
inline constexpr double kPi { 3.14159265358979323846 };
inline constexpr double kDegToRad { kPi / 180.0 };
} // namespace

ScreenFrustum TacticalOverlay::projectFrustum(const MapViewport& viewport,
                                             const Klv::FrustumCorners& frustum) noexcept {
    ScreenFrustum sf;
    sf.corners[0] = viewport.geoToScreen(frustum.topLeft);
    sf.corners[1] = viewport.geoToScreen(frustum.topRight);
    sf.corners[2] = viewport.geoToScreen(frustum.bottomRight);
    sf.corners[3] = viewport.geoToScreen(frustum.bottomLeft);

    // Unroll anti-meridian screen X wrapping relative to corner 0 to maintain contiguous polygon
    const double mapDimension = static_cast<double>(viewport.tileSize()) * std::pow(2.0, viewport.zoom());
    if (mapDimension > 0.0) {
        for (std::size_t i = 1; i < sf.corners.size(); ++i) {
            double dx = sf.corners[i].x - sf.corners[0].x;
            if (dx > mapDimension * 0.5) {
                sf.corners[i].x -= mapDimension;
            } else if (dx < -mapDimension * 0.5) {
                sf.corners[i].x += mapDimension;
            }
        }
    }

    sf.valid = true;
    return sf;
}

ScreenVector TacticalOverlay::projectHeadingVector(const MapViewport& viewport,
                                                   const Klv::GeoPoint2D& platformPos,
                                                   double headingDeg,
                                                   double lengthPixels) noexcept {
    ScreenVector sv;
    sv.origin = viewport.geoToScreen(platformPos);

    const double headingRad = headingDeg * kDegToRad;

    // In 2D screen space: 0 deg (North) is -Y, 90 deg (East) is +X
    sv.tip.x = sv.origin.x + (lengthPixels * std::sin(headingRad));
    sv.tip.y = sv.origin.y - (lengthPixels * std::cos(headingRad));
    sv.valid = true;

    return sv;
}

ScreenVector TacticalOverlay::projectLineOfSight(const MapViewport& viewport,
                                                 const Klv::GeoPoint2D& platformPos,
                                                 const Klv::GeoPoint2D& targetPos) noexcept {
    ScreenVector sv;
    sv.origin = viewport.geoToScreen(platformPos);
    sv.tip = viewport.geoToScreen(targetPos);

    // Unroll anti-meridian screen X wrapping relative to origin
    const double mapDimension = static_cast<double>(viewport.tileSize()) * std::pow(2.0, viewport.zoom());
    if (mapDimension > 0.0) {
        double dx = sv.tip.x - sv.origin.x;
        if (dx > mapDimension * 0.5) {
            sv.tip.x -= mapDimension;
        } else if (dx < -mapDimension * 0.5) {
            sv.tip.x += mapDimension;
        }
    }

    sv.valid = true;
    return sv;
}

bool TacticalOverlay::isFrustumVisible(const ScreenFrustum& frustum,
                                      const ScreenRect& viewportRect) noexcept {
    if (!frustum.valid) {
        return false;
    }

    // 1. Any corner inside viewport
    for (const auto& corner : frustum.corners) {
        if (viewportRect.contains(corner)) {
            return true;
        }
    }

    // 2. Frustum bounding box intersects viewport
    double minX = frustum.corners[0].x;
    double maxX = frustum.corners[0].x;
    double minY = frustum.corners[0].y;
    double maxY = frustum.corners[0].y;

    for (std::size_t i = 1; i < frustum.corners.size(); ++i) {
        minX = std::min(minX, frustum.corners[i].x);
        maxX = std::max(maxX, frustum.corners[i].x);
        minY = std::min(minY, frustum.corners[i].y);
        maxY = std::max(maxY, frustum.corners[i].y);
    }

    if (maxX < viewportRect.left() || minX > viewportRect.right() ||
        maxY < viewportRect.top() || minY > viewportRect.bottom()) {
        return false;
    }

    return true;
}

} // namespace Mapping
