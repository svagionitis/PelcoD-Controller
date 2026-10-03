#include "KlvGeodesy.h"

#include <algorithm>
#include <array>
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

struct Matrix3x3 {
    std::array<std::array<double, 3>, 3> m {};

    [[nodiscard]] static Matrix3x3 identity() noexcept {
        Matrix3x3 r {};
        r.m[0][0] = 1.0;
        r.m[1][1] = 1.0;
        r.m[2][2] = 1.0;
        return r;
    }

    [[nodiscard]] static Matrix3x3 rotX(double rad) noexcept {
        const double c = std::cos(rad);
        const double s = std::sin(rad);
        Matrix3x3 r {};
        r.m[0][0] = 1.0;
        r.m[1][1] = c;  r.m[1][2] = s;
        r.m[2][1] = -s; r.m[2][2] = c;
        return r;
    }

    [[nodiscard]] static Matrix3x3 rotY(double rad) noexcept {
        const double c = std::cos(rad);
        const double s = std::sin(rad);
        Matrix3x3 r {};
        r.m[0][0] = c;  r.m[0][2] = -s;
        r.m[1][1] = 1.0;
        r.m[2][0] = s;  r.m[2][2] = c;
        return r;
    }

    [[nodiscard]] static Matrix3x3 rotZ(double rad) noexcept {
        const double c = std::cos(rad);
        const double s = std::sin(rad);
        Matrix3x3 r {};
        r.m[0][0] = c;  r.m[0][1] = s;
        r.m[1][0] = -s; r.m[1][1] = c;
        r.m[2][2] = 1.0;
        return r;
    }

    [[nodiscard]] Matrix3x3 transpose() const noexcept {
        Matrix3x3 t {};
        for (std::size_t i = 0; i < 3; ++i) {
            for (std::size_t j = 0; j < 3; ++j) {
                t.m[i][j] = m[j][i];
            }
        }
        return t;
    }

    [[nodiscard]] Matrix3x3 operator*(const Matrix3x3& o) const noexcept {
        Matrix3x3 r {};
        for (std::size_t i = 0; i < 3; ++i) {
            for (std::size_t j = 0; j < 3; ++j) {
                double sum = 0.0;
                for (std::size_t k = 0; k < 3; ++k) {
                    sum += m[i][k] * o.m[k][j];
                }
                r.m[i][j] = sum;
            }
        }
        return r;
    }

    [[nodiscard]] Vector3D multVec(const Vector3D& v) const noexcept {
        return Vector3D {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        };
    }
};

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

double KlvGeodesy::horizonDistance(double altitudeM, double groundElevationM) noexcept {
    const double deltaH = std::max(0.0, altitudeM - groundElevationM);
    return std::sqrt(2.0 * kEarthRadiusMeters * deltaH + (deltaH * deltaH));
}

Vector3D KlvGeodesy::computeRayNed(const PlatformAttitude& attitude,
                                   const CameraOrientation& sensor,
                                   double opticalOffsetX,
                                   double opticalOffsetY) noexcept {
    // 1. Platform rotation: NED to Platform Coordinate System (PCS)
    // Yaw psi around Z (Heading), Pitch theta around Y, Roll phi around X
    const double psi = deg2rad(attitude.headingDeg);
    const double theta = deg2rad(attitude.pitchDeg);
    const double phi = deg2rad(attitude.rollDeg);

    const Matrix3x3 rNedToPcs = Matrix3x3::rotX(phi) * Matrix3x3::rotY(theta) * Matrix3x3::rotZ(psi);
    const Matrix3x3 rPcsToNed = rNedToPcs.transpose();

    // 2. Sensor gimbal rotation: PCS to Sensor Coordinate System (SCS)
    // Azimuth alpha around Z, Elevation beta around Y, Optical Roll gamma around X
    const double alpha = deg2rad(sensor.azimuthDeg);
    const double beta = deg2rad(sensor.elevationDeg);
    const double gamma = deg2rad(sensor.rollDeg);

    const Matrix3x3 rPcsToScs = Matrix3x3::rotX(gamma) * Matrix3x3::rotY(beta) * Matrix3x3::rotZ(alpha);
    const Matrix3x3 rScsToPcs = rPcsToScs.transpose();

    // 3. Combined rotation from SCS to NED
    const Matrix3x3 rScsToNed = rPcsToNed * rScsToPcs;

    // 4. Optical ray in camera SCS: forward is +X, right is +Y, down is +Z
    const double norm = std::sqrt(1.0 + (opticalOffsetX * opticalOffsetX) + (opticalOffsetY * opticalOffsetY));
    const Vector3D rayScs { 1.0 / norm, opticalOffsetX / norm, opticalOffsetY / norm };

    return rScsToNed.multVec(rayScs);
}

std::optional<GeoPoint2D> KlvGeodesy::intersectRayEarth(const GeoPoint3D& platformPos,
                                                         const Vector3D& rayNed,
                                                         double groundElevationM,
                                                         bool clipToHorizon) noexcept {
    const double deltaH = platformPos.altitudeM - groundElevationM;
    if (deltaH <= 0.0) {
        return std::nullopt;
    }

    const double rPlatform = kEarthRadiusMeters + platformPos.altitudeM;
    const double rGround = kEarthRadiusMeters + groundElevationM;

    // Ray equation from platform position in local vertical coordinates:
    // r(s) = [0, 0, -rPlatform] + s * [uN, uE, uD]
    // |r(s)|^2 = rGround^2
    // s^2 - 2 * rPlatform * uD * s + (rPlatform^2 - rGround^2) = 0
    const double cVal = deltaH * (rPlatform + rGround);
    const double bVal = -2.0 * rPlatform * rayNed.z;
    const double disc = (bVal * bVal) - (4.0 * cVal);

    const double hDist = horizonDistance(platformPos.altitudeM, groundElevationM);
    const double rayBearing = normalizeAngle360(rad2deg(std::atan2(rayNed.y, rayNed.x)));
    const GeoPoint2D origin { platformPos.latitudeDeg, platformPos.longitudeDeg };

    // If pointing above horizon (disc < 0 or pointing up/away rayNed.z <= 0)
    if (disc < 0.0 || rayNed.z <= 0.0) {
        if (clipToHorizon) {
            return directGeodetic(origin, rayBearing, hDist);
        }
        return std::nullopt;
    }

    // Near intersection (entry point onto the spherical Earth)
    const double slantRange = (rPlatform * rayNed.z) - std::sqrt(std::max(0.0, ((rPlatform * rayNed.z) * (rPlatform * rayNed.z)) - cVal));
    if (slantRange <= 0.0) {
        return std::nullopt;
    }

    // Ground travel distance:
    const double groundPlaneDist = slantRange * std::sqrt((rayNed.x * rayNed.x) + (rayNed.y * rayNed.y));
    const double surfaceDist = 2.0 * kEarthRadiusMeters * std::asin(std::clamp(groundPlaneDist / (2.0 * kEarthRadiusMeters), 0.0, 1.0));

    return directGeodetic(origin, rayBearing, surfaceDist);
}

std::optional<GeoPoint2D> KlvGeodesy::computeFrameCenter(const GeoPoint3D& platformPos,
                                                         const PlatformAttitude& attitude,
                                                         const CameraOrientation& sensor,
                                                         double groundElevationM) noexcept {
    const Vector3D ray = computeRayNed(attitude, sensor, 0.0, 0.0);
    return intersectRayEarth(platformPos, ray, groundElevationM, false);
}

std::optional<GeoPoint2D> KlvGeodesy::computeFrameCenter(const GeoPoint3D& platformPos,
                                                         double platformHeadingDeg,
                                                         double sensorAzimuthDeg,
                                                         double sensorElevationDeg,
                                                         double groundElevationM) noexcept {
    const PlatformAttitude att { platformHeadingDeg, 0.0, 0.0 };
    const CameraOrientation cam { sensorAzimuthDeg, sensorElevationDeg, 0.0 };
    return computeFrameCenter(platformPos, att, cam, groundElevationM);
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

std::optional<FrustumCorners> KlvGeodesy::computeFrustum(const GeoPoint3D& platformPos,
                                                         const PlatformAttitude& attitude,
                                                         const CameraOrientation& sensor,
                                                         double hfovDeg,
                                                         double vfovDeg,
                                                         double groundElevationM) noexcept {
    const double deltaH = platformPos.altitudeM - groundElevationM;
    if (deltaH <= 0.0) {
        return std::nullopt;
    }

    const double tanH = std::tan(deg2rad(hfovDeg / 2.0));
    const double tanV = std::tan(deg2rad(vfovDeg / 2.0));

    // Optical frame offsets:
    // Top-Left: -X_img, -Y_img
    const Vector3D rayTl = computeRayNed(attitude, sensor, -tanH, -tanV);
    const Vector3D rayTr = computeRayNed(attitude, sensor, +tanH, -tanV);
    const Vector3D rayBr = computeRayNed(attitude, sensor, +tanH, +tanV);
    const Vector3D rayBl = computeRayNed(attitude, sensor, -tanH, +tanV);

    const auto tl = intersectRayEarth(platformPos, rayTl, groundElevationM, true);
    const auto tr = intersectRayEarth(platformPos, rayTr, groundElevationM, true);
    const auto br = intersectRayEarth(platformPos, rayBr, groundElevationM, true);
    const auto bl = intersectRayEarth(platformPos, rayBl, groundElevationM, true);

    if (!tl || !tr || !br || !bl) {
        return std::nullopt;
    }

    FrustumCorners corners;
    corners.topLeft = *tl;
    corners.topRight = *tr;
    corners.bottomRight = *br;
    corners.bottomLeft = *bl;

    return corners;
}

std::optional<FrustumCorners> KlvGeodesy::computeFrustum(const GeoPoint3D& platformPos,
                                                         double platformHeadingDeg,
                                                         double sensorAzimuthDeg,
                                                         double sensorElevationDeg,
                                                         double hfovDeg,
                                                         double vfovDeg,
                                                         double groundElevationM) noexcept {
    const PlatformAttitude att { platformHeadingDeg, 0.0, 0.0 };
    const CameraOrientation cam { sensorAzimuthDeg, sensorElevationDeg, 0.0 };
    return computeFrustum(platformPos, att, cam, hfovDeg, vfovDeg, groundElevationM);
}

} // namespace Klv
