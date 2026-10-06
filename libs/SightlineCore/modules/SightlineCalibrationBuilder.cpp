/// @file SightlineCalibrationBuilder.cpp
/// @brief Implementation of the Sightline camera calibration serializer.

#include "SightlineCalibrationBuilder.h"

namespace Sightline {

namespace {
    /// @brief Number of float32 coefficients in 0xC0.
    constexpr std::size_t kCalibFloats { 8U };
} // namespace

std::vector<std::uint8_t> SightlineCalibrationBuilder::buildCameraCalibration(const MsgCameraCalibration& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(1U + (kCalibFloats * sizeof(float)));
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

std::vector<std::uint8_t> SightlineCalibrationBuilder::buildGetCameraCalibration(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::CameraCalibration), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineCalibrationBuilder::buildCameraParameterFile(const MsgCameraParameterFile& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(2U + msg.filename.size() + 1U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.action);
    SightlineFraming::appendString(payload, msg.filename);
    return SightlineFraming::buildPacket(MessageId::CameraParameterFile, payload);
}

} // namespace Sightline
