#include "VmtiEncoder.h"
#include "KlvBer.h"
#include "KlvCrc.h"

#include <algorithm>
#include <cmath>

namespace Klv {

namespace {

void appendTagVarUint(std::uint32_t tag, std::uint32_t val, std::vector<std::uint8_t>& out) {
    KlvBer::encodeTag(tag, out);
    if (val <= 0xFFU) {
        out.push_back(1U);
        out.push_back(static_cast<std::uint8_t>(val));
    } else if (val <= 0xFFFFU) {
        out.push_back(2U);
        out.push_back(static_cast<std::uint8_t>((val >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(val & 0xFFU));
    } else if (val <= 0xFFFFFFU) {
        out.push_back(3U);
        out.push_back(static_cast<std::uint8_t>((val >> 16U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((val >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(val & 0xFFU));
    } else {
        out.push_back(4U);
        out.push_back(static_cast<std::uint8_t>((val >> 24U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((val >> 16U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((val >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(val & 0xFFU));
    }
}

} // namespace

std::uint32_t VmtiEncoder::scaleOffset(double offsetDeg) noexcept {
    const double clamped = std::clamp(offsetDeg, -19.2, 19.2);
    const double scaled = std::round((clamped + 19.2) * 131072.0);
    return static_cast<std::uint32_t>(std::clamp(scaled, 0.0, 16777215.0));
}

std::uint16_t VmtiEncoder::scaleHae(double haeM) noexcept {
    const double clamped = std::clamp(haeM, -900.0, 19000.0);
    const double scaled = std::round(clamped + 900.0);
    return static_cast<std::uint16_t>(std::clamp(scaled, 0.0, 65535.0));
}

void VmtiEncoder::encodeTarget(
    const VTargetPack& target,
    std::uint32_t frameWidth,
    std::uint32_t frameHeight,
    std::vector<std::uint8_t>& out) {
    (void)frameHeight;

    // 1. Mandatory BER-OID targetId (no tag or length, raw BER-OID)
    KlvBer::encodeTag(target.targetId, out);

    // 2. Tag 1: targetCentroid
    if (target.centroid.has_value()) {
        const std::uint32_t pixNum = target.centroid->toPixelNumber(frameWidth);
        appendTagVarUint(static_cast<std::uint32_t>(VTargetTag::TargetCentroid), pixNum, out);
    }

    // 3. Tag 2 & 3: Bounding Box
    if (target.boundingBox.has_value()) {
        const std::uint32_t tlPix = target.boundingBox->topLeft.toPixelNumber(frameWidth);
        const std::uint32_t brPix = target.boundingBox->bottomRight.toPixelNumber(frameWidth);
        appendTagVarUint(static_cast<std::uint32_t>(VTargetTag::BoundingBoxTopLeft), tlPix, out);
        appendTagVarUint(static_cast<std::uint32_t>(VTargetTag::BoundingBoxBottomRight), brPix, out);
    }

    // 4. Tag 4: Target Priority
    if (target.priority.has_value()) {
        out.push_back(static_cast<std::uint8_t>(VTargetTag::TargetPriority));
        out.push_back(1U);
        out.push_back(*target.priority);
    }

    // 5. Tag 5: Target Confidence
    if (target.confidence.has_value()) {
        out.push_back(static_cast<std::uint8_t>(VTargetTag::TargetConfidence));
        out.push_back(1U);
        out.push_back(*target.confidence);
    }

    // 6. Tag 6: Target History
    if (target.history.has_value()) {
        appendTagVarUint(static_cast<std::uint32_t>(VTargetTag::TargetHistory), *target.history, out);
    }

    // 7. Tag 7: Percentage of Target Pixels
    if (target.percentagePixels.has_value()) {
        out.push_back(static_cast<std::uint8_t>(VTargetTag::PercentageTargetPixels));
        out.push_back(1U);
        out.push_back(*target.percentagePixels);
    }

    // 8. Tag 8: Target Color
    if (target.colorRgb.has_value()) {
        out.push_back(static_cast<std::uint8_t>(VTargetTag::TargetColor));
        out.push_back(3U);
        out.push_back((*target.colorRgb)[0]);
        out.push_back((*target.colorRgb)[1]);
        out.push_back((*target.colorRgb)[2]);
    }

    // 9. Tag 10 & 11: Lat/Lon Offset
    if (target.locationOffsetDeg.has_value()) {
        const std::uint32_t latOff = scaleOffset(target.locationOffsetDeg->latitudeDeg);
        const std::uint32_t lonOff = scaleOffset(target.locationOffsetDeg->longitudeDeg);

        out.push_back(static_cast<std::uint8_t>(VTargetTag::TargetLocationOffsetLat));
        out.push_back(3U);
        out.push_back(static_cast<std::uint8_t>((latOff >> 16U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((latOff >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(latOff & 0xFFU));

        out.push_back(static_cast<std::uint8_t>(VTargetTag::TargetLocationOffsetLon));
        out.push_back(3U);
        out.push_back(static_cast<std::uint8_t>((lonOff >> 16U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((lonOff >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(lonOff & 0xFFU));
    }

    // 10. Tag 12: Target HAE
    if (target.heightAboveEllipsoidM.has_value()) {
        const std::uint16_t haeVal = scaleHae(*target.heightAboveEllipsoidM);
        out.push_back(static_cast<std::uint8_t>(VTargetTag::TargetHae));
        out.push_back(2U);
        out.push_back(static_cast<std::uint8_t>((haeVal >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(haeVal & 0xFFU));
    }

    // 11. Tag 17: Target Location Pack (10 bytes: Lat 4, Lon 4, HAE 2)
    if (target.targetLocation.has_value()) {
        const double clampedLat = std::clamp(target.targetLocation->latitudeDeg, -90.0, 90.0);
        const auto rawLat = static_cast<std::int32_t>(
            std::clamp(std::round(clampedLat * (2147483647.0 / 90.0)), -2147483648.0, 2147483647.0));

        const double clampedLon = std::clamp(target.targetLocation->longitudeDeg, -180.0, 180.0);
        const auto rawLon = static_cast<std::int32_t>(
            std::clamp(std::round(clampedLon * (2147483647.0 / 180.0)), -2147483648.0, 2147483647.0));

        const std::uint16_t rawHae = scaleHae(target.targetLocation->altitudeM);

        out.push_back(static_cast<std::uint8_t>(VTargetTag::TargetLocation));
        out.push_back(10U);

        const auto uLat = static_cast<std::uint32_t>(rawLat);
        out.push_back(static_cast<std::uint8_t>((uLat >> 24U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((uLat >> 16U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((uLat >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(uLat & 0xFFU));

        const auto uLon = static_cast<std::uint32_t>(rawLon);
        out.push_back(static_cast<std::uint8_t>((uLon >> 24U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((uLon >> 16U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((uLon >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(uLon & 0xFFU));

        out.push_back(static_cast<std::uint8_t>((rawHae >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(rawHae & 0xFFU));
    }

    // 12. Tag 23: Detection Status
    if (target.detectionStatus.has_value()) {
        out.push_back(static_cast<std::uint8_t>(VTargetTag::DetectionStatus));
        out.push_back(1U);
        out.push_back(*target.detectionStatus);
    }
}

void VmtiEncoder::encodeSeries(
    const std::vector<VTargetPack>& targets,
    std::uint32_t frameWidth,
    std::uint32_t frameHeight,
    std::vector<std::uint8_t>& out) {
    std::vector<std::uint8_t> seriesPayload;
    seriesPayload.reserve(targets.size() * 32U);

    for (const auto& target : targets) {
        std::vector<std::uint8_t> packPayload;
        packPayload.reserve(32U);
        encodeTarget(target, frameWidth, frameHeight, packPayload);

        KlvBer::encodeLength(packPayload.size(), seriesPayload);
        seriesPayload.insert(seriesPayload.end(), packPayload.begin(), packPayload.end());
    }

    KlvBer::encodeTag(static_cast<std::uint32_t>(VmtiTag::VTargetSeries), out);
    KlvBer::encodeLength(seriesPayload.size(), out);
    out.insert(out.end(), seriesPayload.begin(), seriesPayload.end());
}

std::vector<std::uint8_t> VmtiEncoder::encode(const VmtiLocalSet& vmti, bool standalone) {
    std::vector<std::uint8_t> payload;
    payload.reserve(256U);

    // Tag 2: PrecisionTimeStamp (microsecond timestamp, 8 bytes)
    if (vmti.precisionTimeStampUs.has_value()) {
        KlvBer::encodeTag(static_cast<std::uint32_t>(VmtiTag::PrecisionTimeStamp), payload);
        payload.push_back(8U);
        const std::uint64_t ts = *vmti.precisionTimeStampUs;
        for (int i = 7; i >= 0; --i) {
            payload.push_back(static_cast<std::uint8_t>((ts >> (i * 8)) & 0xFFU));
        }
    }

    // Tag 3: SystemName
    if (vmti.systemName.has_value() && !vmti.systemName->empty()) {
        KlvBer::encodeTag(static_cast<std::uint32_t>(VmtiTag::SystemName), payload);
        KlvBer::encodeLength(vmti.systemName->size(), payload);
        for (const char ch : *vmti.systemName) {
            payload.push_back(static_cast<std::uint8_t>(ch));
        }
    }

    // Tag 4: VmtiLsVersionNum (1 byte)
    KlvBer::encodeTag(static_cast<std::uint32_t>(VmtiTag::VmtiLsVersion), payload);
    payload.push_back(1U);
    payload.push_back(vmti.version);

    // Tag 5: TotalTargets
    if (vmti.totalTargetsDetected.has_value()) {
        appendTagVarUint(static_cast<std::uint32_t>(VmtiTag::TotalTargets), *vmti.totalTargetsDetected, payload);
    }

    // Tag 6: ReportedTargets
    if (vmti.numTargetsReported.has_value()) {
        appendTagVarUint(static_cast<std::uint32_t>(VmtiTag::ReportedTargets), *vmti.numTargetsReported, payload);
    }

    // Tag 8: FrameWidth
    appendTagVarUint(static_cast<std::uint32_t>(VmtiTag::FrameWidth), vmti.frameWidth, payload);

    // Tag 9: FrameHeight
    appendTagVarUint(static_cast<std::uint32_t>(VmtiTag::FrameHeight), vmti.frameHeight, payload);

    // Tag 13: MIIS Core Identifier
    if (vmti.miisId.has_value()) {
        const auto miisBytes = vmti.miisId->encode();
        KlvBer::encodeTag(static_cast<std::uint32_t>(VmtiTag::MiisId), payload);
        KlvBer::encodeLength(miisBytes.size(), payload);
        payload.insert(payload.end(), miisBytes.begin(), miisBytes.end());
    }

    // Tag 101: VTargetSeries
    if (!vmti.targets.empty()) {
        encodeSeries(vmti.targets, vmti.frameWidth, vmti.frameHeight, payload);
    }

    if (!standalone) {
        return payload;
    }

    // Standalone packet layout:
    // [16-byte UL] [BER Length of items + checksum] [Items...] [Tag 1 Checksum (4 bytes)]
    std::vector<std::uint8_t> packet;
    packet.reserve(kVmtiUniversalLabelSize + 8U + payload.size() + 4U);

    packet.insert(packet.end(), kMisb0903UniversalLabel.begin(), kMisb0903UniversalLabel.end());

    const std::size_t totalPayloadLen = payload.size() + 4U; // 4 bytes for Checksum Tag 1 (Tag 1, Len 2, CRC 2)
    KlvBer::encodeLength(totalPayloadLen, packet);

    packet.insert(packet.end(), payload.begin(), payload.end());

    // Tag 1 Checksum: Tag=1, Len=2
    packet.push_back(static_cast<std::uint8_t>(VmtiTag::Checksum));
    packet.push_back(2U);

    // Checksum covers from byte 0 of UL through the length byte of the checksum
    const std::uint16_t crc = KlvCrc::computeChecksumForTag1(packet.data(), packet.size());
    packet.push_back(static_cast<std::uint8_t>((crc >> 8U) & 0xFFU));
    packet.push_back(static_cast<std::uint8_t>(crc & 0xFFU));

    return packet;
}

} // namespace Klv
