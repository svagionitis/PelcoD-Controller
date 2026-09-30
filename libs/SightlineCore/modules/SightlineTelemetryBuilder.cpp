/// @file SightlineTelemetryBuilder.cpp
/// @brief Implementation of Sightline platform metadata and telemetry serializers.

#include "SightlineTelemetryBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineTelemetryBuilder::buildSetMetadataValues(
    const MsgSetMetadataValues& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(64U);
    SightlineFraming::appendDouble64Le(payload, msg.platformLatitudeDeg);
    SightlineFraming::appendDouble64Le(payload, msg.platformLongitudeDeg);
    SightlineFraming::appendDouble64Le(payload, msg.platformAltitudeMeters);
    SightlineFraming::appendDouble64Le(payload, msg.platformHeadingDeg);
    SightlineFraming::appendDouble64Le(payload, msg.platformPitchDeg);
    SightlineFraming::appendDouble64Le(payload, msg.platformRollDeg);
    SightlineFraming::appendDouble64Le(payload, msg.sensorHorizontalFovDeg);
    SightlineFraming::appendDouble64Le(payload, msg.sensorVerticalFovDeg);
    return SightlineFraming::buildPacket(MessageId::SetMetadataValues, payload);
}

std::vector<std::uint8_t> SightlineTelemetryBuilder::buildMetadataStaticValues(
    const MsgMetadataStaticValues& msg)
{
    std::vector<std::uint8_t> payload {};
    SightlineFraming::appendString(payload, msg.missionId);
    SightlineFraming::appendString(payload, msg.platformTailNumber);
    SightlineFraming::appendString(payload, msg.securityClassification);
    return SightlineFraming::buildPacket(MessageId::MetadataStaticValues, payload);
}

std::vector<std::uint8_t> SightlineTelemetryBuilder::buildSetMetadataRate(
    const MsgSetMetadataRate& msg)
{
    const std::vector<std::uint8_t> payload { msg.metadataType, msg.ratePeriod };
    return SightlineFraming::buildPacket(MessageId::SetMetadataRate, payload);
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
    std::vector<std::uint8_t> payload {};
    payload.push_back(msg.enable);
    SightlineFraming::appendU16Le(payload, msg.broadcastPort);
    SightlineFraming::appendString(payload, msg.uid);
    SightlineFraming::appendString(payload, msg.cotType);
    return SightlineFraming::buildPacket(MessageId::CursorOnTarget, payload);
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
