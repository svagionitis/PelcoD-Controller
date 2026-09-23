#include "KlvParser.h"
#include "KlvBer.h"
#include "KlvCrc.h"
#include <algorithm>
#include <cstring>

namespace Klv {

namespace {

inline double unscaleLatitude(std::int32_t val) noexcept {
    return static_cast<double>(val) * (90.0 / 2147483647.0);
}

inline double unscaleLongitude(std::int32_t val) noexcept {
    return static_cast<double>(val) * (180.0 / 2147483647.0);
}

inline double unscaleAltitude(std::uint16_t val) noexcept {
    return (static_cast<double>(val) * (19900.0 / 65535.0)) - 900.0;
}

inline double unscaleHeading(std::uint16_t val) noexcept {
    return static_cast<double>(val) * (360.0 / 65535.0);
}

inline double unscalePitch(std::int16_t val) noexcept {
    return static_cast<double>(val) * (20.0 / 32767.0);
}

inline double unscaleRoll(std::int16_t val) noexcept {
    return static_cast<double>(val) * (50.0 / 32767.0);
}

inline double unscaleFov(std::uint16_t val) noexcept {
    return static_cast<double>(val) * (180.0 / 65535.0);
}

inline double unscaleRelAzimuth(std::uint32_t val) noexcept {
    return static_cast<double>(val) * (360.0 / 4294967295.0);
}

inline double unscaleRelElevation(std::int32_t val) noexcept {
    return static_cast<double>(val) * (180.0 / 2147483647.0);
}

inline double unscaleRelRoll(std::uint32_t val) noexcept {
    return static_cast<double>(val) * (360.0 / 4294967295.0);
}

inline double unscaleSlantRange(std::uint32_t val) noexcept {
    return static_cast<double>(val) * (5000000.0 / 4294967295.0);
}

inline double unscaleTargetWidth(std::uint16_t val) noexcept {
    return static_cast<double>(val) * (10000.0 / 65535.0);
}

inline std::uint16_t readUint16BigEndian(const std::uint8_t* ptr) noexcept {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(ptr[0]) << 8U) |
                                      static_cast<std::uint16_t>(ptr[1]));
}

inline std::int16_t readInt16BigEndian(const std::uint8_t* ptr) noexcept {
    return static_cast<std::int16_t>(readUint16BigEndian(ptr));
}

inline std::uint32_t readUint32BigEndian(const std::uint8_t* ptr) noexcept {
    return (static_cast<std::uint32_t>(ptr[0]) << 24U) |
           (static_cast<std::uint32_t>(ptr[1]) << 16U) |
           (static_cast<std::uint32_t>(ptr[2]) << 8U) |
           static_cast<std::uint32_t>(ptr[3]);
}

inline std::int32_t readInt32BigEndian(const std::uint8_t* ptr) noexcept {
    return static_cast<std::int32_t>(readUint32BigEndian(ptr));
}

inline std::uint64_t readUint64BigEndian(const std::uint8_t* ptr) noexcept {
    std::uint64_t val { 0U };
    for (std::size_t i = 0U; i < 8U; ++i) {
        val = (val << 8U) | static_cast<std::uint64_t>(ptr[i]);
    }
    return val;
}

} // namespace

bool KlvParser::isMisb0601(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size < kUniversalLabelSize) {
        return false;
    }

    // First 12 bytes must match SMPTE UL for MISB ST 0601
    return std::memcmp(data, kMisb0601UniversalLabel.data(), kMisb0601PrefixSize) == 0;
}

KlvStatus KlvParser::parseSecurityLocalSet(const std::uint8_t* data,
                                          std::size_t size,
                                          SecurityMetadata& security) {
    if (data == nullptr || size == 0U) {
        return KlvStatus::BufferUnderflow;
    }

    std::size_t offset = 0U;
    while (offset < size) {
        std::uint32_t tag { 0U };
        std::size_t tagBytes { 0U };
        if (!KlvBer::decodeTag(data + offset, size - offset, tag, tagBytes)) {
            return KlvStatus::TagError;
        }
        offset += tagBytes;

        std::size_t len { 0U };
        std::size_t lenBytes { 0U };
        if (!KlvBer::decodeLength(data + offset, size - offset, len, lenBytes)) {
            return KlvStatus::MalformedBerLength;
        }
        offset += lenBytes;

        if (offset + len > size) {
            return KlvStatus::BufferUnderflow;
        }

        const std::uint8_t* valPtr = data + offset;
        switch (tag) {
            case 1U: // Classification
                if (len >= 1U) {
                    security.classification = static_cast<SecurityClassification>(valPtr[0]);
                }
                break;
            case 2U: // Classifying Country
                security.classifyingCountry.assign(reinterpret_cast<const char*>(valPtr), len);
                break;
            case 3U: // SCI / SHI
                security.sciShiInfo.assign(reinterpret_cast<const char*>(valPtr), len);
                break;
            case 4U: // Caveats
                security.caveats.assign(reinterpret_cast<const char*>(valPtr), len);
                break;
            case 5U: // Releasing Instructions
                security.releasingInstructions.assign(reinterpret_cast<const char*>(valPtr), len);
                break;
            default:
                break;
        }
        offset += len;
    }

    return KlvStatus::Success;
}

KlvStatus KlvParser::parse(const std::uint8_t* data,
                          std::size_t size,
                          UasDatalinkMessage& message,
                          bool verifyChecksum) {
    if (data == nullptr || size < kUniversalLabelSize + 2U) {
        return KlvStatus::BufferUnderflow;
    }

    if (!isMisb0601(data, size)) {
        return KlvStatus::InvalidUniversalLabel;
    }

    std::size_t payloadLength { 0U };
    std::size_t berLenConsumed { 0U };
    if (!KlvBer::decodeLength(data + kUniversalLabelSize, size - kUniversalLabelSize, payloadLength, berLenConsumed)) {
        return KlvStatus::MalformedBerLength;
    }

    const std::size_t totalExpectedSize = kUniversalLabelSize + berLenConsumed + payloadLength;
    if (size < totalExpectedSize) {
        return KlvStatus::BufferUnderflow;
    }

    if (verifyChecksum && !KlvCrc::verifyPacket(data, totalExpectedSize)) {
        return KlvStatus::CrcMismatch;
    }

    std::size_t offset = kUniversalLabelSize + berLenConsumed;
    const std::size_t endOffset = totalExpectedSize;

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
        const auto tagEnum = static_cast<Tag>(tag);

        switch (tagEnum) {
            case Tag::PrecisionTimeStamp:
                if (len >= 8U) {
                    message.precisionTimeStampUs = readUint64BigEndian(valPtr);
                }
                break;
            case Tag::MissionId:
                message.missionId = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case Tag::PlatformTailNumber:
                message.platformTailNumber = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case Tag::PlatformHeading:
                if (len >= 2U) {
                    message.platformHeadingDeg = unscaleHeading(readUint16BigEndian(valPtr));
                }
                break;
            case Tag::PlatformPitch:
                if (len >= 2U) {
                    message.platformPitchDeg = unscalePitch(readInt16BigEndian(valPtr));
                }
                break;
            case Tag::PlatformRoll:
                if (len >= 2U) {
                    message.platformRollDeg = unscaleRoll(readInt16BigEndian(valPtr));
                }
                break;
            case Tag::PlatformDesignation:
                message.platformDesignation = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case Tag::ImageSourceSensor:
                message.imageSourceSensor = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case Tag::ImageCoordinateSystem:
                message.imageCoordinateSystem = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case Tag::SensorLatitude:
                if (len >= 4U) {
                    message.sensorLatitudeDeg = unscaleLatitude(readInt32BigEndian(valPtr));
                }
                break;
            case Tag::SensorLongitude:
                if (len >= 4U) {
                    message.sensorLongitudeDeg = unscaleLongitude(readInt32BigEndian(valPtr));
                }
                break;
            case Tag::SensorTrueAltitude:
                if (len >= 2U) {
                    message.sensorTrueAltitudeM = unscaleAltitude(readUint16BigEndian(valPtr));
                }
                break;
            case Tag::SensorHFOV:
                if (len >= 2U) {
                    message.sensorHfovDeg = unscaleFov(readUint16BigEndian(valPtr));
                }
                break;
            case Tag::SensorVFOV:
                if (len >= 2U) {
                    message.sensorVfovDeg = unscaleFov(readUint16BigEndian(valPtr));
                }
                break;
            case Tag::SensorRelAzimuth:
                if (len >= 4U) {
                    message.sensorRelAzimuthDeg = unscaleRelAzimuth(readUint32BigEndian(valPtr));
                }
                break;
            case Tag::SensorRelElevation:
                if (len >= 4U) {
                    message.sensorRelElevationDeg = unscaleRelElevation(readInt32BigEndian(valPtr));
                }
                break;
            case Tag::SensorRelRoll:
                if (len >= 4U) {
                    message.sensorRelRollDeg = unscaleRelRoll(readUint32BigEndian(valPtr));
                }
                break;
            case Tag::SlantRange:
                if (len >= 4U) {
                    message.slantRangeM = unscaleSlantRange(readUint32BigEndian(valPtr));
                }
                break;
            case Tag::TargetWidth:
                if (len >= 2U) {
                    message.targetWidthM = unscaleTargetWidth(readUint16BigEndian(valPtr));
                }
                break;
            case Tag::FrameCenterLat:
                if (len >= 4U) {
                    message.frameCenterLatDeg = unscaleLatitude(readInt32BigEndian(valPtr));
                }
                break;
            case Tag::FrameCenterLon:
                if (len >= 4U) {
                    message.frameCenterLonDeg = unscaleLongitude(readInt32BigEndian(valPtr));
                }
                break;
            case Tag::FrameCenterElev:
                if (len >= 2U) {
                    message.frameCenterElevM = unscaleAltitude(readUint16BigEndian(valPtr));
                }
                break;
            case Tag::CornerLat1:
                if (len >= 4U) {
                    if (!message.cornerCoordinates) message.cornerCoordinates.emplace();
                    message.cornerCoordinates->topLeft.latitudeDeg = unscaleLatitude(readInt32BigEndian(valPtr));
                }
                break;
            case Tag::CornerLon1:
                if (len >= 4U) {
                    if (!message.cornerCoordinates) message.cornerCoordinates.emplace();
                    message.cornerCoordinates->topLeft.longitudeDeg = unscaleLongitude(readInt32BigEndian(valPtr));
                }
                break;
            case Tag::CornerLat2:
                if (len >= 4U) {
                    if (!message.cornerCoordinates) message.cornerCoordinates.emplace();
                    message.cornerCoordinates->topRight.latitudeDeg = unscaleLatitude(readInt32BigEndian(valPtr));
                }
                break;
            case Tag::CornerLon2:
                if (len >= 4U) {
                    if (!message.cornerCoordinates) message.cornerCoordinates.emplace();
                    message.cornerCoordinates->topRight.longitudeDeg = unscaleLongitude(readInt32BigEndian(valPtr));
                }
                break;
            case Tag::CornerLat3:
                if (len >= 4U) {
                    if (!message.cornerCoordinates) message.cornerCoordinates.emplace();
                    message.cornerCoordinates->bottomRight.latitudeDeg = unscaleLatitude(readInt32BigEndian(valPtr));
                }
                break;
            case Tag::CornerLon3:
                if (len >= 4U) {
                    if (!message.cornerCoordinates) message.cornerCoordinates.emplace();
                    message.cornerCoordinates->bottomRight.longitudeDeg = unscaleLongitude(readInt32BigEndian(valPtr));
                }
                break;
            case Tag::CornerLat4:
                if (len >= 4U) {
                    if (!message.cornerCoordinates) message.cornerCoordinates.emplace();
                    message.cornerCoordinates->bottomLeft.latitudeDeg = unscaleLatitude(readInt32BigEndian(valPtr));
                }
                break;
            case Tag::CornerLon4:
                if (len >= 4U) {
                    if (!message.cornerCoordinates) message.cornerCoordinates.emplace();
                    message.cornerCoordinates->bottomLeft.longitudeDeg = unscaleLongitude(readInt32BigEndian(valPtr));
                }
                break;
            case Tag::SecurityLocalSet: {
                SecurityMetadata sec {};
                if (parseSecurityLocalSet(valPtr, len, sec) == KlvStatus::Success) {
                    message.security = sec;
                }
                break;
            }
            case Tag::UasLsVersion:
                if (len >= 1U) {
                    message.uasLsVersion = valPtr[0];
                }
                break;
            case Tag::Checksum:
                // Checksum already verified across whole packet
                break;
            default:
                // Unknown tag - safely skip
                break;
        }

        offset += len;
    }

    return KlvStatus::Success;
}

} // namespace Klv
