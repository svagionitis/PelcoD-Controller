/// @file SightlineNucParser.cpp
/// @brief Implementation of Sightline SLA NUC and dead pixel frame deserializers.

#include "SightlineNucParser.h"

namespace Sightline {

bool SightlineNucParser::parseNucParameters(ByteView packet, MsgNucParameters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::NucParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 3U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.nucAction = payload[1U];
    out.shutterMode = payload[2U];
    return true;
}

bool SightlineNucParser::parseDeadPixel(ByteView packet, MsgDeadPixel& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::DeadPixel) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 4U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.mode = payload[1U];
    out.deadPixelCount = SightlineFraming::readU16Le(payload.data() + 2U);
    return true;
}

bool SightlineNucParser::parseReadWriteNuc(ByteView packet, MsgReadWriteNuc& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::ReadWriteNuc) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 3U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.action = payload[1U];
    out.tableIndex = payload[2U];
    return true;
}

bool SightlineNucParser::parseUserPalette(ByteView packet, MsgUserPalette& out)
{
    const auto id = SightlineFraming::identifyMessage(packet);
    if (id != MessageId::SetUserPalette && id != MessageId::CurrentUserPalette) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.empty()) {
        return false;
    }

    out.paletteIndex = payload[0U];
    if (payload.size() > 1U) {
        out.lutData.assign(payload.data() + 1U, payload.data() + payload.size());
    } else {
        out.lutData.clear();
    }
    return true;
}

bool SightlineNucParser::parseDeadPixelStats(ByteView packet, MsgDeadPixelStats& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::DeadPixelStats) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 7U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.deadPixelCount = SightlineFraming::readU16Le(payload.data() + 1U);
    out.badColumns = SightlineFraming::readU16Le(payload.data() + 3U);
    out.badRows = SightlineFraming::readU16Le(payload.data() + 5U);
    return true;
}

bool SightlineNucParser::parseCameraCalibration(ByteView packet, MsgCameraCalibration& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CameraCalibration) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 33U) {
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

bool SightlineNucParser::parseCameraParameterFile(ByteView packet, MsgCameraParameterFile& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CameraParameterFile) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 2U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.action = payload[1U];

    if (payload.size() > 2U) {
        const char* strStart = reinterpret_cast<const char*>(payload.data() + 2U);
        std::size_t strLen { payload.size() - 2U };
        while (strLen > 0U && strStart[strLen - 1U] == '\0') {
            --strLen;
        }
        out.filename.assign(strStart, strLen);
    } else {
        out.filename.clear();
    }

    return true;
}

} // namespace Sightline
