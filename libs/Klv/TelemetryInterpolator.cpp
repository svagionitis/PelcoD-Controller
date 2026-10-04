#include "TelemetryInterpolator.h"

#include <algorithm>
#include <cmath>

namespace Klv {

double TelemetryInterpolator::lerp(double v1, double v2, double factor) noexcept
{
    return v1 + factor * (v2 - v1);
}

double TelemetryInterpolator::interpolateAngle(double angle1Deg,
                                               double angle2Deg,
                                               double factor) noexcept
{
    const double diff = std::fmod(angle2Deg - angle1Deg + 540.0, 360.0) - 180.0;
    double res = angle1Deg + factor * diff;
    res = std::fmod(res, 360.0);
    if (res < 0.0) {
        res += 360.0;
    }
    return res;
}

double TelemetryInterpolator::interpolateLongitude(double lon1Deg,
                                                   double lon2Deg,
                                                   double factor) noexcept
{
    const double diff = std::fmod(lon2Deg - lon1Deg + 540.0, 360.0) - 180.0;
    double res = lon1Deg + factor * diff;
    while (res > 180.0) {
        res -= 360.0;
    }
    while (res < -180.0) {
        res += 360.0;
    }
    return res;
}

UasDatalinkMessage TelemetryInterpolator::interpolate(const UasDatalinkMessage& m1,
                                                      std::uint64_t pts1,
                                                      const UasDatalinkMessage& m2,
                                                      std::uint64_t pts2,
                                                      std::uint64_t targetPts) noexcept
{
    if (pts1 >= pts2 || targetPts <= pts1) {
        return m1;
    }
    if (targetPts >= pts2) {
        return m2;
    }

    const double factor = static_cast<double>(targetPts - pts1) / static_cast<double>(pts2 - pts1);
    const auto& discreteSrc = (factor < 0.5) ? m1 : m2;

    UasDatalinkMessage out = discreteSrc;

    // Precision timestamp
    if (m1.precisionTimeStampUs && m2.precisionTimeStampUs) {
        const double t1 = static_cast<double>(*m1.precisionTimeStampUs);
        const double t2 = static_cast<double>(*m2.precisionTimeStampUs);
        out.precisionTimeStampUs = static_cast<std::uint64_t>(std::llround(lerp(t1, t2, factor)));
    }

    // Platform attitude
    if (m1.platformHeadingDeg && m2.platformHeadingDeg) {
        out.platformHeadingDeg = interpolateAngle(*m1.platformHeadingDeg, *m2.platformHeadingDeg, factor);
    }
    if (m1.platformPitchDeg && m2.platformPitchDeg) {
        out.platformPitchDeg = lerp(*m1.platformPitchDeg, *m2.platformPitchDeg, factor);
    }
    if (m1.platformRollDeg && m2.platformRollDeg) {
        out.platformRollDeg = lerp(*m1.platformRollDeg, *m2.platformRollDeg, factor);
    }

    // Sensor geodetic position
    if (m1.sensorLatitudeDeg && m2.sensorLatitudeDeg) {
        out.sensorLatitudeDeg = lerp(*m1.sensorLatitudeDeg, *m2.sensorLatitudeDeg, factor);
    }
    if (m1.sensorLongitudeDeg && m2.sensorLongitudeDeg) {
        out.sensorLongitudeDeg = interpolateLongitude(*m1.sensorLongitudeDeg, *m2.sensorLongitudeDeg, factor);
    }
    if (m1.sensorTrueAltitudeM && m2.sensorTrueAltitudeM) {
        out.sensorTrueAltitudeM = lerp(*m1.sensorTrueAltitudeM, *m2.sensorTrueAltitudeM, factor);
    }
    if (m1.sensorAltitudeHaeM && m2.sensorAltitudeHaeM) {
        out.sensorAltitudeHaeM = lerp(*m1.sensorAltitudeHaeM, *m2.sensorAltitudeHaeM, factor);
    }

    // Optical parameters
    if (m1.sensorHfovDeg && m2.sensorHfovDeg) {
        out.sensorHfovDeg = lerp(*m1.sensorHfovDeg, *m2.sensorHfovDeg, factor);
    }
    if (m1.sensorVfovDeg && m2.sensorVfovDeg) {
        out.sensorVfovDeg = lerp(*m1.sensorVfovDeg, *m2.sensorVfovDeg, factor);
    }
    if (m1.sensorRelAzimuthDeg && m2.sensorRelAzimuthDeg) {
        out.sensorRelAzimuthDeg = interpolateAngle(*m1.sensorRelAzimuthDeg, *m2.sensorRelAzimuthDeg, factor);
    }
    if (m1.sensorRelElevationDeg && m2.sensorRelElevationDeg) {
        out.sensorRelElevationDeg = lerp(*m1.sensorRelElevationDeg, *m2.sensorRelElevationDeg, factor);
    }
    if (m1.sensorRelRollDeg && m2.sensorRelRollDeg) {
        out.sensorRelRollDeg = interpolateAngle(*m1.sensorRelRollDeg, *m2.sensorRelRollDeg, factor);
    }
    if (m1.sensorRollAngleDeg && m2.sensorRollAngleDeg) {
        out.sensorRollAngleDeg = interpolateAngle(*m1.sensorRollAngleDeg, *m2.sensorRollAngleDeg, factor);
    }

    // Slant range & target width
    if (m1.slantRangeM && m2.slantRangeM) {
        out.slantRangeM = lerp(*m1.slantRangeM, *m2.slantRangeM, factor);
    }
    if (m1.targetWidthM && m2.targetWidthM) {
        out.targetWidthM = lerp(*m1.targetWidthM, *m2.targetWidthM, factor);
    }

    // Frame center position
    if (m1.frameCenterLatDeg && m2.frameCenterLatDeg) {
        out.frameCenterLatDeg = lerp(*m1.frameCenterLatDeg, *m2.frameCenterLatDeg, factor);
    }
    if (m1.frameCenterLonDeg && m2.frameCenterLonDeg) {
        out.frameCenterLonDeg = interpolateLongitude(*m1.frameCenterLonDeg, *m2.frameCenterLonDeg, factor);
    }
    if (m1.frameCenterElevM && m2.frameCenterElevM) {
        out.frameCenterElevM = lerp(*m1.frameCenterElevM, *m2.frameCenterElevM, factor);
    }
    if (m1.frameCenterElevHaeM && m2.frameCenterElevHaeM) {
        out.frameCenterElevHaeM = lerp(*m1.frameCenterElevHaeM, *m2.frameCenterElevHaeM, factor);
    }

    // 4-corner ground footprint coordinates
    if (m1.cornerCoordinates && m2.cornerCoordinates) {
        FrustumCorners corners {};
        corners.topLeft.latitudeDeg = lerp(m1.cornerCoordinates->topLeft.latitudeDeg, m2.cornerCoordinates->topLeft.latitudeDeg, factor);
        corners.topLeft.longitudeDeg = interpolateLongitude(m1.cornerCoordinates->topLeft.longitudeDeg, m2.cornerCoordinates->topLeft.longitudeDeg, factor);
        corners.topRight.latitudeDeg = lerp(m1.cornerCoordinates->topRight.latitudeDeg, m2.cornerCoordinates->topRight.latitudeDeg, factor);
        corners.topRight.longitudeDeg = interpolateLongitude(m1.cornerCoordinates->topRight.longitudeDeg, m2.cornerCoordinates->topRight.longitudeDeg, factor);
        corners.bottomRight.latitudeDeg = lerp(m1.cornerCoordinates->bottomRight.latitudeDeg, m2.cornerCoordinates->bottomRight.latitudeDeg, factor);
        corners.bottomRight.longitudeDeg = interpolateLongitude(m1.cornerCoordinates->bottomRight.longitudeDeg, m2.cornerCoordinates->bottomRight.longitudeDeg, factor);
        corners.bottomLeft.latitudeDeg = lerp(m1.cornerCoordinates->bottomLeft.latitudeDeg, m2.cornerCoordinates->bottomLeft.latitudeDeg, factor);
        corners.bottomLeft.longitudeDeg = interpolateLongitude(m1.cornerCoordinates->bottomLeft.longitudeDeg, m2.cornerCoordinates->bottomLeft.longitudeDeg, factor);
        out.cornerCoordinates = corners;
    }

    return out;
}

} // namespace Klv
