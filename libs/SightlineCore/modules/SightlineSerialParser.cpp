/// @file SightlineSerialParser.cpp
/// @brief Implementation of Sightline SLA serial port and GPIO frame deserializers.

#include "SightlineSerialParser.h"

namespace Sightline {

bool SightlineSerialParser::parsePortConfiguration(
    const std::vector<std::uint8_t>& packet, MsgSetPortConfiguration& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::CurrentPortConfiguration && id != MessageId::SetPortConfiguration) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 6U) {
        return false;
    }

    out.portIndex = payload[0U];
    out.baudRate = SightlineFraming::readU32Le(payload.data() + 1U);
    out.mode = payload[5U];
    return true;
}

bool SightlineSerialParser::parseCommandPassThrough(
    const std::vector<std::uint8_t>& packet, MsgCommandPassThrough& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CommandPassThrough) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.empty()) {
        return false;
    }

    out.destPort = payload[0U];
    out.data.assign(payload.begin() + 1, payload.end());
    return true;
}

bool SightlineSerialParser::parseGPIO(
    const std::vector<std::uint8_t>& packet, MsgGPIO& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::GPIO) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 3U) {
        return false;
    }

    out.pinMask = payload[0U];
    out.pinValues = payload[1U];
    out.directionMask = payload[2U];
    return true;
}

} // namespace Sightline
