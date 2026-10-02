/// @file SightlineDetectionBuilder.cpp
/// @brief Implementation of Sightline detection command serializers.

#include "SightlineDetectionBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineDetectionBuilder::buildSetDetectionParams(const MsgSetDetectionParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(12U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(static_cast<std::uint8_t>(msg.mode));
    payload.push_back(msg.threshold);
    SightlineFraming::appendU16Le(payload, msg.minTargetSize);
    SightlineFraming::appendU16Le(payload, msg.maxTargetSize);

    const bool hasExtended { msg.detectionIndex != 0U || msg.sensitivityMode != SensitivityMode::Auto
        || msg.bkgdThreshold != 20U || msg.watchFrames != 3U || msg.suspiciousScore != 10U };

    if (hasExtended) {
        payload.push_back(msg.detectionIndex);
        payload.push_back(static_cast<std::uint8_t>(msg.sensitivityMode));
        payload.push_back(msg.bkgdThreshold);
        payload.push_back(msg.watchFrames);
        payload.push_back(msg.suspiciousScore);
    }

    return SightlineFraming::buildPacket(MessageId::SetDetectionParameters, payload);
}

std::vector<std::uint8_t> SightlineDetectionBuilder::buildGetDetectionParams(
    std::uint8_t cameraIndex, std::uint8_t detIdx)
{
    const std::vector<std::uint8_t> payload { cameraIndex, detIdx };
    return SightlineFraming::buildPacket(MessageId::GetDetectionParameters, payload);
}

std::vector<std::uint8_t> SightlineDetectionBuilder::buildSetVMTI(const MsgSetVMTI& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(8U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.enable);
    payload.push_back(msg.sensitivity);
    SightlineFraming::appendU16Le(payload, msg.minTargetArea);
    SightlineFraming::appendU16Le(payload, msg.maxTargetArea);
    payload.push_back(msg.mode);
    return SightlineFraming::buildPacket(MessageId::SetVMTI, payload);
}

std::vector<std::uint8_t> SightlineDetectionBuilder::buildGetVMTI(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::SetVMTI), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineDetectionBuilder::buildSetDetectionROI(const MsgDetectionROI& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(57U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.roiIndex);
    payload.push_back(msg.roiType);
    SightlineFraming::appendU16Le(payload, msg.left);
    SightlineFraming::appendU16Le(payload, msg.top);
    SightlineFraming::appendU16Le(payload, msg.width);
    SightlineFraming::appendU16Le(payload, msg.height);

    const bool hasExtended { msg.detectionIndex != 0U || msg.geometryMode != RoiGeometryMode::BoundingBox
        || msg.lineLeftX != 0U || msg.lineLeftY != 0U || msg.lineRightX != 0U || msg.lineRightY != 0U
        || msg.lineSide != LineReportSide::Both || msg.blocksWide != 8U || msg.blocksHigh != 8U
        || msg.gridMasks[0U] != 0U || msg.gridMasks[1U] != 0U || msg.gridMasks[2U] != 0U || msg.gridMasks[3U] != 0U
        || msg.showRegions };

    if (hasExtended) {
        payload.push_back(msg.detectionIndex);
        payload.push_back(static_cast<std::uint8_t>(msg.geometryMode));
        SightlineFraming::appendU16Le(payload, msg.lineLeftX);
        SightlineFraming::appendU16Le(payload, msg.lineLeftY);
        SightlineFraming::appendU16Le(payload, msg.lineRightX);
        SightlineFraming::appendU16Le(payload, msg.lineRightY);
        payload.push_back(static_cast<std::uint8_t>(msg.lineSide));
        payload.push_back(msg.blocksWide);
        payload.push_back(msg.blocksHigh);
        for (std::size_t i { 0U }; i < 4U; ++i) {
            SightlineFraming::appendU64Le(payload, msg.gridMasks[i]);
        }
        payload.push_back(static_cast<std::uint8_t>(msg.showRegions ? 1U : 0U));
    }

    return SightlineFraming::buildPacket(MessageId::SetDetectionRegionOfInterestParameters, payload);
}

std::vector<std::uint8_t> SightlineDetectionBuilder::buildGetDetectionROI(
    std::uint8_t cameraIndex, std::uint8_t roiIndex)
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::SetDetectionRegionOfInterestParameters), cameraIndex, roiIndex
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineDetectionBuilder::buildSetAdvDetectionParams(
    const MsgAdvancedDetectionParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(25U);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendU16Le(payload, msg.minVelocity);
    SightlineFraming::appendU16Le(payload, msg.maxVelocity);
    payload.push_back(msg.persistenceFrames);
    SightlineFraming::appendU16Le(payload, msg.mergeDistance);

    const bool hasExtended { msg.detectionIndex != 0U || !msg.hideOverlapTracks || msg.detectNearTrack
        || msg.averageTimeConstant != 10U || msg.edgePenalty != 64U || msg.nFramesBack != 10U || !msg.useRegistration
        || msg.updateRate != 32U || msg.surroundSize != 25U || msg.blobDirection != BlobDirection::Both
        || msg.use8BitImages || msg.gasAddOriginal != 128U || msg.gasColor != 0U || msg.aiIouThreshold != 45U
        || msg.enableMtd || msg.downsample != DetectionDownsample::Auto };

    if (hasExtended) {
        payload.push_back(msg.detectionIndex);
        payload.push_back(static_cast<std::uint8_t>(msg.hideOverlapTracks ? 1U : 0U));
        payload.push_back(static_cast<std::uint8_t>(msg.detectNearTrack ? 1U : 0U));
        payload.push_back(msg.averageTimeConstant);
        payload.push_back(msg.edgePenalty);
        payload.push_back(msg.nFramesBack);
        payload.push_back(static_cast<std::uint8_t>(msg.useRegistration ? 1U : 0U));
        payload.push_back(msg.updateRate);
        payload.push_back(msg.surroundSize);
        payload.push_back(static_cast<std::uint8_t>(msg.blobDirection));
        payload.push_back(static_cast<std::uint8_t>(msg.use8BitImages ? 1U : 0U));
        payload.push_back(msg.gasAddOriginal);
        payload.push_back(msg.gasColor);
        payload.push_back(msg.aiIouThreshold);
        payload.push_back(static_cast<std::uint8_t>(msg.enableMtd ? 1U : 0U));
        payload.push_back(static_cast<std::uint8_t>(msg.downsample));
    }

    return SightlineFraming::buildPacket(MessageId::SetAdvancedDetectionParameters, payload);
}

std::vector<std::uint8_t> SightlineDetectionBuilder::buildGetAdvDetectionParams(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::SetAdvancedDetectionParameters),
        cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineDetectionBuilder::buildGetTrackingPixelStats(
    std::uint8_t cameraIndex, std::uint8_t trackId)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::TrackingBoxPixelStats), cameraIndex,
        trackId };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineDetectionBuilder::buildDoDetectSnapShot(const MsgDoDetectSnapShot& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.detectionIndex };
    return SightlineFraming::buildPacket(MessageId::DoDetectSnapShot, payload);
}

} // namespace Sightline
