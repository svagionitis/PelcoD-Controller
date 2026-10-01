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

bool SightlineSerialParser::parseCommandPassThrough(const std::vector<std::uint8_t>& packet, MsgCommandPassThrough& out)
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

bool SightlineSerialParser::parseGPIO(const std::vector<std::uint8_t>& packet, MsgGPIO& out)
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

bool SightlineSerialParser::parseI2CCommand(ByteView packet, MsgI2CCommand& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::I2CCommand) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 5U) {
        return false;
    }

    out.busIndex = payload[0U];
    out.deviceAddress = payload[1U];
    out.subAddress = payload[2U];
    out.writeLength = payload[3U];
    out.readLength = payload[4U];
    if (payload.size() > 5U) {
        out.data.assign(payload.begin() + 5U, payload.end());
    } else {
        out.data.clear();
    }
    return true;
}

bool SightlineSerialParser::parseSendToBTS(ByteView packet, MsgSendToBTS& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::SendToBTS) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.empty()) {
        return false;
    }

    out.btsPort = payload[0U];
    if (payload.size() > 1U) {
        out.data.assign(payload.begin() + 1U, payload.end());
    } else {
        out.data.clear();
    }
    return true;
}

} // namespace Sightline
