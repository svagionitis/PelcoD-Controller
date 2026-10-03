#include "RvtEncoder.h"
#include "KlvBer.h"
#include "KlvCrc.h"

#include <algorithm>
#include <cmath>

namespace Klv {

namespace {

void appendTagBytes(std::uint32_t tag, const std::vector<std::uint8_t>& val, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(val.size(), out);
    out.insert(out.end(), val.begin(), val.end());
}

void appendTagUint8(std::uint32_t tag, std::uint8_t val, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(1U, out);
    out.push_back(val);
}

void appendTagInt8(std::uint32_t tag, std::int8_t val, std::vector<std::uint8_t>& out) {
    appendTagUint8(tag, static_cast<std::uint8_t>(val), out);
}

void appendTagUint16(std::uint32_t tag, std::uint16_t val, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(2U, out);
    out.push_back(static_cast<std::uint8_t>((val >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(val & 0xFFU));
}

void appendTagUint24(std::uint32_t tag, std::uint32_t val, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(3U, out);
    out.push_back(static_cast<std::uint8_t>((val >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((val >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(val & 0xFFU));
}

void appendTagUint32(std::uint32_t tag, std::uint32_t val, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(4U, out);
    out.push_back(static_cast<std::uint8_t>((val >> 24U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((val >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((val >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(val & 0xFFU));
}

void appendTagInt32(std::uint32_t tag, std::int32_t val, std::vector<std::uint8_t>& out) {
    appendTagUint32(tag, static_cast<std::uint32_t>(val), out);
}

void appendTagUint64(std::uint32_t tag, std::uint64_t val, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(8U, out);
    for (int i = 7; i >= 0; --i) {
        out.push_back(static_cast<std::uint8_t>((val >> (static_cast<unsigned>(i) * 8U)) & 0xFFU));
    }
}

void appendTagString(std::uint32_t tag, const std::string& str, std::vector<std::uint8_t>& out) {
    if (str.empty()) return;
    KlvBer::encodeTag(tag, out);
    KlvBer::encodeLength(str.size(), out);
    for (const char ch : str) {
        out.push_back(static_cast<std::uint8_t>(ch));
    }
}

} // namespace

std::int32_t RvtEncoder::scaleLatitude(double latDeg) noexcept {
    constexpr double kMaxVal = 2147483647.0;
    const double clamped = std::clamp(latDeg, -90.0, 90.0);
    return static_cast<std::int32_t>(std::round(clamped * (kMaxVal / 90.0)));
}

std::int32_t RvtEncoder::scaleLongitude(double lonDeg) noexcept {
    constexpr double kMaxVal = 2147483647.0;
    const double clamped = std::clamp(lonDeg, -180.0, 180.0);
    return static_cast<std::int32_t>(std::round(clamped * (kMaxVal / 180.0)));
}

std::uint16_t RvtEncoder::scaleAltitude(double altM) noexcept {
    constexpr double kRange = 19900.0;
    constexpr double kMaxVal = 65535.0;
    constexpr double kOffset = 900.0;
    const double clamped = std::clamp(altM, -900.0, 19000.0);
    return static_cast<std::uint16_t>(std::round((clamped + kOffset) * (kMaxVal / kRange)));
}

void RvtEncoder::encodePoi(const PoiPack& poi, std::vector<std::uint8_t>& out) {
    std::vector<std::uint8_t> payload;

    // Tag 1: POI Number (Mandatory)
    appendTagUint16(static_cast<std::uint32_t>(PoiTag::PoiNumber), poi.poiNumber, payload);

    // Tag 2: POI Latitude (Mandatory)
    appendTagInt32(static_cast<std::uint32_t>(PoiTag::PoiLatitude), scaleLatitude(poi.latitudeDeg), payload);

    // Tag 3: POI Longitude (Mandatory)
    appendTagInt32(static_cast<std::uint32_t>(PoiTag::PoiLongitude), scaleLongitude(poi.longitudeDeg), payload);

    // Tag 4: POI Altitude MSL (Optional)
    if (poi.altitudeMslM.has_value()) {
        appendTagUint16(static_cast<std::uint32_t>(PoiTag::PoiAltitude), scaleAltitude(*poi.altitudeMslM), payload);
    }

    // Tag 5: POI Type (Optional)
    if (poi.type.has_value()) {
        appendTagInt8(static_cast<std::uint32_t>(PoiTag::PoiType), static_cast<std::int8_t>(*poi.type), payload);
    }

    // Tag 6: POI Text (Optional)
    if (poi.text.has_value()) {
        appendTagString(static_cast<std::uint32_t>(PoiTag::PoiText), *poi.text, payload);
    }

    // Tag 7: Source Icon (Optional)
    if (poi.sourceIcon.has_value()) {
        appendTagString(static_cast<std::uint32_t>(PoiTag::PoiSourceIcon), *poi.sourceIcon, payload);
    }

    // Tag 8: Source ID (Optional)
    if (poi.sourceId.has_value()) {
        appendTagString(static_cast<std::uint32_t>(PoiTag::PoiSourceId), *poi.sourceId, payload);
    }

    // Tag 9: Label (Optional)
    if (poi.label.has_value()) {
        appendTagString(static_cast<std::uint32_t>(PoiTag::PoiLabel), *poi.label, payload);
    }

    // Tag 10: Operation ID (Optional)
    if (poi.operationId.has_value()) {
        appendTagString(static_cast<std::uint32_t>(PoiTag::PoiOperationId), *poi.operationId, payload);
    }

    out.insert(out.end(), payload.begin(), payload.end());
}

void RvtEncoder::encodeAoi(const AoiPack& aoi, std::vector<std::uint8_t>& out) {
    std::vector<std::uint8_t> payload;

    // Tag 1: AOI Number (Mandatory)
    appendTagUint16(static_cast<std::uint32_t>(AoiTag::AoiNumber), aoi.aoiNumber, payload);

    // Tag 2: Corner Lat 1 (Mandatory)
    appendTagInt32(static_cast<std::uint32_t>(AoiTag::CornerLat1), scaleLatitude(aoi.corner1Nw.latitudeDeg), payload);

    // Tag 3: Corner Lon 1 (Mandatory)
    appendTagInt32(static_cast<std::uint32_t>(AoiTag::CornerLon1), scaleLongitude(aoi.corner1Nw.longitudeDeg), payload);

    // Tag 4: Corner Lat 3 (Mandatory)
    appendTagInt32(static_cast<std::uint32_t>(AoiTag::CornerLat3), scaleLatitude(aoi.corner3Se.latitudeDeg), payload);

    // Tag 5: Corner Lon 3 (Mandatory)
    appendTagInt32(static_cast<std::uint32_t>(AoiTag::CornerLon3), scaleLongitude(aoi.corner3Se.longitudeDeg), payload);

    // Tag 6: AOI Type (Mandatory)
    appendTagInt8(static_cast<std::uint32_t>(AoiTag::AoiType), static_cast<std::int8_t>(aoi.type), payload);

    // Tag 7: AOI Text (Optional)
    if (aoi.text.has_value()) {
        appendTagString(static_cast<std::uint32_t>(AoiTag::AoiText), *aoi.text, payload);
    }

    // Tag 8: Source ID (Optional)
    if (aoi.sourceId.has_value()) {
        appendTagString(static_cast<std::uint32_t>(AoiTag::AoiSourceId), *aoi.sourceId, payload);
    }

    // Tag 9: Label (Optional)
    if (aoi.label.has_value()) {
        appendTagString(static_cast<std::uint32_t>(AoiTag::AoiLabel), *aoi.label, payload);
    }

    // Tag 10: Operation ID (Optional)
    if (aoi.operationId.has_value()) {
        appendTagString(static_cast<std::uint32_t>(AoiTag::AoiOperationId), *aoi.operationId, payload);
    }

    out.insert(out.end(), payload.begin(), payload.end());
}

void RvtEncoder::encodeUserDefined(const UserDefinedPack& userDef, std::vector<std::uint8_t>& out) {
    std::vector<std::uint8_t> payload;

    // Tag 1: Numeric ID and Data Type
    const auto typeBits = static_cast<std::uint8_t>(static_cast<std::uint8_t>(userDef.dataType) << 6U);
    const auto idBits = static_cast<std::uint8_t>(userDef.numericId & 0x3FU);
    appendTagUint8(static_cast<std::uint32_t>(UserDefinedTag::NumericIdType), static_cast<std::uint8_t>(typeBits | idBits), payload);

    // Tag 2: User Data
    appendTagBytes(static_cast<std::uint32_t>(UserDefinedTag::UserData), userDef.data, payload);

    out.insert(out.end(), payload.begin(), payload.end());
}

std::vector<std::uint8_t> RvtEncoder::encode(const RvtLocalSet& rvt, bool standalone) {
    std::vector<std::uint8_t> payload;

    // Requirement ST 0806.4-02: Precision Time Stamp MUST be the first element
    if (rvt.precisionTimeStampUs.has_value()) {
        appendTagUint64(static_cast<std::uint32_t>(RvtTag::PrecisionTimeStamp), *rvt.precisionTimeStampUs, payload);
    }

    // Tag 3: Platform True Airspeed
    if (rvt.platformTrueAirspeedMps.has_value()) {
        appendTagUint16(static_cast<std::uint32_t>(RvtTag::PlatformTrueAirspeed), *rvt.platformTrueAirspeedMps, payload);
    }

    // Tag 4: Platform Indicated Airspeed
    if (rvt.platformIndicatedAirspeedMps.has_value()) {
        appendTagUint16(static_cast<std::uint32_t>(RvtTag::PlatformIndicatedAirspeed), *rvt.platformIndicatedAirspeedMps, payload);
    }

    // Tag 5: Telemetry Accuracy Indicator
    if (rvt.telemetryAccuracy.has_value()) {
        appendTagUint8(static_cast<std::uint32_t>(RvtTag::TelemetryAccuracyIndicator), *rvt.telemetryAccuracy, payload);
    }

    // Tag 6: Frag Circle Radius
    if (rvt.fragCircleRadiusM.has_value()) {
        appendTagUint16(static_cast<std::uint32_t>(RvtTag::FragCircleRadius), *rvt.fragCircleRadiusM, payload);
    }

    // Tag 7: Frame Code
    if (rvt.frameCode.has_value()) {
        appendTagUint32(static_cast<std::uint32_t>(RvtTag::FrameCode), *rvt.frameCode, payload);
    }

    // Tag 8: Version Number (2 for ST 0806.4)
    appendTagUint8(static_cast<std::uint32_t>(RvtTag::UasLsVersion), rvt.version, payload);

    // Tag 9: Video Data Rate
    if (rvt.videoDataRate.has_value()) {
        appendTagUint32(static_cast<std::uint32_t>(RvtTag::VideoDataRate), *rvt.videoDataRate, payload);
    }

    // Tag 10: Digital Video File Format
    if (rvt.digitalVideoFileFormat.has_value()) {
        appendTagString(static_cast<std::uint32_t>(RvtTag::DigitalVideoFileFormat), *rvt.digitalVideoFileFormat, payload);
    }

    // Tag 11: User Defined Local Sets
    for (const auto& u : rvt.userDefined) {
        std::vector<std::uint8_t> uBytes;
        encodeUserDefined(u, uBytes);
        appendTagBytes(static_cast<std::uint32_t>(RvtTag::UserDefinedLs), uBytes, payload);
    }

    // Tag 12: Points of Interest
    for (const auto& poi : rvt.pois) {
        std::vector<std::uint8_t> poiBytes;
        encodePoi(poi, poiBytes);
        appendTagBytes(static_cast<std::uint32_t>(RvtTag::PointOfInterestLs), poiBytes, payload);
    }

    // Tag 13: Areas of Interest
    for (const auto& aoi : rvt.aois) {
        std::vector<std::uint8_t> aoiBytes;
        encodeAoi(aoi, aoiBytes);
        appendTagBytes(static_cast<std::uint32_t>(RvtTag::AreaOfInterestLs), aoiBytes, payload);
    }

    // Tags 14..17: Aircraft MGRS
    if (rvt.aircraftMgrs.has_value()) {
        appendTagUint8(static_cast<std::uint32_t>(RvtTag::AircraftMgrsZone), rvt.aircraftMgrs->zone, payload);
        appendTagString(static_cast<std::uint32_t>(RvtTag::AircraftMgrsLatBandSquare), rvt.aircraftMgrs->bandAndGridSquare, payload);
        appendTagUint24(static_cast<std::uint32_t>(RvtTag::AircraftMgrsEasting), rvt.aircraftMgrs->eastingM, payload);
        appendTagUint24(static_cast<std::uint32_t>(RvtTag::AircraftMgrsNorthing), rvt.aircraftMgrs->northingM, payload);
    }

    // Tags 18..21: Frame Center MGRS
    if (rvt.frameCenterMgrs.has_value()) {
        appendTagUint8(static_cast<std::uint32_t>(RvtTag::FrameCenterMgrsZone), rvt.frameCenterMgrs->zone, payload);
        appendTagString(static_cast<std::uint32_t>(RvtTag::FrameCenterMgrsLatBandSquare), rvt.frameCenterMgrs->bandAndGridSquare, payload);
        appendTagUint24(static_cast<std::uint32_t>(RvtTag::FrameCenterMgrsEasting), rvt.frameCenterMgrs->eastingM, payload);
        appendTagUint24(static_cast<std::uint32_t>(RvtTag::FrameCenterMgrsNorthing), rvt.frameCenterMgrs->northingM, payload);
    }

    if (!standalone) {
        return payload;
    }

    // Tag 1 (Checksum) adds 6 bytes: Tag (0x01), Length (0x04), 4 bytes CRC-32
    constexpr std::size_t kChecksumTagOverhead = 6U;
    const std::size_t totalPayloadLength = payload.size() + kChecksumTagOverhead;

    std::vector<std::uint8_t> packet;
    packet.reserve(kRvtUniversalLabelSize + KlvBer::encodedLengthSize(totalPayloadLength) + totalPayloadLength);

    // 1. Append 16-byte Universal Label
    packet.insert(packet.end(), kMisb0806UniversalLabel.begin(), kMisb0806UniversalLabel.end());

    // 2. Append BER Length of total payload (including Tag 1)
    KlvBer::encodeLength(totalPayloadLength, packet);

    // 3. Append metadata tags
    packet.insert(packet.end(), payload.begin(), payload.end());

    // 4. Append Tag 1 Key and Length: 0x01, 0x04
    packet.push_back(0x01U);
    packet.push_back(0x04U);

    // 5. Compute CRC-32 (ISO/IEC 13818-1) over entire packet up to and including the 0x01 0x04
    const std::uint32_t crc = KlvCrc::calculateCrc32Mpeg(packet.data(), packet.size());

    // 6. Append CRC-32 (Big-Endian)
    packet.push_back(static_cast<std::uint8_t>((crc >> 24U) & 0xFFU));
    packet.push_back(static_cast<std::uint8_t>((crc >> 16U) & 0xFFU));
    packet.push_back(static_cast<std::uint8_t>((crc >> 8U) & 0xFFU));
    packet.push_back(static_cast<std::uint8_t>(crc & 0xFFU));

    return packet;
}

} // namespace Klv
