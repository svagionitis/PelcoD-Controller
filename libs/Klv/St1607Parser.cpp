#include "St1607Parser.h"
#include "GeoRegistrationParser.h"
#include "KlvBer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Klv {

namespace {

inline double unscaleLat(std::int32_t val) noexcept {
    return static_cast<double>(val) * (90.0 / 2147483647.0);
}

inline double unscaleLon(std::int32_t val) noexcept {
    return static_cast<double>(val) * (180.0 / 2147483647.0);
}

inline double unscaleAlt(std::uint16_t val) noexcept {
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

inline double unscaleAzimuth(std::uint32_t val) noexcept {
    return static_cast<double>(val) * (360.0 / 4294967295.0);
}

inline double unscaleElevation(std::int32_t val) noexcept {
    return static_cast<double>(val) * (180.0 / 2147483647.0);
}

inline double unscaleRelRoll(std::uint32_t val) noexcept {
    return static_cast<double>(val) * (360.0 / 4294967295.0);
}

inline double unscaleSlant(std::uint32_t val) noexcept {
    return static_cast<double>(val) * (5000000.0 / 4294967295.0);
}

inline double unscaleWidth(std::uint16_t val) noexcept {
    return static_cast<double>(val) * (10000.0 / 65535.0);
}

inline std::uint16_t readU16(const std::uint8_t* ptr) noexcept {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(ptr[0]) << 8U) |
                                      static_cast<std::uint16_t>(ptr[1]));
}

inline std::int16_t readI16(const std::uint8_t* ptr) noexcept {
    return static_cast<std::int16_t>(readU16(ptr));
}

inline std::uint32_t readU32(const std::uint8_t* ptr) noexcept {
    return (static_cast<std::uint32_t>(ptr[0]) << 24U) |
           (static_cast<std::uint32_t>(ptr[1]) << 16U) |
           (static_cast<std::uint32_t>(ptr[2]) << 8U) |
           static_cast<std::uint32_t>(ptr[3]);
}

inline std::int32_t readI32(const std::uint8_t* ptr) noexcept {
    return static_cast<std::int32_t>(readU32(ptr));
}

} // namespace

bool St1607Parser::isAmendLocalSet(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size < kUniversalLabelSize) {
        return false;
    }
    return std::equal(AmendLocalSetUl.begin(), AmendLocalSetUl.end(), data);
}

bool St1607Parser::isSegmentLocalSet(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size < kUniversalLabelSize) {
        return false;
    }
    return std::equal(SegmentLocalSetUl.begin(), SegmentLocalSetUl.end(), data);
}

bool St1607Parser::parseMsid(const std::uint8_t* data,
                             std::size_t size,
                             MetadataSubstreamId& outMsid,
                             std::size_t& bytesRead) noexcept {
    if (data == nullptr || size == 0U) {
        return false;
    }

    std::uint32_t localId { 0U };
    std::size_t oidBytes { 0U };
    if (!KlvBer::decodeTag(data, size, localId, oidBytes)) {
        return false;
    }

    outMsid.localId = localId;
    if (localId > 0U) {
        // ST 0601.19-46: Local ID > 0, UUID omitted
        outMsid.universalId.reset();
        bytesRead = oidBytes;
        return true;
    }

    // ST 0601.19-47: Local ID == 0, 16-byte UUID follows
    if (size < oidBytes + 16U) {
        return false;
    }

    std::array<std::uint8_t, 16> uuid {};
    std::copy_n(data + oidBytes, 16U, uuid.begin());
    outMsid.universalId = uuid;
    bytesRead = oidBytes + 16U;
    return true;
}

KlvStatus St1607Parser::parseAmend(const std::uint8_t* data,
                                   std::size_t size,
                                   AmendLocalSet& outSet) noexcept {
    if (data == nullptr && size > 0U) {
        return KlvStatus::BufferUnderflow;
    }

    std::size_t offset { 0U };
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

        const auto* valPtr = data + offset;
        switch (tag) {
            case 5U: // Platform Heading
                if (len >= 2U) outSet.platformHeadingDeg = unscaleHeading(readU16(valPtr));
                break;
            case 6U: // Platform Pitch
                if (len >= 2U) outSet.platformPitchDeg = unscalePitch(readI16(valPtr));
                break;
            case 7U: // Platform Roll
                if (len >= 2U) outSet.platformRollDeg = unscaleRoll(readI16(valPtr));
                break;
            case 11U: // Image Source Sensor
                outSet.imageSourceSensor = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case 12U: // Image Coordinate System
                outSet.imageCoordinateSystem = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case 13U: // Sensor Latitude
                if (len >= 4U) outSet.sensorLatitudeDeg = unscaleLat(readI32(valPtr));
                break;
            case 14U: // Sensor Longitude
                if (len >= 4U) outSet.sensorLongitudeDeg = unscaleLon(readI32(valPtr));
                break;
            case 15U: // Sensor True Altitude
                if (len >= 2U) outSet.sensorTrueAltitudeM = unscaleAlt(readU16(valPtr));
                break;
            case 16U: // Sensor HFOV
                if (len >= 2U) outSet.sensorHfovDeg = unscaleFov(readU16(valPtr));
                break;
            case 17U: // Sensor VFOV
                if (len >= 2U) outSet.sensorVfovDeg = unscaleFov(readU16(valPtr));
                break;
            case 18U: // Sensor Relative Azimuth
                if (len >= 4U) outSet.sensorRelAzimuthDeg = unscaleAzimuth(readU32(valPtr));
                break;
            case 19U: // Sensor Relative Elevation
                if (len >= 4U) outSet.sensorRelElevationDeg = unscaleElevation(readI32(valPtr));
                break;
            case 20U: // Sensor Relative Roll
                if (len >= 4U) outSet.sensorRelRollDeg = unscaleRelRoll(readU32(valPtr));
                break;
            case 21U: // Slant Range
                if (len >= 4U) outSet.slantRangeM = unscaleSlant(readU32(valPtr));
                break;
            case 22U: // Target Width
                if (len >= 2U) outSet.targetWidthM = unscaleWidth(readU16(valPtr));
                break;
            case 23U: // Frame Center Latitude
                if (len >= 4U) outSet.frameCenterLatDeg = unscaleLat(readI32(valPtr));
                break;
            case 24U: // Frame Center Longitude
                if (len >= 4U) outSet.frameCenterLonDeg = unscaleLon(readI32(valPtr));
                break;
            case 25U: // Frame Center Elevation
                if (len >= 2U) outSet.frameCenterElevM = unscaleAlt(readU16(valPtr));
                break;
            case 45U: // Target CE90
                if (len >= 2U) outSet.targetErrorCe90M = static_cast<double>(readU16(valPtr));
                break;
            case 46U: // Target LE90
                if (len >= 2U) outSet.targetErrorLe90M = static_cast<double>(readU16(valPtr));
                break;
            case 48U: { // Security Local Set subset (ST 1607.2-09)
                std::size_t secOff { 0U };
                while (secOff < len) {
                    std::uint32_t sTag { 0U };
                    std::size_t sTagB { 0U };
                    if (!KlvBer::decodeTag(valPtr + secOff, len - secOff, sTag, sTagB)) break;
                    secOff += sTagB;
                    std::size_t sLen { 0U };
                    std::size_t sLenB { 0U };
                    if (!KlvBer::decodeLength(valPtr + secOff, len - secOff, sLen, sLenB)) break;
                    secOff += sLenB;
                    if (secOff + sLen > len) break;
                    if (sTag == 12U && sLen >= 1U) {
                        outSet.securityCountryCodingMethod = valPtr[secOff];
                    } else if (sTag == 13U) {
                        outSet.securityObjectCountryCodes = std::string(reinterpret_cast<const char*>(valPtr + secOff), sLen);
                    }
                    secOff += sLen;
                }
                break;
            }
            case 75U: // Sensor Altitude HAE
                if (len >= 2U) outSet.sensorAltitudeHaeM = unscaleAlt(readU16(valPtr));
                break;
            case 78U: // Frame Center Elevation HAE
                if (len >= 2U) outSet.frameCenterElevHaeM = unscaleAlt(readU16(valPtr));
                break;
            case 98U: { // MISB ST 1601 Geo-Registration Local Set
                GeoRegistrationLocalSet geoReg {};
                if (GeoRegistrationParser::parse(valPtr, len, geoReg) == KlvStatus::Success) {
                    outSet.geoRegistration = geoReg;
                }
                break;
            }
            case 101U: { // Child Amend Local Set
                AmendLocalSet childAmend {};
                if (parseAmend(valPtr, len, childAmend) == KlvStatus::Success) {
                    outSet.childAmends.push_back(childAmend);
                }
                break;
            }
            case 118U: // Sensor Roll Angle
                if (len >= 4U) outSet.sensorRollAngleDeg = unscaleRelRoll(readU32(valPtr));
                break;
            case 143U: { // MSID Pack
                std::size_t msidRead { 0U };
                if (!parseMsid(valPtr, len, outSet.msid, msidRead)) {
                    // Malformed MSID pack
                }
                break;
            }
            default:
                break;
        }

        offset += len;
    }

    return KlvStatus::Success;
}

KlvStatus St1607Parser::parseSegment(const std::uint8_t* data,
                                     std::size_t size,
                                     SegmentLocalSet& outSet) noexcept {
    if (data == nullptr && size > 0U) {
        return KlvStatus::BufferUnderflow;
    }

    std::size_t offset { 0U };
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

        const auto* valPtr = data + offset;
        switch (tag) {
            case 11U:
                outSet.imageSourceSensor = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case 12U:
                outSet.imageCoordinateSystem = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case 13U:
                if (len >= 4U) outSet.sensorLatitudeDeg = unscaleLat(readI32(valPtr));
                break;
            case 14U:
                if (len >= 4U) outSet.sensorLongitudeDeg = unscaleLon(readI32(valPtr));
                break;
            case 15U:
                if (len >= 2U) outSet.sensorTrueAltitudeM = unscaleAlt(readU16(valPtr));
                break;
            case 16U:
                if (len >= 2U) outSet.sensorHfovDeg = unscaleFov(readU16(valPtr));
                break;
            case 17U:
                if (len >= 2U) outSet.sensorVfovDeg = unscaleFov(readU16(valPtr));
                break;
            case 18U:
                if (len >= 4U) outSet.sensorRelAzimuthDeg = unscaleAzimuth(readU32(valPtr));
                break;
            case 19U:
                if (len >= 4U) outSet.sensorRelElevationDeg = unscaleElevation(readI32(valPtr));
                break;
            case 20U:
                if (len >= 4U) outSet.sensorRelRollDeg = unscaleRelRoll(readU32(valPtr));
                break;
            case 21U:
                if (len >= 4U) outSet.slantRangeM = unscaleSlant(readU32(valPtr));
                break;
            case 22U:
                if (len >= 2U) outSet.targetWidthM = unscaleWidth(readU16(valPtr));
                break;
            case 23U:
                if (len >= 4U) outSet.frameCenterLatDeg = unscaleLat(readI32(valPtr));
                break;
            case 24U:
                if (len >= 4U) outSet.frameCenterLonDeg = unscaleLon(readI32(valPtr));
                break;
            case 25U:
                if (len >= 2U) outSet.frameCenterElevM = unscaleAlt(readU16(valPtr));
                break;
            case 48U: {
                std::size_t secOff { 0U };
                while (secOff < len) {
                    std::uint32_t sTag { 0U };
                    std::size_t sTagB { 0U };
                    if (!KlvBer::decodeTag(valPtr + secOff, len - secOff, sTag, sTagB)) break;
                    secOff += sTagB;
                    std::size_t sLen { 0U };
                    std::size_t sLenB { 0U };
                    if (!KlvBer::decodeLength(valPtr + secOff, len - secOff, sLen, sLenB)) break;
                    secOff += sLenB;
                    if (secOff + sLen > len) break;
                    if (sTag == 12U && sLen >= 1U) {
                        outSet.securityCountryCodingMethod = valPtr[secOff];
                    } else if (sTag == 13U) {
                        outSet.securityObjectCountryCodes = std::string(reinterpret_cast<const char*>(valPtr + secOff), sLen);
                    }
                    secOff += sLen;
                }
                break;
            }
            case 75U:
                if (len >= 2U) outSet.sensorAltitudeHaeM = unscaleAlt(readU16(valPtr));
                break;
            case 78U:
                if (len >= 2U) outSet.frameCenterElevHaeM = unscaleAlt(readU16(valPtr));
                break;
            case 94U:
                outSet.miisCoreId = std::string(reinterpret_cast<const char*>(valPtr), len);
                break;
            case 100U: { // Child Segment Local Set
                SegmentLocalSet childSeg {};
                if (parseSegment(valPtr, len, childSeg) == KlvStatus::Success) {
                    outSet.childSegments.push_back(childSeg);
                }
                break;
            }
            case 101U: { // Child Amend Local Set (ST 1607.2 allows Amend child inside Segment)
                AmendLocalSet childAmend {};
                if (parseAmend(valPtr, len, childAmend) == KlvStatus::Success) {
                    outSet.childAmends.push_back(childAmend);
                }
                break;
            }
            case 118U:
                if (len >= 4U) outSet.sensorRollAngleDeg = unscaleRelRoll(readU32(valPtr));
                break;
            case 143U: {
                std::size_t msidRead { 0U };
                if (!parseMsid(valPtr, len, outSet.msid, msidRead)) {
                    // Malformed MSID pack
                }
                break;
            }
            default:
                break;
        }

        offset += len;
    }

    return KlvStatus::Success;
}

KlvStatus St1607Parser::parsePacket(const std::uint8_t* data,
                                    std::size_t size,
                                    AmendLocalSet& outSet) noexcept {
    if (!isAmendLocalSet(data, size)) {
        return KlvStatus::InvalidUniversalLabel;
    }

    std::size_t offset = kUniversalLabelSize;
    std::size_t length { 0U };
    std::size_t lengthBytes { 0U };

    if (!KlvBer::decodeLength(data + offset, size - offset, length, lengthBytes)) {
        return KlvStatus::MalformedBerLength;
    }
    offset += lengthBytes;

    if (offset + length > size) {
        return KlvStatus::BufferUnderflow;
    }

    return parseAmend(data + offset, length, outSet);
}

KlvStatus St1607Parser::parsePacket(const std::uint8_t* data,
                                    std::size_t size,
                                    SegmentLocalSet& outSet) noexcept {
    if (!isSegmentLocalSet(data, size)) {
        return KlvStatus::InvalidUniversalLabel;
    }

    std::size_t offset = kUniversalLabelSize;
    std::size_t length { 0U };
    std::size_t lengthBytes { 0U };

    if (!KlvBer::decodeLength(data + offset, size - offset, length, lengthBytes)) {
        return KlvStatus::MalformedBerLength;
    }
    offset += lengthBytes;

    if (offset + length > size) {
        return KlvStatus::BufferUnderflow;
    }

    return parseSegment(data + offset, length, outSet);
}

} // namespace Klv
