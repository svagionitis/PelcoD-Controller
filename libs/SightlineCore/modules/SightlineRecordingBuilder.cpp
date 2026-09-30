/// @file SightlineRecordingBuilder.cpp
/// @brief Implementation of Sightline Recording module builder.

#include "SightlineRecordingBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineRecordingBuilder::buildSetSDRecording(
    const MsgSetSDRecordingParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(2U + msg.filenamePrefix.size() + 1U);
    payload.push_back(msg.recordingState);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendString(payload, msg.filenamePrefix);
    return SightlineFraming::buildPacket(MessageId::SetSDRecordingParameters, payload);
}

std::vector<std::uint8_t> SightlineRecordingBuilder::buildGetSnapShot(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetSnapShot, payload);
}

std::vector<std::uint8_t> SightlineRecordingBuilder::buildGetSDRecording()
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::SetSDRecordingParameters)
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
