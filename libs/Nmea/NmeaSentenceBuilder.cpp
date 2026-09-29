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

std::string NmeaSentenceBuilder::buildRmb(const RmbData& data, std::string_view talkerId)
{
    std::string latStr {};
    char latHemi { 'N' };
    std::string lonStr {};
    char lonHemi { 'E' };
    formatCoordinate(data.destCoordinates.latitudeDeg, true, latStr, latHemi);
    formatCoordinate(data.destCoordinates.longitudeDeg, false, lonStr, lonHemi);

    char buf[256] {};
    std::snprintf(buf, sizeof(buf), "%.*sRMB,%c,%.2f,%c,%s,%s,%s,%c,%s,%c,%.1f,%.1f,%.1f,%c,%c",
        static_cast<int>(talkerId.size()), talkerId.data(), data.statusActive ? 'A' : 'V', data.crossTrackErrorNmi,
        data.directionToSteer, data.destWaypointId.c_str(), data.originWaypointId.c_str(), latStr.c_str(), latHemi,
        lonStr.c_str(), lonHemi, data.rangeToDestNmi, data.bearingToDestTrueDeg, data.closingVelocityKnots,
        data.arrivalAlarm ? 'A' : 'V', static_cast<char>(data.faaMode));
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildRte(const RteData& data, std::string_view talkerId)
{
    std::string body {};
    char headerBuf[64] {};
    std::snprintf(headerBuf, sizeof(headerBuf), "%.*sRTE,%u,%u,%c,%s", static_cast<int>(talkerId.size()),
        talkerId.data(), data.totalSentences, data.sentenceNumber, data.routeType, data.routeName.c_str());
    body = headerBuf;

    for (const auto& wpt : data.waypointIds) {
        body += ",";
        body += wpt;
    }
    return NmeaChecksum::frameSentence(body);
}

std::string NmeaSentenceBuilder::buildWpl(const WplData& data, std::string_view talkerId)
{
    std::string latStr {};
    char latHemi { 'N' };
    std::string lonStr {};
    char lonHemi { 'E' };
    formatCoordinate(data.coordinates.latitudeDeg, true, latStr, latHemi);
    formatCoordinate(data.coordinates.longitudeDeg, false, lonStr, lonHemi);

    char buf[128] {};
    std::snprintf(buf, sizeof(buf), "%.*sWPL,%s,%c,%s,%c,%s", static_cast<int>(talkerId.size()), talkerId.data(),
        latStr.c_str(), latHemi, lonStr.c_str(), lonHemi, data.waypointId.c_str());
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildMtw(const MtwData& data, std::string_view talkerId)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "%.*sMTW,%.1f,C", static_cast<int>(talkerId.size()), talkerId.data(),
        data.waterTemperatureCelsius);
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildMmb(const MmbData& data, std::string_view talkerId)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "%.*sMMB,%.4f,I,%.4f,B", static_cast<int>(talkerId.size()), talkerId.data(),
        data.pressureInHg, data.pressureBars);
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildMda(const MdaData& data, std::string_view talkerId)
{
    auto fmtOpt = [](std::optional<double> opt, int precision) -> std::string {
        if (!opt.has_value()) {
            return {};
        }
        char b[32] {};
        if (precision == 4) {
            std::snprintf(b, sizeof(b), "%.4f", *opt);
        } else {
            std::snprintf(b, sizeof(b), "%.1f", *opt);
        }
        return b;
    };

    const std::string pInHg = fmtOpt(data.barometricPressureInHg, 4);
    const std::string pBars = fmtOpt(data.barometricPressureBars, 4);
    const std::string aTemp = fmtOpt(data.airTemperatureCelsius, 1);
    const std::string wTemp = fmtOpt(data.waterTemperatureCelsius, 1);
    const std::string relHum = fmtOpt(data.relativeHumidityPercent, 1);
    const std::string absHum = fmtOpt(data.absoluteHumidityGPerM3, 1);
    const std::string dewPt = fmtOpt(data.dewPointCelsius, 1);
    const std::string wDirT = fmtOpt(data.windDirectionTrueDeg, 1);
    const std::string wDirM = fmtOpt(data.windDirectionMagneticDeg, 1);
    const std::string wSpdK = fmtOpt(data.windSpeedKnots, 1);
    const std::string wSpdM = fmtOpt(data.windSpeedMps, 1);

    char buf[256] {};
    std::snprintf(buf, sizeof(buf), "%.*sMDA,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s",
        static_cast<int>(talkerId.size()), talkerId.data(), pInHg.c_str(), pInHg.empty() ? "" : "I", pBars.c_str(),
        pBars.empty() ? "" : "B", aTemp.c_str(), aTemp.empty() ? "" : "C", wTemp.c_str(), wTemp.empty() ? "" : "C",
        relHum.c_str(), absHum.c_str(), dewPt.c_str(), dewPt.empty() ? "" : "C", wDirT.c_str(),
        wDirT.empty() ? "" : "T", wDirM.c_str(), wDirM.empty() ? "" : "M", wSpdK.c_str(), wSpdK.empty() ? "" : "N",
        wSpdM.c_str(), wSpdM.empty() ? "" : "M");
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

std::string NmeaSentenceBuilder::buildGsa(const GsaData& data, std::string_view talkerId)
{
    std::string body {};
    body.reserve(80U);
    body.append(talkerId);
    body.append("GSA,");
    body.push_back(data.selectionMode);
    body.push_back(',');
    body.append(std::to_string(static_cast<unsigned int>(data.fixMode)));

    for (std::size_t i = 0U; i < 12U; ++i) {
        body.push_back(',');
        if (i < data.activeSatellitePrns.size()) {
            char prnBuf[8] {};
            std::snprintf(prnBuf, sizeof(prnBuf), "%02u", static_cast<unsigned int>(data.activeSatellitePrns[i]));
            body.append(prnBuf);
        }
    }

    char dopBuf[32] {};
    std::snprintf(dopBuf, sizeof(dopBuf), ",%.1f,%.1f,%.1f", data.pdop, data.hdop, data.vdop);
    body.append(dopBuf);

    if (data.systemId.has_value()) {
        body.push_back(',');
        body.append(std::to_string(static_cast<unsigned int>(*data.systemId)));
    }

    return NmeaChecksum::frameSentence(body);
}

std::string NmeaSentenceBuilder::buildGsv(const GsvData& data, std::string_view talkerId)
{
    std::string body {};
    body.reserve(90U);
    body.append(talkerId);
    body.append("GSV,");
    body.append(std::to_string(static_cast<unsigned int>(data.totalSentences)));
    body.push_back(',');
    body.append(std::to_string(static_cast<unsigned int>(data.sentenceNumber)));
    body.push_back(',');
    body.append(std::to_string(static_cast<unsigned int>(data.totalSatellitesInView)));

    for (const auto& sat : data.satellites) {
        char satBuf[32] {};
        if (sat.snrDb.has_value()) {
            std::snprintf(satBuf, sizeof(satBuf), ",%02u,%02.0f,%03.0f,%02.0f",
                static_cast<unsigned int>(sat.prn), sat.elevationDeg, sat.azimuthDeg, *sat.snrDb);
        } else {
            std::snprintf(satBuf, sizeof(satBuf), ",%02u,%02.0f,%03.0f,",
                static_cast<unsigned int>(sat.prn), sat.elevationDeg, sat.azimuthDeg);
        }
        body.append(satBuf);
    }

    if (data.signalId.has_value()) {
        body.push_back(',');
        body.append(std::to_string(static_cast<unsigned int>(*data.signalId)));
    }

    return NmeaChecksum::frameSentence(body);
}

std::string NmeaSentenceBuilder::buildZda(const ZdaData& data, std::string_view talkerId)
{
    char buf[80] {};
    std::snprintf(buf, sizeof(buf), "%sZDA,%02u%02u%02u.%02u,%02u,%02u,%04u,%02d,%02u",
        std::string(talkerId).c_str(),
        static_cast<unsigned int>(data.utcTime.hour),
        static_cast<unsigned int>(data.utcTime.minute),
        static_cast<unsigned int>(data.utcTime.second),
        static_cast<unsigned int>(data.utcTime.millisecond / 10U),
        static_cast<unsigned int>(data.day),
        static_cast<unsigned int>(data.month),
        static_cast<unsigned int>(data.year),
        static_cast<int>(data.localZoneHours),
        static_cast<unsigned int>(data.localZoneMinutes));
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildVbw(const VbwData& data, std::string_view talkerId)
{
    char buf[128] {};
    if (data.sternWaterSpeedKnots.has_value() || data.sternGroundSpeedKnots.has_value()) {
        std::snprintf(buf, sizeof(buf), "%sVBW,%.2f,%.2f,%c,%.2f,%.2f,%c,%.2f,%c,%.2f,%c",
            std::string(talkerId).c_str(),
            data.longitudinalWaterSpeedKnots, data.transverseWaterSpeedKnots, data.waterSpeedStatus,
            data.longitudinalGroundSpeedKnots, data.transverseGroundSpeedKnots, data.groundSpeedStatus,
            data.sternWaterSpeedKnots.value_or(0.0), data.sternWaterStatus.value_or('V'),
            data.sternGroundSpeedKnots.value_or(0.0), data.sternGroundStatus.value_or('V'));
    } else {
        std::snprintf(buf, sizeof(buf), "%sVBW,%.2f,%.2f,%c,%.2f,%.2f,%c",
            std::string(talkerId).c_str(),
            data.longitudinalWaterSpeedKnots, data.transverseWaterSpeedKnots, data.waterSpeedStatus,
            data.longitudinalGroundSpeedKnots, data.transverseGroundSpeedKnots, data.groundSpeedStatus);
    }
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildVhw(const VhwData& data, std::string_view talkerId)
{
    char buf[96] {};
    std::string hdgTStr {};
    if (data.headingDegreesTrue.has_value()) {
        char hBuf[16] {};
        std::snprintf(hBuf, sizeof(hBuf), "%.1f", *data.headingDegreesTrue);
        hdgTStr = hBuf;
    }
    std::string hdgMStr {};
    if (data.headingDegreesMagnetic.has_value()) {
        char hBuf[16] {};
        std::snprintf(hBuf, sizeof(hBuf), "%.1f", *data.headingDegreesMagnetic);
        hdgMStr = hBuf;
    }
    std::string spdKnStr {};
    if (data.speedWaterKnots.has_value()) {
        char sBuf[16] {};
        std::snprintf(sBuf, sizeof(sBuf), "%.1f", *data.speedWaterKnots);
        spdKnStr = sBuf;
    }
    std::string spdKmStr {};
    if (data.speedWaterKmh.has_value()) {
        char sBuf[16] {};
        std::snprintf(sBuf, sizeof(sBuf), "%.1f", *data.speedWaterKmh);
        spdKmStr = sBuf;
    }

    std::snprintf(buf, sizeof(buf), "%sVHW,%s,%s,%s,%s,%s,%s,%s,%s",
        std::string(talkerId).c_str(),
        hdgTStr.c_str(), hdgTStr.empty() ? "" : "T",
        hdgMStr.c_str(), hdgMStr.empty() ? "" : "M",
        spdKnStr.c_str(), spdKnStr.empty() ? "" : "N",
        spdKmStr.c_str(), spdKmStr.empty() ? "" : "K");
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildDpt(const DptData& data, std::string_view talkerId)
{
    char buf[64] {};
    if (data.maximumRangeScaleMeters.has_value()) {
        std::snprintf(buf, sizeof(buf), "%sDPT,%.1f,%.1f,%.1f",
            std::string(talkerId).c_str(), data.waterDepthMeters, data.offsetMeters, *data.maximumRangeScaleMeters);
    } else {
        std::snprintf(buf, sizeof(buf), "%sDPT,%.1f,%.1f",
            std::string(talkerId).c_str(), data.waterDepthMeters, data.offsetMeters);
    }
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildDbt(const DbtData& data, std::string_view talkerId)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "%sDBT,%.1f,f,%.1f,M,%.1f,F",
        std::string(talkerId).c_str(), data.depthFeet, data.depthMeters, data.depthFathoms);
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildAlf(const Bam::AlfData& data, std::string_view talkerId)
{
    char buf[160] {};
    std::snprintf(buf, sizeof(buf), "%sALF,%u,%u,%u,%02u%02u%02u.%02u,%c,%c,%c,%u,%u,%u,%u,%s",
        std::string(talkerId).c_str(),
        static_cast<unsigned int>(data.totalSentences),
        static_cast<unsigned int>(data.sentenceNumber),
        static_cast<unsigned int>(data.sequentialMessageId),
        static_cast<unsigned int>(data.timeOfLastChange.hour),
        static_cast<unsigned int>(data.timeOfLastChange.minute),
        static_cast<unsigned int>(data.timeOfLastChange.second),
        static_cast<unsigned int>(data.timeOfLastChange.millisecond / 10U),
        data.alertPriority, data.alertCategory, data.alertState,
        static_cast<unsigned int>(data.alertIdentifier),
        static_cast<unsigned int>(data.alertInstance),
        static_cast<unsigned int>(data.revisionCounter),
        static_cast<unsigned int>(data.escalationCounter),
        data.alertText.c_str());
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildAlc(const Bam::AlcData& data, std::string_view talkerId)
{
    std::string body {};
    body.reserve(120U);
    body.append(talkerId);
    body.append("ALC,");
    body.append(std::to_string(static_cast<unsigned int>(data.totalSentences)));
    body.push_back(',');
    body.append(std::to_string(static_cast<unsigned int>(data.sentenceNumber)));
    body.push_back(',');
    body.append(std::to_string(static_cast<unsigned int>(data.sequentialMessageId)));
    body.push_back(',');
    body.append(std::to_string(static_cast<unsigned int>(data.alertCount)));

    for (const auto& entry : data.alertEntries) {
        char entBuf[32] {};
        std::snprintf(entBuf, sizeof(entBuf), ",%u,%u,%u",
            static_cast<unsigned int>(entry.alertIdentifier),
            static_cast<unsigned int>(entry.alertInstance),
            static_cast<unsigned int>(entry.revisionCounter));
        body.append(entBuf);
    }

    return NmeaChecksum::frameSentence(body);
}

std::string NmeaSentenceBuilder::buildArc(const Bam::ArcData& data, std::string_view talkerId)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "%sARC,%02u%02u%02u.%02u,%u,%u,%c",
        std::string(talkerId).c_str(),
        static_cast<unsigned int>(data.releaseTime.hour),
        static_cast<unsigned int>(data.releaseTime.minute),
        static_cast<unsigned int>(data.releaseTime.second),
        static_cast<unsigned int>(data.releaseTime.millisecond / 10U),
        static_cast<unsigned int>(data.alertIdentifier),
        static_cast<unsigned int>(data.alertInstance),
        data.command);
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildHbt(const Bam::HbtData& data, std::string_view talkerId)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "%sHBT,%.1f,%c,%u",
        std::string(talkerId).c_str(),
        data.configuredIntervalSec,
        data.equipmentStatus,
        static_cast<unsigned int>(data.sequentialSentenceId));
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildAlr(const Bam::AlrData& data, std::string_view talkerId)
{
    char buf[128] {};
    std::snprintf(buf, sizeof(buf), "%sALR,%02u%02u%02u.%02u,%u,%c,%c,%s",
        std::string(talkerId).c_str(),
        static_cast<unsigned int>(data.timeOfLastChange.hour),
        static_cast<unsigned int>(data.timeOfLastChange.minute),
        static_cast<unsigned int>(data.timeOfLastChange.second),
        static_cast<unsigned int>(data.timeOfLastChange.millisecond / 10U),
        static_cast<unsigned int>(data.alertIdentifier),
        data.condition,
        data.acknowledgeState,
        data.alertText.c_str());
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildAck(const Bam::AckData& data, std::string_view talkerId)
{
    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "%sACK,%u",
        std::string(talkerId).c_str(),
        static_cast<unsigned int>(data.alertIdentifier));
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildPfecPalette(std::uint8_t paletteIndex)
{
    char buf[32] {};
    std::snprintf(buf, sizeof(buf), "PFEC,GPcam,p,%u", static_cast<unsigned int>(paletteIndex));
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildPfecStabilization(bool enable)
{
    char buf[32] {};
    std::snprintf(buf, sizeof(buf), "PFEC,GPcam,s,%s", enable ? "on" : "off");
    return NmeaChecksum::frameSentence(buf);
}

std::string NmeaSentenceBuilder::buildPfecNuc()
{
    return NmeaChecksum::frameSentence("PFEC,GPcam,nuc");
}

std::string NmeaSentenceBuilder::buildPfecDigitalZoom(double zoomFactor)
{
    char buf[32] {};
    std::snprintf(buf, sizeof(buf), "PFEC,GPcam,z,%.1f", zoomFactor);
    return NmeaChecksum::frameSentence(buf);
}

} // namespace Nmea
