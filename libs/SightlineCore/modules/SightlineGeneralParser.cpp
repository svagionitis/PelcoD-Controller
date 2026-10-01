/// @file SightlineGeneralParser.cpp
/// @brief Implementation of Sightline General module response deserializer.

#include "SightlineGeneralParser.h"

#include <algorithm>
#include <string>

namespace Sightline {

bool SightlineGeneralParser::parseVersionNumber(const std::vector<std::uint8_t>& packet, MsgVersionNumber& out)
{
    const auto id = SightlineFraming::identifyMessage(packet);
    if (id != MessageId::GetVersionNumber && id != MessageId::VersionNumber) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 4U) {
        return false;
    }

    out.softwareMajor = payload[0U];
    out.softwareMinor = payload[1U];
    out.hardwareVersion = payload[2U];
    out.degreesF = payload[3U];

    if (payload.size() >= 7U) {
        out.hardwareId = static_cast<std::uint32_t>(payload[4U]) | (static_cast<std::uint32_t>(payload[5U]) << 8U)
            | (static_cast<std::uint32_t>(payload[6U]) << 16U);
    }
    if (payload.size() >= 11U) {
        out.appBits = SightlineFraming::readU32Le(payload.data() + 7U);
    }
    if (payload.size() >= 12U) {
        out.boardType = payload[11U];
        out.hardwareType = out.boardType;
    }
    if (payload.size() >= 13U) {
        out.softwareRelease = payload[12U];
        out.softwarePatch = out.softwareRelease;
    }
    if (payload.size() >= 15U) {
        out.otherVersion = SightlineFraming::readU16Le(payload.data() + 13U);
        out.boardRevision = static_cast<std::uint32_t>((out.otherVersion >> 8U) & 0xFFU);
    }
    if (payload.size() >= 19U) {
        out.srcRevision = SightlineFraming::readU32Le(payload.data() + 15U);
    }
    if (payload.size() >= 23U) {
        out.buildDate = SightlineFraming::readU32Le(payload.data() + 19U);
    }
    if (payload.size() >= 27U) {
        out.buildTime = SightlineFraming::readU32Le(payload.data() + 23U);
    }
    if (payload.size() >= 29U) {
        out.softwareBuild = SightlineFraming::readU16Le(payload.data() + 27U);
    }
    if (payload.size() >= 31U) {
        out.v4AppBits = SightlineFraming::readU16Le(payload.data() + 29U);
    }
    if (payload.size() >= 33U) {
        out.degreesC = SightlineFraming::readS16Le(payload.data() + 31U);
    } else {
        out.degreesC = static_cast<std::int16_t>((static_cast<int>(out.degreesF) - 32) * 5 / 9);
    }
    if (payload.size() >= 37U) {
        out.adapters = SightlineFraming::readU32Le(payload.data() + 33U);
    }

    out.versionString = std::to_string(out.softwareMajor) + "." + std::to_string(out.softwareMinor) + "."
        + std::to_string(out.softwareRelease);
    if (out.softwareBuild > 0U) {
        out.versionString += " (build " + std::to_string(out.softwareBuild) + ")";
    }

    return true;
}

bool SightlineGeneralParser::parseUserWarning(const std::vector<std::uint8_t>& packet, MsgUserWarningMessage& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::UserWarningMessage) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 2U) {
        return false;
    }

    out.warningCode = SightlineFraming::readU16Le(payload.data());
    if (payload.size() > 2U) {
        out.message = std::string(reinterpret_cast<const char*>(payload.data() + 2U), payload.size() - 2U);
        while (!out.message.empty() && out.message.back() == '\0') {
            out.message.pop_back();
        }
    } else {
        out.message.clear();
    }

    return true;
}

bool SightlineGeneralParser::parseSystemStatus(const std::vector<std::uint8_t>& packet, MsgSystemStatusMessage& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::SystemStatusMessage) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 14U) {
        return false;
    }

    out.errorFlags = static_cast<std::uint64_t>(SightlineFraming::readU32Le(payload.data()))
        | (static_cast<std::uint64_t>(SightlineFraming::readU32Le(payload.data() + 4U)) << 32U);

    out.temperatureF = SightlineFraming::readS16Le(payload.data() + 8U);

    out.load0 = payload[10U];
    out.load1 = payload[11U];
    out.load2 = payload[12U];
    out.load3 = payload[13U];
    out.cpuLoadPercent = static_cast<std::uint16_t>(std::max({ out.load0, out.load1, out.load2, out.load3 }));

    if (payload.size() >= 16U) {
        out.coreTempC = SightlineFraming::readS16Le(payload.data() + 14U);
    } else {
        out.coreTempC = static_cast<std::int16_t>((static_cast<int>(out.temperatureF) - 32) * 5 / 9);
    }

    if (payload.size() >= 20U) {
        out.missedFrames0 = payload[16U];
        out.missedFrames1 = payload[17U];
        out.missedFrames2 = payload[18U];
        out.missedFrames3 = payload[19U];
    }

    return true;
}

bool SightlineGeneralParser::parseCurrentConfiguration(
    const std::vector<std::uint8_t>& packet, MsgCurrentConfiguration& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CurrentConfiguration) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 4U) {
        return false;
    }

    out.maxCameras = payload[0U];
    out.maxVirtCameras = payload[1U];
    out.maxStreams = payload[2U];
    out.maxProcessed = payload[3U];
    out.numVideoInputs = out.maxCameras;
    out.numVideoOutputs = out.maxStreams;
    out.numDisplays = out.maxProcessed;
    out.hardwareType = out.maxCameras;

    if (payload.size() >= 6U) {
        out.cameraConfiguredBits = SightlineFraming::readU16Le(payload.data() + 4U);
    }
    if (payload.size() >= 8U) {
        out.cameraConnectedBits = SightlineFraming::readU16Le(payload.data() + 6U);
    }
    if (payload.size() >= 12U) {
        out.displayPresentBits = SightlineFraming::readU32Le(payload.data() + 8U);
    }
    if (payload.size() >= 16U) {
        out.captureStateBits = SightlineFraming::readU32Le(payload.data() + 12U);
    }
    return true;
}

bool SightlineGeneralParser::parseSystemStatusMode(const std::vector<std::uint8_t>& packet, MsgSystemStatusMode& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::SystemStatusMode) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 2U) {
        return false;
    }

    out.systemStatusBits = SightlineFraming::readU16Le(payload.data());
    if (payload.size() >= 6U) {
        out.systemDebugBits = SightlineFraming::readU32Le(payload.data() + 2U);
    }
    return true;
}

bool SightlineGeneralParser::parseSystemValue(ByteView packet, MsgSystemValue& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::SetSystemValue && id != MessageId::CurrentSystemValue) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 5U) {
        return false;
    }

    out.systemValueId = payload[0U];
    out.value = SightlineFraming::readU32Le(payload.data() + 1U);
    return true;
}

bool SightlineGeneralParser::parseTagData(ByteView packet, MsgTagData& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::TagData) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 2U) {
        return false;
    }

    out.tagId = SightlineFraming::readU16Le(payload.data());
    if (payload.size() > 2U) {
        out.data.assign(payload.begin() + 2U, payload.end());
    } else {
        out.data.clear();
    }
    return true;
}

bool SightlineGeneralParser::parseTagDataRate(ByteView packet, MsgTagDataRate& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::TagDataRate) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 3U) {
        return false;
    }

    out.tagId = SightlineFraming::readU16Le(payload.data());
    out.rate = payload[2U];
    return true;
}

bool SightlineGeneralParser::parseTagSourceSelector(ByteView packet, MsgTagSourceSelector& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::TagSourceSelector) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 3U) {
        return false;
    }

    out.tagId = SightlineFraming::readU16Le(payload.data());
    out.source = payload[2U];
    return true;
}

bool SightlineGeneralParser::parseDetailedTiming(ByteView packet, MsgDetailedTiming& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::DetailedTimingMessage) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 16U) {
        return false;
    }

    out.frameNumber = SightlineFraming::readU32Le(payload.data());
    out.captureLatencyUs = SightlineFraming::readU32Le(payload.data() + 4U);
    out.processLatencyUs = SightlineFraming::readU32Le(payload.data() + 8U);
    out.transmitLatencyUs = SightlineFraming::readU32Le(payload.data() + 12U);
    return true;
}

bool SightlineGeneralParser::parseAppendedMetadata(ByteView packet, MsgAppendedMetadata& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::AppendedMetadata) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 2U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.enable = payload[1U];
    return true;
}

bool SightlineGeneralParser::parseFrameIndex(ByteView packet, MsgFrameIndex& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::FrameIndex) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 13U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.frameIndex = SightlineFraming::readU32Le(payload.data() + 1U);
    out.timestampUs = SightlineFraming::readU64Le(payload.data() + 5U);
    return true;
}

} // namespace Sightline
