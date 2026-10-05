#include "Misb0805.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Klv {

namespace {

    [[nodiscard]] std::uint64_t getCurrentTimeUs() noexcept
    {
        const auto now = std::chrono::system_clock::now();
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        return static_cast<std::uint64_t>(us > 0 ? us : 0);
    }

} // namespace

std::string CotEvent::toXml() const
{
    std::ostringstream oss;
    oss << "<event version=\"" << version << "\""
        << " uid=\"" << uid << "\""
        << " type=\"" << type << "\""
        << " time=\"" << time << "\""
        << " start=\"" << start << "\""
        << " stale=\"" << stale << "\""
        << " how=\"" << how << "\">" << std::fixed << std::setprecision(6) << "<point lat=\"" << point.lat << "\""
        << " lon=\"" << point.lon << "\"" << std::setprecision(2) << " hae=\"" << point.hae << "\""
        << " ce=\"" << point.ce << "\""
        << " le=\"" << point.le << "\"/>";
    if (!detailXml.empty()) {
        oss << "<detail>" << detailXml << "</detail>";
    }
    oss << "</event>";
    return oss.str();
}

std::string Misb0805::formatIso8601(std::uint64_t timestampUs) noexcept
{
    const auto sec = static_cast<std::time_t>(timestampUs / 1000000ULL);
    const auto ms = static_cast<unsigned int>((timestampUs % 1000000ULL) / 1000ULL);

    std::tm tmBuf {};
#if defined(_WIN32)
    gmtime_s(&tmBuf, &sec);
#else
    gmtime_r(&sec, &tmBuf);
#endif

    char buf[64] {};
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03uZ", tmBuf.tm_year + 1900, tmBuf.tm_mon + 1,
        tmBuf.tm_mday, tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec, ms);
    return std::string(buf);
}

double Misb0805::ce90ToSigma1(double ce90M) noexcept
{
    constexpr double kCe90ToSigma = 2.146;
    return ce90M / kCe90ToSigma;
}

double Misb0805::le90ToSigma1(double le90M) noexcept
{
    constexpr double kLe90ToSigma = 1.645;
    return le90M / kLe90ToSigma;
}

std::string Misb0805::targetTypeToCot(RvtTargetType type) noexcept
{
    switch (type) {
    case RvtTargetType::Friendly:
        return "a-f-G";
    case RvtTargetType::Hostile:
        return "a-h-G";
    case RvtTargetType::Target:
        return "b-m-p-s-p-i";
    case RvtTargetType::Unknown:
    default:
        return "a-u-G";
    }
}

CotEvent Misb0805::toPlatformPosition(const UasDatalinkMessage& msg, double staleSec, const std::string& platformType)
{
    CotEvent event {};
    event.version = "2.0";

    // ST 0805.1 Table 1: UID is concatenation of Tag 10 (Device Designation) and Tag 3 (Mission ID)
    if (msg.platformDesignation.has_value() && msg.missionId.has_value()) {
        event.uid = *msg.platformDesignation + "_" + *msg.missionId;
    } else if (msg.platformTailNumber.has_value()) {
        event.uid = *msg.platformTailNumber;
    } else {
        event.uid = "UAS_PLATFORM";
    }

    event.type = platformType;

    const std::uint64_t ts = msg.precisionTimeStampUs.value_or(getCurrentTimeUs());
    event.time = formatIso8601(ts);
    event.start = event.time;
    event.stale = formatIso8601(ts + static_cast<std::uint64_t>(staleSec * 1000000.0));
    event.how = "m-p";

    event.point.lat = msg.sensorLatitudeDeg.value_or(0.0);
    event.point.lon = msg.sensorLongitudeDeg.value_or(0.0);
    event.point.hae = msg.sensorAltitudeHaeM.value_or(msg.sensorTrueAltitudeM.value_or(0.0));
    event.point.ce = 9999999.0;
    event.point.le = 9999999.0;

    std::ostringstream detail;
    detail << "<_flow-tags_ ST0601CoT=\"" << event.time << "\"/>";

    const double heading = msg.platformHeadingDeg.value_or(0.0);
    const double relAz = msg.sensorRelAzimuthDeg.value_or(0.0);
    double absAzimuth = std::fmod(heading + relAz, 360.0);
    if (absAzimuth < 0.0) {
        absAzimuth += 360.0;
    }

    detail << std::fixed << std::setprecision(2);
    detail << "<sensor azimuth=\"" << absAzimuth << "\"";
    if (msg.sensorHfovDeg.has_value()) {
        detail << " fov=\"" << *msg.sensorHfovDeg << "\"";
    }
    if (msg.sensorVfovDeg.has_value()) {
        detail << " vfov=\"" << *msg.sensorVfovDeg << "\"";
    }
    if (msg.imageSourceSensor.has_value()) {
        detail << " model=\"" << *msg.imageSourceSensor << "\"";
    }
    if (msg.slantRangeM.has_value()) {
        detail << " range=\"" << *msg.slantRangeM << "\"";
    }
    detail << "/>";

    event.detailXml = detail.str();
    return event;
}

CotEvent Misb0805::toSensorPointOfInterest(
    const UasDatalinkMessage& msg, double staleSec, const std::string& platformType)
{
    CotEvent event {};
    event.version = "2.0";

    std::string platUid = "UAS_PLATFORM";
    if (msg.platformDesignation.has_value() && msg.missionId.has_value()) {
        platUid = *msg.platformDesignation + "_" + *msg.missionId;
    } else if (msg.platformTailNumber.has_value()) {
        platUid = *msg.platformTailNumber;
    }

    // ST 0805.1 Table 2: Concatenate Tags 10, 3, and 11
    if (msg.imageSourceSensor.has_value()) {
        event.uid = platUid + "_" + *msg.imageSourceSensor;
    } else {
        event.uid = platUid + "_SPI";
    }

    event.type = "b-m-p-s-p-i";

    const std::uint64_t ts = msg.precisionTimeStampUs.value_or(getCurrentTimeUs());
    event.time = formatIso8601(ts);
    event.start = event.time;
    event.stale = formatIso8601(ts + static_cast<std::uint64_t>(staleSec * 1000000.0));
    event.how = "m-p";

    event.point.lat = msg.frameCenterLatDeg.value_or(0.0);
    event.point.lon = msg.frameCenterLonDeg.value_or(0.0);
    event.point.hae = msg.frameCenterElevHaeM.value_or(msg.frameCenterElevM.value_or(0.0));

    if (msg.targetErrorCe90M.has_value()) {
        event.point.ce = ce90ToSigma1(*msg.targetErrorCe90M);
    } else {
        event.point.ce = 9999999.0;
    }

    if (msg.targetErrorLe90M.has_value()) {
        event.point.le = le90ToSigma1(*msg.targetErrorLe90M);
    } else {
        event.point.le = 9999999.0;
    }

    std::ostringstream detail;
    detail << "<_flow-tags_ ST0601CoT=\"" << event.time << "\"/>";
    detail << "<link relation=\"p-p\" type=\"" << platformType << "\" uid=\"" << platUid << "\"/>";

    event.detailXml = detail.str();
    return event;
}

CotEvent Misb0805::toCot(const PoiPack& poi, const std::string& parentUid, std::uint64_t timestampUs, double staleSec)
{
    CotEvent event {};
    event.version = "2.0";

    if (poi.sourceId.has_value() && !poi.sourceId->empty()) {
        event.uid = *poi.sourceId;
    } else if (!parentUid.empty()) {
        event.uid = parentUid + "_POI_" + std::to_string(poi.poiNumber);
    } else {
        event.uid = "POI_" + std::to_string(poi.poiNumber);
    }

    event.type = targetTypeToCot(poi.type.value_or(RvtTargetType::Unknown));

    const std::uint64_t ts = (timestampUs > 0U) ? timestampUs : getCurrentTimeUs();
    event.time = formatIso8601(ts);
    event.start = event.time;
    event.stale = formatIso8601(ts + static_cast<std::uint64_t>(staleSec * 1000000.0));
    event.how = "m-p";

    event.point.lat = poi.latitudeDeg;
    event.point.lon = poi.longitudeDeg;
    event.point.hae = poi.altitudeMslM.value_or(0.0);
    event.point.ce = 9999999.0;
    event.point.le = 9999999.0;

    std::ostringstream detail;
    detail << "<_flow-tags_ ST0806CoT=\"" << event.time << "\"/>";

    if (poi.label.has_value() && !poi.label->empty()) {
        detail << "<contact callsign=\"" << *poi.label << "\"/>";
    }

    if (poi.text.has_value() && !poi.text->empty()) {
        detail << "<remarks>" << *poi.text << "</remarks>";
    }

    if (!parentUid.empty()) {
        detail << "<link relation=\"p-p\" type=\"a-f-A-M-F\" uid=\"" << parentUid << "\"/>";
    }

    if (poi.sourceIcon.has_value() && !poi.sourceIcon->empty()) {
        detail << "<usericon iconsetpath=\"" << *poi.sourceIcon << "\"/>";
    }

    event.detailXml = detail.str();
    return event;
}

std::vector<CotEvent> Misb0805::toCot(const RvtLocalSet& rvt, const std::string& parentUid, double staleSec)
{
    std::vector<CotEvent> events;
    events.reserve(rvt.pois.size());

    const std::uint64_t ts = rvt.precisionTimeStampUs.value_or(getCurrentTimeUs());
    for (const auto& poi : rvt.pois) {
        events.push_back(toCot(poi, parentUid, ts, staleSec));
    }
    return events;
}

} // namespace Klv
