/// @file SightlineDetectionParser.cpp
/// @brief Implementation of Sightline detection frame deserializers.

#include "SightlineDetectionParser.h"

namespace Sightline {

bool SightlineDetectionParser::parseDetectionParams(
    const std::vector<std::uint8_t>& packet, MsgSetDetectionParameters& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::CurrentDetectionParameters && id != MessageId::SetDetectionParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 7U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.mode = payload[1U];
    out.threshold = payload[2U];
    out.minTargetSize = SightlineFraming::readU16Le(payload.data() + 3U);
    out.maxTargetSize = SightlineFraming::readU16Le(payload.data() + 5U);
    return true;
}

} // namespace Sightline
