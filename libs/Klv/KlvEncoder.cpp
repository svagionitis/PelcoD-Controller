#include "KlvEncoder.h"
#include "KlvBer.h"
#include "KlvCrc.h"
#include "RvtEncoder.h"
#include "St1607Encoder.h"
#include "VmtiEncoder.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace Klv {

namespace {

inline std::int32_t scaleLatitude(double deg) noexcept {
    constexpr double kMaxVal = 2147483647.0;
    const double clamped = std::clamp(deg, -90.0, 90.0);
    return static_cast<std::int32_t>(std::round(clamped * (kMaxVal / 90.0)));
}

inline std::int32_t scaleLongitude(double deg) noexcept {
    constexpr double kMaxVal = 2147483647.0;
    const double clamped = std::clamp(deg, -180.0, 180.0);
    return static_cast<std::int32_t>(std::round(clamped * (kMaxVal / 180.0)));
}

inline std::int16_t scaleCornerOffset(double offsetDeg) noexcept {
    const double clamped = std::clamp(offsetDeg, -0.075, 0.075);
    return static_cast<std::int16_t>(std::round(clamped * (32767.0 / 0.075)));
}

inline std::uint16_t scaleAltitude(double altM) noexcept {
    // Range: [-900.0, +19000.0] meters -> [0, 65535]
    const double clamped = std::clamp(altM, -900.0, 19000.0);
    return static_cast<std::uint16_t>(std::round((clamped + 900.0) * (65535.0 / 19900.0)));
}

inline std::uint16_t scaleHeading(double deg) noexcept {
    double wrapped = std::fmod(deg, 360.0);
    if (wrapped < 0.0) {
        wrapped += 360.0;
    }
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

inline std::uint32_t scaleRelAzimuth(double deg) noexcept {
    double wrapped = std::fmod(deg, 360.0);
    if (wrapped < 0.0) {
        wrapped += 360.0;
    }
    return static_cast<std::uint32_t>(std::round(wrapped * (4294967295.0 / 360.0)));
}

inline std::int32_t scaleRelElevation(double deg) noexcept {
    const double clamped = std::clamp(deg, -180.0, 180.0);
    return static_cast<std::int32_t>(std::round(clamped * (2147483647.0 / 180.0)));
}

inline std::uint32_t scaleRelRoll(double deg) noexcept {
    double wrapped = std::fmod(deg, 360.0);
    if (wrapped < 0.0) {
        wrapped += 360.0;
    }
    return static_cast<std::uint32_t>(std::round(wrapped * (4294967295.0 / 360.0)));
}

inline std::uint32_t scaleSlantRange(double rangeM) noexcept {
    const double clamped = std::clamp(rangeM, 0.0, 5000000.0);
    return static_cast<std::uint32_t>(std::round(clamped * (4294967295.0 / 5000000.0)));
}

inline std::uint16_t scaleTargetWidth(double widthM) noexcept {
    const double clamped = std::clamp(widthM, 0.0, 10000.0);
    return static_cast<std::uint16_t>(std::round(clamped * (65535.0 / 10000.0)));
}

} // namespace

void KlvEncoder::appendTagUint8(std::uint32_t tag, std::uint8_t value, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(1U, out);
    out.push_back(value);
}

void KlvEncoder::appendTagUint16(std::uint32_t tag, std::uint16_t value, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(2U, out);
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void KlvEncoder::appendTagInt16(std::uint32_t tag, std::int16_t value, std::vector<std::uint8_t>& out) {
    appendTagUint16(tag, static_cast<std::uint16_t>(value), out);
}

void KlvEncoder::appendTagUint32(std::uint32_t tag, std::uint32_t value, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(4U, out);
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void KlvEncoder::appendTagInt32(std::uint32_t tag, std::int32_t value, std::vector<std::uint8_t>& out) {
    appendTagUint32(tag, static_cast<std::uint32_t>(value), out);
}

void KlvEncoder::appendTagUint64(std::uint32_t tag, std::uint64_t value, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(8U, out);
    for (int i = 7; i >= 0; --i) {
        out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFU));
    }
}

void KlvEncoder::appendTagString(std::uint32_t tag, const std::string& value, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(value.size(), out);
    for (const char ch : value) {
        out.push_back(static_cast<std::uint8_t>(ch));
    }
}

void KlvEncoder::appendTagBytes(std::uint32_t tag, const std::vector<std::uint8_t>& value, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(value.size(), out);
    out.insert(out.end(), value.begin(), value.end());
}

std::vector<std::uint8_t> KlvEncoder::encodeSecurityLocalSet(const SecurityMetadata& security) {
    std::vector<std::uint8_t> inner;
    // Sub-tag 1: Classification (1 byte)
    appendTagUint8(1U, static_cast<std::uint8_t>(security.classification), inner);

    // Sub-tag 2: Classifying Country and Releasing Instructions Coding Method (1 byte)
    appendTagUint8(2U, security.countryCodingMethod, inner);

    // Sub-tag 3: Classifying Country
    if (!security.classifyingCountry.empty()) {
        appendTagString(3U, security.classifyingCountry, inner);
    }
    // Sub-tag 4: SCI / SHI
    if (!security.sciShiInfo.empty()) {
        appendTagString(4U, security.sciShiInfo, inner);
    }
    // Sub-tag 5: Caveats
    if (!security.caveats.empty()) {
        appendTagString(5U, security.caveats, inner);
    }
    // Sub-tag 6: Releasing Instructions
    if (!security.releasingInstructions.empty()) {
        appendTagString(6U, security.releasingInstructions, inner);
    }
    // Sub-tag 12: Object Country Coding Method
    if (security.objectCountryCodingMethod != 0U) {
        appendTagUint8(12U, security.objectCountryCodingMethod, inner);
    }
    // Sub-tag 13: Object Country Codes
    if (!security.objectCountryCodes.empty()) {
        appendTagString(13U, security.objectCountryCodes, inner);
    }
    // Sub-tag 22: Version (1 byte)
    appendTagUint8(22U, security.version, inner);

    return inner;
}

std::vector<std::uint8_t> KlvEncoder::encode(const UasDatalinkMessage& msg) {
    std::vector<std::uint8_t> payload;
    payload.reserve(256U);

    // Tag 2: Precision Time Stamp (8 bytes, mandatory in MISB ST 0601)
    std::uint64_t pts = 0ULL;
    if (msg.precisionTimeStampUs.has_value()) {
        pts = *msg.precisionTimeStampUs;
    } else {
        pts = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }
    appendTagUint64(static_cast<std::uint32_t>(Tag::PrecisionTimeStamp), pts, payload);

    // Tag 3: Mission ID
    if (msg.missionId.has_value()) {
        appendTagString(static_cast<std::uint32_t>(Tag::MissionId), *msg.missionId, payload);
    }

    // Tag 4: Platform Tail Number
    if (msg.platformTailNumber.has_value()) {
        appendTagString(static_cast<std::uint32_t>(Tag::PlatformTailNumber), *msg.platformTailNumber, payload);
    }

    // Tag 5: Platform Heading Angle
    if (msg.platformHeadingDeg.has_value()) {
        appendTagUint16(static_cast<std::uint32_t>(Tag::PlatformHeading), scaleHeading(*msg.platformHeadingDeg), payload);
    }

    // Tag 6: Platform Pitch Angle
    if (msg.platformPitchDeg.has_value()) {
        appendTagInt16(static_cast<std::uint32_t>(Tag::PlatformPitch), scalePitch(*msg.platformPitchDeg), payload);
    }

    // Tag 7: Platform Roll Angle
    if (msg.platformRollDeg.has_value()) {
        appendTagInt16(static_cast<std::uint32_t>(Tag::PlatformRoll), scaleRoll(*msg.platformRollDeg), payload);
    }

    // Tag 10: Platform Designation
    if (msg.platformDesignation.has_value()) {
        appendTagString(static_cast<std::uint32_t>(Tag::PlatformDesignation), *msg.platformDesignation, payload);
    }

    // Tag 11: Image Source Sensor
    if (msg.imageSourceSensor.has_value()) {
        appendTagString(static_cast<std::uint32_t>(Tag::ImageSourceSensor), *msg.imageSourceSensor, payload);
    }

    // Tag 12: Image Coordinate System
    if (msg.imageCoordinateSystem.has_value()) {
        appendTagString(static_cast<std::uint32_t>(Tag::ImageCoordinateSystem), *msg.imageCoordinateSystem, payload);
    }

    // Tag 13: Sensor Latitude
    if (msg.sensorLatitudeDeg.has_value()) {
        appendTagInt32(static_cast<std::uint32_t>(Tag::SensorLatitude), scaleLatitude(*msg.sensorLatitudeDeg), payload);
    }

    // Tag 14: Sensor Longitude
    if (msg.sensorLongitudeDeg.has_value()) {
        appendTagInt32(static_cast<std::uint32_t>(Tag::SensorLongitude), scaleLongitude(*msg.sensorLongitudeDeg), payload);
    }

    // Tag 15: Sensor True Altitude
    if (msg.sensorTrueAltitudeM.has_value()) {
        appendTagUint16(static_cast<std::uint32_t>(Tag::SensorTrueAltitude), scaleAltitude(*msg.sensorTrueAltitudeM), payload);
    }

    // Tag 16: Sensor HFOV
    if (msg.sensorHfovDeg.has_value()) {
        appendTagUint16(static_cast<std::uint32_t>(Tag::SensorHFOV), scaleFov(*msg.sensorHfovDeg), payload);
    }

    // Tag 17: Sensor VFOV
    if (msg.sensorVfovDeg.has_value()) {
        appendTagUint16(static_cast<std::uint32_t>(Tag::SensorVFOV), scaleFov(*msg.sensorVfovDeg), payload);
    }

    // Tag 18: Sensor Relative Azimuth
    if (msg.sensorRelAzimuthDeg.has_value()) {
        appendTagUint32(static_cast<std::uint32_t>(Tag::SensorRelAzimuth), scaleRelAzimuth(*msg.sensorRelAzimuthDeg), payload);
    }

    // Tag 19: Sensor Relative Elevation
    if (msg.sensorRelElevationDeg.has_value()) {
        appendTagInt32(static_cast<std::uint32_t>(Tag::SensorRelElevation), scaleRelElevation(*msg.sensorRelElevationDeg), payload);
    }

    // Tag 20: Sensor Relative Roll
    if (msg.sensorRelRollDeg.has_value()) {
        appendTagUint32(static_cast<std::uint32_t>(Tag::SensorRelRoll), scaleRelRoll(*msg.sensorRelRollDeg), payload);
    }

    // Tag 21: Slant Range
    if (msg.slantRangeM.has_value()) {
        appendTagUint32(static_cast<std::uint32_t>(Tag::SlantRange), scaleSlantRange(*msg.slantRangeM), payload);
    }

    // Tag 22: Target Width
    if (msg.targetWidthM.has_value()) {
        appendTagUint16(static_cast<std::uint32_t>(Tag::TargetWidth), scaleTargetWidth(*msg.targetWidthM), payload);
    }

    // Tag 23: Frame Center Latitude
    if (msg.frameCenterLatDeg.has_value()) {
        appendTagInt32(static_cast<std::uint32_t>(Tag::FrameCenterLat), scaleLatitude(*msg.frameCenterLatDeg), payload);
    }

    // Tag 24: Frame Center Longitude
    if (msg.frameCenterLonDeg.has_value()) {
        appendTagInt32(static_cast<std::uint32_t>(Tag::FrameCenterLon), scaleLongitude(*msg.frameCenterLonDeg), payload);
    }

    // Tag 25: Frame Center Elevation
    if (msg.frameCenterElevM.has_value()) {
        appendTagUint16(static_cast<std::uint32_t>(Tag::FrameCenterElev), scaleAltitude(*msg.frameCenterElevM), payload);
    }

    // Tags 26..33 (Offsets) and Tags 82..89 (Full Coordinates)
    if (msg.cornerCoordinates.has_value()) {
        const auto& c = *msg.cornerCoordinates;

        // If Frame Center is available, encode standard 2-byte offsets (Tags 26..33)
        if (msg.frameCenterLatDeg.has_value() && msg.frameCenterLonDeg.has_value()) {
            const double cLat = *msg.frameCenterLatDeg;
            const double cLon = *msg.frameCenterLonDeg;
            appendTagInt16(static_cast<std::uint32_t>(Tag::OffsetCornerLat1), scaleCornerOffset(c.topLeft.latitudeDeg - cLat), payload);
            appendTagInt16(static_cast<std::uint32_t>(Tag::OffsetCornerLon1), scaleCornerOffset(c.topLeft.longitudeDeg - cLon), payload);
            appendTagInt16(static_cast<std::uint32_t>(Tag::OffsetCornerLat2), scaleCornerOffset(c.topRight.latitudeDeg - cLat), payload);
            appendTagInt16(static_cast<std::uint32_t>(Tag::OffsetCornerLon2), scaleCornerOffset(c.topRight.longitudeDeg - cLon), payload);
            appendTagInt16(static_cast<std::uint32_t>(Tag::OffsetCornerLat3), scaleCornerOffset(c.bottomRight.latitudeDeg - cLat), payload);
            appendTagInt16(static_cast<std::uint32_t>(Tag::OffsetCornerLon3), scaleCornerOffset(c.bottomRight.longitudeDeg - cLon), payload);
            appendTagInt16(static_cast<std::uint32_t>(Tag::OffsetCornerLat4), scaleCornerOffset(c.bottomLeft.latitudeDeg - cLat), payload);
            appendTagInt16(static_cast<std::uint32_t>(Tag::OffsetCornerLon4), scaleCornerOffset(c.bottomLeft.longitudeDeg - cLon), payload);
        }

        // Also encode standard ST 0601.8+ 4-byte Full Corner Coordinates (Tags 82..89)
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLat1Full), scaleLatitude(c.topLeft.latitudeDeg), payload);
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLon1Full), scaleLongitude(c.topLeft.longitudeDeg), payload);
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLat2Full), scaleLatitude(c.topRight.latitudeDeg), payload);
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLon2Full), scaleLongitude(c.topRight.longitudeDeg), payload);
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLat3Full), scaleLatitude(c.bottomRight.latitudeDeg), payload);
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLon3Full), scaleLongitude(c.bottomRight.longitudeDeg), payload);
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLat4Full), scaleLatitude(c.bottomLeft.latitudeDeg), payload);
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLon4Full), scaleLongitude(c.bottomLeft.longitudeDeg), payload);
    }

    // Tag 45: Target Error CE90
    if (msg.targetErrorCe90M.has_value()) {
        const double clamped = std::clamp(*msg.targetErrorCe90M, 0.0, 65535.0);
        appendTagUint16(static_cast<std::uint32_t>(Tag::TargetErrorCe90), static_cast<std::uint16_t>(std::round(clamped)), payload);
    }

    // Tag 46: Target Error LE90
    if (msg.targetErrorLe90M.has_value()) {
        const double clamped = std::clamp(*msg.targetErrorLe90M, 0.0, 65535.0);
        appendTagUint16(static_cast<std::uint32_t>(Tag::TargetErrorLe90), static_cast<std::uint16_t>(std::round(clamped)), payload);
    }

    // Tag 48: Security Local Set (Mandatory in MISB ST 0601)
    const SecurityMetadata sec = msg.security.value_or(SecurityMetadata{});
    const auto secBytes = encodeSecurityLocalSet(sec);
    appendTagBytes(static_cast<std::uint32_t>(Tag::SecurityLocalSet), secBytes, payload);

    // Tag 65: UAS LS Version (Mandatory in MISB ST 0601)
    const std::uint8_t uasVer = msg.uasLsVersion.value_or(16U);
    appendTagUint8(static_cast<std::uint32_t>(Tag::UasLsVersion), uasVer, payload);

    // Tag 73: MISB ST 0806 RVT Local Set
    if (msg.rvt.has_value()) {
        const auto rvtBytes = RvtEncoder::encode(*msg.rvt, false);
        appendTagBytes(static_cast<std::uint32_t>(Tag::RvtLocalSet), rvtBytes, payload);
    }

    // Tag 74: MISB ST 0903 VMTI Local Set
    if (msg.vmti.has_value()) {
        const auto vmtiBytes = VmtiEncoder::encode(*msg.vmti, false);
        appendTagBytes(static_cast<std::uint32_t>(Tag::VmtiLocalSet), vmtiBytes, payload);
    }

    // Tag 75: Sensor Altitude HAE
    if (msg.sensorAltitudeHaeM.has_value()) {
        appendTagUint16(static_cast<std::uint32_t>(Tag::SensorAltitudeHae), scaleAltitude(*msg.sensorAltitudeHaeM), payload);
    }

    // Tag 78: Frame Center Elevation HAE
    if (msg.frameCenterElevHaeM.has_value()) {
        appendTagUint16(static_cast<std::uint32_t>(Tag::FrameCenterElevHae), scaleAltitude(*msg.frameCenterElevHaeM), payload);
    }

    // Tag 118: Sensor Roll Angle
    if (msg.sensorRollAngleDeg.has_value()) {
        appendTagUint32(static_cast<std::uint32_t>(Tag::SensorRollAngle), scaleRelRoll(*msg.sensorRollAngleDeg), payload);
    }

    // Tag 100: Segment Local Sets
    for (const auto& seg : msg.segments) {
        std::vector<std::uint8_t> segBytes {};
        if (St1607Encoder::encodeSegment(seg, segBytes) == KlvStatus::Success) {
            appendTagBytes(static_cast<std::uint32_t>(Tag::SegmentLocalSet), segBytes, payload);
        }
    }

    // Tag 101: Amend Local Sets
    for (const auto& amend : msg.amends) {
        std::vector<std::uint8_t> amendBytes {};
        if (St1607Encoder::encodeAmend(amend, amendBytes) == KlvStatus::Success) {
            appendTagBytes(static_cast<std::uint32_t>(Tag::AmendLocalSet), amendBytes, payload);
        }
    }

    // Tag 1 (Checksum) adds 4 bytes: Tag (0x01), Length (0x02), 2 bytes CRC
    constexpr std::size_t kChecksumTagOverhead = 4U;
    const std::size_t totalPayloadLength = payload.size() + kChecksumTagOverhead;

    std::vector<std::uint8_t> packet;
    packet.reserve(kUniversalLabelSize + KlvBer::encodedLengthSize(totalPayloadLength) + totalPayloadLength);

    // 1. Append 16-byte Universal Label
    packet.insert(packet.end(), kMisb0601UniversalLabel.begin(), kMisb0601UniversalLabel.end());

    // 2. Append BER Length of the total payload (including Tag 1)
    KlvBer::encodeLength(totalPayloadLength, packet);

    // 3. Append metadata tags
    packet.insert(packet.end(), payload.begin(), payload.end());

    // 4. Append Tag 1 Key and Length: 0x01, 0x02
    packet.push_back(0x01U);
    packet.push_back(0x02U);

    // 5. Compute CRC-16 over entire packet up to and including the 0x01 0x02
    const std::uint16_t crc = KlvCrc::computeChecksumForTag1(packet.data(), packet.size());

    // 6. Append CRC-16 (Big-Endian)
    packet.push_back(static_cast<std::uint8_t>((crc >> 8U) & 0xFFU));
    packet.push_back(static_cast<std::uint8_t>(crc & 0xFFU));

    return packet;
}

} // namespace Klv
