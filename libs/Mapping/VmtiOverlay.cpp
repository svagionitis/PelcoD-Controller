/// @file VmtiOverlay.cpp
/// @brief Implementation of MISB ST 0903 VMTI tactical target projection and visibility culling.

#include "VmtiOverlay.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "GeoTypes.h"
#include "KlvTypes.h"
#include "MapViewport.h"
#include "MercatorProjection.h"
#include "VmtiTypes.h"

namespace Mapping {

ScreenVmtiTarget VmtiOverlay::projectTarget(
    const MapViewport& viewport, const Klv::VTargetPack& pack, const std::optional<Klv::GeoPoint2D>& frameCenter)
{
    ScreenVmtiTarget target { };
    target.targetId = pack.targetId;
    target.callsign = "TRK-" + std::to_string(pack.targetId);
    target.confidence = static_cast<double>(pack.confidence.value_or(0U));
    target.isMoving = (pack.detectionStatus.value_or(0U) == 1U);

    // Resolve target geodetic coordinates
    std::optional<Klv::GeoPoint2D> targetGeo { };

    if (pack.targetLocation.has_value() && std::isfinite(pack.targetLocation->latitudeDeg)
        && std::isfinite(pack.targetLocation->longitudeDeg)) {
        targetGeo = Klv::GeoPoint2D { pack.targetLocation->latitudeDeg, pack.targetLocation->longitudeDeg };
    } else if (pack.locationOffsetDeg.has_value() && frameCenter.has_value()
        && std::isfinite(pack.locationOffsetDeg->latitudeDeg) && std::isfinite(pack.locationOffsetDeg->longitudeDeg)
        && std::isfinite(frameCenter->latitudeDeg) && std::isfinite(frameCenter->longitudeDeg)) {
        targetGeo = Klv::GeoPoint2D { frameCenter->latitudeDeg + pack.locationOffsetDeg->latitudeDeg,
            frameCenter->longitudeDeg + pack.locationOffsetDeg->longitudeDeg };
    }

    if (!targetGeo.has_value()) {
        target.valid = false;
        return target;
    }

    target.screenPos = viewport.geoToScreen(*targetGeo);

    // Project CE90 horizontal uncertainty circle
    if (pack.targetCe90M.has_value() && std::isfinite(*pack.targetCe90M) && *pack.targetCe90M > 0.0) {
        const double mpp { MercatorProjection::metersPerPixel(
            targetGeo->latitudeDeg, viewport.zoom(), viewport.tileSize()) };

        if (std::isfinite(mpp) && mpp > 1e-6) {
            target.ce90Pixels = *pack.targetCe90M / mpp;
        }
    }

    target.valid = true;
    return target;
}

std::vector<ScreenVmtiTarget> VmtiOverlay::projectTargetSeries(
    const MapViewport& viewport, const Klv::VmtiLocalSet& vmti, const std::optional<Klv::GeoPoint2D>& frameCenter)
{
    std::vector<ScreenVmtiTarget> results { };
    results.reserve(vmti.targets.size());

    for (const auto& pack : vmti.targets) {
        ScreenVmtiTarget screenTarget { projectTarget(viewport, pack, frameCenter) };
        if (screenTarget.valid) {
            results.push_back(std::move(screenTarget));
        }
    }

    return results;
}

bool VmtiOverlay::isTargetVisible(const ScreenVmtiTarget& target, const ScreenRect& viewportRect) noexcept
{
    if (!target.valid) {
        return false;
    }

    const double safeCe90 { std::isfinite(target.ce90Pixels) ? target.ce90Pixels : 0.0 };
    const double radius { std::max(safeCe90, 8.0) }; // At least 8px symbology hit-box
    const ScreenRect targetBox { target.screenPos.x - radius, target.screenPos.y - radius, radius * 2.0, radius * 2.0 };

    if (targetBox.right() < viewportRect.left() || targetBox.left() > viewportRect.right()
        || targetBox.bottom() < viewportRect.top() || targetBox.top() > viewportRect.bottom()) {
        return false;
    }

    return true;
}

} // namespace Mapping
