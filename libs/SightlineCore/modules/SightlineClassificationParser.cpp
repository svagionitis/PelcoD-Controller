/// @file SightlineClassificationParser.cpp
/// @brief Implementation of Sightline AI classification frame deserializers.

#include "SightlineClassificationParser.h"

namespace Sightline {

bool SightlineClassificationParser::parseCustomAIDetect(
    const std::vector<std::uint8_t>& packet, MsgCustomAIDetect& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CustomAIDetect) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 4U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.modelId = payload[1U];
    out.confidenceThreshold = payload[2U];
    out.nmsThreshold = payload[3U];
    return true;
}

} // namespace Sightline
