#include "St1607Encoder.h"
#include "GeoRegistrationEncoder.h"
#include "KlvBer.h"

#include <algorithm>
#include <cmath>

namespace Klv {

namespace {

inline void appendU8(std::uint32_t tag, std::uint8_t val, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(1U, out);
    out.push_back(val);
}

inline void appendU16(std::uint32_t tag, std::uint16_t val, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(2U, out);
    out.push_back(static_cast<std::uint8_t>((val >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(val & 0xFFU));
}

inline void appendI16(std::uint32_t tag, std::int16_t val, std::vector<std::uint8_t>& out) {
    appendU16(tag, static_cast<std::uint16_t>(val), out);
}

inline void appendU32(std::uint32_t tag, std::uint32_t val, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(4U, out);
    out.push_back(static_cast<std::uint8_t>((val >> 24U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((val >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((val >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(val & 0xFFU));
}

inline void appendI32(std::uint32_t tag, std::int32_t val, std::vector<std::uint8_t>& out) {
    appendU32(tag, static_cast<std::uint32_t>(val), out);
}

inline void appendStr(std::uint32_t tag, const std::string& str, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(str.size(), out);
    for (const char c : str) {
        out.push_back(static_cast<std::uint8_t>(c));
    }
}

inline void appendBytes(std::uint32_t tag, const std::vector<std::uint8_t>& b, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(b.size(), out);
    out.insert(out.end(), b.begin(), b.end());
}

inline std::uint16_t scaleAlt(double altM) noexcept {
    const double clamped = std::clamp(altM, -900.0, 19000.0);
    return static_cast<std::uint16_t>(std::round((clamped + 900.0) * (65535.0 / 19900.0)));
}

inline std::uint16_t scaleHeading(double deg) noexcept {
    double wrapped = std::fmod(deg, 360.0);
    if (wrapped < 0.0) wrapped += 360.0;
    return static_cast<std::uint16_t>(std::round(wrapped * (65535.0 / 360.0)));
}

inline std::int16_t scalePitch(double deg) noexcept {
    const double clamped = std::clamp(deg, -20.0, 20.0);
    return static_cast<std::int16_t>(std::round(clamped * (32767.0 / 20.0)));
}

inline std::int16_t scaleRoll(double deg) noexcept {
    const double clamped = std::clamp(deg, -50.0, 50.0);
    return static_cast<std::int16_t>(std::round(clamped * (32767.0 / 50.0)));
}

inline std::uint16_t scaleFov(double deg) noexcept {
    const double clamped = std::clamp(deg, 0.0, 180.0);
    return static_cast<std::uint16_t>(std::round(clamped * (65535.0 / 180.0)));
}

inline std::uint32_t scaleAzimuth(double deg) noexcept {
    double wrapped = std::fmod(deg, 360.0);
    if (wrapped < 0.0) wrapped += 360.0;
    return static_cast<std::uint32_t>(std::round(wrapped * (4294967295.0 / 360.0)));
}

inline std::int32_t scaleElevation(double deg) noexcept {
    const double clamped = std::clamp(deg, -180.0, 180.0);
    return static_cast<std::int32_t>(std::round(clamped * (2147483647.0 / 180.0)));
}

inline std::uint32_t scaleRelRoll(double deg) noexcept {
    double wrapped = std::fmod(deg, 360.0);
    if (wrapped < 0.0) wrapped += 360.0;
    return static_cast<std::uint32_t>(std::round(wrapped * (4294967295.0 / 360.0)));
}

inline std::uint32_t scaleSlant(double m) noexcept {
    const double clamped = std::clamp(m, 0.0, 5000000.0);
    return static_cast<std::uint32_t>(std::round(clamped * (4294967295.0 / 5000000.0)));
}

inline std::uint16_t scaleWidth(double m) noexcept {
    const double clamped = std::clamp(m, 0.0, 10000.0);
    return static_cast<std::uint16_t>(std::round(clamped * (65535.0 / 10000.0)));
}

inline std::int32_t scaleLat(double deg) noexcept {
    const double clamped = std::clamp(deg, -90.0, 90.0);
    return static_cast<std::int32_t>(std::round(clamped * (2147483647.0 / 90.0)));
}

inline std::int32_t scaleLon(double deg) noexcept {
    const double clamped = std::clamp(deg, -180.0, 180.0);
    return static_cast<std::int32_t>(std::round(clamped * (2147483647.0 / 180.0)));
}

} // namespace

bool St1607Encoder::encodeMsid(const MetadataSubstreamId& msid, std::vector<std::uint8_t>& out) {
    if (msid.localId > 0U) {
        // ST 0601.19-46: Local ID encoded using BER-OID; UUID omitted
        KlvBer::encodeTag(msid.localId, out);
        return true;
    }

    if (msid.universalId.has_value()) {
        // ST 0601.19-47: Local ID 0, followed by 16-byte UUID
        out.push_back(0x00U);
        out.insert(out.end(), msid.universalId->begin(), msid.universalId->end());
        return true;
    }

    return false;
}

KlvStatus St1607Encoder::encodeAmend(const AmendLocalSet& set, std::vector<std::uint8_t>& out) {
    // Tag 143: MSID Pack (Mandatory per ST 0601.19-42)
    std::vector<std::uint8_t> msidBytes {};
    if (encodeMsid(set.msid, msidBytes)) {
        appendBytes(143U, msidBytes, out);
    }

    if (set.platformHeadingDeg)     appendU16(5U, scaleHeading(*set.platformHeadingDeg), out);
    if (set.platformPitchDeg)       appendI16(6U, scalePitch(*set.platformPitchDeg), out);
    if (set.platformRollDeg)        appendI16(7U, scaleRoll(*set.platformRollDeg), out);
    if (set.imageSourceSensor)      appendStr(11U, *set.imageSourceSensor, out);
    if (set.imageCoordinateSystem)  appendStr(12U, *set.imageCoordinateSystem, out);
    if (set.sensorLatitudeDeg)      appendI32(13U, scaleLat(*set.sensorLatitudeDeg), out);
    if (set.sensorLongitudeDeg)     appendI32(14U, scaleLon(*set.sensorLongitudeDeg), out);
    if (set.sensorTrueAltitudeM)    appendU16(15U, scaleAlt(*set.sensorTrueAltitudeM), out);
    if (set.sensorHfovDeg)          appendU16(16U, scaleFov(*set.sensorHfovDeg), out);
    if (set.sensorVfovDeg)          appendU16(17U, scaleFov(*set.sensorVfovDeg), out);
    if (set.sensorRelAzimuthDeg)    appendU32(18U, scaleAzimuth(*set.sensorRelAzimuthDeg), out);
    if (set.sensorRelElevationDeg)  appendI32(19U, scaleElevation(*set.sensorRelElevationDeg), out);
    if (set.sensorRelRollDeg)       appendU32(20U, scaleRelRoll(*set.sensorRelRollDeg), out);
    if (set.slantRangeM)            appendU32(21U, scaleSlant(*set.slantRangeM), out);
    if (set.targetWidthM)           appendU16(22U, scaleWidth(*set.targetWidthM), out);
    if (set.frameCenterLatDeg)      appendI32(23U, scaleLat(*set.frameCenterLatDeg), out);
    if (set.frameCenterLonDeg)      appendI32(24U, scaleLon(*set.frameCenterLonDeg), out);
    if (set.frameCenterElevM)       appendU16(25U, scaleAlt(*set.frameCenterElevM), out);

    if (set.cornerCoordinates) {
        appendI32(82U, scaleLat(set.cornerCoordinates->topLeft.latitudeDeg), out);
        appendI32(83U, scaleLon(set.cornerCoordinates->topLeft.longitudeDeg), out);
        appendI32(84U, scaleLat(set.cornerCoordinates->topRight.latitudeDeg), out);
        appendI32(85U, scaleLon(set.cornerCoordinates->topRight.longitudeDeg), out);
        appendI32(86U, scaleLat(set.cornerCoordinates->bottomRight.latitudeDeg), out);
        appendI32(87U, scaleLon(set.cornerCoordinates->bottomRight.longitudeDeg), out);
        appendI32(88U, scaleLat(set.cornerCoordinates->bottomLeft.latitudeDeg), out);
        appendI32(89U, scaleLon(set.cornerCoordinates->bottomLeft.longitudeDeg), out);
    }

    if (set.targetErrorCe90M)       appendU16(45U, static_cast<std::uint16_t>(*set.targetErrorCe90M), out);
    if (set.targetErrorLe90M)       appendU16(46U, static_cast<std::uint16_t>(*set.targetErrorLe90M), out);

    if (set.securityCountryCodingMethod || set.securityObjectCountryCodes) {
        std::vector<std::uint8_t> secPayload {};
        if (set.securityCountryCodingMethod) {
            appendU8(12U, *set.securityCountryCodingMethod, secPayload);
        }
        if (set.securityObjectCountryCodes) {
            appendStr(13U, *set.securityObjectCountryCodes, secPayload);
        }
        appendBytes(48U, secPayload, out);
    }

    if (set.sensorAltitudeHaeM)     appendU16(75U, scaleAlt(*set.sensorAltitudeHaeM), out);
    if (set.frameCenterElevHaeM)    appendU16(78U, scaleAlt(*set.frameCenterElevHaeM), out);

    if (set.geoRegistration) {
        std::vector<std::uint8_t> geoBytes {};
        if (GeoRegistrationEncoder::encode(*set.geoRegistration, geoBytes) == KlvStatus::Success) {
            appendBytes(98U, geoBytes, out);
        }
    }

    for (const auto& child : set.childAmends) {
        std::vector<std::uint8_t> childBytes {};
        if (encodeAmend(child, childBytes) == KlvStatus::Success) {
            appendBytes(101U, childBytes, out);
        }
    }

    if (set.sensorRollAngleDeg)     appendU32(118U, scaleRelRoll(*set.sensorRollAngleDeg), out);

    return KlvStatus::Success;
}

KlvStatus St1607Encoder::encodeSegment(const SegmentLocalSet& set, std::vector<std::uint8_t>& out) {
    // Tag 143: MSID Pack (Mandatory per ST 0601.19-41)
    std::vector<std::uint8_t> msidBytes {};
    if (encodeMsid(set.msid, msidBytes)) {
        appendBytes(143U, msidBytes, out);
    }

    if (set.imageSourceSensor)      appendStr(11U, *set.imageSourceSensor, out);
    if (set.imageCoordinateSystem)  appendStr(12U, *set.imageCoordinateSystem, out);
    if (set.sensorLatitudeDeg)      appendI32(13U, scaleLat(*set.sensorLatitudeDeg), out);
    if (set.sensorLongitudeDeg)     appendI32(14U, scaleLon(*set.sensorLongitudeDeg), out);
    if (set.sensorTrueAltitudeM)    appendU16(15U, scaleAlt(*set.sensorTrueAltitudeM), out);
    if (set.sensorHfovDeg)          appendU16(16U, scaleFov(*set.sensorHfovDeg), out);
    if (set.sensorVfovDeg)          appendU16(17U, scaleFov(*set.sensorVfovDeg), out);
    if (set.sensorRelAzimuthDeg)    appendU32(18U, scaleAzimuth(*set.sensorRelAzimuthDeg), out);
    if (set.sensorRelElevationDeg)  appendI32(19U, scaleElevation(*set.sensorRelElevationDeg), out);
    if (set.sensorRelRollDeg)       appendU32(20U, scaleRelRoll(*set.sensorRelRollDeg), out);
    if (set.slantRangeM)            appendU32(21U, scaleSlant(*set.slantRangeM), out);
    if (set.targetWidthM)           appendU16(22U, scaleWidth(*set.targetWidthM), out);
    if (set.frameCenterLatDeg)      appendI32(23U, scaleLat(*set.frameCenterLatDeg), out);
    if (set.frameCenterLonDeg)      appendI32(24U, scaleLon(*set.frameCenterLonDeg), out);
    if (set.frameCenterElevM)       appendU16(25U, scaleAlt(*set.frameCenterElevM), out);

    if (set.cornerCoordinates) {
        appendI32(82U, scaleLat(set.cornerCoordinates->topLeft.latitudeDeg), out);
        appendI32(83U, scaleLon(set.cornerCoordinates->topLeft.longitudeDeg), out);
        appendI32(84U, scaleLat(set.cornerCoordinates->topRight.latitudeDeg), out);
        appendI32(85U, scaleLon(set.cornerCoordinates->topRight.longitudeDeg), out);
        appendI32(86U, scaleLat(set.cornerCoordinates->bottomRight.latitudeDeg), out);
        appendI32(87U, scaleLon(set.cornerCoordinates->bottomRight.longitudeDeg), out);
        appendI32(88U, scaleLat(set.cornerCoordinates->bottomLeft.latitudeDeg), out);
        appendI32(89U, scaleLon(set.cornerCoordinates->bottomLeft.longitudeDeg), out);
    }

    if (set.securityCountryCodingMethod || set.securityObjectCountryCodes) {
        std::vector<std::uint8_t> secPayload {};
        if (set.securityCountryCodingMethod) {
            appendU8(12U, *set.securityCountryCodingMethod, secPayload);
        }
        if (set.securityObjectCountryCodes) {
            appendStr(13U, *set.securityObjectCountryCodes, secPayload);
        }
        appendBytes(48U, secPayload, out);
    }

    if (set.sensorAltitudeHaeM)     appendU16(75U, scaleAlt(*set.sensorAltitudeHaeM), out);
    if (set.frameCenterElevHaeM)    appendU16(78U, scaleAlt(*set.frameCenterElevHaeM), out);
    if (set.miisCoreId) {
        const auto miisBytes = set.miisCoreId->encode();
        appendBytes(94U, miisBytes, out);
    }

    for (const auto& child : set.childSegments) {
        std::vector<std::uint8_t> childBytes {};
        if (encodeSegment(child, childBytes) == KlvStatus::Success) {
            appendBytes(100U, childBytes, out);
        }
    }

    for (const auto& child : set.childAmends) {
        std::vector<std::uint8_t> childBytes {};
        if (encodeAmend(child, childBytes) == KlvStatus::Success) {
            appendBytes(101U, childBytes, out);
        }
    }

    if (set.sensorRollAngleDeg)     appendU32(118U, scaleRelRoll(*set.sensorRollAngleDeg), out);

    return KlvStatus::Success;
}

KlvStatus St1607Encoder::encodePacket(const AmendLocalSet& set, std::vector<std::uint8_t>& out) {
    std::vector<std::uint8_t> payload {};
    const auto status = encodeAmend(set, payload);
    if (status != KlvStatus::Success) {
        return status;
    }

    out.insert(out.end(), AmendLocalSetUl.begin(), AmendLocalSetUl.end());
    KlvBer::encodeLength(payload.size(), out);
    out.insert(out.end(), payload.begin(), payload.end());
    return KlvStatus::Success;
}

KlvStatus St1607Encoder::encodePacket(const SegmentLocalSet& set, std::vector<std::uint8_t>& out) {
    std::vector<std::uint8_t> payload {};
    const auto status = encodeSegment(set, payload);
    if (status != KlvStatus::Success) {
        return status;
    }

    out.insert(out.end(), SegmentLocalSetUl.begin(), SegmentLocalSetUl.end());
    KlvBer::encodeLength(payload.size(), out);
    out.insert(out.end(), payload.begin(), payload.end());
    return KlvStatus::Success;
}

} // namespace Klv
