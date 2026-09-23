#include "KlvGeodesy.h"
#include <algorithm>
#include <cmath>

namespace Klv {

namespace {

constexpr double kPi { 3.14159265358979323846 };

inline double deg2rad(double deg) noexcept {
    return deg * (kPi / 180.0);
}

inline double rad2deg(double rad) noexcept {
    return rad * (180.0 / kPi);
}

inline double normalizeAngle360(double deg) noexcept {
    double wrapped = std::fmod(deg, 360.0);
    if (wrapped < 0.0) {
        wrapped += 360.0;
    }
    return wrapped;
}

inline double normalizeLon(double lonDeg) noexcept {
    while (lonDeg > 180.0) lonDeg -= 360.0;
    while (lonDeg < -180.0) lonDeg += 360.0;
    return lonDeg;
}

} // namespace

GeoPoint2D KlvGeodesy::directGeodetic(const GeoPoint2D& origin,
                                      double bearingDeg,
                                      double distanceMeters) noexcept {
    if (distanceMeters <= 0.0) {
        return origin;
    }

    const double phi1 = deg2rad(origin.latitudeDeg);
    const double lambda1 = deg2rad(origin.longitudeDeg);
    const double theta = deg2rad(normalizeAngle360(bearingDeg));
    const double delta = distanceMeters / kEarthRadiusMeters;

    const double sinPhi1 = std::sin(phi1);
    const double cosPhi1 = std::cos(phi1);
    const double sinDelta = std::sin(delta);
    const double cosDelta = std::cos(delta);
    const double sinTheta = std::sin(theta);
    const double cosTheta = std::cos(theta);

    const double sinPhi2 = sinPhi1 * cosDelta + cosPhi1 * sinDelta * cosTheta;
    const double phi2 = std::asin(std::clamp(sinPhi2, -1.0, 1.0));

    const double y = sinTheta * sinDelta * cosPhi1;
    const double x = cosDelta - sinPhi1 * sinPhi2;
    const double lambda2 = lambda1 + std::atan2(y, x);

    return GeoPoint2D {
        rad2deg(phi2),
        normalizeLon(rad2deg(lambda2))
    };
}

double KlvGeodesy::distanceMeters(const GeoPoint2D& p1, const GeoPoint2D& p2) noexcept {
    const double phi1 = deg2rad(p1.latitudeDeg);
    const double phi2 = deg2rad(p2.latitudeDeg);
    const double deltaPhi = deg2rad(p2.latitudeDeg - p1.latitudeDeg);
    const double deltaLambda = deg2rad(p2.longitudeDeg - p1.longitudeDeg);

    const double a = std::sin(deltaPhi / 2.0) * std::sin(deltaPhi / 2.0) +
                     std::cos(phi1) * std::cos(phi2) *
                     std::sin(deltaLambda / 2.0) * std::sin(deltaLambda / 2.0);

    const double c = 2.0 * std::atan2(std::sqrt(std::clamp(a, 0.0, 1.0)),
                                      std::sqrt(std::clamp(1.0 - a, 0.0, 1.0)));

    return kEarthRadiusMeters * c;
}

double KlvGeodesy::bearingDeg(const GeoPoint2D& from, const GeoPoint2D& to) noexcept {
    const double phi1 = deg2rad(from.latitudeDeg);
    const double phi2 = deg2rad(to.latitudeDeg);
    const double deltaLambda = deg2rad(to.longitudeDeg - from.longitudeDeg);

    const double y = std::sin(deltaLambda) * std::cos(phi2);
    const double x = std::cos(phi1) * std::sin(phi2) -
                     std::sin(phi1) * std::cos(phi2) * std::cos(deltaLambda);

    return normalizeAngle360(rad2deg(std::atan2(y, x)));
}

std::optional<double> KlvGeodesy::computeSlantRange(double platformAltitudeM,
                                                    double sensorElevationDeg,
                                                    double groundElevationM) noexcept {
    const double deltaH = platformAltitudeM - groundElevationM;
    if (deltaH <= 0.0 || sensorElevationDeg >= -0.01) {
        return std::nullopt;
    }

    const double depressionRad = deg2rad(-sensorElevationDeg);
    const double sinDep = std::sin(depressionRad);
    if (sinDep <= 1e-6) {
        return std::nullopt;
    }

    return deltaH / sinDep;
}

std::optional<GeoPoint2D> KlvGeodesy::computeFrameCenter(const GeoPoint3D& platformPos,
                                                         double platformHeadingDeg,
                                                         double sensorAzimuthDeg,
                                                         double sensorElevationDeg,
                                                         double groundElevationM) noexcept {
    const double deltaH = platformPos.altitudeM - groundElevationM;
    if (deltaH <= 0.0 || sensorElevationDeg >= -0.01) {
        return std::nullopt;
    }

    const double depressionRad = deg2rad(-sensorElevationDeg);
    const double tanDep = std::tan(depressionRad);
    if (tanDep <= 1e-6) {
        return std::nullopt;
    }

    const double groundDist = deltaH / tanDep;
    const double worldBearing = normalizeAngle360(platformHeadingDeg + sensorAzimuthDeg);

    return directGeodetic(GeoPoint2D { platformPos.latitudeDeg, platformPos.longitudeDeg },
                          worldBearing, groundDist);
}

std::optional<FrustumCorners> KlvGeodesy::computeFrustum(const GeoPoint3D& platformPos,
                                                         double platformHeadingDeg,
                                                         double sensorAzimuthDeg,
                                                         double sensorElevationDeg,
                                                         double hfovDeg,
                                                         double vfovDeg,
                                                         double groundElevationM) noexcept {
    const double deltaH = platformPos.altitudeM - groundElevationM;
    if (deltaH <= 0.0) {
        return std::nullopt;
    }

    const double halfHfov = hfovDeg / 2.0;
    const double halfVfov = vfovDeg / 2.0;
    const double centerAz = normalizeAngle360(platformHeadingDeg + sensorAzimuthDeg);

    auto projectRay = [&](double azOffset, double elOffset) -> GeoPoint2D {
        const double rayAz = normalizeAngle360(centerAz + azOffset);
        // Clamp elevation to slightly below horizon (-0.05 deg) if looking above/at horizon
        const double rayEl = std::min(sensorElevationDeg + elOffset, -0.05);
        const double depRad = deg2rad(-rayEl);
        const double dist = deltaH / std::tan(depRad);
        return directGeodetic(GeoPoint2D { platformPos.latitudeDeg, platformPos.longitudeDeg }, rayAz, dist);
    };

    FrustumCorners corners;
    corners.topLeft = projectRay(-halfHfov, +halfVfov);
    corners.topRight = projectRay(+halfHfov, +halfVfov);
    corners.bottomRight = projectRay(+halfHfov, -halfVfov);
    corners.bottomLeft = projectRay(-halfHfov, -halfVfov);

    return corners;
}

} // namespace Klv
