/// @file SightlineDetectionBuilder.cpp
/// @brief Implementation of Sightline detection command serializers.

#include "SightlineDetectionBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineDetectionBuilder::buildSetDetectionParams(const MsgSetDetectionParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(7U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.mode);
    payload.push_back(msg.threshold);
    SightlineFraming::appendU16Le(payload, msg.minTargetSize);
    SightlineFraming::appendU16Le(payload, msg.maxTargetSize);
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
    payload.reserve(11U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.roiIndex);
    payload.push_back(msg.roiType);
    SightlineFraming::appendU16Le(payload, msg.left);
    SightlineFraming::appendU16Le(payload, msg.top);
    SightlineFraming::appendU16Le(payload, msg.width);
    SightlineFraming::appendU16Le(payload, msg.height);
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
    payload.reserve(8U);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendU16Le(payload, msg.minVelocity);
    SightlineFraming::appendU16Le(payload, msg.maxVelocity);
    payload.push_back(msg.persistenceFrames);
    SightlineFraming::appendU16Le(payload, msg.mergeDistance);
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
