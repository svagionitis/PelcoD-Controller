/// @file SightlineSerialBuilder.cpp
/// @brief Implementation of Sightline serial port, passthrough, and GPIO serializers.

#include "SightlineSerialBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineSerialBuilder::buildSetPortConfiguration(const MsgSetPortConfiguration& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(6U);
    payload.push_back(msg.portIndex);
    SightlineFraming::appendU32Le(payload, msg.baudRate);
    payload.push_back(msg.mode);
    return SightlineFraming::buildPacket(MessageId::SetPortConfiguration, payload);
}

std::vector<std::uint8_t> SightlineSerialBuilder::buildCommandPassThrough(const MsgCommandPassThrough& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(1U + msg.data.size());
    payload.push_back(msg.destPort);
    payload.insert(payload.end(), msg.data.begin(), msg.data.end());
    return SightlineFraming::buildPacket(MessageId::CommandPassThrough, payload);
}

std::vector<std::uint8_t> SightlineSerialBuilder::buildGPIO(const MsgGPIO& msg)
{
    const std::vector<std::uint8_t> payload { msg.pinMask, msg.pinValues, msg.directionMask };
    return SightlineFraming::buildPacket(MessageId::GPIO, payload);
}

std::vector<std::uint8_t> SightlineSerialBuilder::buildGetPortConfiguration(std::uint8_t port)
{
    const std::vector<std::uint8_t> payload { port };
    return SightlineFraming::buildPacket(MessageId::GetPortConfiguration, payload);
}

std::vector<std::uint8_t> SightlineSerialBuilder::buildGetGPIO()
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::GPIO) };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineSerialBuilder::buildI2CCommand(const MsgI2CCommand& msg)
{
    std::vector<std::uint8_t> payload {};
    const auto writeLen { msg.writeLength > 0U ? msg.writeLength : static_cast<std::uint8_t>(msg.data.size()) };
    payload.reserve(5U + msg.data.size());
    payload.push_back(msg.busIndex);
    payload.push_back(msg.deviceAddress);
    payload.push_back(msg.subAddress);
    payload.push_back(writeLen);
    payload.push_back(msg.readLength);
    payload.insert(payload.end(), msg.data.begin(), msg.data.end());
    return SightlineFraming::buildPacket(MessageId::I2CCommand, payload);
}

} // namespace Sightline
