/// @file SightlineCaptureBuilder.cpp
/// @brief Implementation of Sightline capture command serializers.

#include "SightlineCaptureBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineCaptureBuilder::buildSetVideoParameters(const MsgSetVideoParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.autoChop, msg.chopTop, msg.chopBottom, msg.chopLeft, msg.chopRight,
        msg.deinterlace, msg.autoReset, msg.cameraIndex };
    return SightlineFraming::buildPacket(MessageId::SetVideoParameters, payload);
}

std::vector<std::uint8_t> SightlineCaptureBuilder::buildSetVideoMode(const MsgSetVideoMode& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.freeze, msg.digitalZoom, msg.mirror, msg.flip };
    return SightlineFraming::buildPacket(MessageId::SetVideoMode, payload);
}

std::vector<std::uint8_t> SightlineCaptureBuilder::buildGetVideoParameters(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetVideoParameters, payload);
}

std::vector<std::uint8_t> SightlineCaptureBuilder::buildGetVideoMode()
{
    return SightlineFraming::buildPacket(MessageId::GetVideoMode, {});
}

std::vector<std::uint8_t> SightlineCaptureBuilder::buildCameraSwitch(const MsgCameraSwitch& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.switchType, msg.flags };
    return SightlineFraming::buildPacket(MessageId::CameraSwitch, payload);
}

std::vector<std::uint8_t> SightlineCaptureBuilder::buildSetAdvCaptureParams(const MsgAdvancedCaptureParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(8U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.bitDepth);
    payload.push_back(msg.laneCount);
    SightlineFraming::appendU32Le(payload, msg.pixelClockHz);
    payload.push_back(msg.syncFlags);
    return SightlineFraming::buildPacket(MessageId::AdvancedCaptureParameters, payload);
}

std::vector<std::uint8_t> SightlineCaptureBuilder::buildGetAdvCaptureParams(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::AdvancedCaptureParameters),
        cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineCaptureBuilder::buildSetDigiVideoParser(const MsgDigitalVideoParserParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.videoStandard, msg.embeddedSync, msg.clockEdge,
        msg.flags };
    return SightlineFraming::buildPacket(MessageId::DigitalVideoParserParameters, payload);
}

std::vector<std::uint8_t> SightlineCaptureBuilder::buildGetDigiVideoParser(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::DigitalVideoParserParameters),
        cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineCaptureBuilder::buildGetCameraCapabilities(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::CameraCapabilities), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
