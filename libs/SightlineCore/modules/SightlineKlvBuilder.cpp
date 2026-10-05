/// @file SightlineKlvBuilder.cpp
/// @brief Implementation of Sightline KLV Metadata builder conforming to IDD 3.11 and EAN-KLV-Metadata.

#include "SightlineKlvBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineKlvBuilder::buildSetMetadataValues(const MsgSetMetadataValues& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(44U);
    SightlineFraming::appendU16Le(payload, msg.validDataMask);
    SightlineFraming::appendU64Le(payload, msg.utcTime);
    SightlineFraming::appendU16Le(payload, msg.heading);
    SightlineFraming::appendS16Le(payload, msg.pitch);
    SightlineFraming::appendS16Le(payload, msg.roll);
    SightlineFraming::appendS32Le(payload, msg.lat);
    SightlineFraming::appendS32Le(payload, msg.lon);
    SightlineFraming::appendU16Le(payload, msg.alt);
    SightlineFraming::appendU16Le(payload, msg.hfov);
    SightlineFraming::appendU16Le(payload, msg.vfov);
    SightlineFraming::appendU32Le(payload, msg.az);
    SightlineFraming::appendS32Le(payload, msg.el);
    SightlineFraming::appendU32Le(payload, msg.sensorRoll);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    return SightlineFraming::buildPacket(MessageId::SetMetadataValues, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildMetadataStaticValues(const MsgMetadataStaticValues& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(msg.value.size() + 4U);
    payload.push_back(static_cast<std::uint8_t>(msg.type));
    payload.push_back(static_cast<std::uint8_t>(msg.value.size()));
    payload.insert(payload.end(), msg.value.begin(), msg.value.end());
    SightlineFraming::appendU16Le(payload, msg.displayId);
    return SightlineFraming::buildPacket(MessageId::MetadataStaticValues, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildSetMetadataFrameValues(const MsgSetMetadataFrameValues& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(49U);
    SightlineFraming::appendU16Le(payload, msg.validDataMask);
    SightlineFraming::appendS32Le(payload, msg.frameCenterLat);
    SightlineFraming::appendS32Le(payload, msg.frameCenterLon);
    SightlineFraming::appendU16Le(payload, msg.frameCenterEl);
    SightlineFraming::appendU16Le(payload, msg.frameWidth);
    SightlineFraming::appendU32Le(payload, msg.slantRange);
    payload.push_back(msg.userSuppliedFlags);
    SightlineFraming::appendS32Le(payload, msg.targetLat);
    SightlineFraming::appendS32Le(payload, msg.targetLon);
    SightlineFraming::appendU16Le(payload, msg.targetEl);
    payload.push_back(msg.targetTrackGateHeight);
    payload.push_back(msg.targetTrackGateWidth);
    SightlineFraming::appendS16Le(payload, msg.offsetCornerLat1);
    SightlineFraming::appendS16Le(payload, msg.offsetCornerLon1);
    SightlineFraming::appendS16Le(payload, msg.offsetCornerLat2);
    SightlineFraming::appendS16Le(payload, msg.offsetCornerLon2);
    SightlineFraming::appendS16Le(payload, msg.offsetCornerLat3);
    SightlineFraming::appendS16Le(payload, msg.offsetCornerLon3);
    SightlineFraming::appendS16Le(payload, msg.offsetCornerLat4);
    SightlineFraming::appendS16Le(payload, msg.offsetCornerLon4);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    return SightlineFraming::buildPacket(MessageId::SetMetadataFrameValues, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildSetKlvData(const MsgSetKlvData& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(msg.klvData.size() + 2U);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    payload.insert(payload.end(), msg.klvData.begin(), msg.klvData.end());
    return SightlineFraming::buildPacket(MessageId::SetKlvData, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildSetMetadataRate(const MsgSetMetadataRate& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(11U);
    SightlineFraming::appendU64Le(payload, msg.enables);
    payload.push_back(msg.frameStep);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    return SightlineFraming::buildPacket(MessageId::SetMetadataRate, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildSetVmti(const MsgSetVmti& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve((msg.targets.size() * 12U) + 3U);
    payload.push_back(static_cast<std::uint8_t>(msg.targets.size()));
    for (const auto& t : msg.targets) {
        payload.push_back(t.targetId);
        payload.push_back(t.confidence);
        SightlineFraming::appendU16Le(payload, t.col);
        SightlineFraming::appendU16Le(payload, t.row);
        SightlineFraming::appendU16Le(payload, t.width);
        SightlineFraming::appendU16Le(payload, t.height);
        SightlineFraming::appendU16Le(payload, t.newTargetDetectionFlag);
    }
    SightlineFraming::appendU16Le(payload, msg.displayId);
    return SightlineFraming::buildPacket(MessageId::SetVMTI, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildAppendedMetadata(const MsgAppendedMetadata& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(msg.data.size() + 2U);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    payload.insert(payload.end(), msg.data.begin(), msg.data.end());
    return SightlineFraming::buildPacket(MessageId::AppendedMetadata, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildTagData(const MsgTagData& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(msg.data.size() + 9U);
    payload.push_back(msg.reserved1);
    payload.push_back(msg.reserved2);
    payload.push_back(msg.tagId);
    payload.push_back(msg.tagSubId);
    SightlineFraming::appendU16Le(payload, msg.reservedInternal);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    payload.push_back(static_cast<std::uint8_t>(msg.data.size()));
    payload.insert(payload.end(), msg.data.begin(), msg.data.end());
    return SightlineFraming::buildPacket(MessageId::TagData, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildSetTagDataRate(const MsgTagDataRate& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(9U);
    payload.push_back(msg.reserved1);
    payload.push_back(msg.reserved2);
    payload.push_back(msg.mode);
    payload.push_back(msg.tagId1);
    payload.push_back(msg.tagId2);
    SightlineFraming::appendU16Le(payload, msg.frameStep);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    return SightlineFraming::buildPacket(MessageId::TagDataRate, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildGetTagDataRate(std::uint8_t tagId, std::uint16_t displayId)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(5U);
    payload.push_back(static_cast<std::uint8_t>(MessageId::TagDataRate));
    payload.push_back(0U);
    payload.push_back(tagId);
    SightlineFraming::appendU16Le(payload, displayId);
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildSetTagSourceSelector(const MsgTagSourceSelector& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(9U);
    payload.push_back(msg.reserved1);
    payload.push_back(msg.reserved2);
    payload.push_back(msg.mode);
    payload.push_back(msg.tagId1);
    payload.push_back(msg.tagId2);
    SightlineFraming::appendU16Le(payload, msg.selector);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    return SightlineFraming::buildPacket(MessageId::TagSourceSelector, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildGetTagSourceSelector(std::uint8_t tagId, std::uint16_t displayId)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(5U);
    payload.push_back(static_cast<std::uint8_t>(MessageId::TagSourceSelector));
    payload.push_back(0U);
    payload.push_back(tagId);
    SightlineFraming::appendU16Le(payload, displayId);
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildAncillaryTextMetadata(const MsgAncillaryTextMetadata& msg)
{
    std::vector<std::uint8_t> payload {};
    SightlineFraming::appendU64Le(payload, msg.creationTime);
    auto appendStr = [&](const std::string& str) {
        payload.push_back(static_cast<std::uint8_t>(str.size()));
        for (const auto c : str) {
            payload.push_back(static_cast<std::uint8_t>(c));
        }
    };
    appendStr(msg.source);
    appendStr(msg.originator);
    appendStr(msg.messageBody);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    return SightlineFraming::buildPacket(MessageId::AncillaryTextMetadata, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildVmtiChips(const MsgVmtiChips& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(10U);
    payload.push_back(msg.mode);
    payload.push_back(static_cast<std::uint8_t>(msg.format));
    payload.push_back(static_cast<std::uint8_t>(msg.sizeType));
    SightlineFraming::appendU16Le(payload, msg.sizeHint);
    payload.push_back(msg.maxPerFrame);
    payload.push_back(msg.minFramesBetween);
    payload.push_back(msg.reserved0);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    return SightlineFraming::buildPacket(MessageId::VMTIChips, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildCursorOnTarget(const MsgCursorOnTarget& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(12U);
    SightlineFraming::appendU16Le(payload, msg.mode);
    // ipAddr and port are big-endian network byte order per IDD
    payload.push_back(static_cast<std::uint8_t>((msg.ipAddress >> 24U) & 0xFFU));
    payload.push_back(static_cast<std::uint8_t>((msg.ipAddress >> 16U) & 0xFFU));
    payload.push_back(static_cast<std::uint8_t>((msg.ipAddress >> 8U) & 0xFFU));
    payload.push_back(static_cast<std::uint8_t>(msg.ipAddress & 0xFFU));
    payload.push_back(static_cast<std::uint8_t>((msg.port >> 8U) & 0xFFU));
    payload.push_back(static_cast<std::uint8_t>(msg.port & 0xFFU));
    SightlineFraming::appendU16Le(payload, msg.rate);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    return SightlineFraming::buildPacket(MessageId::CursorOnTarget, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildVmtiFields(const MsgVmtiFields& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(6U);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    SightlineFraming::appendU16Le(payload, msg.fields);
    SightlineFraming::appendU16Le(payload, msg.ontologySeriesRate);
    return SightlineFraming::buildPacket(MessageId::VMTIFields, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildGetMetadataValues()
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::SetMetadataValues) };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildGetMetadataStaticValues()
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::MetadataStaticValues) };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildGetMetadataFrame()
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::SetMetadataFrameValues) };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildGetMetadataRate()
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::SetMetadataRate) };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildGetVmtiChips()
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::VMTIChips) };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildGetCursorOnTarget()
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::CursorOnTarget) };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildGetVmtiFields()
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::VMTIFields) };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildGetAppendedMetadata(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::AppendedMetadata), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
