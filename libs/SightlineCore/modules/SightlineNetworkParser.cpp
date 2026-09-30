/// @file SightlineNetworkParser.cpp
/// @brief Implementation of Sightline Network module response deserializer.

#include "SightlineNetworkParser.h"

namespace Sightline {

bool SightlineNetworkParser::parseNetworkParameters(
    const std::vector<std::uint8_t>& packet, MsgSetNetworkParameters& out)
{
    const auto id = SightlineFraming::identifyMessage(packet);
    if (id != MessageId::CurrentNetworkParameters && id != MessageId::SetNetworkParameters) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 17U) {
        return false;
    }

    out.ipAddress = SightlineFraming::readU32Le(payload.data());
    out.subnetMask = SightlineFraming::readU32Le(payload.data() + 4U);
    out.gateway = SightlineFraming::readU32Le(payload.data() + 8U);
    out.dhcpEnable = payload[12U];
    out.commandPort = SightlineFraming::readU16Le(payload.data() + 13U);
    out.replyPort = SightlineFraming::readU16Le(payload.data() + 15U);
    return true;
}

bool SightlineNetworkParser::parseEthernetVideo(
    const std::vector<std::uint8_t>& packet, MsgSetEthernetVideoParameters& out)
{
    const auto id = SightlineFraming::identifyMessage(packet);
    if (id != MessageId::CurrentEthernetVideoParameters && id != MessageId::SetEthernetVideoParameters) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 10U) {
        return false;
    }

    out.quality = payload[0U];
    out.foveal = payload[1U];
    out.frameStep = payload[2U];
    out.frameSize = payload[3U];
    out.displayId = SightlineFraming::readU16Le(payload.data() + 4U);
    out.customWide = SightlineFraming::readU16Le(payload.data() + 6U);
    out.customHigh = SightlineFraming::readU16Le(payload.data() + 8U);
    return true;
}

bool SightlineNetworkParser::parseEthernetDisplay(
    const std::vector<std::uint8_t>& packet, MsgSetEthernetDisplayParameters& out)
{
    const auto id = SightlineFraming::identifyMessage(packet);
    if (id != MessageId::CurrentEthernetDisplayParameters && id != MessageId::SetEthernetDisplayParameters) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 13U) {
        return false;
    }

    out.protocol = payload[0U];
    out.ipAddress = (static_cast<std::uint32_t>(payload[1U]) << 24U)
        | (static_cast<std::uint32_t>(payload[2U]) << 16U)
        | (static_cast<std::uint32_t>(payload[3U]) << 8U)
        | static_cast<std::uint32_t>(payload[4U]);
    out.port = SightlineFraming::readU16Le(payload.data() + 5U);
    out.displayId = SightlineFraming::readU16Le(payload.data() + 7U);
    out.maxPacket = SightlineFraming::readU16Le(payload.data() + 9U);
    out.maxRawPacket = SightlineFraming::readU16Le(payload.data() + 11U);
    return true;
}

bool SightlineNetworkParser::parseNetworkList(
    const std::vector<std::uint8_t>& packet, MsgCurrentNetworkList& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CurrentNetworkList) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.empty()) {
        return false;
    }

    out.numInterfaces = payload[0U];
    out.interfaceNames.clear();

    std::size_t offset { 1U };
    while (offset < payload.size() && out.interfaceNames.size() < out.numInterfaces) {
        std::string name {};
        while (offset < payload.size() && payload[offset] != '\0') {
            name.push_back(static_cast<char>(payload[offset++]));
        }
        if (offset < payload.size() && payload[offset] == '\0') {
            ++offset;
        }
        if (!name.empty()) {
            out.interfaceNames.push_back(std::move(name));
        }
    }
    return true;
}

} // namespace Sightline
