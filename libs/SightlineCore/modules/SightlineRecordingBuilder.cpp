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

std::vector<std::uint8_t> SightlineRecordingBuilder::buildCmdAck(
    const MsgCommandAck& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(9U);
    SightlineFraming::appendU16Le(payload, msg.sequenceId);
    payload.push_back(msg.originalMsgId);
    payload.push_back(static_cast<std::uint8_t>(msg.statusCode));
    SightlineFraming::appendU32Le(payload, msg.freeStorageMB);
    payload.push_back(msg.subsystemState);
    return SightlineFraming::buildPacket(MessageId::CommandAck, payload);
}

std::vector<std::uint8_t> SightlineRecordingBuilder::buildSetFileRecordingV2(
    const MsgSetFileRecordingParamsV2& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(12U + msg.baseFilename.size() + 1U);
    SightlineFraming::appendU16Le(payload, msg.sequenceId);
    payload.push_back(msg.cameraIndex);
    payload.push_back(static_cast<std::uint8_t>(msg.action));
    payload.push_back(static_cast<std::uint8_t>(msg.destination));
    payload.push_back(msg.flags);
    SightlineFraming::appendU32Le(payload, msg.maxSplitSizeBytes);
    SightlineFraming::appendU16Le(payload, msg.maxSplitFrames);
    SightlineFraming::appendString(payload, msg.baseFilename);
    return SightlineFraming::buildPacket(MessageId::SetFileRecordingParamsV2, payload);
}

std::vector<std::uint8_t> SightlineRecordingBuilder::buildDoSnapShotV2(
    const MsgDoSnapShotV2& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(7U + msg.customFilename.size() + 1U);
    SightlineFraming::appendU16Le(payload, msg.sequenceId);
    payload.push_back(msg.cameraIndex);
    payload.push_back(static_cast<std::uint8_t>(msg.format));
    payload.push_back(static_cast<std::uint8_t>(msg.domain));
    payload.push_back(msg.qualityLevel);
    payload.push_back(msg.burstCount);
    if (!msg.customFilename.empty()) {
        SightlineFraming::appendString(payload, msg.customFilename);
    }
    return SightlineFraming::buildPacket(MessageId::DoSnapShotV2, payload);
}

} // namespace Sightline
