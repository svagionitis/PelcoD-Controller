#include "RvtParser.h"
#include "KlvBer.h"
#include "KlvCrc.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Klv {

namespace {

[[nodiscard]] inline std::uint16_t readUint16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8U) |
                                      static_cast<std::uint16_t>(p[1]));
}

[[nodiscard]] inline std::uint32_t readUint24(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint32_t>(p[0]) << 16U) |
           (static_cast<std::uint32_t>(p[1]) << 8U) |
           static_cast<std::uint32_t>(p[2]);
}

[[nodiscard]] inline std::uint32_t readUint32(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint32_t>(p[0]) << 24U) |
           (static_cast<std::uint32_t>(p[1]) << 16U) |
           (static_cast<std::uint32_t>(p[2]) << 8U) |
           static_cast<std::uint32_t>(p[3]);
}

[[nodiscard]] inline std::uint64_t readUint64(const std::uint8_t* p) noexcept {
    std::uint64_t v { 0U };
    for (std::size_t i = 0U; i < 8U; ++i) {
        v = (v << 8U) | static_cast<std::uint64_t>(p[i]);
    }
    return v;
}

[[nodiscard]] inline std::int32_t readInt32(const std::uint8_t* p) noexcept {
    return static_cast<std::int32_t>(readUint32(p));
}

} // namespace

bool RvtParser::isRvt(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size < kMisb0806PrefixSize) {
        return false;
    }
    return std::memcmp(data, kMisb0806UniversalLabel.data(), kMisb0806PrefixSize) == 0;
}

double RvtParser::unscaleLatitude(std::int32_t rawVal) noexcept {
    if (rawVal == static_cast<std::int32_t>(0x80000000U)) {
        return 0.0;
    }
    constexpr double kMaxVal = 2147483647.0;
    return std::clamp(static_cast<double>(rawVal) * (90.0 / kMaxVal), -90.0, 90.0);
}

double RvtParser::unscaleLongitude(std::int32_t rawVal) noexcept {
    if (rawVal == static_cast<std::int32_t>(0x80000000U)) {
        return 0.0;
    }
    constexpr double kMaxVal = 2147483647.0;
    return std::clamp(static_cast<double>(rawVal) * (180.0 / kMaxVal), -180.0, 180.0);
}

double RvtParser::unscaleAltitude(std::uint16_t rawVal) noexcept {
    constexpr double kRange = 19900.0;
    constexpr double kMaxVal = 65535.0;
    constexpr double kOffset = -900.0;
    return (static_cast<double>(rawVal) * (kRange / kMaxVal)) + kOffset;
}

KlvStatus RvtParser::parse(
    const std::uint8_t* data,
    std::size_t size,
    RvtLocalSet& rvt,
    bool verifyChecksum) noexcept {
    if (data == nullptr || size == 0U) {
        return KlvStatus::BufferUnderflow;
    }

    std::size_t offset { 0U };
    std::size_t endOffset { size };

    const bool standalone = isRvt(data, size);
    if (standalone) {
        if (size < kRvtUniversalLabelSize) {
            return KlvStatus::BufferUnderflow;
        }

        std::size_t payloadLength { 0U };
        std::size_t berLenConsumed { 0U };
        if (!KlvBer::decodeLength(data + kRvtUniversalLabelSize, size - kRvtUniversalLabelSize, payloadLength, berLenConsumed)) {
            return KlvStatus::MalformedBerLength;
        }

        const std::size_t totalExpectedSize = kRvtUniversalLabelSize + berLenConsumed + payloadLength;
        if (size < totalExpectedSize) {
            return KlvStatus::BufferUnderflow;
        }

        if (verifyChecksum) {
            if (totalExpectedSize < 4U) {
                return KlvStatus::BufferUnderflow;
            }
            const std::uint32_t calcCrc = KlvCrc::calculateCrc32Mpeg(data, totalExpectedSize - 4U);
            const std::uint32_t storedCrc = readUint32(data + totalExpectedSize - 4U);
            if (calcCrc != storedCrc) {
                return KlvStatus::CrcMismatch;
            }
        }

        offset = kRvtUniversalLabelSize + berLenConsumed;
        endOffset = totalExpectedSize;
    }

    while (offset < endOffset) {
        std::uint32_t tag { 0U };
        std::size_t tagBytes { 0U };
        if (!KlvBer::decodeTag(data + offset, endOffset - offset, tag, tagBytes)) {
            return KlvStatus::TagError;
        }
        offset += tagBytes;

        std::size_t len { 0U };
        std::size_t lenBytes { 0U };
        if (!KlvBer::decodeLength(data + offset, endOffset - offset, len, lenBytes)) {
            return KlvStatus::MalformedBerLength;
        }
        offset += lenBytes;

        if (offset + len > endOffset) {
            return KlvStatus::BufferUnderflow;
        }

        const std::uint8_t* valPtr = data + offset;
        const auto rvtTag = static_cast<RvtTag>(tag);

        switch (rvtTag) {
            case RvtTag::PrecisionTimeStamp:
                if (len >= 8U) {
                    rvt.precisionTimeStampUs = readUint64(valPtr);
                }
                break;
            case RvtTag::PlatformTrueAirspeed:
                if (len >= 2U) {
                    rvt.platformTrueAirspeedMps = readUint16(valPtr);
                }
                break;
            case RvtTag::PlatformIndicatedAirspeed:
                if (len >= 2U) {
                    rvt.platformIndicatedAirspeedMps = readUint16(valPtr);
                }
                break;
            case RvtTag::TelemetryAccuracyIndicator:
                if (len >= 1U) {
                    rvt.telemetryAccuracy = valPtr[0];
                }
                break;
            case RvtTag::FragCircleRadius:
                if (len >= 2U) {
                    rvt.fragCircleRadiusM = readUint16(valPtr);
                }
                break;
            case RvtTag::FrameCode:
                if (len >= 4U) {
                    rvt.frameCode = readUint32(valPtr);
                }
                break;
            case RvtTag::UasLsVersion:
                if (len >= 1U) {
                    rvt.version = valPtr[0];
                }
                break;
            case RvtTag::VideoDataRate:
                if (len >= 4U) {
                    rvt.videoDataRate = readUint32(valPtr);
                }
                break;
            case RvtTag::DigitalVideoFileFormat:
                rvt.digitalVideoFileFormat = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case RvtTag::PointOfInterestLs: {
                PoiPack poi {};
                if (parsePoi(valPtr, len, poi)) {
                    rvt.pois.push_back(poi);
                }
                break;
            }
            case RvtTag::AreaOfInterestLs: {
                AoiPack aoi {};
                if (parseAoi(valPtr, len, aoi)) {
                    rvt.aois.push_back(aoi);
                }
                break;
            }
            case RvtTag::UserDefinedLs: {
                UserDefinedPack userDef {};
                if (parseUserDefined(valPtr, len, userDef)) {
                    rvt.userDefined.push_back(userDef);
                }
                break;
            }
            case RvtTag::AircraftMgrsZone:
                if (len >= 1U) {
                    if (!rvt.aircraftMgrs) rvt.aircraftMgrs.emplace();
                    rvt.aircraftMgrs->zone = valPtr[0];
                }
                break;
            case RvtTag::AircraftMgrsLatBandSquare:
                if (!rvt.aircraftMgrs) rvt.aircraftMgrs.emplace();
                rvt.aircraftMgrs->bandAndGridSquare = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case RvtTag::AircraftMgrsEasting:
                if (len >= 3U) {
                    if (!rvt.aircraftMgrs) rvt.aircraftMgrs.emplace();
                    rvt.aircraftMgrs->eastingM = readUint24(valPtr);
                }
                break;
            case RvtTag::AircraftMgrsNorthing:
                if (len >= 3U) {
                    if (!rvt.aircraftMgrs) rvt.aircraftMgrs.emplace();
                    rvt.aircraftMgrs->northingM = readUint24(valPtr);
                }
                break;
            case RvtTag::FrameCenterMgrsZone:
                if (len >= 1U) {
                    if (!rvt.frameCenterMgrs) rvt.frameCenterMgrs.emplace();
                    rvt.frameCenterMgrs->zone = valPtr[0];
                }
                break;
            case RvtTag::FrameCenterMgrsLatBandSquare:
                if (!rvt.frameCenterMgrs) rvt.frameCenterMgrs.emplace();
                rvt.frameCenterMgrs->bandAndGridSquare = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case RvtTag::FrameCenterMgrsEasting:
                if (len >= 3U) {
                    if (!rvt.frameCenterMgrs) rvt.frameCenterMgrs.emplace();
                    rvt.frameCenterMgrs->eastingM = readUint24(valPtr);
                }
                break;
            case RvtTag::FrameCenterMgrsNorthing:
                if (len >= 3U) {
                    if (!rvt.frameCenterMgrs) rvt.frameCenterMgrs.emplace();
                    rvt.frameCenterMgrs->northingM = readUint24(valPtr);
                }
                break;
            case RvtTag::Checksum:
                // Tag 1 checksum verified at packet framing level
                break;
            default:
                break;
        }

        offset += len;
    }

    return KlvStatus::Success;
}

bool RvtParser::parsePoi(
    const std::uint8_t* data,
    std::size_t size,
    PoiPack& poi) noexcept {
    if (data == nullptr || size == 0U) {
        return false;
    }

    std::size_t offset { 0U };
    std::size_t endOffset { size };

    // Support optional 16-byte POI UL prefix
    if (size >= kRvtUniversalLabelSize &&
        std::memcmp(data, kPoiUniversalLabel.data(), kRvtUniversalLabelSize) == 0) {
        offset = kRvtUniversalLabelSize;
        std::size_t len { 0U };
        std::size_t lenBytes { 0U };
        if (!KlvBer::decodeLength(data + offset, endOffset - offset, len, lenBytes)) {
            return false;
        }
        offset += lenBytes;
        endOffset = std::min(endOffset, offset + len);
    }

    bool hasNumber { false };
    bool hasLat { false };
    bool hasLon { false };

    while (offset < endOffset) {
        std::uint32_t tag { 0U };
        std::size_t tagBytes { 0U };
        if (!KlvBer::decodeTag(data + offset, endOffset - offset, tag, tagBytes)) {
            return false;
        }
        offset += tagBytes;

        std::size_t len { 0U };
        std::size_t lenBytes { 0U };
        if (!KlvBer::decodeLength(data + offset, endOffset - offset, len, lenBytes)) {
            return false;
        }
        offset += lenBytes;

        if (offset + len > endOffset) {
            return false;
        }

        const std::uint8_t* valPtr = data + offset;
        const auto poiTag = static_cast<PoiTag>(tag);

        switch (poiTag) {
            case PoiTag::PoiNumber:
                if (len >= 2U) {
                    poi.poiNumber = readUint16(valPtr);
                    hasNumber = true;
                }
                break;
            case PoiTag::PoiLatitude:
                if (len >= 4U) {
                    poi.latitudeDeg = unscaleLatitude(readInt32(valPtr));
                    hasLat = true;
                }
                break;
            case PoiTag::PoiLongitude:
                if (len >= 4U) {
                    poi.longitudeDeg = unscaleLongitude(readInt32(valPtr));
                    hasLon = true;
                }
                break;
            case PoiTag::PoiAltitude:
                if (len >= 2U) {
                    poi.altitudeMslM = unscaleAltitude(readUint16(valPtr));
                }
                break;
            case PoiTag::PoiType:
                if (len >= 1U) {
                    poi.type = static_cast<RvtTargetType>(static_cast<std::int8_t>(valPtr[0]));
                }
                break;
            case PoiTag::PoiText:
                poi.text = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case PoiTag::PoiSourceIcon:
                poi.sourceIcon = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case PoiTag::PoiSourceId:
                poi.sourceId = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case PoiTag::PoiLabel:
                poi.label = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case PoiTag::PoiOperationId:
                poi.operationId = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            default:
                break;
        }

        offset += len;
    }

    return hasNumber && hasLat && hasLon;
}

bool RvtParser::parseAoi(
    const std::uint8_t* data,
    std::size_t size,
    AoiPack& aoi) noexcept {
    if (data == nullptr || size == 0U) {
        return false;
    }

    std::size_t offset { 0U };
    std::size_t endOffset { size };

    // Support optional 16-byte AOI UL prefix
    if (size >= kRvtUniversalLabelSize &&
        std::memcmp(data, kAoiUniversalLabel.data(), kRvtUniversalLabelSize) == 0) {
        offset = kRvtUniversalLabelSize;
        std::size_t len { 0U };
        std::size_t lenBytes { 0U };
        if (!KlvBer::decodeLength(data + offset, endOffset - offset, len, lenBytes)) {
            return false;
        }
        offset += lenBytes;
        endOffset = std::min(endOffset, offset + len);
    }

    bool hasNumber { false };
    bool hasLat1 { false };
    bool hasLon1 { false };
    bool hasLat3 { false };
    bool hasLon3 { false };
    bool hasType { false };

    while (offset < endOffset) {
        std::uint32_t tag { 0U };
        std::size_t tagBytes { 0U };
        if (!KlvBer::decodeTag(data + offset, endOffset - offset, tag, tagBytes)) {
            return false;
        }
        offset += tagBytes;

        std::size_t len { 0U };
        std::size_t lenBytes { 0U };
        if (!KlvBer::decodeLength(data + offset, endOffset - offset, len, lenBytes)) {
            return false;
        }
        offset += lenBytes;

        if (offset + len > endOffset) {
            return false;
        }

        const std::uint8_t* valPtr = data + offset;
        const auto aoiTag = static_cast<AoiTag>(tag);

        switch (aoiTag) {
            case AoiTag::AoiNumber:
                if (len >= 2U) {
                    aoi.aoiNumber = readUint16(valPtr);
                    hasNumber = true;
                }
                break;
            case AoiTag::CornerLat1:
                if (len >= 4U) {
                    aoi.corner1Nw.latitudeDeg = unscaleLatitude(readInt32(valPtr));
                    hasLat1 = true;
                }
                break;
            case AoiTag::CornerLon1:
                if (len >= 4U) {
                    aoi.corner1Nw.longitudeDeg = unscaleLongitude(readInt32(valPtr));
                    hasLon1 = true;
                }
                break;
            case AoiTag::CornerLat3:
                if (len >= 4U) {
                    aoi.corner3Se.latitudeDeg = unscaleLatitude(readInt32(valPtr));
                    hasLat3 = true;
                }
                break;
            case AoiTag::CornerLon3:
                if (len >= 4U) {
                    aoi.corner3Se.longitudeDeg = unscaleLongitude(readInt32(valPtr));
                    hasLon3 = true;
                }
                break;
            case AoiTag::AoiType:
                if (len >= 1U) {
                    aoi.type = static_cast<RvtTargetType>(static_cast<std::int8_t>(valPtr[0]));
                    hasType = true;
                }
                break;
            case AoiTag::AoiText:
                aoi.text = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case AoiTag::AoiSourceId:
                aoi.sourceId = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case AoiTag::AoiLabel:
                aoi.label = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case AoiTag::AoiOperationId:
                aoi.operationId = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            default:
                break;
        }

        offset += len;
    }

    return hasNumber && hasLat1 && hasLon1 && hasLat3 && hasLon3 && hasType;
}

bool RvtParser::parseUserDefined(
    const std::uint8_t* data,
    std::size_t size,
    UserDefinedPack& userDef) noexcept {
    if (data == nullptr || size == 0U) {
        return false;
    }

    std::size_t offset { 0U };
    std::size_t endOffset { size };

    // Support optional 16-byte User Defined UL prefix
    if (size >= kRvtUniversalLabelSize &&
        std::memcmp(data, kUserDefinedUniversalLabel.data(), kRvtUniversalLabelSize) == 0) {
        offset = kRvtUniversalLabelSize;
        std::size_t len { 0U };
        std::size_t lenBytes { 0U };
        if (!KlvBer::decodeLength(data + offset, endOffset - offset, len, lenBytes)) {
            return false;
        }
        offset += lenBytes;
        endOffset = std::min(endOffset, offset + len);
    }

    bool hasIdType { false };
    bool hasData { false };

    while (offset < endOffset) {
        std::uint32_t tag { 0U };
        std::size_t tagBytes { 0U };
        if (!KlvBer::decodeTag(data + offset, endOffset - offset, tag, tagBytes)) {
            return false;
        }
        offset += tagBytes;

        std::size_t len { 0U };
        std::size_t lenBytes { 0U };
        if (!KlvBer::decodeLength(data + offset, endOffset - offset, len, lenBytes)) {
            return false;
        }
        offset += lenBytes;

        if (offset + len > endOffset) {
            return false;
        }

        const std::uint8_t* valPtr = data + offset;
        const auto uTag = static_cast<UserDefinedTag>(tag);

        switch (uTag) {
            case UserDefinedTag::NumericIdType:
                if (len >= 1U) {
                    const std::uint8_t raw = valPtr[0];
                    userDef.dataType = static_cast<RvtUserDataType>((raw >> 6U) & 0x03U);
                    userDef.numericId = raw & 0x3FU;
                    hasIdType = true;
                }
                break;
            case UserDefinedTag::UserData:
                userDef.data.assign(valPtr, valPtr + len);
                hasData = true;
                break;
            default:
                break;
        }

        offset += len;
    }

    return hasIdType && hasData;
}

} // namespace Klv
