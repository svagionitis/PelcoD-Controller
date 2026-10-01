/// @file SightlineGeneralBuilder.cpp
/// @brief Implementation of Sightline general system configuration serializers.

#include "SightlineGeneralBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineGeneralBuilder::buildGetVersionNumber()
{
    return SightlineFraming::buildPacket(MessageId::GetVersionNumber, {});
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildGetParameters(std::uint8_t queryId)
{
    const std::vector<std::uint8_t> payload { queryId };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildResetAllParameters(const MsgResetAllParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.resetType };
    return SightlineFraming::buildPacket(MessageId::ResetAllParameters, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildSaveParameters(const MsgSaveParameters& /*msg*/)
{
    return SightlineFraming::buildPacket(MessageId::SaveParameters, {});
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildSystemStatusMode(const MsgSystemStatusMode& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(6U);
    SightlineFraming::appendU16Le(payload, msg.systemStatusBits);
    SightlineFraming::appendU32Le(payload, msg.systemDebugBits);
    return SightlineFraming::buildPacket(MessageId::SystemStatusMode, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildGetHardwareId()
{
    return SightlineFraming::buildPacket(MessageId::GetHardwareID, {});
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildGetSystemStatusMode()
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::SystemStatusMode) };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildGetCurrentConfig()
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::CurrentConfiguration) };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildSetSystemValue(const MsgSystemValue& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(5U);
    payload.push_back(msg.systemValueId);
    SightlineFraming::appendU32Le(payload, msg.value);
    return SightlineFraming::buildPacket(MessageId::SetSystemValue, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildGetSystemValue(std::uint8_t systemValueId)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::SetSystemValue), systemValueId };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildTagData(const MsgTagData& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(2U + msg.data.size());
    SightlineFraming::appendU16Le(payload, msg.tagId);
    payload.insert(payload.end(), msg.data.begin(), msg.data.end());
    return SightlineFraming::buildPacket(MessageId::TagData, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildSetTagDataRate(const MsgTagDataRate& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(3U);
    SightlineFraming::appendU16Le(payload, msg.tagId);
    payload.push_back(msg.rate);
    return SightlineFraming::buildPacket(MessageId::TagDataRate, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildGetTagDataRate(std::uint16_t tagId)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(3U);
    payload.push_back(static_cast<std::uint8_t>(MessageId::TagDataRate));
    SightlineFraming::appendU16Le(payload, tagId);
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildSetTagSourceSelector(const MsgTagSourceSelector& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(3U);
    SightlineFraming::appendU16Le(payload, msg.tagId);
    payload.push_back(msg.source);
    return SightlineFraming::buildPacket(MessageId::TagSourceSelector, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildGetTagSourceSelector(std::uint16_t tagId)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(3U);
    payload.push_back(static_cast<std::uint8_t>(MessageId::TagSourceSelector));
    SightlineFraming::appendU16Le(payload, tagId);
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildDetailedTiming(const MsgDetailedTiming& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(16U);
    SightlineFraming::appendU32Le(payload, msg.frameNumber);
    SightlineFraming::appendU32Le(payload, msg.captureLatencyUs);
    SightlineFraming::appendU32Le(payload, msg.processLatencyUs);
    SightlineFraming::appendU32Le(payload, msg.transmitLatencyUs);
    return SightlineFraming::buildPacket(MessageId::DetailedTimingMessage, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildSetAppendedMetadata(const MsgAppendedMetadata& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.enable };
    return SightlineFraming::buildPacket(MessageId::AppendedMetadata, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildGetAppendedMetadata(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::AppendedMetadata), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildFrameIndex(const MsgFrameIndex& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(13U);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendU32Le(payload, msg.frameIndex);
    SightlineFraming::appendU64Le(payload, msg.timestampUs);
    return SightlineFraming::buildPacket(MessageId::FrameIndex, payload);
}

} // namespace Sightline
