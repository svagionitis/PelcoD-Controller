/// @file SightlineGeneralBuilder.cpp
/// @brief Implementation of Sightline general system configuration serializers.

#include "SightlineGeneralBuilder.h"
#include "SightlineKlvBuilder.h"

#include <algorithm>

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
    const std::size_t count { std::clamp(
        static_cast<std::size_t>(msg.numValues), std::size_t { 1U }, std::size_t { 4U }) };
    payload.reserve(1U + (count * 4U));
    payload.push_back(msg.systemValueId);
    SightlineFraming::appendU32Le(payload, msg.value);
    if (count >= 2U) {
        SightlineFraming::appendU32Le(payload, msg.value1);
    }
    if (count >= 3U) {
        SightlineFraming::appendU32Le(payload, msg.value2);
    }
    if (count >= 4U) {
        SightlineFraming::appendU32Le(payload, msg.value3);
    }
    return SightlineFraming::buildPacket(MessageId::SetSystemValue, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildSetTrafficControl(
    std::uint32_t rateKbps, std::uint32_t burstBytes, std::uint32_t mtuBytes)
{
    MsgSystemValue msg {};
    msg.systemValueId = MsgSystemValue::TrafficControl;
    msg.value = rateKbps;
    msg.value1 = burstBytes;
    msg.value2 = mtuBytes;
    msg.value3 = 0U;
    msg.numValues = 4U;
    return buildSetSystemValue(msg);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildGetSystemValue(std::uint8_t systemValueId)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::SetSystemValue), systemValueId };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildTagData(const MsgTagData& msg)
{
    return SightlineKlvBuilder::buildTagData(msg);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildSetTagDataRate(const MsgTagDataRate& msg)
{
    return SightlineKlvBuilder::buildTagDataRate(msg);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildGetTagDataRate(std::uint16_t tagId)
{
    return SightlineKlvBuilder::buildGetTagDataRate(static_cast<std::uint8_t>(tagId));
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildSetTagSourceSelector(const MsgTagSourceSelector& msg)
{
    return SightlineKlvBuilder::buildSetTagSourceSelector(msg);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildGetTagSourceSelector(std::uint16_t tagId)
{
    return SightlineKlvBuilder::buildGetTagSourceSelector(static_cast<std::uint8_t>(tagId));
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
    return SightlineKlvBuilder::buildSetAppendedMetadata(msg);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildGetAppendedMetadata(std::uint8_t cameraIndex)
{
    return SightlineKlvBuilder::buildGetAppendedMetadata(cameraIndex);
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
