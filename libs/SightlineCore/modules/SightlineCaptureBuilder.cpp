/// @file SightlineCaptureBuilder.cpp
/// @brief Implementation of Sightline capture command serializers.

#include "SightlineCaptureBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineCaptureBuilder::buildSetVideoParameters(
    const MsgSetVideoParameters& msg)
{
    const std::vector<std::uint8_t> payload {
        msg.autoChop,
        msg.chopTop,
        msg.chopBottom,
        msg.chopLeft,
        msg.chopRight,
        msg.deinterlace,
        msg.autoReset,
        msg.cameraIndex
    };
    return SightlineFraming::buildPacket(MessageId::SetVideoParameters, payload);
}

std::vector<std::uint8_t> SightlineCaptureBuilder::buildSetVideoMode(
    const MsgSetVideoMode& msg)
{
    const std::vector<std::uint8_t> payload {
        msg.cameraIndex,
        msg.freeze,
        msg.digitalZoom,
        msg.mirror,
        msg.flip
    };
    return SightlineFraming::buildPacket(MessageId::SetVideoMode, payload);
}

std::vector<std::uint8_t> SightlineCaptureBuilder::buildGetVideoParameters(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetVideoParameters, payload);
}

std::vector<std::uint8_t> SightlineCaptureBuilder::buildGetVideoMode()
{
    return SightlineFraming::buildPacket(MessageId::GetVideoMode, {});
}

} // namespace Sightline
