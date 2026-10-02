/// @file SightlineOverlayBuilder.cpp
/// @brief Implementation of Sightline Overlays module builder.

#include "SightlineOverlayBuilder.h"

#include <algorithm>

namespace Sightline {

std::vector<std::uint8_t> SightlineOverlayBuilder::buildSetOverlayMode(const MsgSetOverlayMode& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(21U);
    payload.push_back(msg.primaryReticle);
    payload.push_back(msg.secondaryReticle);
    SightlineFraming::appendU16Le(payload, msg.graphics);
    payload.push_back(msg.mtiColor);
    payload.push_back(msg.mtiSelectableColor);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.selectedReticle);
    payload.push_back(msg.personReticle);
    payload.push_back(msg.cursorReticle);
    payload.push_back(msg.lineThickness);
    payload.push_back(msg.fontScale);
    payload.push_back(msg.fontId);
    payload.push_back(msg.extraLabels);
    payload.push_back(msg.modernMode);
    payload.push_back(msg.mtiReticle);
    payload.push_back(msg.mtiLabelAdv);
    payload.push_back(msg.mtiMultiColor);
    payload.push_back(msg.detCounterColor);
    payload.push_back(msg.mtiDisplayLabelLimit);
    payload.push_back(msg.detCounterPos);
    payload.push_back(msg.mtiLabelDeconflict);
    return SightlineFraming::buildPacket(MessageId::SetOverlayMode, payload);
}

std::vector<std::uint8_t> SightlineOverlayBuilder::buildGetOverlayMode(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetOverlayMode, payload);
}

std::vector<std::uint8_t> SightlineOverlayBuilder::buildDrawOverlay(const MsgDrawOverlay& msg)
{
    std::vector<std::uint8_t> payload {};
    const auto textLen = static_cast<std::uint8_t>(std::min<std::size_t>(msg.text.size(), 64U));
    std::size_t estimatedSize { 15U + textLen };
    if (msg.hasE || msg.hasF) {
        estimatedSize += 2U;
        if (msg.hasF) {
            estimatedSize += 2U;
        }
    }
    payload.reserve(estimatedSize);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.objectId);
    payload.push_back(msg.action);
    payload.push_back(msg.propertyFlags);
    payload.push_back(static_cast<std::uint8_t>(msg.type));
    SightlineFraming::appendU16Le(payload, msg.a);
    SightlineFraming::appendU16Le(payload, msg.b);
    SightlineFraming::appendU16Le(payload, msg.c);
    SightlineFraming::appendU16Le(payload, msg.d);
    payload.push_back(msg.backgroundColor);
    payload.push_back(textLen);
    if (textLen > 0U) {
        payload.insert(payload.end(), msg.text.begin(), msg.text.begin() + textLen);
    }
    if (msg.hasE || msg.hasF) {
        SightlineFraming::appendU16Le(payload, msg.e);
        if (msg.hasF) {
            SightlineFraming::appendU16Le(payload, msg.f);
        }
    }
    return SightlineFraming::buildPacket(MessageId::DrawOverlay, payload);
}

std::vector<std::uint8_t> SightlineOverlayBuilder::buildDrawObject(const MsgDrawObject& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(13U + msg.text.size() + 1U);
    payload.push_back(msg.objectId);
    payload.push_back(msg.action);
    payload.push_back(msg.propertyFlags);
    payload.push_back(msg.shapeType);
    SightlineFraming::appendU16Le(payload, msg.a);
    SightlineFraming::appendU16Le(payload, msg.b);
    SightlineFraming::appendU16Le(payload, msg.c);
    SightlineFraming::appendU16Le(payload, msg.d);
    payload.push_back(msg.color);
    SightlineFraming::appendString(payload, msg.text);
    return SightlineFraming::buildPacket(MessageId::DrawObject, payload);
}

std::vector<std::uint8_t> SightlineOverlayBuilder::buildSetLogoParameters(const MsgLogoParameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(6U);
    payload.push_back(msg.cameraIndex);
    payload.push_back(msg.logoOpacity);
    SightlineFraming::appendU16Le(payload, msg.offsetX);
    SightlineFraming::appendU16Le(payload, msg.offsetY);
    return SightlineFraming::buildPacket(MessageId::LogoParameters, payload);
}

std::vector<std::uint8_t> SightlineOverlayBuilder::buildGetLogoParameters(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::LogoParameters), cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineOverlayBuilder::buildUserFont(const MsgUserFont& msg)
{
    std::vector<std::uint8_t> payload {};
    const auto nameLen = static_cast<std::uint8_t>(std::min<std::size_t>(msg.fontFileName.size(), 255U));
    payload.reserve(2U + nameLen);
    payload.push_back(msg.userFontIndex);
    payload.push_back(nameLen);
    if (nameLen > 0U) {
        payload.insert(payload.end(), msg.fontFileName.begin(), msg.fontFileName.begin() + nameLen);
    }
    return SightlineFraming::buildPacket(MessageId::UserFont, payload);
}

std::vector<std::uint8_t> SightlineOverlayBuilder::buildGetOverlayObjectsIds(std::uint8_t cameraIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::CurrentOverlayObjectsIds),
        cameraIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineOverlayBuilder::buildGetOverlayObjectParams(std::uint8_t objectId)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::CurrentOverlayObjectParameters),
        objectId };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

std::vector<std::uint8_t> SightlineOverlayBuilder::buildAncillaryTextMetadata(const MsgAncillaryTextMetadata& msg)
{
    std::vector<std::uint8_t> payload {};
    const auto srcLen = static_cast<std::uint8_t>(std::min<std::size_t>(msg.source.size(), 8U));
    const auto origLen = static_cast<std::uint8_t>(std::min<std::size_t>(msg.originator.size(), 16U));
    const auto bodyLen = static_cast<std::uint8_t>(std::min<std::size_t>(msg.messageBody.size(), 255U));
    payload.reserve(8U + 1U + srcLen + 1U + origLen + 1U + bodyLen + 2U);
    SightlineFraming::appendU64Le(payload, msg.creationTime);
    payload.push_back(srcLen);
    if (srcLen > 0U) {
        payload.insert(payload.end(), msg.source.begin(), msg.source.begin() + srcLen);
    }
    payload.push_back(origLen);
    if (origLen > 0U) {
        payload.insert(payload.end(), msg.originator.begin(), msg.originator.begin() + origLen);
    }
    payload.push_back(bodyLen);
    if (bodyLen > 0U) {
        payload.insert(payload.end(), msg.messageBody.begin(), msg.messageBody.begin() + bodyLen);
    }
    SightlineFraming::appendU16Le(payload, msg.displayId);
    return SightlineFraming::buildPacket(MessageId::AncillaryTextMetadata, payload);
}

} // namespace Sightline
