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

std::optional<Klv::GeoPoint3D> GeoreferenceUtils::computeTargetFromDem(
    const IDemProvider& dem, const Klv::GeoPoint3D& platformPos, double platformHeadingDeg,
    double gimbalPanDeg, double gimbalTiltDeg, const DemRayConfig& config) noexcept
{
    const auto result = DemRayCaster::intersect(
        dem, platformPos, platformHeadingDeg, gimbalPanDeg, gimbalTiltDeg, config);
    if (!result) {
        return std::nullopt;
    }
    return result->targetPosition;
}

std::optional<Klv::FrustumCorners> GeoreferenceUtils::computeFrustumCorners(const Klv::GeoPoint3D& platformPos,
    double platformHeadingDeg, double gimbalPanDeg, double gimbalTiltDeg, double hfovDeg, double vfovDeg,
    double groundElevationM, double gimbalRollDeg) noexcept
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

    if (std::abs(gimbalRollDeg) < 1e-4) {
        return Klv::KlvGeodesy::computeFrustum(
            platformPos, platformHeadingDeg, gimbalPanDeg, gimbalTiltDeg, hfovDeg, effectiveVfov, groundElevationM);
    }

    const double deltaH = platformPos.altitudeM - groundElevationM;
    const double halfHfov = hfovDeg / 2.0;
    const double halfVfov = effectiveVfov / 2.0;
    const double centerAz = normalizeAngle360(platformHeadingDeg + gimbalPanDeg);

    const double radRoll = deg2rad(gimbalRollDeg);
    const double cosR = std::cos(radRoll);
    const double sinR = std::sin(radRoll);

    auto projectRay = [&](double dx, double dy) -> Klv::GeoPoint2D {
        const double rotDx = dx * cosR - dy * sinR;
        const double rotDy = dx * sinR + dy * cosR;
        const double rayAz = normalizeAngle360(centerAz + rotDx);
        const double rayEl = std::min(gimbalTiltDeg + rotDy, -0.05);
        const double depRad = deg2rad(-rayEl);
        const double dist = deltaH / std::tan(depRad);
        return Klv::KlvGeodesy::directGeodetic(
            Klv::GeoPoint2D { platformPos.latitudeDeg, platformPos.longitudeDeg }, rayAz, dist);
    };

    Klv::FrustumCorners corners;
    corners.topLeft = projectRay(-halfHfov, +halfVfov);
    corners.topRight = projectRay(+halfHfov, +halfVfov);
    corners.bottomRight = projectRay(+halfHfov, -halfVfov);
    corners.bottomLeft = projectRay(-halfHfov, -halfVfov);

    return corners;
}

std::optional<Klv::FrustumCorners> GeoreferenceUtils::computeFrustumCorners(
    const IDemProvider& dem, const Klv::GeoPoint3D& platformPos,
    double platformHeadingDeg, double gimbalPanDeg, double gimbalTiltDeg,
    double hfovDeg, double vfovDeg, double gimbalRollDeg, const DemRayConfig& config) noexcept
{
    if (hfovDeg <= 0.0 || gimbalTiltDeg >= -0.01) {
        return std::nullopt;
    }

    double effectiveVfov = vfovDeg;
    if (effectiveVfov <= 0.0) {
        effectiveVfov = 2.0 * rad2deg(std::atan(std::tan(deg2rad(hfovDeg / 2.0)) * (9.0 / 16.0)));
    }

    const double halfH = hfovDeg * 0.5;
    const double halfV = effectiveVfov * 0.5;

    const double radRoll = deg2rad(gimbalRollDeg);
    const double cosR = std::cos(radRoll);
    const double sinR = std::sin(radRoll);

    auto castCorner = [&](double dx, double dy) {
        const double rotDx = dx * cosR - dy * sinR;
        const double rotDy = dx * sinR + dy * cosR;
        return DemRayCaster::intersect(
            dem, platformPos, platformHeadingDeg, gimbalPanDeg + rotDx, gimbalTiltDeg + rotDy, config);
    };

    const auto tlRes = castCorner(-halfH, +halfV);
    const auto trRes = castCorner(+halfH, +halfV);
    const auto brRes = castCorner(+halfH, -halfV);
    const auto blRes = castCorner(-halfH, -halfV);

    if (!tlRes || !trRes || !brRes || !blRes) {
        return std::nullopt;
    }

    Klv::FrustumCorners corners;
    corners.topLeft = { tlRes->targetPosition.latitudeDeg, tlRes->targetPosition.longitudeDeg };
    corners.topRight = { trRes->targetPosition.latitudeDeg, trRes->targetPosition.longitudeDeg };
    corners.bottomRight = { brRes->targetPosition.latitudeDeg, brRes->targetPosition.longitudeDeg };
    corners.bottomLeft = { blRes->targetPosition.latitudeDeg, blRes->targetPosition.longitudeDeg };

    return corners;
}

double GeoreferenceUtils::computeLevelingRoll(double platformRollDeg, double platformPitchDeg,
    double gimbalPanDeg, double gimbalTiltDeg) noexcept
{
    // Near nadir / zenith (+/-90 deg), optical axis is collinear with gravity vector.
    // Horizon leveling is degenerate when looking straight down or straight up.
    if (std::abs(std::abs(gimbalTiltDeg) - 90.0) < 1.0) {
        return 0.0;
    }

    const double phiP = deg2rad(platformRollDeg);
    const double thetaP = deg2rad(platformPitchDeg);
    const double psiG = deg2rad(gimbalPanDeg);
    const double thetaG = deg2rad(gimbalTiltDeg);

    // Gimbal camera right axis v_cam_x and down axis v_cam_y in platform body frame:
    // With azimuth psiG (around Z) and elevation thetaG (around gimbal Y right axis):
    // v_cam_x = [-sin(psiG), cos(psiG), 0]
    // v_cam_y = [-sin(thetaG)*cos(psiG), -sin(thetaG)*sin(psiG), cos(thetaG)]
    const double vx_x = -std::sin(psiG);
    const double vx_y = std::cos(psiG);
    const double vx_z = 0.0;

    const double vy_x = -std::sin(thetaG) * std::cos(psiG);
    const double vy_y = -std::sin(thetaG) * std::sin(psiG);
    const double vy_z = std::cos(thetaG);

    // Transform Z component to NED frame via platform pitch (thetaP) and roll (phiP).
    // NED Z row of R_body^NED: [-sin(thetaP), cos(thetaP)*sin(phiP), cos(thetaP)*cos(phiP)]
    const double r31 = -std::sin(thetaP);
    const double r32 = std::cos(thetaP) * std::sin(phiP);
    const double r33 = std::cos(thetaP) * std::cos(phiP);

    const double x_ned_z = r31 * vx_x + r32 * vx_y + r33 * vx_z;
    const double y_ned_z = r31 * vy_x + r32 * vy_y + r33 * vy_z;

    if (std::abs(x_ned_z) < 1e-9 && std::abs(y_ned_z) < 1e-9) {
        return 0.0;
    }

    const double rollRad = std::atan2(-x_ned_z, y_ned_z);
    return rad2deg(rollRad);
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
