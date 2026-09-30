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

std::vector<std::uint8_t> SightlineGeneralBuilder::buildResetAllParameters(
    const MsgResetAllParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.resetType };
    return SightlineFraming::buildPacket(MessageId::ResetAllParameters, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildSaveParameters(
    const MsgSaveParameters& /*msg*/)
{
    return SightlineFraming::buildPacket(MessageId::SaveParameters, {});
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildSystemStatusMode(
    const MsgSystemStatusMode& msg)
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
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::SystemStatusMode)
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineGeneralBuilder::buildGetCurrentConfig()
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::CurrentConfiguration)
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
