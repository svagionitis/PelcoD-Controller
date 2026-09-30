/// @file SightlineProtocolParser.cpp
/// @brief Implementation of Sightline SLA frame deserializer.

#include "SightlineProtocolParser.h"

#include <algorithm>
#include <cstring>

namespace Sightline {

namespace {

    std::uint16_t readU16Le(const std::uint8_t* ptr) noexcept
    {
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(ptr[0]) | (static_cast<std::uint16_t>(ptr[1]) << 8U));
    }

    std::int16_t readS16Le(const std::uint8_t* ptr) noexcept
    {
        return static_cast<std::int16_t>(readU16Le(ptr));
    }

    std::uint32_t readU32Le(const std::uint8_t* ptr) noexcept
    {
        return static_cast<std::uint32_t>(ptr[0]) | (static_cast<std::uint32_t>(ptr[1]) << 8U)
            | (static_cast<std::uint32_t>(ptr[2]) << 16U) | (static_cast<std::uint32_t>(ptr[3]) << 24U);
    }

    [[maybe_unused]] std::int32_t readS32Le(const std::uint8_t* ptr) noexcept
    {
        return static_cast<std::int32_t>(readU32Le(ptr));
    }

    std::uint64_t readU64Le(const std::uint8_t* ptr) noexcept
    {
        return static_cast<std::uint64_t>(readU32Le(ptr)) | (static_cast<std::uint64_t>(readU32Le(ptr + 4U)) << 32U);
    }

    double readDouble64Le(const std::uint8_t* ptr) noexcept
    {
        const std::uint64_t raw = readU64Le(ptr);
        double val { 0.0 };
        std::memcpy(&val, &raw, sizeof(double));
        return val;
    }

} // namespace

std::size_t SightlineProtocolParser::getHeaderLength(const std::vector<std::uint8_t>& packet) noexcept
{
    if (packet.size() < 3U) {
        return 0U;
    }
    if ((packet[2U] & 0x80U) == 0U) {
        return 3U; // 1-byte length
    }
    if (packet.size() >= 4U) {
        return 4U; // 2-byte extended length
    }
    return 0U;
}

MessageId SightlineProtocolParser::identifyMessage(const std::vector<std::uint8_t>& packet) noexcept
{
    const std::size_t hLen = getHeaderLength(packet);
    if (hLen == 0U || packet.size() <= hLen) {
        return MessageId::Unknown;
    }
    return static_cast<MessageId>(packet[hLen]);
}

std::vector<std::uint8_t> SightlineProtocolParser::extractPayload(const std::vector<std::uint8_t>& packet)
{
    const std::size_t hLen = getHeaderLength(packet);
    // Packet must have header + MessageId (1) + Checksum (1)
    if (hLen == 0U || packet.size() < (hLen + 2U)) {
        return {};
    }
    // Payload is strictly between MessageId and Checksum
    return std::vector<std::uint8_t>(packet.begin() + static_cast<std::ptrdiff_t>(hLen + 1U), packet.end() - 1);
}

// ==============================================================================
// 1. System, Diagnostic & Configuration Deserializers
// ==============================================================================

bool SightlineProtocolParser::parseVersionNumber(const std::vector<std::uint8_t>& packet, MsgVersionNumber& out)
{
    const auto id = identifyMessage(packet);
    if (id != MessageId::GetVersionNumber && id != MessageId::VersionNumber) {
        return false;
    }

    const auto payload = extractPayload(packet);
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
        out.appBits = readU32Le(payload.data() + 7U);
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
        out.otherVersion = readU16Le(payload.data() + 13U);
        out.boardRevision = static_cast<std::uint32_t>((out.otherVersion >> 8U) & 0xFFU);
    }
    if (payload.size() >= 19U) {
        out.srcRevision = readU32Le(payload.data() + 15U);
    }
    if (payload.size() >= 23U) {
        out.buildDate = readU32Le(payload.data() + 19U);
    }
    if (payload.size() >= 27U) {
        out.buildTime = readU32Le(payload.data() + 23U);
    }
    if (payload.size() >= 29U) {
        out.softwareBuild = readU16Le(payload.data() + 27U);
    }
    if (payload.size() >= 31U) {
        out.v4AppBits = readU16Le(payload.data() + 29U);
    }
    if (payload.size() >= 33U) {
        out.degreesC = readS16Le(payload.data() + 31U);
    } else {
        out.degreesC = static_cast<std::int16_t>((static_cast<int>(out.degreesF) - 32) * 5 / 9);
    }
    if (payload.size() >= 37U) {
        out.adapters = readU32Le(payload.data() + 33U);
    }

    out.versionString = std::to_string(out.softwareMajor) + "." + std::to_string(out.softwareMinor) + "."
        + std::to_string(out.softwareRelease);
    if (out.softwareBuild > 0U) {
        out.versionString += " (build " + std::to_string(out.softwareBuild) + ")";
    }

    return true;
}

bool SightlineProtocolParser::parseUserWarning(const std::vector<std::uint8_t>& packet, MsgUserWarningMessage& out)
{
    if (identifyMessage(packet) != MessageId::UserWarningMessage) {
        return false;
    }

    const auto payload = extractPayload(packet);
    if (payload.size() < 2U) {
        return false;
    }

    out.warningCode = readU16Le(payload.data());
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

bool SightlineProtocolParser::parseSystemStatus(const std::vector<std::uint8_t>& packet, MsgSystemStatusMessage& out)
{
    if (identifyMessage(packet) != MessageId::SystemStatusMessage) {
        return false;
    }

    const auto payload = extractPayload(packet);
    if (payload.size() < 14U) {
        return false;
    }

    out.errorFlags = static_cast<std::uint64_t>(readU32Le(payload.data()))
        | (static_cast<std::uint64_t>(readU32Le(payload.data() + 4U)) << 32U);

    out.temperatureF = readS16Le(payload.data() + 8U);

    out.load0 = payload[10U];
    out.load1 = payload[11U];
    out.load2 = payload[12U];
    out.load3 = payload[13U];
    out.cpuLoadPercent = std::max({ out.load0, out.load1, out.load2, out.load3 });

    if (payload.size() >= 16U) {
        out.coreTempC = readS16Le(payload.data() + 14U);
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

bool SightlineProtocolParser::parseCurrentConfiguration(
    const std::vector<std::uint8_t>& packet, MsgCurrentConfiguration& out)
{
    if (identifyMessage(packet) != MessageId::CurrentConfiguration) {
        return false;
    }

    const auto payload = extractPayload(packet);
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
        out.cameraConfiguredBits = readU16Le(payload.data() + 4U);
    }
    if (payload.size() >= 8U) {
        out.cameraConnectedBits = readU16Le(payload.data() + 6U);
    }
    if (payload.size() >= 12U) {
        out.displayPresentBits = readU32Le(payload.data() + 8U);
    }
    if (payload.size() >= 16U) {
        out.captureStateBits = readU32Le(payload.data() + 12U);
    }
    return true;
}

// ==============================================================================
// 2. Video Tracking & Motion Telemetry Deserializers
// ==============================================================================

bool SightlineProtocolParser::parseTrackingPosition(const std::vector<std::uint8_t>& packet, MsgTrackingPosition& out)
{
    if (identifyMessage(packet) != MessageId::TrackingPosition) {
        return false;
    }

    const auto payload = extractPayload(packet);
    if (payload.size() < 17U) {
        return false;
    }

    out.col = static_cast<double>(readS16Le(payload.data() + 0U));
    out.row = static_cast<double>(readS16Le(payload.data() + 2U));
    out.translationCol = static_cast<double>(readS16Le(payload.data() + 4U));
    out.translationRow = static_cast<double>(readS16Le(payload.data() + 6U));
    out.offsetCol = static_cast<double>(readS16Le(payload.data() + 8U));
    out.offsetRow = static_cast<double>(readS16Le(payload.data() + 10U));
    out.confidence = payload[12U];
    out.sceneConfidence = payload[13U];
    out.rotationDeg = static_cast<double>(readS16Le(payload.data() + 14U)) / 128.0;
    out.cameraIndex = payload[16U];

    if (payload.size() >= 18U) {
        out.userTrackId = payload[17U];
    }
    if (payload.size() >= 24U) {
        out.scale = static_cast<double>(readU16Le(payload.data() + 22U)) / 256.0;
    }
    if (payload.size() >= 32U) {
        out.timestampUs = readU64Le(payload.data() + 24U);
    }
    if (payload.size() >= 36U) {
        out.frameNumber = readU32Le(payload.data() + 32U);
    }

    return true;
}

bool SightlineProtocolParser::parseTrackingPositions(const std::vector<std::uint8_t>& packet, MsgTrackingPositions& out)
{
    if (identifyMessage(packet) != MessageId::TrackingPositions) {
        return false;
    }

    const auto payload = extractPayload(packet);
    if (payload.size() < 2U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    const std::uint8_t numTracks = payload[1U] & 0x7FU;

    out.tracks.clear();
    std::size_t offset = 2U;

    for (std::uint8_t i = 0U; i < numTracks; ++i) {
        if ((offset + 15U) > payload.size()) {
            break;
        }

        TrackCoordinate track {};
        track.trackId = payload[offset];
        track.centerCol = static_cast<double>(readS16Le(payload.data() + offset + 1U));
        track.centerRow = static_cast<double>(readS16Le(payload.data() + offset + 3U));
        track.width = static_cast<double>(readU16Le(payload.data() + offset + 5U));
        track.height = static_cast<double>(readU16Le(payload.data() + offset + 7U));
        track.velocityCol = static_cast<double>(readS16Le(payload.data() + offset + 9U)) / 256.0;
        track.velocityRow = static_cast<double>(readS16Le(payload.data() + offset + 11U)) / 256.0;
        track.confidence = payload[offset + 13U];
        track.isPrimary = ((payload[offset + 14U] & 0x01U) != 0U);

        out.tracks.push_back(track);
        offset += 15U;
    }

    if ((offset + 8U) <= payload.size()) {
        out.timestampUs = readU64Le(payload.data() + offset);
    }
    if ((offset + 12U) <= payload.size()) {
        out.frameNumber = readU32Le(payload.data() + offset + 8U);
    }

    return true;
}

bool SightlineProtocolParser::parsePositionsExtended(
    const std::vector<std::uint8_t>& packet, MsgTrackingPositionsExtended& out)
{
    if (identifyMessage(packet) != MessageId::TrackingPositionsExtended) {
        return false;
    }

    const auto payload = extractPayload(packet);
    if (payload.size() < 2U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    const std::uint8_t numTracks = payload[1U] & 0x7FU;

    out.tracks.clear();
    out.classIds.clear();
    std::size_t offset = 2U;

    for (std::uint8_t i = 0U; i < numTracks; ++i) {
        if ((offset + 21U) > payload.size()) {
            break;
        }

        TrackCoordinate track {};
        track.trackId = payload[offset];
        track.centerCol = static_cast<double>(readS16Le(payload.data() + offset + 1U));
        track.centerRow = static_cast<double>(readS16Le(payload.data() + offset + 3U));
        track.width = static_cast<double>(readU16Le(payload.data() + offset + 5U));
        track.height = static_cast<double>(readU16Le(payload.data() + offset + 7U));
        track.velocityCol = static_cast<double>(readS16Le(payload.data() + offset + 9U)) / 256.0;
        track.velocityRow = static_cast<double>(readS16Le(payload.data() + offset + 11U)) / 256.0;
        track.confidence = payload[offset + 13U];
        track.isPrimary = ((payload[offset + 14U] & 0x01U) != 0U);

        out.tracks.push_back(track);
        out.classIds.push_back(payload[offset + 15U]);
        offset += 21U;
    }

    if ((offset + 8U) <= payload.size()) {
        out.timestampUs = readU64Le(payload.data() + offset);
    }
    if ((offset + 12U) <= payload.size()) {
        out.frameNumber = readU32Le(payload.data() + offset + 8U);
    }

    return true;
}

bool SightlineProtocolParser::parseTrackTrails(const std::vector<std::uint8_t>& packet, MsgTrackTrails& out)
{
    if (identifyMessage(packet) != MessageId::TrackTrails) {
        return false;
    }

    const auto payload = extractPayload(packet);
    if (payload.size() < 7U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.flags = readU16Le(payload.data() + 1U);
    out.tracksLen = readU16Le(payload.data() + 3U);
    out.detectionLen = readU16Le(payload.data() + 5U);
    out.historyPoints.clear();

    return true;
}

// ==============================================================================
// 3. Configuration State Deserializers
// ==============================================================================

bool SightlineProtocolParser::parseStabilizationParams(
    const std::vector<std::uint8_t>& packet, MsgSetStabilizationParameters& out)
{
    if (identifyMessage(packet) != MessageId::CurrentStabilizationParameters) {
        return false;
    }

    const auto payload = extractPayload(packet);
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

bool SightlineProtocolParser::parseVideoParameters(const std::vector<std::uint8_t>& packet, MsgSetVideoParameters& out)
{
    if (identifyMessage(packet) != MessageId::CurrentVideoParameters) {
        return false;
    }

    const auto payload = extractPayload(packet);
    if (payload.size() < 8U) {
        return false;
    }

    out.autoChop = payload[0U];
    out.chopTop = payload[1U];
    out.chopBottom = payload[2U];
    out.chopLeft = payload[3U];
    out.chopRight = payload[4U];
    out.deinterlace = payload[5U];
    out.autoReset = payload[6U];
    out.cameraIndex = payload[7U];
    return true;
}

bool SightlineProtocolParser::parseH264Parameters(const std::vector<std::uint8_t>& packet, MsgSetH264Parameters& out)
{
    if (identifyMessage(packet) != MessageId::CurrentH264Parameters) {
        return false;
    }

    const auto payload = extractPayload(packet);
    if (payload.size() < 13U) {
        return false;
    }

    out.targetBitrateBps = readU32Le(payload.data());
    out.intraFrameInterval = payload[4U];
    out.lfDisableIdc = payload[5U];
    out.airMbPeriod = payload[6U];
    out.sliceRefreshRowNumber = payload[7U];
    out.flags = payload[8U];
    out.displayId = readU16Le(payload.data() + 9U);
    out.minQp = payload[11U];
    out.maxQp = payload[12U];
    return true;
}

bool SightlineProtocolParser::parseFocusStats(const std::vector<std::uint8_t>& packet, MsgFocusParameters& out)
{
    if (identifyMessage(packet) != MessageId::FocusStats) {
        return false;
    }

    const auto payload = extractPayload(packet);
    if (payload.size() < 10U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.focusMode = payload[1U];
    out.roiX = readU16Le(payload.data() + 2U);
    out.roiY = readU16Le(payload.data() + 4U);
    out.roiWidth = readU16Le(payload.data() + 6U);
    out.roiHeight = readU16Le(payload.data() + 8U);
    return true;
}

bool SightlineProtocolParser::parseMetadataValues(const std::vector<std::uint8_t>& packet, MsgSetMetadataValues& out)
{
    if (identifyMessage(packet) != MessageId::CurrentMetadataValues) {
        return false;
    }

    const auto payload = extractPayload(packet);
    if (payload.size() < 64U) {
        return false;
    }

    out.platformLatitudeDeg = readDouble64Le(payload.data());
    out.platformLongitudeDeg = readDouble64Le(payload.data() + 8U);
    out.platformAltitudeMeters = readDouble64Le(payload.data() + 16U);
    out.platformHeadingDeg = readDouble64Le(payload.data() + 24U);
    out.platformPitchDeg = readDouble64Le(payload.data() + 32U);
    out.platformRollDeg = readDouble64Le(payload.data() + 40U);
    out.sensorHorizontalFovDeg = readDouble64Le(payload.data() + 48U);
    out.sensorVerticalFovDeg = readDouble64Le(payload.data() + 56U);
    return true;
}

} // namespace Sightline
