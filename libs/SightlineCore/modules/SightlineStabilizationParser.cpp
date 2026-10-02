/// @file SightlineStabilizationParser.cpp
/// @brief Implementation of Sightline SLA stabilization frame deserializers.

#include "SightlineStabilizationParser.h"

namespace Sightline {

bool SightlineStabilizationParser::parseStabilizationParams(ByteView packet, MsgSetStabilizationParameters& out)
{
    const auto id = SightlineFraming::identifyMessage(packet);
    if (id != MessageId::CurrentStabilizationParameters && id != MessageId::SetStabilizationParameters) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 5U) {
        return false;
    }

    out.mode = payload[0U];
    out.rate = payload[1U];
    out.translationLimit = payload[2U];
    out.angleLimit = payload[3U];
    out.cameraIndex = payload[4U];
    if (payload.size() >= 6U) {
        out.maxStabOff = payload[5U];
    }
    if (payload.size() >= 9U) {
        out.edgeY = payload[6U];
        out.edgeU = payload[7U];
        out.edgeV = payload[8U];
    }
    return true;
}

bool SightlineStabilizationParser::parseStabilizationParams(
    const std::vector<std::uint8_t>& packet, MsgSetStabilizationParameters& out)
{
    return parseStabilizationParams(ByteView(packet.data(), packet.size()), out);
}

bool SightlineStabilizationParser::parseStabilizationBias(ByteView packet, MsgSetStabilizationBias& out)
{
    const auto id = SightlineFraming::identifyMessage(packet);
    if (id != MessageId::StabilizationBias && id != MessageId::CurrentStabilizationBias
        && id != MessageId::SetStabilizationBias) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 5U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.biasCol = SightlineFraming::readS16Le(payload.data() + 1U);
    out.biasRow = SightlineFraming::readS16Le(payload.data() + 3U);
    if (payload.size() >= 6U) {
        out.autoBias = payload[5U];
    }
    if (payload.size() >= 7U) {
        out.updateRate = payload[6U];
    }
    return true;
}

bool SightlineStabilizationParser::parseStabilizationBias(
    const std::vector<std::uint8_t>& packet, MsgSetStabilizationBias& out)
{
    return parseStabilizationBias(ByteView(packet.data(), packet.size()), out);
}

bool SightlineStabilizationParser::parseRegistration(ByteView packet, MsgSetRegistrationParameters& out)
{
    const auto id = SightlineFraming::identifyMessage(packet);
    if (id != MessageId::RegistrationParameters && id != MessageId::CurrentRegistrationParameters
        && id != MessageId::SetRegistrationParameters) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 4U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    if (payload.size() >= 15U) {
        out.maxTranslation = SightlineFraming::readU16Le(payload.data() + 1U);
        out.maxRotation = payload[3U];
        out.zoomRange = payload[4U];
        out.left = SightlineFraming::readU16Le(payload.data() + 5U);
        out.right = SightlineFraming::readU16Le(payload.data() + 7U);
        out.top = SightlineFraming::readU16Le(payload.data() + 9U);
        out.bottom = SightlineFraming::readU16Le(payload.data() + 11U);
        out.updateRate = payload[13U];
        out.flags = payload[14U];
    } else {
        out.searchRange = payload[1U];
        out.pyramidLevels = payload[2U];
        out.flags = payload[3U];
    }
    return true;
}

bool SightlineStabilizationParser::parseRegistration(
    const std::vector<std::uint8_t>& packet, MsgSetRegistrationParameters& out)
{
    return parseRegistration(ByteView(packet.data(), packet.size()), out);
}

} // namespace Sightline
