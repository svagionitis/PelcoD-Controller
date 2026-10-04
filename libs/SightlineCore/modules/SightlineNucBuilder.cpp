/// @file SightlineNucBuilder.cpp
/// @brief Implementation of Sightline NUC and dead pixel command serializers.

#include "SightlineNucBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineNucBuilder::buildNucParameters(const MsgNucParameters& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.nucAction, msg.shutterMode };
    return SightlineFraming::buildPacket(MessageId::NucParameters, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildDeadPixel(const MsgDeadPixel& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(4U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.mode);
    SightlineFraming::appendU16Le(payload, msg.deadPixelCount);
    return SightlineFraming::buildPacket(MessageId::DeadPixel, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildGetNucParameters(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::NucParameters), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildGetDeadPixel(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::DeadPixel), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildReadWriteNuc(const MsgReadWriteNuc& msg)
{
    const std::vector<std::uint8_t> payload { msg.cameraIndex, msg.action, msg.tableIndex };
    return SightlineFraming::buildPacket(MessageId::ReadWriteNuc, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildSetUserPalette(const MsgUserPalette& msg)
{
    std::vector<std::uint8_t> payload {};
    if (msg.lutData.size() == 768U && msg.paletteIndex == 0U) {
        payload = msg.lutData;
    } else {
        payload.reserve(1U + msg.lutData.size());
        payload.push_back(msg.paletteIndex);
        payload.insert(payload.end(), msg.lutData.begin(), msg.lutData.end());
    }
    return SightlineFraming::buildPacket(MessageId::SetUserPalette, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildGetUserPalette(std::uint8_t paletteIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::SetUserPalette), paletteIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildGetDeadPixelStats(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::DeadPixelStats), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildCameraCalibration(const MsgCameraCalibration& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(1U + (8U * sizeof(float)));
    payload.push_back(msg.cameraIndex);
    SightlineFraming::appendFloat32Le(payload, msg.focalLengthX);
    SightlineFraming::appendFloat32Le(payload, msg.focalLengthY);
    SightlineFraming::appendFloat32Le(payload, msg.principalPointX);
    SightlineFraming::appendFloat32Le(payload, msg.principalPointY);
    SightlineFraming::appendFloat32Le(payload, msg.radialDistortionK1);
    SightlineFraming::appendFloat32Le(payload, msg.radialDistortionK2);
    SightlineFraming::appendFloat32Le(payload, msg.tangentialP1);
    SightlineFraming::appendFloat32Le(payload, msg.tangentialP2);
    return SightlineFraming::buildPacket(MessageId::CameraCalibration, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildGetCameraCalibration(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::CameraCalibration), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineNucBuilder::buildCameraParameterFile(const MsgCameraParameterFile& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(2U + msg.filename.size() + 1U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.action);
    SightlineFraming::appendString(payload, msg.filename);
    return SightlineFraming::buildPacket(MessageId::CameraParameterFile, payload);
}

} // namespace Sightline
