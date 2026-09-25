#include "GeoreferenceUtils.h"
#include "Klv/KlvGeodesy.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

namespace {

    constexpr double kPi { 3.14159265358979323846 };

    inline double deg2rad(double deg) noexcept
    {
        return deg * (kPi / 180.0);
    }

    inline double rad2deg(double rad) noexcept
    {
        return rad * (180.0 / kPi);
    }

    inline double normalizeAngle360(double deg) noexcept
    {
        double wrapped = std::fmod(deg, 360.0);
        if (wrapped < 0.0) {
            wrapped += 360.0;
        }
        return wrapped;
    }

    inline double normalizeAngle180(double deg) noexcept
    {
        double wrapped = std::fmod(deg + 180.0, 360.0);
        if (wrapped < 0.0) {
            wrapped += 360.0;
        }
        return wrapped - 180.0;
    }

} // namespace

std::optional<Klv::GeoPoint3D> GeoreferenceUtils::computeTargetFromSlantRange(const Klv::GeoPoint3D& platformPos,
    double platformHeadingDeg, double gimbalPanDeg, double gimbalTiltDeg, double slantRangeMeters) noexcept
{
    if (slantRangeMeters <= 0.0) {
        return std::nullopt;
    }

    const double trueBearingDeg = normalizeAngle360(platformHeadingDeg + gimbalPanDeg);
    const double tiltRad = deg2rad(gimbalTiltDeg);

    const double groundDistanceM = slantRangeMeters * std::cos(tiltRad);
    const double deltaAltM = slantRangeMeters * std::sin(tiltRad);
    const double targetAltM = platformPos.altitudeM + deltaAltM;

    const Klv::GeoPoint2D origin2D { platformPos.latitudeDeg, platformPos.longitudeDeg };
    const Klv::GeoPoint2D target2D = Klv::KlvGeodesy::directGeodetic(origin2D, trueBearingDeg, groundDistanceM);

    return Klv::GeoPoint3D { target2D.latitudeDeg, target2D.longitudeDeg, targetAltM };
}

std::optional<Klv::GeoPoint3D> GeoreferenceUtils::computeTargetFromGroundIntersection(
    const Klv::GeoPoint3D& platformPos, double platformHeadingDeg, double gimbalPanDeg, double gimbalTiltDeg,
    double groundElevationM) noexcept
{
    const std::optional<Klv::GeoPoint2D> target2D = Klv::KlvGeodesy::computeFrameCenter(
        platformPos, platformHeadingDeg, gimbalPanDeg, gimbalTiltDeg, groundElevationM);

    if (!target2D.has_value()) {
        return std::nullopt;
    }

    return Klv::GeoPoint3D { target2D->latitudeDeg, target2D->longitudeDeg, groundElevationM };
}

std::optional<Klv::GeoPoint3D> GeoreferenceUtils::computeTargetFromGroundIntersection(
    const Klv::GeoPoint3D& platformPos, double platformHeadingDeg, double gimbalPanDeg, double gimbalTiltDeg,
    const ElevationLookupFn& elevationLookup, double fallbackGroundElevationM) noexcept
{
    if (!elevationLookup) {
        return computeTargetFromGroundIntersection(
            platformPos, platformHeadingDeg, gimbalPanDeg, gimbalTiltDeg, fallbackGroundElevationM);
    }

    double currentElev = fallbackGroundElevationM;
    std::optional<Klv::GeoPoint2D> currentTarget;

    // Iterative ray-marching convergence against DEM elevation surface (max 8 iterations)
    for (int iter = 0; iter < 8; ++iter) {
        currentTarget = Klv::KlvGeodesy::computeFrameCenter(
            platformPos, platformHeadingDeg, gimbalPanDeg, gimbalTiltDeg, currentElev);

        if (!currentTarget.has_value()) {
            return std::nullopt;
        }

        const double sampledElev = elevationLookup(currentTarget->latitudeDeg, currentTarget->longitudeDeg);
        if (std::abs(sampledElev - currentElev) < 0.1) {
            currentElev = sampledElev;
            break;
        }
        currentElev = sampledElev;
    }

    if (!currentTarget.has_value()) {
        return std::nullopt;
    }

    return Klv::GeoPoint3D { currentTarget->latitudeDeg, currentTarget->longitudeDeg, currentElev };
}

std::optional<Klv::FrustumCorners> GeoreferenceUtils::computeFrustumCorners(const Klv::GeoPoint3D& platformPos,
    double platformHeadingDeg, double gimbalPanDeg, double gimbalTiltDeg, double hfovDeg, double vfovDeg,
    double groundElevationM) noexcept
{
    if (hfovDeg <= 0.0 || gimbalTiltDeg >= -0.01) {
        return std::nullopt;
    }
    if (platformPos.altitudeM <= groundElevationM) {
        return std::nullopt;
    }

    double effectiveVfov = vfovDeg;
    if (effectiveVfov <= 0.0) {
        effectiveVfov = 2.0 * rad2deg(std::atan(std::tan(deg2rad(hfovDeg / 2.0)) * (9.0 / 16.0)));
    }

    return Klv::KlvGeodesy::computeFrustum(
        platformPos, platformHeadingDeg, gimbalPanDeg, gimbalTiltDeg, hfovDeg, effectiveVfov, groundElevationM);
}

GimbalLookAngles GeoreferenceUtils::computeLookAnglesToTarget(
    const Klv::GeoPoint3D& platformPos, double platformHeadingDeg, const Klv::GeoPoint3D& targetPos) noexcept
{
    const Klv::GeoPoint2D pOrigin { platformPos.latitudeDeg, platformPos.longitudeDeg };
    const Klv::GeoPoint2D pTarget { targetPos.latitudeDeg, targetPos.longitudeDeg };

    const double bearing = Klv::KlvGeodesy::bearingDeg(pOrigin, pTarget);
    const double groundDist = Klv::KlvGeodesy::distanceMeters(pOrigin, pTarget);
    const double deltaAlt = targetPos.altitudeM - platformPos.altitudeM;

    const double slantRange = std::sqrt(groundDist * groundDist + deltaAlt * deltaAlt);

    double tiltDeg { 0.0 };
    if (slantRange > 1e-6) {
        tiltDeg = rad2deg(std::atan2(deltaAlt, groundDist));
    }

    const double relPan = normalizeAngle180(bearing - platformHeadingDeg);

    return GimbalLookAngles { relPan, tiltDeg, slantRange, bearing };
}

} // namespace PayloadHal
