/// @file SightlineOverlayBuilder.cpp
/// @brief Implementation of Sightline Overlays module builder.

#include "SightlineOverlayBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineOverlayBuilder::buildSetOverlayMode(
    const MsgSetOverlayMode& msg)
{
    const std::vector<std::uint8_t> payload {
        msg.displayIndex, msg.reticleMode, msg.trackingBoxMode, msg.telemetryTextMode
    };
    return SightlineFraming::buildPacket(MessageId::SetOverlayMode, payload);
}

std::vector<std::uint8_t> SightlineOverlayBuilder::buildDrawObject(
    const MsgDrawObject& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(15U + msg.text.size() + 1U);
    payload.push_back(msg.displayIndex);
    payload.push_back(msg.objectId);
    payload.push_back(msg.shapeType);
    SightlineFraming::appendU16Le(payload, msg.x);
    SightlineFraming::appendU16Le(payload, msg.y);
    SightlineFraming::appendU16Le(payload, msg.width);
    SightlineFraming::appendU16Le(payload, msg.height);
    SightlineFraming::appendU32Le(payload, msg.colorRgba);
    SightlineFraming::appendString(payload, msg.text);
    return SightlineFraming::buildPacket(MessageId::DrawObject, payload);
}

std::vector<std::uint8_t> SightlineOverlayBuilder::buildDrawOverlay(
    const MsgDrawOverlay& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(2U + (msg.objects.size() * 16U));
    payload.push_back(msg.displayIndex);
    payload.push_back(msg.clearDisplay);
    payload.push_back(static_cast<std::uint8_t>(msg.objects.size()));

    for (const auto& obj : msg.objects) {
        payload.push_back(obj.objectId);
        payload.push_back(obj.shapeType);
        SightlineFraming::appendU16Le(payload, obj.x);
        SightlineFraming::appendU16Le(payload, obj.y);
        SightlineFraming::appendU16Le(payload, obj.width);
        SightlineFraming::appendU16Le(payload, obj.height);
        SightlineFraming::appendU32Le(payload, obj.colorRgba);
        SightlineFraming::appendString(payload, obj.text);
    }

    return SightlineFraming::buildPacket(MessageId::DrawOverlay, payload);
}

std::vector<std::uint8_t> SightlineOverlayBuilder::buildGetOverlayMode(
    std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetOverlayMode, payload);
}

} // namespace Sightline
