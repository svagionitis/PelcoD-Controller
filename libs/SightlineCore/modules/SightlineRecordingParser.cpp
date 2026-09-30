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

} // namespace Sightline
