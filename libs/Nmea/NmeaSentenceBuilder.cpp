/// @file NmeaSentenceBuilder.cpp
/// @brief Implementation of NMEA 0183 sentence builder.

#include "NmeaSentenceBuilder.h"

#include <cmath>
#include <cstdio>
#include <sstream>

namespace Nmea {

void NmeaSentenceBuilder::formatCoordinate(double deg, bool isLatitude, std::string& outCoord, char& outHemi)
{
    if (isLatitude) {
        outHemi = (deg >= 0.0) ? 'N' : 'S';
    } else {
        outHemi = (deg >= 0.0) ? 'E' : 'W';
    }

    const double absDeg { std::abs(deg) };
    const int wholeDegrees { static_cast<int>(std::floor(absDeg)) };
    const double minutes { (absDeg - static_cast<double>(wholeDegrees)) * 60.0 };

    char buf[32] {};
    if (isLatitude) {
        // ddmm.mmmm (2 digits degrees)
        std::snprintf(buf, sizeof(buf), "%02d%07.4f", wholeDegrees, minutes);
    } else {
        // dddmm.mmmm (3 digits degrees)
        std::snprintf(buf, sizeof(buf), "%03d%07.4f", wholeDegrees, minutes);
    }
    outCoord = buf;
}

std::string NmeaSentenceBuilder::buildHdt(double headingDeg, std::string_view talkerId)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "%.*sHDT,%.1f,T", static_cast<int>(talkerId.size()), talkerId.data(), headingDeg);
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildThs(double headingDeg, NmeaFaaMode mode, std::string_view talkerId)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "%.*sTHS,%.1f,%c", static_cast<int>(talkerId.size()), talkerId.data(), headingDeg,
        static_cast<char>(mode));
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildGga(const GgaData& data, std::string_view talkerId)
{
    std::string latStr {};
    std::string lonStr {};
    char latHemi { 'N' };
    char lonHemi { 'E' };

    formatCoordinate(data.coordinates.latitudeDeg, true, latStr, latHemi);
    formatCoordinate(data.coordinates.longitudeDeg, false, lonStr, lonHemi);

    char buf[160] {};
    std::snprintf(buf, sizeof(buf), "%.*sGGA,%02u%02u%02u.%02u,%s,%c,%s,%c,%u,%02u,%.1f,%.1f,M,%.1f,M,%.1f,%04u",
        static_cast<int>(talkerId.size()), talkerId.data(), data.utcTime.hour, data.utcTime.minute, data.utcTime.second,
        data.utcTime.millisecond / 10U, latStr.c_str(), latHemi, lonStr.c_str(), lonHemi,
        static_cast<unsigned int>(data.fixQuality), static_cast<unsigned int>(data.numSatellites), data.hdop,
        data.altitudeMeters, data.geoidalSeparationMeters, data.dgpsAgeSeconds, data.dgpsStationId);

    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildRmc(const RmcData& data, std::string_view talkerId)
{
    std::string latStr {};
    std::string lonStr {};
    char latHemi { 'N' };
    char lonHemi { 'E' };

    formatCoordinate(data.coordinates.latitudeDeg, true, latStr, latHemi);
    formatCoordinate(data.coordinates.longitudeDeg, false, lonStr, lonHemi);

    const char statusChar = data.statusActive ? 'A' : 'V';
    const double absMag = std::abs(data.magneticVariationDegrees);
    const char magHemi = (data.magneticVariationDegrees >= 0.0) ? 'E' : 'W';
    const unsigned int yy = static_cast<unsigned int>(data.date.year % 100U);

    char buf[160] {};
    std::snprintf(buf, sizeof(buf), "%.*sRMC,%02u%02u%02u.%02u,%c,%s,%c,%s,%c,%.1f,%.1f,%02u%02u%02u,%.1f,%c,%c",
        static_cast<int>(talkerId.size()), talkerId.data(), data.utcTime.hour, data.utcTime.minute, data.utcTime.second,
        data.utcTime.millisecond / 10U, statusChar, latStr.c_str(), latHemi, lonStr.c_str(), lonHemi,
        data.speedOverGroundKnots, data.courseOverGroundDegrees, data.date.day, data.date.month, yy, absMag, magHemi,
        static_cast<char>(data.faaMode));

    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildTtm(const TtmData& data, std::string_view talkerId)
{
    const char bearingRef = (data.bearingReference == TtmReference::Relative) ? 'R' : 'T';
    const char courseRef = (data.courseReference == TtmReference::Relative) ? 'R' : 'T';
    const char refTarget = data.referenceTarget ? 'R' : 'T';

    char buf[160] {};
    std::snprintf(buf, sizeof(buf), "%.*sTTM,%02u,%.2f,%.1f,%c,%.1f,%.1f,%c,%.2f,%.1f,%c,%s,%c,%c,%02u%02u%02u.%02u,%c",
        static_cast<int>(talkerId.size()), talkerId.data(), data.targetNumber, data.targetDistanceNmi,
        data.bearingDegrees, bearingRef, data.targetSpeedKnots, data.targetCourseDegrees, courseRef,
        data.distanceCpaNmi, data.timeCpaMinutes, data.speedDistanceUnits, data.targetName.c_str(),
        static_cast<char>(data.status), refTarget, data.utcTimeTag.hour, data.utcTimeTag.minute, data.utcTimeTag.second,
        data.utcTimeTag.millisecond / 10U, data.acquisitionType);

    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildTll(const TllData& data, std::string_view talkerId)
{
    std::string latStr {};
    std::string lonStr {};
    char latHemi { 'N' };
    char lonHemi { 'E' };

    formatCoordinate(data.coordinates.latitudeDeg, true, latStr, latHemi);
    formatCoordinate(data.coordinates.longitudeDeg, false, lonStr, lonHemi);

    const char refTarget = data.referenceTarget ? 'R' : 'T';

    char buf[160] {};
    std::snprintf(buf, sizeof(buf), "%.*sTLL,%02u,%s,%c,%s,%c,%s,%02u%02u%02u.%02u,%c,%c",
        static_cast<int>(talkerId.size()), talkerId.data(), data.targetNumber, latStr.c_str(), latHemi, lonStr.c_str(),
        lonHemi, data.targetName.c_str(), data.utcTimeTag.hour, data.utcTimeTag.minute, data.utcTimeTag.second,
        data.utcTimeTag.millisecond / 10U, static_cast<char>(data.status), refTarget);

    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildXdrPitchRoll(double pitchDeg, double rollDeg, std::string_view talkerId)
{
    char buf[128] {};
    std::snprintf(buf, sizeof(buf), "%.*sXDR,A,%.2f,D,PITCH,A,%.2f,D,ROLL", static_cast<int>(talkerId.size()),
        talkerId.data(), pitchDeg, rollDeg);
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildRsd(const RsdData& data, std::string_view talkerId)
{
    char buf[128] {};
    std::snprintf(buf, sizeof(buf), "%.*sRSD,,,,,,,,,%.2f,%.1f,%.2f,%c", static_cast<int>(talkerId.size()),
        talkerId.data(), data.cursorRangeNmi, data.cursorBearingDeg, data.rangeScaleNmi, data.displayRotation);
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildOsd(const OsdData& data, std::string_view talkerId)
{
    char buf[128] {};
    std::snprintf(buf, sizeof(buf), "%.*sOSD,%.1f,%c,%.1f,%c,%.1f,%c,%.1f,%.1f,%c", static_cast<int>(talkerId.size()),
        talkerId.data(), data.headingDegrees, data.headingValid ? 'A' : 'V', data.courseDegrees, data.courseReference,
        data.vesselSpeed, data.speedReference, data.vesselSetDeg, data.vesselDriftSpeed, data.speedUnits);
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildApb(const ApbData& data, std::string_view talkerId)
{
    char buf[160] {};
    std::snprintf(buf, sizeof(buf), "%.*sAPB,%c,%c,%.3f,%c,%c,%c,%c,%.1f,%c,%s,%.1f,%c,%.1f,%c,%c",
        static_cast<int>(talkerId.size()), talkerId.data(), data.generalWarning ? 'V' : 'A',
        data.cycleLockWarning ? 'V' : 'A', data.crossTrackErrorNmi, data.directionToSteer, data.xteUnits,
        data.arrivalCircleEntered ? 'A' : 'V', data.perpendicularPassed ? 'A' : 'V', data.bearingOriginToDestDeg,
        data.bearingOriginRef, data.destWaypointId.c_str(), data.bearingPresentToDestDeg, data.bearingPresentRef,
        data.headingToSteerDeg, data.headingToSteerRef, static_cast<char>(data.faaMode));
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildBwc(const BwcData& data, std::string_view talkerId)
{
    std::string latStr {};
    std::string lonStr {};
    char latHemi { 'N' };
    char lonHemi { 'E' };

    formatCoordinate(data.waypointCoordinates.latitudeDeg, true, latStr, latHemi);
    formatCoordinate(data.waypointCoordinates.longitudeDeg, false, lonStr, lonHemi);

    char buf[160] {};
    std::snprintf(buf, sizeof(buf), "%.*sBWC,%02u%02u%02u,%s,%c,%s,%c,%.1f,T,%.1f,M,%.1f,N,%s,%c",
        static_cast<int>(talkerId.size()), talkerId.data(), static_cast<unsigned int>(data.utcTime.hour),
        static_cast<unsigned int>(data.utcTime.minute), static_cast<unsigned int>(data.utcTime.second), latStr.c_str(),
        latHemi, lonStr.c_str(), lonHemi, data.bearingTrueDeg, data.bearingMagneticDeg, data.distanceNmi,
        data.waypointId.c_str(), static_cast<char>(data.faaMode));
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildMwv(const MwvData& data, std::string_view talkerId)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "%.*sMWV,%.1f,%c,%.1f,%c,%c", static_cast<int>(talkerId.size()), talkerId.data(),
        data.windAngleDeg, data.reference, data.windSpeed, data.speedUnits, data.valid ? 'A' : 'V');
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildHdg(const HdgData& data, std::string_view talkerId)
{
    std::string devStr {};
    std::string devDirStr {};
    if (data.hasDeviation) {
        char buf[32] {};
        std::snprintf(buf, sizeof(buf), "%.1f", std::abs(data.magneticDeviationDeg));
        devStr = buf;
        devDirStr = (data.magneticDeviationDeg >= 0.0) ? "E" : "W";
    }

    std::string varStr {};
    std::string varDirStr {};
    if (data.hasVariation) {
        char buf[32] {};
        std::snprintf(buf, sizeof(buf), "%.1f", std::abs(data.magneticVariationDeg));
        varStr = buf;
        varDirStr = (data.magneticVariationDeg >= 0.0) ? "E" : "W";
    }

    char buf[128] {};
    std::snprintf(buf, sizeof(buf), "%.*sHDG,%.1f,%s,%s,%s,%s", static_cast<int>(talkerId.size()), talkerId.data(),
        data.magneticHeadingDeg, devStr.c_str(), devDirStr.c_str(), varStr.c_str(), varDirStr.c_str());
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildPfecVelocity(int panSpeed, int tiltSpeed)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "PFEC,GPcmd,p,%d,%d", panSpeed, tiltSpeed);
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildPfecAbsolute(double panDeg, double tiltDeg)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "PFEC,GPcmd,a,%.1f,%.1f", panDeg, tiltDeg);
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildPfecPreset(char action, std::uint8_t presetId)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "PFEC,GPcmd,%c,%u", action, static_cast<unsigned int>(presetId));
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildPfecZoom(int speed)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "PFEC,GPcmd,z,%d", speed);
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildPfecQueryPos()
{
    return NmeaChecksum::frameSentence("PFEC,GPpos");
}

std::string NmeaSentenceBuilder::buildPfecPosReport(double panDeg, double tiltDeg)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "PFEC,GPpos,%.1f,%.1f", panDeg, tiltDeg);
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildPfecCameraCommand(std::string_view command)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "PFEC,GPcam,%.*s", static_cast<int>(command.size()), command.data());
    return NmeaChecksum::frameSentence(buf);
}

} // namespace Nmea
