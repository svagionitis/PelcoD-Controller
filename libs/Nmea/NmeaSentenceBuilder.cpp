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

} // namespace Nmea
