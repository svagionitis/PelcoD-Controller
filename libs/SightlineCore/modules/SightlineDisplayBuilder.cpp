/// @file SightlineDisplayBuilder.cpp
/// @brief Implementation of Sightline display layout command serializers.

#include "SightlineDisplayBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineDisplayBuilder::buildSetDisplayParams(const MsgSetDisplayParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(10U);
    payload.push_back(msg.displayIndex);
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendU16Le(payload, msg.xOffset);
    SightlineFraming::appendU16Le(payload, msg.yOffset);
    SightlineFraming::appendU16Le(payload, msg.displayWidth);
    SightlineFraming::appendU16Le(payload, msg.displayHeight);
    return SightlineFraming::buildPacket(MessageId::SetDisplayParameters, payload);
}

std::vector<std::uint8_t> SightlineDisplayBuilder::buildGetDisplayParams(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetDisplayParameters, payload);
}

std::vector<std::uint8_t> SightlineDisplayBuilder::buildSetVideoDisplay(const MsgVideoDisplay& msg)
{
    const std::vector<std::uint8_t> payload { msg.displayIndex, msg.cameraIndex, msg.aspectRatio, msg.rotation,
        msg.mirror };
    return SightlineFraming::buildPacket(MessageId::VideoDisplay, payload);
}

std::vector<std::uint8_t> SightlineDisplayBuilder::buildGetVideoDisplay(std::uint8_t displayIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::VideoDisplay), displayIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineDisplayBuilder::buildSetMultiDisplay(const MsgMultiDisplay& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(11U);
    payload.push_back(msg.displayIndex);
    payload.push_back(msg.layout);
    payload.push_back(msg.pipCameraIndex);
    SightlineFraming::appendU16Le(payload, msg.pipX);
    SightlineFraming::appendU16Le(payload, msg.pipY);
    SightlineFraming::appendU16Le(payload, msg.pipWidth);
    SightlineFraming::appendU16Le(payload, msg.pipHeight);
    return SightlineFraming::buildPacket(MessageId::MultiDisplay, payload);
}

std::vector<std::uint8_t> SightlineDisplayBuilder::buildGetMultiDisplay(std::uint8_t displayIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::MultiDisplay), displayIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
