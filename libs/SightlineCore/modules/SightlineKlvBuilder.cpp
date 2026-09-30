/// @file SightlineKlvBuilder.cpp
/// @brief Implementation of Sightline KLV Metadata builder.

#include "SightlineKlvBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineKlvBuilder::buildSetMetadataValues(
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

std::vector<std::uint8_t> SightlineKlvBuilder::buildMetadataStaticValues(
    const MsgMetadataStaticValues& msg)
{
    std::vector<std::uint8_t> payload {};
    SightlineFraming::appendString(payload, msg.missionId);
    SightlineFraming::appendString(payload, msg.platformTailNumber);
    SightlineFraming::appendString(payload, msg.securityClassification);
    return SightlineFraming::buildPacket(MessageId::MetadataStaticValues, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildSetMetadataRate(
    const MsgSetMetadataRate& msg)
{
    const std::vector<std::uint8_t> payload { msg.metadataType, msg.ratePeriod };
    return SightlineFraming::buildPacket(MessageId::SetMetadataRate, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildCursorOnTarget(
    const MsgCursorOnTarget& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.push_back(msg.enable);
    SightlineFraming::appendU16Le(payload, msg.broadcastPort);
    SightlineFraming::appendString(payload, msg.uid);
    SightlineFraming::appendString(payload, msg.cotType);
    return SightlineFraming::buildPacket(MessageId::CursorOnTarget, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildGetMetadataValues()
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::SetMetadataValues)
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildGetMetadataStaticValues()
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::MetadataStaticValues)
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineKlvBuilder::buildGetMetadataRate()
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::SetMetadataRate)
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
