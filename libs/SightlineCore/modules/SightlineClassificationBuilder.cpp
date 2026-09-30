/// @file SightlineClassificationBuilder.cpp
/// @brief Implementation of Sightline AI classification command serializers.

#include "SightlineClassificationBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineClassificationBuilder::buildCustomAIDetect(
    const MsgCustomAIDetect& msg)
{
    const std::vector<std::uint8_t> payload {
        msg.cameraIndex,
        msg.modelId,
        msg.confidenceThreshold,
        msg.nmsThreshold
    };
    return SightlineFraming::buildPacket(MessageId::CustomAIDetect, payload);
}

std::vector<std::uint8_t> SightlineClassificationBuilder::buildGetCustomAIDetect()
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::CustomAIDetect) };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
