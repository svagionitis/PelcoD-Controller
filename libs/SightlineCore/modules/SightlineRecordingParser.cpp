/// @file SightlineRecordingParser.cpp
/// @brief Implementation of Sightline Recording module response deserializer.

#include "SightlineRecordingParser.h"

namespace Sightline {

bool SightlineRecordingParser::parseSDRecording(
    const std::vector<std::uint8_t>& packet, MsgSetSDRecordingParameters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::SetSDRecordingParameters) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 2U) {
        return false;
    }

    out.recordingState = payload[0U];
    out.cameraIndex = payload[1U];
    if (payload.size() > 2U) {
        out.filenamePrefix = std::string(
            reinterpret_cast<const char*>(payload.data() + 2U), payload.size() - 2U);
        while (!out.filenamePrefix.empty() && out.filenamePrefix.back() == '\0') {
            out.filenamePrefix.pop_back();
        }
    } else {
        out.filenamePrefix.clear();
    }
    return true;
}

bool SightlineRecordingParser::parseSnapShot(
    const std::vector<std::uint8_t>& packet, MsgCurrentSnapShot& out)
{
    const auto msgId { SightlineFraming::identifyMessage(packet) };
    if (msgId != MessageId::GetSnapShot &&
        msgId != MessageId::CurrentSnapShot &&
        msgId != MessageId::SetSnapShot &&
        msgId != MessageId::DoSnapShot) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.empty()) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.status = (payload.size() > 1U) ? payload[1U] : 0U;
    if (payload.size() > 2U) {
        out.fileName = std::string(
            reinterpret_cast<const char*>(payload.data() + 2U), payload.size() - 2U);
        while (!out.fileName.empty() && out.fileName.back() == '\0') {
            out.fileName.pop_back();
        }
    } else {
        out.fileName.clear();
    }
    return true;
}

bool SightlineRecordingParser::parseCmdAck(
    const std::vector<std::uint8_t>& packet, MsgCommandAck& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CommandAck) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 9U) {
        return false;
    }

    out.sequenceId = SightlineFraming::readU16Le(payload.data());
    out.originalMsgId = payload[2U];
    out.statusCode = static_cast<RecordingStatusCode>(payload[3U]);
    out.freeStorageMB = SightlineFraming::readU32Le(payload.data() + 4U);
    out.subsystemState = payload[8U];
    return true;
}

bool SightlineRecordingParser::parseSetFileRecordingV2(
    const std::vector<std::uint8_t>& packet, MsgSetFileRecordingParamsV2& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::SetFileRecordingParamsV2) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 12U) {
        return false;
    }

    out.sequenceId = SightlineFraming::readU16Le(payload.data());
    out.cameraIndex = payload[2U];
    out.action = static_cast<RecordingAction>(payload[3U]);
    out.destination = static_cast<StorageDestination>(payload[4U]);
    out.flags = payload[5U];
    out.maxSplitSizeBytes = SightlineFraming::readU32Le(payload.data() + 6U);
    out.maxSplitFrames = SightlineFraming::readU16Le(payload.data() + 10U);
    if (payload.size() > 12U) {
        out.baseFilename = std::string(
            reinterpret_cast<const char*>(payload.data() + 12U), payload.size() - 12U);
        while (!out.baseFilename.empty() && out.baseFilename.back() == '\0') {
            out.baseFilename.pop_back();
        }
    } else {
        out.baseFilename.clear();
    }
    return true;
}

bool SightlineRecordingParser::parseDoSnapShotV2(
    const std::vector<std::uint8_t>& packet, MsgDoSnapShotV2& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::DoSnapShotV2) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 7U) {
        return false;
    }

    out.sequenceId = SightlineFraming::readU16Le(payload.data());
    out.cameraIndex = payload[2U];
    out.format = static_cast<SnapshotFormat>(payload[3U]);
    out.domain = static_cast<SnapshotDomain>(payload[4U]);
    out.qualityLevel = payload[5U];
    out.burstCount = payload[6U];
    if (payload.size() > 7U) {
        out.customFilename = std::string(
            reinterpret_cast<const char*>(payload.data() + 7U), payload.size() - 7U);
        while (!out.customFilename.empty() && out.customFilename.back() == '\0') {
            out.customFilename.pop_back();
        }
    } else {
        out.customFilename.clear();
    }
    return true;
}

bool SightlineRecordingParser::parseRecordingEvent(
    const std::vector<std::uint8_t>& packet, MsgFileRecordingEvent& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::FileRecordingEvent) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 19U) {
        return false;
    }

    out.timestampUs = SightlineFraming::readU64Le(payload.data());
    out.cameraIndex = payload[8U];
    out.eventType = static_cast<RecordingEventType>(payload[9U]);
    out.statusCode = SightlineFraming::readU32Le(payload.data() + 10U);
    out.freeStorageMB = SightlineFraming::readU32Le(payload.data() + 14U);
    out.queueFullPercent = payload[18U];
    if (payload.size() > 19U) {
        out.eventPayload = std::string(
            reinterpret_cast<const char*>(payload.data() + 19U), payload.size() - 19U);
        while (!out.eventPayload.empty() && out.eventPayload.back() == '\0') {
            out.eventPayload.pop_back();
        }
    } else {
        out.eventPayload.clear();
    }
    return true;
}

bool SightlineRecordingParser::parseRecordingStatusV2(
    const std::vector<std::uint8_t>& packet, MsgCurrentRecordingStatusV2& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CurrentRecordingStatusV2) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 27U) {
        return false;
    }

    out.sequenceId = SightlineFraming::readU16Le(payload.data());
    out.cameraIndex = payload[2U];
    out.recordingState = payload[3U];
    out.currentBitrateKbps = SightlineFraming::readU32Le(payload.data() + 4U);
    out.totalBytesWritten = SightlineFraming::readU64Le(payload.data() + 8U);
    out.freeStorageMB = SightlineFraming::readU32Le(payload.data() + 16U);
    out.estRemainingSecs = SightlineFraming::readU16Le(payload.data() + 20U);
    out.ringBufferPercent = payload[22U];
    out.droppedFrames = SightlineFraming::readU16Le(payload.data() + 23U);
    out.activeFileFrameCount = SightlineFraming::readU16Le(payload.data() + 25U);
    if (payload.size() > 27U) {
        out.activeFilename = std::string(
            reinterpret_cast<const char*>(payload.data() + 27U), payload.size() - 27U);
        while (!out.activeFilename.empty() && out.activeFilename.back() == '\0') {
            out.activeFilename.pop_back();
        }
    } else {
        out.activeFilename.clear();
    }
    return true;
}

bool SightlineRecordingParser::parseGetDirListing(
    const std::vector<std::uint8_t>& packet, MsgGetDirectoryListing& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::GetDirectoryListing) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 6U) {
        return false;
    }

    out.sequenceId = SightlineFraming::readU16Le(payload.data());
    out.destination = static_cast<StorageDestination>(payload[2U]);
    out.startIndex = SightlineFraming::readU16Le(payload.data() + 3U);
    out.maxEntries = payload[5U];
    if (payload.size() > 6U) {
        out.pathFilter = std::string(
            reinterpret_cast<const char*>(payload.data() + 6U), payload.size() - 6U);
        while (!out.pathFilter.empty() && out.pathFilter.back() == '\0') {
            out.pathFilter.pop_back();
        }
    } else {
        out.pathFilter.clear();
    }
    return true;
}

bool SightlineRecordingParser::parseDirListingReply(
    const std::vector<std::uint8_t>& packet, MsgDirectoryListingReply& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::DirectoryListingReply) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 7U) {
        return false;
    }

    out.sequenceId = SightlineFraming::readU16Le(payload.data());
    out.totalFiles = SightlineFraming::readU16Le(payload.data() + 2U);
    out.startIndex = SightlineFraming::readU16Le(payload.data() + 4U);
    const auto entryCount = static_cast<std::size_t>(payload[6U]);

    out.entries.clear();
    out.entries.reserve(entryCount);

    std::size_t offset { 7U };
    for (std::size_t i = 0U; i < entryCount && (offset + 18U <= payload.size()); ++i) {
        DirListEntry entry {};
        entry.fileSizeBytes = SightlineFraming::readU64Le(payload.data() + offset);
        entry.timestampUs = SightlineFraming::readU64Le(payload.data() + offset + 8U);
        entry.isPinned = (payload[offset + 16U] != 0U);
        entry.formatType = payload[offset + 17U];
        offset += 18U;

        std::size_t strEnd = offset;
        while (strEnd < payload.size() && payload[strEnd] != 0U) {
            ++strEnd;
        }
        entry.filename = std::string(
            reinterpret_cast<const char*>(payload.data() + offset), strEnd - offset);
        offset = (strEnd < payload.size()) ? (strEnd + 1U) : strEnd;

        out.entries.push_back(std::move(entry));
    }

    return true;
}

bool SightlineRecordingParser::parseFileStorageMgmt(
    const std::vector<std::uint8_t>& packet, MsgFileStorageManagement& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::FileStorageManagement) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 4U) {
        return false;
    }

    out.sequenceId = SightlineFraming::readU16Le(payload.data());
    out.operation = static_cast<FileStorageOp>(payload[2U]);
    out.destination = static_cast<StorageDestination>(payload[3U]);
    if (payload.size() > 4U) {
        out.targetFilename = std::string(
            reinterpret_cast<const char*>(payload.data() + 4U), payload.size() - 4U);
        while (!out.targetFilename.empty() && out.targetFilename.back() == '\0') {
            out.targetFilename.pop_back();
        }
    } else {
        out.targetFilename.clear();
    }
    return true;
}

} // namespace Sightline
