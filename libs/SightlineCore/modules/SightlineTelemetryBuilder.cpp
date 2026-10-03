/// @file SightlineTelemetryBuilder.cpp
/// @brief Implementation of Sightline platform metadata and telemetry serializers.

#include "SightlineTelemetryBuilder.h"
#include "SightlineKlvBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineTelemetryBuilder::buildSetMetadataValues(
    const MsgSetMetadataValues& msg)
{
    return SightlineKlvBuilder::buildSetMetadataValues(msg);
}

std::vector<std::uint8_t> SightlineTelemetryBuilder::buildMetadataStaticValues(
    const MsgMetadataStaticValues& msg)
{
    return SightlineKlvBuilder::buildMetadataStaticValues(msg);
}

std::vector<std::uint8_t> SightlineTelemetryBuilder::buildSetMetadataRate(
    const MsgSetMetadataRate& msg)
{
    return SightlineKlvBuilder::buildSetMetadataRate(msg);
}

std::vector<std::uint8_t> SightlineTelemetryBuilder::buildSetTelemetryDest(
    const MsgSetTelemetryDestination& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(8U);
    payload.push_back(msg.clientIndex);
    SightlineFraming::appendU32Le(payload, msg.clientIpAddress);
    SightlineFraming::appendU16Le(payload, msg.clientPort);
    payload.push_back(msg.flags);
    return SightlineFraming::buildPacket(MessageId::SetTelemetryDestination, payload);
}

std::vector<std::uint8_t> SightlineTelemetryBuilder::buildSetReportingMode(
    const MsgCoordinateReportingMode& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(4U);
    payload.push_back(msg.framePeriod);
    SightlineFraming::appendU16Le(payload, msg.flags);
    payload.push_back(msg.cameraIndex);
    return SightlineFraming::buildPacket(MessageId::CoordinateReportingMode, payload);
}

std::vector<std::uint8_t> SightlineTelemetryBuilder::buildCursorOnTarget(
    const MsgCursorOnTarget& msg)
{
    return SightlineKlvBuilder::buildCursorOnTarget(msg);
}

std::vector<std::uint8_t> SightlineTelemetryBuilder::buildGetCoordReportingMode(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::CoordinateReportingMode), cameraIndex
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineTelemetryBuilder::buildGetTelemetryDest()
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::SetTelemetryDestination)
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
