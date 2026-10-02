/// @file SightlineTrackingParser.cpp
/// @brief Implementation of Sightline SLA tracking frame deserializers.

#include "SightlineTrackingParser.h"

namespace Sightline {

bool SightlineTrackingParser::parseTrackingPosition(
    const std::vector<std::uint8_t>& packet, MsgTrackingPosition& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::TrackingPosition) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 17U) {
        return false;
    }

    const auto baseCol = static_cast<double>(SightlineFraming::readS16Le(payload.data() + 0U));
    const auto baseRow = static_cast<double>(SightlineFraming::readS16Le(payload.data() + 2U));
    const auto baseSceneCol = static_cast<double>(SightlineFraming::readS16Le(payload.data() + 4U));
    const auto baseSceneRow = static_cast<double>(SightlineFraming::readS16Le(payload.data() + 6U));
    out.offsetCol = static_cast<double>(SightlineFraming::readS16Le(payload.data() + 8U));
    out.offsetRow = static_cast<double>(SightlineFraming::readS16Le(payload.data() + 10U));

    const std::uint8_t rawConf { payload[12U] };
    out.isCoasting = ((rawConf & 0x80U) != 0U);
    out.confidence = static_cast<std::uint8_t>(rawConf & 0x7FU);

    out.sceneConfidence = payload[13U];
    out.rotationDeg = static_cast<double>(SightlineFraming::readS16Le(payload.data() + 14U)) / 128.0;
    out.cameraIndex = payload[16U];

    if (payload.size() >= 18U) {
        out.userTrackId = payload[17U];
    }

    double fracCol { 0.0 };
    double fracRow { 0.0 };
    double fracSceneCol { 0.0 };
    double fracSceneRow { 0.0 };

    if (payload.size() >= 22U) {
        fracCol = static_cast<double>(payload[18U]) / 256.0;
        fracRow = static_cast<double>(payload[19U]) / 256.0;
        fracSceneCol = static_cast<double>(payload[20U]) / 256.0;
        fracSceneRow = static_cast<double>(payload[21U]) / 256.0;
    }

    out.col = (baseCol >= 0.0) ? (baseCol + fracCol) : (baseCol - fracCol);
    out.row = (baseRow >= 0.0) ? (baseRow + fracRow) : (baseRow - fracRow);
    out.translationCol = (baseSceneCol >= 0.0) ? (baseSceneCol + fracSceneCol) : (baseSceneCol - fracSceneCol);
    out.translationRow = (baseSceneRow >= 0.0) ? (baseSceneRow + fracSceneRow) : (baseSceneRow - fracSceneRow);

    if (payload.size() >= 24U) {
        out.scale = static_cast<double>(SightlineFraming::readU16Le(payload.data() + 22U)) / 256.0;
    }
    if (payload.size() >= 32U) {
        out.timestampUs = SightlineFraming::readU64Le(payload.data() + 24U);
    }
    if (payload.size() >= 36U) {
        out.frameNumber = SightlineFraming::readU32Le(payload.data() + 32U);
    }

    return true;
}

bool SightlineTrackingParser::parseTrackingPositions(
    const std::vector<std::uint8_t>& packet, MsgTrackingPositions& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::TrackingPositions) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 2U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    const std::uint8_t numTracks { static_cast<std::uint8_t>(payload[1U] & 0x7FU) };

    out.tracks.clear();
    std::size_t offset { 2U };

    for (std::uint8_t i { 0U }; i < numTracks; ++i) {
        if ((offset + 15U) > payload.size()) {
            break;
        }

        TrackCoordinate track {};
        track.trackId = payload[offset];
        track.centerCol = static_cast<double>(SightlineFraming::readS16Le(payload.data() + offset + 1U));
        track.centerRow = static_cast<double>(SightlineFraming::readS16Le(payload.data() + offset + 3U));
        track.width = static_cast<double>(SightlineFraming::readU16Le(payload.data() + offset + 5U));
        track.height = static_cast<double>(SightlineFraming::readU16Le(payload.data() + offset + 7U));
        track.velocityCol = static_cast<double>(SightlineFraming::readS16Le(payload.data() + offset + 9U)) / 256.0;
        track.velocityRow = static_cast<double>(SightlineFraming::readS16Le(payload.data() + offset + 11U)) / 256.0;

        const std::uint8_t rawConf { payload[offset + 13U] };
        track.isCoasting = ((rawConf & 0x80U) != 0U);
        track.confidence = static_cast<std::uint8_t>(rawConf & 0x7FU);
        track.isPrimary = ((payload[offset + 14U] & 0x01U) != 0U);

        out.tracks.push_back(track);
        offset += 15U;
    }

    if ((offset + 8U) <= payload.size()) {
        out.timestampUs = SightlineFraming::readU64Le(payload.data() + offset);
    }
    if ((offset + 12U) <= payload.size()) {
        out.frameNumber = SightlineFraming::readU32Le(payload.data() + offset + 8U);
    }

    return true;
}

bool SightlineTrackingParser::parsePositionsExtended(
    const std::vector<std::uint8_t>& packet, MsgTrackingPositionsExtended& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::TrackingPositionsExtended) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 2U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    const std::uint8_t numTracks { static_cast<std::uint8_t>(payload[1U] & 0x7FU) };

    out.tracks.clear();
    out.classIds.clear();
    std::size_t offset { 2U };

    for (std::uint8_t i { 0U }; i < numTracks; ++i) {
        if ((offset + 21U) > payload.size()) {
            break;
        }

        TrackCoordinate track {};
        track.trackId = payload[offset];
        track.centerCol = static_cast<double>(SightlineFraming::readS16Le(payload.data() + offset + 1U));
        track.centerRow = static_cast<double>(SightlineFraming::readS16Le(payload.data() + offset + 3U));
        track.width = static_cast<double>(SightlineFraming::readU16Le(payload.data() + offset + 5U));
        track.height = static_cast<double>(SightlineFraming::readU16Le(payload.data() + offset + 7U));
        track.velocityCol = static_cast<double>(SightlineFraming::readS16Le(payload.data() + offset + 9U)) / 256.0;
        track.velocityRow = static_cast<double>(SightlineFraming::readS16Le(payload.data() + offset + 11U)) / 256.0;

        const std::uint8_t rawConf { payload[offset + 13U] };
        track.isCoasting = ((rawConf & 0x80U) != 0U);
        track.confidence = static_cast<std::uint8_t>(rawConf & 0x7FU);
        track.isPrimary = ((payload[offset + 14U] & 0x01U) != 0U);

        out.tracks.push_back(track);
        out.classIds.push_back(payload[offset + 15U]);
        offset += 21U;
    }

    if ((offset + 8U) <= payload.size()) {
        out.timestampUs = SightlineFraming::readU64Le(payload.data() + offset);
    }
    if ((offset + 12U) <= payload.size()) {
        out.frameNumber = SightlineFraming::readU32Le(payload.data() + offset + 8U);
    }

    return true;
}

bool SightlineTrackingParser::parseTrackTrails(
    const std::vector<std::uint8_t>& packet, MsgTrackTrails& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::TrackTrails) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 7U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.flags = SightlineFraming::readU16Le(payload.data() + 1U);
    out.tracksLen = SightlineFraming::readU16Le(payload.data() + 3U);
    out.detectionLen = SightlineFraming::readU16Le(payload.data() + 5U);
    out.historyPoints.clear();

    return true;
}

bool SightlineTrackingParser::parseTrackingParameters(
    const std::vector<std::uint8_t>& packet, MsgSetTrackingParameters& out)
{
    const auto msgId { SightlineFraming::identifyMessage(packet) };
    if (msgId != MessageId::CurrentTrackingParameters && msgId != MessageId::SetTrackingParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 16U) {
        return false;
    }

    out.objectSize = payload[0U];
    out.mode = payload[1U];
    out.mode2 = payload[2U];
    out.maxMisses = payload[3U];
    out.nearVal = SightlineFraming::readU16Le(payload.data() + 4U);
    out.objectHeight = payload[6U];
    out.cameraIndex = payload[7U];
    out.zoomSmoothing = payload[8U];
    out.rollSmoothing = payload[9U];
    out.maxTracks = payload[10U];
    out.acquisitionSearchCol = SightlineFraming::readU16Le(payload.data() + 11U);
    out.acquisitionSearchRow = SightlineFraming::readU16Le(payload.data() + 13U);
    out.flags = payload[15U];
    if (payload.size() >= 17U) {
        out.maxPauseTime = payload[16U];
    } else {
        out.maxPauseTime = 0U;
    }

    return true;
}

bool SightlineTrackingParser::parseStartTracking(
    const std::vector<std::uint8_t>& packet, MsgStartTracking& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::StartTracking) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 10U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.centerCol = SightlineFraming::readU16Le(payload.data() + 1U);
    out.centerRow = SightlineFraming::readU16Le(payload.data() + 3U);
    out.width = SightlineFraming::readU16Le(payload.data() + 5U);
    out.height = SightlineFraming::readU16Le(payload.data() + 7U);
    out.flags = payload[9U];

    if (payload.size() >= 12U) {
        out.nearVal = SightlineFraming::readU16Le(payload.data() + 10U);
    } else {
        out.nearVal = 0U;
    }

    if (payload.size() >= 13U) {
        out.userTrackId = payload[12U];
    } else {
        out.userTrackId = 0U;
    }

    if (payload.size() >= 21U) {
        out.framePts = SightlineFraming::readU64Le(payload.data() + 13U);
    } else {
        out.framePts = 0ULL;
    }

    return true;
}

bool SightlineTrackingParser::parseModifyTracking(
    const std::vector<std::uint8_t>& packet, MsgModifyTracking& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::ModifyTracking) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 7U) {
        return false;
    }

    out.col = SightlineFraming::readU16Le(payload.data() + 0U);
    out.row = SightlineFraming::readU16Le(payload.data() + 2U);
    out.flags = payload[4U];
    out.width = payload[5U];
    out.height = payload[6U];

    if (payload.size() >= 8U) {
        out.cameraIndex = payload[7U];
    } else {
        out.cameraIndex = 0U;
    }

    if (payload.size() >= 9U) {
        out.trackId = payload[8U];
    } else {
        out.trackId = 0U;
    }

    if (payload.size() >= 10U) {
        out.mode = payload[9U];
    } else {
        out.mode = 0U;
    }

    return true;
}

bool SightlineTrackingParser::parseModifyTrackIndex(
    const std::vector<std::uint8_t>& packet, MsgModifyTrackIndex& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::ModifyTrackIndex) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 3U) {
        return false;
    }

    out.trackIndex = payload[0U];
    out.flags = payload[1U];
    out.cameraIndex = payload[2U];

    if (payload.size() >= 5U) {
        out.width = SightlineFraming::readU16Le(payload.data() + 3U);
    } else {
        out.width = 0U;
    }

    if (payload.size() >= 7U) {
        out.height = SightlineFraming::readU16Le(payload.data() + 5U);
    } else {
        out.height = 0U;
    }

    return true;
}

bool SightlineTrackingParser::parseNudgeTracking(
    const std::vector<std::uint8_t>& packet, MsgNudgeTrackingCoordinate& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::NudgeTrackingCoordinate) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 4U) {
        return false;
    }

    out.offsetCol = static_cast<std::int8_t>(payload[0U]);
    out.offsetRow = static_cast<std::int8_t>(payload[1U]);
    out.rotate = payload[2U];
    out.cameraIndex = payload[3U];

    if (payload.size() >= 8U) {
        out.deltaCol = SightlineFraming::readS16Le(payload.data() + 4U);
        out.deltaRow = SightlineFraming::readS16Le(payload.data() + 6U);
    } else {
        out.deltaCol = out.offsetCol;
        out.deltaRow = out.offsetRow;
    }

    return true;
}

bool SightlineTrackingParser::parseShiftSelectedTrack(
    const std::vector<std::uint8_t>& packet, MsgShiftSelectedTrack& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::ShiftSelectedTrack) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 6U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.trackId = payload[1U];
    out.shiftCol = SightlineFraming::readS16Le(payload.data() + 2U);
    out.shiftRow = SightlineFraming::readS16Le(payload.data() + 4U);

    return true;
}

} // namespace Sightline
