/// @file SightlineTrackingBuilder.cpp
/// @brief Implementation of Sightline video tracking and motion command serializers.

#include "SightlineTrackingBuilder.h"
#include "SightlineDetectionBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineTrackingBuilder::buildStartTracking(const MsgStartTracking& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(21U);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendU16Le(payload, msg.centerCol);
    SightlineFraming::appendU16Le(payload, msg.centerRow);
    SightlineFraming::appendU16Le(payload, msg.width);
    SightlineFraming::appendU16Le(payload, msg.height);
    payload.push_back(msg.flags);
    SightlineFraming::appendU16Le(payload, msg.nearVal);
    payload.push_back(msg.userTrackId);
    SightlineFraming::appendU64Le(payload, msg.framePts);

    return SightlineFraming::buildPacket(MessageId::StartTracking, payload);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildStartPrecision(
    std::uint8_t cameraIndex, std::uint16_t col, std::uint16_t row,
    std::uint16_t width, std::uint16_t height, std::uint64_t framePtsUs,
    std::uint8_t flags)
{
    MsgStartTracking msg {};
    msg.cameraIndex = cameraIndex;
    msg.centerCol = col;
    msg.centerRow = row;
    msg.width = width;
    msg.height = height;
    msg.flags = flags;
    msg.framePts = framePtsUs;
    return buildStartTracking(msg);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildStopTracking(const MsgStopTracking& msg)
{
    const std::vector<std::uint8_t> payload { 0x00U, 0x00U, 0x00U, msg.cameraIndex };
    return SightlineFraming::buildPacket(MessageId::StopTracking, payload);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildModifyTracking(const MsgModifyTracking& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve((msg.trackId != 0U || msg.mode != 0U) ? 10U : 7U);
    SightlineFraming::appendU16Le(payload, msg.col);
    SightlineFraming::appendU16Le(payload, msg.row);
    payload.push_back(msg.flags);
    payload.push_back(msg.width);
    payload.push_back(msg.height);
    payload.push_back(msg.cameraIndex);
    if (msg.trackId != 0U || msg.mode != 0U) {
        payload.push_back(msg.trackId);
        payload.push_back(msg.mode);
    }
    return SightlineFraming::buildPacket(MessageId::ModifyTracking, payload);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildModifyTrackingMode(
    std::uint8_t cameraIndex, std::uint16_t col, std::uint16_t row,
    ModifyMode mode, std::uint8_t trackId,
    std::uint8_t width, std::uint8_t height)
{
    MsgModifyTracking msg {};
    msg.cameraIndex = cameraIndex;
    msg.col = col;
    msg.row = row;
    msg.width = width;
    msg.height = height;
    msg.trackId = trackId;
    msg.mode = static_cast<std::uint8_t>(mode);
    return buildModifyTracking(msg);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildNudgeTracking(const MsgNudgeTrackingCoordinate& msg)
{
    const std::int8_t offCol { (msg.offsetCol != 0) ? msg.offsetCol : static_cast<std::int8_t>(msg.deltaCol) };
    const std::int8_t offRow { (msg.offsetRow != 0) ? msg.offsetRow : static_cast<std::int8_t>(msg.deltaRow) };
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(offCol), static_cast<std::uint8_t>(offRow),
        msg.rotate, msg.cameraIndex };
    return SightlineFraming::buildPacket(MessageId::NudgeTrackingCoordinate, payload);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildNudgeTrackingRotated(
    std::uint8_t cameraIndex, std::int16_t deltaCol, std::int16_t deltaRow,
    NudgeCoordinateMode coordMode)
{
    MsgNudgeTrackingCoordinate msg {};
    msg.cameraIndex = cameraIndex;
    msg.deltaCol = deltaCol;
    msg.deltaRow = deltaRow;
    msg.rotate = static_cast<std::uint8_t>(coordMode);
    return buildNudgeTracking(msg);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildSetReportingMode(const MsgCoordinateReportingMode& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(4U);
    payload.push_back(msg.framePeriod);
    const std::uint16_t f { (msg.flags != 0U) ? msg.flags : static_cast<std::uint16_t>(msg.reportingFlags) };
    SightlineFraming::appendU16Le(payload, f);
    payload.push_back(msg.cameraIndex);
    return SightlineFraming::buildPacket(MessageId::CoordinateReportingMode, payload);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildSetTrackingParameters(const MsgSetTrackingParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve((msg.maxPauseTime > 0U) ? 17U : 16U);
    payload.push_back(msg.objectSize);
    payload.push_back(msg.mode);
    payload.push_back(msg.mode2);
    payload.push_back(msg.maxMisses);
    SightlineFraming::appendU16Le(payload, msg.nearVal);
    payload.push_back(msg.objectHeight);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.zoomSmoothing);
    payload.push_back(msg.rollSmoothing);
    payload.push_back(msg.maxTracks);
    SightlineFraming::appendU16Le(payload, msg.acquisitionSearchCol);
    SightlineFraming::appendU16Le(payload, msg.acquisitionSearchRow);
    payload.push_back(msg.flags);
    if (msg.maxPauseTime > 0U) {
        payload.push_back(msg.maxPauseTime);
    }
    return SightlineFraming::buildPacket(MessageId::SetTrackingParameters, payload);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildModifyTrackIndex(const MsgModifyTrackIndex& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(7U);
    payload.push_back(msg.trackIndex);
    payload.push_back(msg.flags);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendU16Le(payload, msg.width);
    SightlineFraming::appendU16Le(payload, msg.height);
    return SightlineFraming::buildPacket(MessageId::ModifyTrackIndex, payload);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildModifyTrackIndex(
    std::uint8_t cameraIndex, std::uint8_t trackIndex, TrackIndexAction action,
    std::uint16_t width, std::uint16_t height)
{
    MsgModifyTrackIndex msg {};
    msg.cameraIndex = cameraIndex;
    msg.trackIndex = trackIndex;
    msg.flags = static_cast<std::uint8_t>(action);
    msg.width = width;
    msg.height = height;
    return buildModifyTrackIndex(msg);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildForcedCoasting(
    std::uint8_t cameraIndex, std::uint8_t trackIndex, ForcedCoastingMode mode)
{
    TrackIndexAction action { TrackIndexAction::CoastNone };
    switch (mode) {
    case ForcedCoastingMode::FreezeUpdates:
        action = TrackIndexAction::CoastFreezeUpdates;
        break;
    case ForcedCoastingMode::FreezeSearch:
        action = TrackIndexAction::CoastFreezeSearch;
        break;
    case ForcedCoastingMode::FreezePropagation:
        action = TrackIndexAction::CoastFreezePropagation;
        break;
    case ForcedCoastingMode::None:
    default:
        action = TrackIndexAction::CoastNone;
        break;
    }
    return buildModifyTrackIndex(cameraIndex, trackIndex, action);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildTrackTrails(const MsgTrackTrails& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(7U);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendU16Le(payload, msg.flags);
    SightlineFraming::appendU16Le(payload, msg.tracksLen);
    SightlineFraming::appendU16Le(payload, msg.detectionLen);
    return SightlineFraming::buildPacket(MessageId::TrackTrails, payload);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildDesignatePrimary(const MsgDesignateSelectedTrackPrimary& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.trackId };
    return SightlineFraming::buildPacket(MessageId::DesignateSelectedTrackPrimary, payload);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildShiftSelectedTrack(const MsgShiftSelectedTrack& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(6U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.trackId);
    SightlineFraming::appendS16Le(payload, msg.shiftCol);
    SightlineFraming::appendS16Le(payload, msg.shiftRow);
    return SightlineFraming::buildPacket(MessageId::ShiftSelectedTrack, payload);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildStopSelectedTrack(const MsgStopSelectedTrack& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.trackId };
    return SightlineFraming::buildPacket(MessageId::StopSelectedTrack, payload);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildSetDetectionParams(const MsgSetDetectionParameters& msg)
{
    return SightlineDetectionBuilder::buildSetDetectionParams(msg);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildCustomAIDetect(const MsgCustomAIDetect& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.modelId, msg.confidenceThreshold, msg.nmsThreshold };
    return SightlineFraming::buildPacket(MessageId::CustomAIDetect, payload);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildGetTrackingParameters(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetTrackingParameters, payload);
}

std::vector<std::uint8_t> SightlineTrackingBuilder::buildGetTrackTrails(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::TrackTrails), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
