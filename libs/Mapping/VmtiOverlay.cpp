#include "VmtiOverlay.h"

#include <algorithm>
#include <cmath>

namespace Mapping {

ScreenVmtiTarget VmtiOverlay::projectTarget(
    const MapViewport& viewport,
    const Klv::VTargetPack& pack,
    const std::optional<Klv::GeoPoint2D>& frameCenter) noexcept
{
    ScreenVmtiTarget target;
    target.targetId = pack.targetId;
    target.callsign = "TRK-" + std::to_string(pack.targetId);
    target.confidence = static_cast<double>(pack.confidence.value_or(0U));
    target.isMoving = (pack.detectionStatus.value_or(0U) == 1U);

    // Resolve target geodetic coordinates
    std::optional<Klv::GeoPoint2D> targetGeo;

    if (pack.targetLocation.has_value()) {
        targetGeo = Klv::GeoPoint2D {
            pack.targetLocation->latitudeDeg,
            pack.targetLocation->longitudeDeg
        };
    } else if (pack.locationOffsetDeg.has_value() && frameCenter.has_value()) {
        targetGeo = Klv::GeoPoint2D {
            frameCenter->latitudeDeg + pack.locationOffsetDeg->latitudeDeg,
            frameCenter->longitudeDeg + pack.locationOffsetDeg->longitudeDeg
        };
    }

    if (!targetGeo.has_value()) {
        target.valid = false;
        return target;
    }

    target.screenPos = viewport.geoToScreen(*targetGeo);

    // Project CE90 horizontal uncertainty circle
    if (pack.targetCe90M.has_value() && *pack.targetCe90M > 0.0) {
        const double mpp = MercatorProjection::metersPerPixel(
            targetGeo->latitudeDeg,
            viewport.zoom(),
            viewport.tileSize());

        if (mpp > 1e-6) {
            target.ce90Pixels = *pack.targetCe90M / mpp;
        }
    }

    target.valid = true;
    return target;
}

std::vector<ScreenVmtiTarget> VmtiOverlay::projectTargetSeries(
    const MapViewport& viewport,
    const Klv::VmtiLocalSet& vmti,
    const std::optional<Klv::GeoPoint2D>& frameCenter)
{
    std::vector<ScreenVmtiTarget> results;
    results.reserve(vmti.targets.size());

    for (const auto& pack : vmti.targets) {
        ScreenVmtiTarget screenTarget = projectTarget(viewport, pack, frameCenter);
        if (screenTarget.valid) {
            results.push_back(std::move(screenTarget));
        }
    }

    return results;
}

bool VmtiOverlay::isTargetVisible(
    const ScreenVmtiTarget& target,
    const ScreenRect& viewportRect) noexcept
{
    if (!target.valid) {
        return false;
    }

    const double radius = std::max(target.ce90Pixels, 8.0); // At least 8px symbology hit-box
    const ScreenRect targetBox {
        target.screenPos.x - radius,
        target.screenPos.y - radius,
        radius * 2.0,
        radius * 2.0
    };

    if (targetBox.right() < viewportRect.left() || targetBox.left() > viewportRect.right() ||
        targetBox.bottom() < viewportRect.top() || targetBox.top() > viewportRect.bottom()) {
        return false;
    }

    return true;
}

} // namespace Mapping
