/// @file SightlineFocusBuilder.cpp
/// @brief Implementation of Sightline focus and lens command serializers.

#include "SightlineFocusBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineFocusBuilder::buildLensCommand(
    const MsgLensCommand& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(4U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.commandType);
    SightlineFraming::appendS16Le(payload, msg.rateOrPosition);
    return SightlineFraming::buildPacket(MessageId::LensCommand, payload);
}

std::vector<std::uint8_t> SightlineFocusBuilder::buildFocusParameters(
    const MsgFocusParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(10U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.focusMode);
    SightlineFraming::appendU16Le(payload, msg.roiX);
    SightlineFraming::appendU16Le(payload, msg.roiY);
    SightlineFraming::appendU16Le(payload, msg.roiWidth);
    SightlineFraming::appendU16Le(payload, msg.roiHeight);
    return SightlineFraming::buildPacket(MessageId::FocusParameters, payload);
}

std::vector<std::uint8_t> SightlineFocusBuilder::buildSetLensParameters(
    const MsgSetLensParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(33U);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendDouble64Le(payload, msg.minFocalLengthMm);
    SightlineFraming::appendDouble64Le(payload, msg.maxFocalLengthMm);
    SightlineFraming::appendDouble64Le(payload, msg.horizontalFovWideDeg);
    SightlineFraming::appendDouble64Le(payload, msg.horizontalFovTeleDeg);
    return SightlineFraming::buildPacket(MessageId::SetLensParameters, payload);
}

std::vector<std::uint8_t> SightlineFocusBuilder::buildGetFocusParameters(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::FocusParameters), cameraIndex
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineFocusBuilder::buildGetLensParameters(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::SetLensParameters), cameraIndex
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
