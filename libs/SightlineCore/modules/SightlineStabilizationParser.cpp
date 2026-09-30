/// @file SightlineStabilizationParser.cpp
/// @brief Implementation of Sightline SLA stabilization frame deserializers.

#include "SightlineStabilizationParser.h"

namespace Sightline {

bool SightlineStabilizationParser::parseStabilizationParams(
    const std::vector<std::uint8_t>& packet, MsgSetStabilizationParameters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CurrentStabilizationParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
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

bool SightlineStabilizationParser::parseStabilizationBias(
    const std::vector<std::uint8_t>& packet, MsgSetStabilizationBias& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::StabilizationBias) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 7U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.biasCol = SightlineFraming::readS16Le(payload.data() + 1U);
    out.biasRow = SightlineFraming::readS16Le(payload.data() + 3U);
    out.biasRotation = SightlineFraming::readS16Le(payload.data() + 5U);
    return true;
}

bool SightlineStabilizationParser::parseRegistration(
    const std::vector<std::uint8_t>& packet, MsgSetRegistrationParameters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::SetRegistrationParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 4U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.searchRange = payload[1U];
    out.pyramidLevels = payload[2U];
    out.flags = payload[3U];
    return true;
}

} // namespace Sightline
