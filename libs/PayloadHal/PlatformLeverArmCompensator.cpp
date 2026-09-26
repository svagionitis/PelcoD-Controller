/// @file PlatformLeverArmCompensator.cpp
/// @brief Implementation of rigid-body platform lever arm coordinate transformations.

#include "PlatformLeverArmCompensator.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kWgs84A = 6378137.0;          // WGS-84 semi-major axis (meters)
constexpr double kWgs84E2 = 0.00669437999014;  // WGS-84 first eccentricity squared

double degToRad(double deg)
{
    return deg * (kPi / 180.0);
}

double radToDeg(double rad)
{
    return rad * (180.0 / kPi);
}

double normalizeAngleDeg(double angle)
{
    while (angle > 180.0) {
        angle -= 360.0;
    }
    while (angle < -180.0) {
        angle += 360.0;
    }
    return angle;
}

struct Matrix3x3 {
    double m[3][3] { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };

    Vector3D multiply(const Vector3D& v) const
    {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        };
    }

    Matrix3x3 multiply(const Matrix3x3& b) const
    {
        Matrix3x3 res {};
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                res.m[r][c] = m[r][0] * b.m[0][c] + m[r][1] * b.m[1][c] + m[r][2] * b.m[2][c];
            }
        }
        return res;
    }

    Matrix3x3 transpose() const
    {
        Matrix3x3 res {};
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                res.m[r][c] = m[c][r];
            }
        }
        return res;
    }
};

Matrix3x3 eulerZyxToMatrix(double yawDeg, double pitchDeg, double rollDeg)
{
    const double psi = degToRad(yawDeg);
    const double theta = degToRad(pitchDeg);
    const double phi = degToRad(rollDeg);

    const double cPsi = std::cos(psi);
    const double sPsi = std::sin(psi);
    const double cTheta = std::cos(theta);
    const double sTheta = std::sin(theta);
    const double cPhi = std::cos(phi);
    const double sPhi = std::sin(phi);

    Matrix3x3 r {};
    r.m[0][0] = cPsi * cTheta;
    r.m[0][1] = cPsi * sTheta * sPhi - sPsi * cPhi;
    r.m[0][2] = cPsi * sTheta * cPhi + sPsi * sPhi;

    r.m[1][0] = sPsi * cTheta;
    r.m[1][1] = sPsi * sTheta * sPhi + cPsi * cPhi;
    r.m[1][2] = sPsi * sTheta * cPhi - cPsi * sPhi;

    r.m[2][0] = -sTheta;
    r.m[2][1] = cTheta * sPhi;
    r.m[2][2] = cTheta * cPhi;

    return r;
}

Matrix3x3 getMountingMatrix(const PlatformLeverArmConfig& config)
{
    switch (config.mountingType) {
    case GimbalMountingType::Inverted: {
        // 180° rotation about X-axis (roll)
        Matrix3x3 r {};
        r.m[0][0] = 1.0;
        r.m[0][1] = 0.0;
        r.m[0][2] = 0.0;
        r.m[1][0] = 0.0;
        r.m[1][1] = -1.0;
        r.m[1][2] = 0.0;
        r.m[2][0] = 0.0;
        r.m[2][1] = 0.0;
        r.m[2][2] = -1.0;
        return r;
    }
    case GimbalMountingType::Custom:
        return eulerZyxToMatrix(
            config.mountOrientation.yawDeg,
            config.mountOrientation.pitchDeg,
            config.mountOrientation.rollDeg);
    case GimbalMountingType::Upright:
    default: {
        Matrix3x3 ident {};
        return ident;
    }
    }
}

void getWgs84Radii(double latDeg, double& outRn, double& outRm)
{
    const double latRad = degToRad(latDeg);
    const double sinLat = std::sin(latRad);
    const double w = std::sqrt(1.0 - kWgs84E2 * sinLat * sinLat);
    outRn = kWgs84A / w;
    outRm = kWgs84A * (1.0 - kWgs84E2) / (w * w * w);
}

Klv::GeoPoint3D nedToGeodetic(const Klv::GeoPoint3D& refGps, const Vector3D& ned)
{
    double rn = 0.0;
    double rm = 0.0;
    getWgs84Radii(refGps.latitudeDeg, rn, rm);

    const double latRad = degToRad(refGps.latitudeDeg);
    const double cosLat = std::max(1e-6, std::cos(latRad));

    const double deltaLatRad = ned.x / (rm + refGps.altitudeM);
    const double deltaLonRad = ned.y / ((rn + refGps.altitudeM) * cosLat);

    Klv::GeoPoint3D geo {};
    geo.latitudeDeg = refGps.latitudeDeg + radToDeg(deltaLatRad);
    geo.longitudeDeg = refGps.longitudeDeg + radToDeg(deltaLonRad);
    geo.altitudeM = refGps.altitudeM - ned.z; // NED Z is down

    return geo;
}

Vector3D geodeticToNed(const Klv::GeoPoint3D& refGps, const Klv::GeoPoint3D& targetGeo)
{
    double rn = 0.0;
    double rm = 0.0;
    getWgs84Radii(refGps.latitudeDeg, rn, rm);

    const double latRad = degToRad(refGps.latitudeDeg);
    const double cosLat = std::max(1e-6, std::cos(latRad));

    const double deltaLatRad = degToRad(targetGeo.latitudeDeg - refGps.latitudeDeg);
    const double deltaLonRad = degToRad(targetGeo.longitudeDeg - refGps.longitudeDeg);

    Vector3D ned {};
    ned.x = deltaLatRad * (rm + refGps.altitudeM);
    ned.y = deltaLonRad * ((rn + refGps.altitudeM) * cosLat);
    ned.z = -(targetGeo.altitudeM - refGps.altitudeM);

    return ned;
}

} // namespace

PlatformLeverArmCompensator::PlatformLeverArmCompensator(const PlatformLeverArmConfig& config)
    : m_config(config)
{
}

void PlatformLeverArmCompensator::setConfig(const PlatformLeverArmConfig& config) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

PlatformLeverArmConfig PlatformLeverArmCompensator::config() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

Vector3D PlatformLeverArmCompensator::computeSensorPositionNed(
    const PlatformPose& pose, double panAngleDeg, double tiltAngleDeg) const
{
    (void)panAngleDeg;
    (void)tiltAngleDeg;

    PlatformLeverArmConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cfg = m_config;
    }

    const auto rBodyToNed = eulerZyxToMatrix(pose.headingDeg, pose.pitchDeg, pose.rollDeg);
    const auto rMount = getMountingMatrix(cfg);
    const auto rGimbalToNed = rBodyToNed.multiply(rMount);

    // Gimbal base offset in NED
    const auto gimbalBaseNed = rBodyToNed.multiply(cfg.gpsToGimbalBodyM);

    // Sensor nodal point offset in NED
    const auto sensorOffsetNed = rGimbalToNed.multiply(cfg.gimbalToSensorM);

    return Vector3D {
        gimbalBaseNed.x + sensorOffsetNed.x,
        gimbalBaseNed.y + sensorOffsetNed.y,
        gimbalBaseNed.z + sensorOffsetNed.z
    };
}

Klv::GeoPoint3D PlatformLeverArmCompensator::computeSensorPosition(
    const PlatformPose& pose, double panAngleDeg, double tiltAngleDeg) const
{
    const auto totalSensorNed = computeSensorPositionNed(pose, panAngleDeg, tiltAngleDeg);
    return nedToGeodetic(pose.gpsPosition, totalSensorNed);
}

Vector3D PlatformLeverArmCompensator::computeLineOfSightNed(
    const PlatformPose& pose, double panAngleDeg, double tiltAngleDeg) const
{
    PlatformLeverArmConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cfg = m_config;
    }

    const auto rBodyToNed = eulerZyxToMatrix(pose.headingDeg, pose.pitchDeg, pose.rollDeg);
    const auto rMount = getMountingMatrix(cfg);
    const auto rGimbalToNed = rBodyToNed.multiply(rMount);

    // Unit line of sight in gimbal frame
    const double azRad = degToRad(panAngleDeg);
    const double elRad = degToRad(tiltAngleDeg);

    Vector3D losGimbal {
        std::cos(elRad) * std::cos(azRad),
        std::cos(elRad) * std::sin(azRad),
        -std::sin(elRad) // In NED Z is down, positive elevation is up (-Z)
    };

    return rGimbalToNed.multiply(losGimbal);
}

std::optional<Klv::GeoPoint3D> PlatformLeverArmCompensator::computeTargetFromSlantRange(
    const PlatformPose& pose, double panAngleDeg, double tiltAngleDeg, double slantRangeMeters) const
{
    if (slantRangeMeters <= 0.0) {
        return std::nullopt;
    }

    PlatformLeverArmConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cfg = m_config;
    }

    const auto rBodyToNed = eulerZyxToMatrix(pose.headingDeg, pose.pitchDeg, pose.rollDeg);
    const auto rMount = getMountingMatrix(cfg);
    const auto rGimbalToNed = rBodyToNed.multiply(rMount);

    // Total sensor position in NED
    const auto gimbalBaseNed = rBodyToNed.multiply(cfg.gpsToGimbalBodyM);
    const auto sensorOffsetNed = rGimbalToNed.multiply(cfg.gimbalToSensorM);

    const Vector3D sensorNed {
        gimbalBaseNed.x + sensorOffsetNed.x,
        gimbalBaseNed.y + sensorOffsetNed.y,
        gimbalBaseNed.z + sensorOffsetNed.z
    };

    // Line of sight in NED
    const auto losNed = computeLineOfSightNed(pose, panAngleDeg, tiltAngleDeg);

    const Vector3D targetNed {
        sensorNed.x + slantRangeMeters * losNed.x,
        sensorNed.y + slantRangeMeters * losNed.y,
        sensorNed.z + slantRangeMeters * losNed.z
    };

    return nedToGeodetic(pose.gpsPosition, targetNed);
}

std::optional<Klv::GeoPoint3D> PlatformLeverArmCompensator::computeTargetFromGroundIntersection(
    const PlatformPose& pose, double panAngleDeg, double tiltAngleDeg, double groundElevationM) const
{
    PlatformLeverArmConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cfg = m_config;
    }

    const auto rBodyToNed = eulerZyxToMatrix(pose.headingDeg, pose.pitchDeg, pose.rollDeg);
    const auto rMount = getMountingMatrix(cfg);
    const auto rGimbalToNed = rBodyToNed.multiply(rMount);

    const auto gimbalBaseNed = rBodyToNed.multiply(cfg.gpsToGimbalBodyM);
    const auto sensorOffsetNed = rGimbalToNed.multiply(cfg.gimbalToSensorM);

    const Vector3D sensorNed {
        gimbalBaseNed.x + sensorOffsetNed.x,
        gimbalBaseNed.y + sensorOffsetNed.y,
        gimbalBaseNed.z + sensorOffsetNed.z
    };

    const auto losNed = computeLineOfSightNed(pose, panAngleDeg, tiltAngleDeg);

    // Target ground elevation in NED relative to GPS antenna:
    // targetNed.z = -(groundElevationM - pose.gpsPosition.altitudeM)
    const double groundZ = -(groundElevationM - pose.gpsPosition.altitudeM);

    const double deltaZ = groundZ - sensorNed.z;
    if (deltaZ <= 0.0 || losNed.z <= 1e-6) {
        // Sensor is below/at ground, or ray points upward/parallel to ground
        return std::nullopt;
    }

    const double slantRangeMeters = deltaZ / losNed.z;

    const Vector3D targetNed {
        sensorNed.x + slantRangeMeters * losNed.x,
        sensorNed.y + slantRangeMeters * losNed.y,
        sensorNed.z + slantRangeMeters * losNed.z
    };

    return nedToGeodetic(pose.gpsPosition, targetNed);
}

GimbalLookAngles PlatformLeverArmCompensator::computeLookAnglesToTarget(
    const PlatformPose& pose, const Klv::GeoPoint3D& targetPos) const
{
    PlatformLeverArmConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cfg = m_config;
    }

    const auto rBodyToNed = eulerZyxToMatrix(pose.headingDeg, pose.pitchDeg, pose.rollDeg);
    const auto rMount = getMountingMatrix(cfg);
    const auto rGimbalToNed = rBodyToNed.multiply(rMount);

    // Sensor position in NED
    const auto gimbalBaseNed = rBodyToNed.multiply(cfg.gpsToGimbalBodyM);
    const auto sensorOffsetNed = rGimbalToNed.multiply(cfg.gimbalToSensorM);

    const Vector3D sensorNed {
        gimbalBaseNed.x + sensorOffsetNed.x,
        gimbalBaseNed.y + sensorOffsetNed.y,
        gimbalBaseNed.z + sensorOffsetNed.z
    };

    // Target in NED relative to GPS antenna
    const auto targetNed = geodeticToNed(pose.gpsPosition, targetPos);

    // Vector from true sensor position to target in NED
    const Vector3D deltaNed {
        targetNed.x - sensorNed.x,
        targetNed.y - sensorNed.y,
        targetNed.z - sensorNed.z
    };

    // Transform into Gimbal Frame: rGimbal = (rGimbalToNed)^T * deltaNed
    const auto rNedToGimbal = rGimbalToNed.transpose();
    const auto rGimbal = rNedToGimbal.multiply(deltaNed);

    const double xyDist = std::sqrt(rGimbal.x * rGimbal.x + rGimbal.y * rGimbal.y);

    GimbalLookAngles angles {};
    angles.panAngleDeg = normalizeAngleDeg(radToDeg(std::atan2(rGimbal.y, rGimbal.x)));
    angles.tiltAngleDeg = radToDeg(std::atan2(-rGimbal.z, std::max(1e-6, xyDist)));
    angles.slantRangeMeters = std::sqrt(deltaNed.x * deltaNed.x + deltaNed.y * deltaNed.y + deltaNed.z * deltaNed.z);
    angles.trueBearingDeg = normalizeAngleDeg(radToDeg(std::atan2(deltaNed.y, deltaNed.x)));

    return angles;
}

} // namespace PayloadHal
