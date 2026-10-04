#include "VmtiParser.h"
#include "KlvBer.h"
#include "KlvCrc.h"

#include <algorithm>
#include <cstring>

namespace Klv {

namespace {

bool readVarUint(const std::uint8_t* data, std::size_t len, std::uint32_t& out) noexcept {
    if (len == 0U || len > 4U) {
        return false;
    }
    std::uint32_t val { 0U };
    for (std::size_t i = 0U; i < len; ++i) {
        val = (val << 8U) | static_cast<std::uint32_t>(data[i]);
    }
    out = val;
    return true;
}

} // namespace

bool VmtiParser::isVmti(const std::uint8_t* data, std::size_t size) noexcept {
    if (data == nullptr || size < kVmtiUniversalLabelSize) {
        return false;
    }
    return std::equal(
        kMisb0903UniversalLabel.begin(),
        kMisb0903UniversalLabel.begin() + kMisb0903PrefixSize,
        data);
}

double VmtiParser::unscaleOffset(std::uint32_t rawVal) noexcept {
    const auto masked = static_cast<double>(rawVal & 0x00FFFFFFU);
    return (masked / 131072.0) - 19.2;
}

double VmtiParser::unscaleHae(std::uint16_t rawVal) noexcept {
    return static_cast<double>(rawVal) - 900.0;
}

bool VmtiParser::parseTarget(
    const std::uint8_t* data,
    std::size_t size,
    VTargetPack& target,
    std::uint32_t frameWidth,
    std::uint32_t frameHeight) noexcept {
    (void)frameHeight;
    if (data == nullptr || size == 0U) {
        return false;
    }

    std::size_t oidConsumed { 0U };
    if (!KlvBer::decodeTag(data, size, target.targetId, oidConsumed)) {
        return false;
    }

    std::size_t offset = oidConsumed;
    while (offset < size) {
        std::uint32_t tagId { 0U };
        std::size_t tagConsumed { 0U };
        if (!KlvBer::decodeTag(data + offset, size - offset, tagId, tagConsumed)) {
            return false;
        }
        offset += tagConsumed;

        std::size_t itemLen { 0U };
        std::size_t lenConsumed { 0U };
        if (!KlvBer::decodeLength(data + offset, size - offset, itemLen, lenConsumed)) {
            return false;
        }
        offset += lenConsumed;

        if (offset + itemLen > size) {
            return false;
        }

        const std::uint8_t* val = data + offset;
        switch (static_cast<VTargetTag>(tagId)) {
        case VTargetTag::TargetCentroid: {
            std::uint32_t pixNum { 0U };
            if (readVarUint(val, itemLen, pixNum)) {
                target.centroid = PixelCoord::fromPixelNumber(pixNum, frameWidth);
            }
            break;
        }
        case VTargetTag::BoundingBoxTopLeft: {
            std::uint32_t pixNum { 0U };
            if (readVarUint(val, itemLen, pixNum)) {
                if (!target.boundingBox.has_value()) {
                    target.boundingBox = PixelBoundingBox {};
                }
                target.boundingBox->topLeft = PixelCoord::fromPixelNumber(pixNum, frameWidth);
            }
            break;
        }
        case VTargetTag::BoundingBoxBottomRight: {
            std::uint32_t pixNum { 0U };
            if (readVarUint(val, itemLen, pixNum)) {
                if (!target.boundingBox.has_value()) {
                    target.boundingBox = PixelBoundingBox {};
                }
                target.boundingBox->bottomRight = PixelCoord::fromPixelNumber(pixNum, frameWidth);
            }
            break;
        }
        case VTargetTag::TargetPriority:
            if (itemLen >= 1U) {
                target.priority = val[0];
            }
            break;
        case VTargetTag::TargetConfidence:
            if (itemLen >= 1U) {
                target.confidence = val[0];
            }
            break;
        case VTargetTag::TargetHistory: {
            std::uint32_t hist { 0U };
            if (readVarUint(val, itemLen, hist)) {
                target.history = static_cast<std::uint16_t>(hist);
            }
            break;
        }
        case VTargetTag::PercentageTargetPixels:
            if (itemLen >= 1U) {
                target.percentagePixels = val[0];
            }
            break;
        case VTargetTag::TargetColor:
            if (itemLen >= 3U) {
                target.colorRgb = std::array<std::uint8_t, 3> { val[0], val[1], val[2] };
            }
            break;
        case VTargetTag::TargetLocationOffsetLat:
            if (itemLen >= 3U) {
                const auto raw = (static_cast<std::uint32_t>(val[0]) << 16U) |
                                 (static_cast<std::uint32_t>(val[1]) << 8U) |
                                 static_cast<std::uint32_t>(val[2]);
                if (!target.locationOffsetDeg.has_value()) {
                    target.locationOffsetDeg = GeoPoint2D {};
                }
                target.locationOffsetDeg->latitudeDeg = unscaleOffset(raw);
            }
            break;
        case VTargetTag::TargetLocationOffsetLon:
            if (itemLen >= 3U) {
                const auto raw = (static_cast<std::uint32_t>(val[0]) << 16U) |
                                 (static_cast<std::uint32_t>(val[1]) << 8U) |
                                 static_cast<std::uint32_t>(val[2]);
                if (!target.locationOffsetDeg.has_value()) {
                    target.locationOffsetDeg = GeoPoint2D {};
                }
                target.locationOffsetDeg->longitudeDeg = unscaleOffset(raw);
            }
            break;
        case VTargetTag::TargetHae:
            if (itemLen >= 2U) {
                const auto raw = static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(val[0]) << 8U) | static_cast<std::uint16_t>(val[1]));
                target.heightAboveEllipsoidM = unscaleHae(raw);
            }
            break;
        case VTargetTag::TargetLocation:
            if (itemLen >= 10U) {
                const auto rawLat = static_cast<std::int32_t>(
                    (static_cast<std::uint32_t>(val[0]) << 24U) |
                    (static_cast<std::uint32_t>(val[1]) << 16U) |
                    (static_cast<std::uint32_t>(val[2]) << 8U) |
                    static_cast<std::uint32_t>(val[3]));

                const auto rawLon = static_cast<std::int32_t>(
                    (static_cast<std::uint32_t>(val[4]) << 24U) |
                    (static_cast<std::uint32_t>(val[5]) << 16U) |
                    (static_cast<std::uint32_t>(val[6]) << 8U) |
                    static_cast<std::uint32_t>(val[7]));

                const auto rawHae = static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(val[8]) << 8U) | static_cast<std::uint16_t>(val[9]));

                const double lat = static_cast<double>(rawLat) * (90.0 / 2147483647.0);
                const double lon = static_cast<double>(rawLon) * (180.0 / 2147483647.0);
                const double hae = unscaleHae(rawHae);

                target.targetLocation = GeoPoint3D { lat, lon, hae };
            }
            break;
        case VTargetTag::DetectionStatus:
            if (itemLen >= 1U) {
                target.detectionStatus = val[0];
            }
            break;
        default:
            break;
        }

        offset += itemLen;
    }

    return true;
}

bool VmtiParser::parseSeries(
    const std::uint8_t* data,
    std::size_t size,
    std::vector<VTargetPack>& targets,
    std::uint32_t frameWidth,
    std::uint32_t frameHeight) noexcept {
    if (data == nullptr || size == 0U) {
        return true;
    }

    std::size_t offset { 0U };
    while (offset < size) {
        std::size_t packLen { 0U };
        std::size_t lenConsumed { 0U };
        if (!KlvBer::decodeLength(data + offset, size - offset, packLen, lenConsumed)) {
            return false;
        }
        offset += lenConsumed;

        if (offset + packLen > size) {
            return false;
        }

        VTargetPack pack;
        if (parseTarget(data + offset, packLen, pack, frameWidth, frameHeight)) {
            targets.push_back(pack);
        }

        offset += packLen;
    }

    return true;
}

KlvStatus VmtiParser::parse(
    const std::uint8_t* data,
    std::size_t size,
    VmtiLocalSet& vmti,
    bool verifyChecksum) noexcept {
    if (data == nullptr || size == 0U) {
        return KlvStatus::BufferUnderflow;
    }

    const std::uint8_t* payload = data;
    std::size_t payloadSize = size;

    if (isVmti(data, size)) {
        if (verifyChecksum) {
            if (size < kVmtiUniversalLabelSize + 4U) {
                return KlvStatus::BufferUnderflow;
            }
            if (!KlvCrc::verifyPacket(data, size)) {
                return KlvStatus::CrcMismatch;
            }
        }

        std::size_t offset = kVmtiUniversalLabelSize;
        std::size_t berLen { 0U };
        std::size_t lenConsumed { 0U };
        if (!KlvBer::decodeLength(data + offset, size - offset, berLen, lenConsumed)) {
            return KlvStatus::MalformedBerLength;
        }
        offset += lenConsumed;

        if (offset + berLen > size) {
            return KlvStatus::BufferUnderflow;
        }

        payload = data + offset;
        payloadSize = berLen;
    }

    const std::uint8_t* seriesData { nullptr };
    std::size_t seriesLen { 0U };

    std::size_t offset { 0U };
    while (offset < payloadSize) {
        std::uint32_t tagId { 0U };
        std::size_t tagConsumed { 0U };
        if (!KlvBer::decodeTag(payload + offset, payloadSize - offset, tagId, tagConsumed)) {
            return KlvStatus::TagError;
        }
        offset += tagConsumed;

        std::size_t itemLen { 0U };
        std::size_t lenConsumed { 0U };
        if (!KlvBer::decodeLength(payload + offset, payloadSize - offset, itemLen, lenConsumed)) {
            return KlvStatus::MalformedBerLength;
        }
        offset += lenConsumed;

        if (offset + itemLen > payloadSize) {
            return KlvStatus::BufferUnderflow;
        }

        const std::uint8_t* val = payload + offset;
        switch (static_cast<VmtiTag>(tagId)) {
        case VmtiTag::PrecisionTimeStamp:
            if (itemLen == 8U) {
                std::uint64_t ts { 0U };
                for (std::size_t i = 0U; i < 8U; ++i) {
                    ts = (ts << 8U) | static_cast<std::uint64_t>(val[i]);
                }
                vmti.precisionTimeStampUs = ts;
            }
            break;
        case VmtiTag::SystemName:
            if (itemLen > 0U) {
                vmti.systemName = std::string(reinterpret_cast<const char*>(val), itemLen);
            }
            break;
        case VmtiTag::VmtiLsVersion:
            if (itemLen >= 1U) {
                vmti.version = val[0];
            }
            break;
        case VmtiTag::TotalTargets: {
            std::uint32_t total { 0U };
            if (readVarUint(val, itemLen, total)) {
                vmti.totalTargetsDetected = total;
            }
            break;
        }
        case VmtiTag::ReportedTargets: {
            std::uint32_t reported { 0U };
            if (readVarUint(val, itemLen, reported)) {
                vmti.numTargetsReported = reported;
            }
            break;
        }
        case VmtiTag::FrameWidth: {
            std::uint32_t width { 0U };
            if (readVarUint(val, itemLen, width)) {
                vmti.frameWidth = width;
            }
            break;
        }
        case VmtiTag::FrameHeight: {
            std::uint32_t height { 0U };
            if (readVarUint(val, itemLen, height)) {
                vmti.frameHeight = height;
            }
            break;
        }
        case VmtiTag::MiisId: {
            MiisCoreId miis {};
            if (miis.decode(val, itemLen) == KlvStatus::Success) {
                vmti.miisId = miis;
            }
            break;
        }
        case VmtiTag::VTargetSeries:
            seriesData = val;
            seriesLen = itemLen;
            break;
        default:
            break;
        }

        offset += itemLen;
    }

    if (seriesData != nullptr && seriesLen > 0U) {
        if (!parseSeries(seriesData, seriesLen, vmti.targets, vmti.frameWidth, vmti.frameHeight)) {
            return KlvStatus::TagError;
        }
    }

    return KlvStatus::Success;
}

} // namespace Klv
