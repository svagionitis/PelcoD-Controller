/// @file SightlineDetectionBuilder.cpp
/// @brief Implementation of Sightline detection command serializers.

#include "SightlineDetectionBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineDetectionBuilder::buildSetDetectionParams(
    const MsgSetDetectionParameters& msg)
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

} // namespace Sightline
