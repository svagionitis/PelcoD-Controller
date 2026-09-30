/// @file SightlineKlvParser.cpp
/// @brief Implementation of Sightline KLV Metadata response deserializer.

#include "SightlineKlvParser.h"

namespace Sightline {

bool SightlineKlvParser::parseMetadataValues(
    const std::vector<std::uint8_t>& packet, MsgSetMetadataValues& out)
{
    const auto msgId { SightlineFraming::identifyMessage(packet) };
    if (msgId != MessageId::CurrentMetadataValues && msgId != MessageId::SetMetadataValues) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 64U) {
        return false;
    }

    out.platformLatitudeDeg = SightlineFraming::readDouble64Le(payload.data());
    out.platformLongitudeDeg = SightlineFraming::readDouble64Le(payload.data() + 8U);
    out.platformAltitudeMeters = SightlineFraming::readDouble64Le(payload.data() + 16U);
    out.platformHeadingDeg = SightlineFraming::readDouble64Le(payload.data() + 24U);
    out.platformPitchDeg = SightlineFraming::readDouble64Le(payload.data() + 32U);
    out.platformRollDeg = SightlineFraming::readDouble64Le(payload.data() + 40U);
    out.sensorHorizontalFovDeg = SightlineFraming::readDouble64Le(payload.data() + 48U);
    out.sensorVerticalFovDeg = SightlineFraming::readDouble64Le(payload.data() + 56U);
    return true;
}

bool SightlineKlvParser::parseMetadataStaticValues(
    const std::vector<std::uint8_t>& packet, MsgMetadataStaticValues& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::MetadataStaticValues) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    std::size_t offset { 0U };
    auto readStr = [&](std::string& target) {
        target.clear();
        while (offset < payload.size() && payload[offset] != '\0') {
            target.push_back(static_cast<char>(payload[offset++]));
        }
        if (offset < payload.size() && payload[offset] == '\0') {
            ++offset;
        }
    };

    readStr(out.missionId);
    readStr(out.platformTailNumber);
    readStr(out.securityClassification);
    return true;
}

bool SightlineKlvParser::parseMetadataRate(
    const std::vector<std::uint8_t>& packet, MsgSetMetadataRate& out)
{
    const auto msgId { SightlineFraming::identifyMessage(packet) };
    if (msgId != MessageId::SetMetadataRate && msgId != MessageId::CurrentMetadataRate) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 2U) {
        return false;
    }

    out.metadataType = payload[0U];
    out.ratePeriod = payload[1U];
    return true;
}

} // namespace Sightline
