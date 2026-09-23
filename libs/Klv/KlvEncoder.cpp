#include "KlvEncoder.h"
#include "KlvBer.h"
#include "KlvCrc.h"
#include <algorithm>
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
    out.insert(out.end(), value.begin(), value.end());
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

    // Sub-tag 2: Classifying Country
    if (!security.classifyingCountry.empty()) {
        appendTagString(2U, security.classifyingCountry, inner);
    }
    // Sub-tag 3: SCI / SHI
    if (!security.sciShiInfo.empty()) {
        appendTagString(3U, security.sciShiInfo, inner);
    }
    // Sub-tag 4: Caveats
    if (!security.caveats.empty()) {
        appendTagString(4U, security.caveats, inner);
    }
    // Sub-tag 5: Releasing Instructions
    if (!security.releasingInstructions.empty()) {
        appendTagString(5U, security.releasingInstructions, inner);
    }
    return inner;
}

std::vector<std::uint8_t> KlvEncoder::encode(const UasDatalinkMessage& msg) {
    std::vector<std::uint8_t> payload;
    payload.reserve(256U);

    // Tag 2: Precision Time Stamp (8 bytes)
    if (msg.precisionTimeStampUs.has_value()) {
        appendTagUint64(static_cast<std::uint32_t>(Tag::PrecisionTimeStamp), *msg.precisionTimeStampUs, payload);
    }

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

    // Tags 26..33: 4 Footprint Corner Coordinates
    if (msg.cornerCoordinates.has_value()) {
        const auto& c = *msg.cornerCoordinates;
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLat1), scaleLatitude(c.topLeft.latitudeDeg), payload);
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLon1), scaleLongitude(c.topLeft.longitudeDeg), payload);
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLat2), scaleLatitude(c.topRight.latitudeDeg), payload);
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLon2), scaleLongitude(c.topRight.longitudeDeg), payload);
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLat3), scaleLatitude(c.bottomRight.latitudeDeg), payload);
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLon3), scaleLongitude(c.bottomRight.longitudeDeg), payload);
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLat4), scaleLatitude(c.bottomLeft.latitudeDeg), payload);
        appendTagInt32(static_cast<std::uint32_t>(Tag::CornerLon4), scaleLongitude(c.bottomLeft.longitudeDeg), payload);
    }

    // Tag 48: Security Local Set
    if (msg.security.has_value()) {
        const auto secBytes = encodeSecurityLocalSet(*msg.security);
        appendTagBytes(static_cast<std::uint32_t>(Tag::SecurityLocalSet), secBytes, payload);
    }

    // Tag 65: UAS LS Version
    if (msg.uasLsVersion.has_value()) {
        appendTagUint8(static_cast<std::uint32_t>(Tag::UasLsVersion), *msg.uasLsVersion, payload);
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
