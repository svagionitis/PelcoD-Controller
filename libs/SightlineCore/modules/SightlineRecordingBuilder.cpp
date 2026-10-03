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

std::vector<std::uint8_t> SightlineRecordingBuilder::buildRecordingEvent(
    const MsgFileRecordingEvent& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(19U + msg.eventPayload.size() + 1U);
    SightlineFraming::appendU64Le(payload, msg.timestampUs);
    payload.push_back(msg.cameraIndex);
    payload.push_back(static_cast<std::uint8_t>(msg.eventType));
    SightlineFraming::appendU32Le(payload, msg.statusCode);
    SightlineFraming::appendU32Le(payload, msg.freeStorageMB);
    payload.push_back(msg.queueFullPercent);
    if (!msg.eventPayload.empty()) {
        SightlineFraming::appendString(payload, msg.eventPayload);
    }
    return SightlineFraming::buildPacket(MessageId::FileRecordingEvent, payload);
}

std::vector<std::uint8_t> SightlineRecordingBuilder::buildRecordingStatusV2(
    const MsgCurrentRecordingStatusV2& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(27U + msg.activeFilename.size() + 1U);
    SightlineFraming::appendU16Le(payload, msg.sequenceId);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.recordingState);
    SightlineFraming::appendU32Le(payload, msg.currentBitrateKbps);
    SightlineFraming::appendU64Le(payload, msg.totalBytesWritten);
    SightlineFraming::appendU32Le(payload, msg.freeStorageMB);
    SightlineFraming::appendU16Le(payload, msg.estRemainingSecs);
    payload.push_back(msg.ringBufferPercent);
    SightlineFraming::appendU16Le(payload, msg.droppedFrames);
    SightlineFraming::appendU16Le(payload, msg.activeFileFrameCount);
    if (!msg.activeFilename.empty()) {
        SightlineFraming::appendString(payload, msg.activeFilename);
    }
    return SightlineFraming::buildPacket(MessageId::CurrentRecordingStatusV2, payload);
}

std::vector<std::uint8_t> SightlineRecordingBuilder::buildGetDirListing(
    const MsgGetDirectoryListing& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(6U + msg.pathFilter.size() + 1U);
    SightlineFraming::appendU16Le(payload, msg.sequenceId);
    payload.push_back(static_cast<std::uint8_t>(msg.destination));
    SightlineFraming::appendU16Le(payload, msg.startIndex);
    payload.push_back(msg.maxEntries);
    if (!msg.pathFilter.empty()) {
        SightlineFraming::appendString(payload, msg.pathFilter);
    }
    return SightlineFraming::buildPacket(MessageId::GetDirectoryListing, payload);
}

std::vector<std::uint8_t> SightlineRecordingBuilder::buildDirListingReply(
    const MsgDirectoryListingReply& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(7U + msg.entries.size() * 32U);
    SightlineFraming::appendU16Le(payload, msg.sequenceId);
    SightlineFraming::appendU16Le(payload, msg.totalFiles);
    SightlineFraming::appendU16Le(payload, msg.startIndex);
    payload.push_back(static_cast<std::uint8_t>(msg.entries.size()));

    for (const auto& entry : msg.entries) {
        SightlineFraming::appendU64Le(payload, entry.fileSizeBytes);
        SightlineFraming::appendU64Le(payload, entry.timestampUs);
        payload.push_back(entry.isPinned ? 1U : 0U);
        payload.push_back(entry.formatType);
        SightlineFraming::appendString(payload, entry.filename);
    }

    return SightlineFraming::buildPacket(MessageId::DirectoryListingReply, payload);
}

std::vector<std::uint8_t> SightlineRecordingBuilder::buildFileStorageMgmt(
    const MsgFileStorageManagement& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(4U + msg.targetFilename.size() + 1U);
    SightlineFraming::appendU16Le(payload, msg.sequenceId);
    payload.push_back(static_cast<std::uint8_t>(msg.operation));
    payload.push_back(static_cast<std::uint8_t>(msg.destination));
    if (!msg.targetFilename.empty()) {
        SightlineFraming::appendString(payload, msg.targetFilename);
    }
    return SightlineFraming::buildPacket(MessageId::FileStorageManagement, payload);
}

} // namespace Sightline
