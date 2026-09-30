/// @file SightlineNetworkBuilder.cpp
/// @brief Implementation of Sightline Network module builder.

#include "SightlineNetworkBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineNetworkBuilder::buildSetNetworkParameters(
    const MsgSetNetworkParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(17U);
    SightlineFraming::appendU32Le(payload, msg.ipAddress);
    SightlineFraming::appendU32Le(payload, msg.subnetMask);
    SightlineFraming::appendU32Le(payload, msg.gateway);
    payload.push_back(msg.dhcpEnable);
    SightlineFraming::appendU16Le(payload, msg.commandPort);
    SightlineFraming::appendU16Le(payload, msg.replyPort);
    return SightlineFraming::buildPacket(MessageId::SetNetworkParameters, payload);
}

std::vector<std::uint8_t> SightlineNetworkBuilder::buildSetEthernetVideo(
    const MsgSetEthernetVideoParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(10U);
    payload.push_back(msg.quality);
    payload.push_back(msg.foveal);
    payload.push_back(msg.frameStep);
    payload.push_back(msg.frameSize);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    SightlineFraming::appendU16Le(payload, msg.customWide);
    SightlineFraming::appendU16Le(payload, msg.customHigh);
    return SightlineFraming::buildPacket(MessageId::SetEthernetVideoParameters, payload);
}

std::vector<std::uint8_t> SightlineNetworkBuilder::buildSetEthernetDisplay(
    const MsgSetEthernetDisplayParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(13U);
    payload.push_back(msg.protocol);
    payload.push_back(static_cast<std::uint8_t>((msg.ipAddress >> 24U) & 0xFFU));
    payload.push_back(static_cast<std::uint8_t>((msg.ipAddress >> 16U) & 0xFFU));
    payload.push_back(static_cast<std::uint8_t>((msg.ipAddress >> 8U) & 0xFFU));
    payload.push_back(static_cast<std::uint8_t>(msg.ipAddress & 0xFFU));
    SightlineFraming::appendU16Le(payload, msg.port);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    SightlineFraming::appendU16Le(payload, msg.maxPacket);
    SightlineFraming::appendU16Le(payload, msg.maxRawPacket);
    return SightlineFraming::buildPacket(MessageId::SetEthernetDisplayParameters, payload);
}

std::vector<std::uint8_t> SightlineNetworkBuilder::buildGetNetworkParameters(
    std::uint8_t index)
{
    const std::vector<std::uint8_t> payload { index };
    return SightlineFraming::buildPacket(MessageId::GetNetworkParameters, payload);
}

std::vector<std::uint8_t> SightlineNetworkBuilder::buildGetEthernetVideo(
    std::uint16_t displayId)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(2U);
    SightlineFraming::appendU16Le(payload, displayId);
    return SightlineFraming::buildPacket(MessageId::GetEthernetVideoParameters, payload);
}

std::vector<std::uint8_t> SightlineNetworkBuilder::buildGetEthernetDisplay(
    std::uint16_t displayId)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(2U);
    SightlineFraming::appendU16Le(payload, displayId);
    return SightlineFraming::buildPacket(MessageId::GetEthernetDisplayParameters, payload);
}

std::vector<std::uint8_t> SightlineNetworkBuilder::buildGetNetworkList()
{
    return SightlineFraming::buildPacket(MessageId::GetNetworkList, {});
}

} // namespace Sightline
