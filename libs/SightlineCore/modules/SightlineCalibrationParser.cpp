/// @file SightlineCalibrationParser.cpp
/// @brief Implementation of the Sightline camera calibration deserializer.

#include "SightlineCalibrationParser.h"

namespace Sightline {

namespace {
    /// @brief 0xC0 payload size: camera index + 8 x float32.
    constexpr std::size_t kCalibSize { 33U };
    /// @brief 0xC2 fixed prefix: camera index + action.
    constexpr std::size_t kParamFileMin { 2U };
} // namespace

bool SightlineCalibrationParser::parseCameraCalibration(ByteView packet, MsgCameraCalibration& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CameraCalibration) {
        return false;
    }

    const ByteView payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < kCalibSize) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.focalLengthX = SightlineFraming::readFloat32Le(payload.data() + 1U);
    out.focalLengthY = SightlineFraming::readFloat32Le(payload.data() + 5U);
    out.principalPointX = SightlineFraming::readFloat32Le(payload.data() + 9U);
    out.principalPointY = SightlineFraming::readFloat32Le(payload.data() + 13U);
    out.radialDistortionK1 = SightlineFraming::readFloat32Le(payload.data() + 17U);
    out.radialDistortionK2 = SightlineFraming::readFloat32Le(payload.data() + 21U);
    out.tangentialP1 = SightlineFraming::readFloat32Le(payload.data() + 25U);
    out.tangentialP2 = SightlineFraming::readFloat32Le(payload.data() + 29U);
    return true;
}

bool SightlineCalibrationParser::parseCameraParameterFile(ByteView packet, MsgCameraParameterFile& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CameraParameterFile) {
        return false;
    }

    const ByteView payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < kParamFileMin) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.action = payload[1U];

    // Copy byte-wise (no reinterpret_cast), then drop the trailing NUL terminator(s).
    std::size_t len { payload.size() - kParamFileMin };
    while ((len > 0U) && (payload[(kParamFileMin + len) - 1U] == 0U)) {
        --len;
    }
    out.filename.clear();
    out.filename.reserve(len);
    for (std::size_t i { 0U }; i < len; ++i) {
        out.filename.push_back(static_cast<char>(payload[kParamFileMin + i]));
    }
    return true;
}

} // namespace Sightline
