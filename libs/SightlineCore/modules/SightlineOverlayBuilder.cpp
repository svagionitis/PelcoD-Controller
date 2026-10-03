/// @file SightlineOverlayBuilder.cpp
/// @brief Implementation of Sightline Overlays module builder.

#include "SightlineOverlayBuilder.h"
#include "SightlineKlvBuilder.h"

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
        for (std::size_t i { 0U }; i < textLen; ++i) {
            payload.push_back(static_cast<std::uint8_t>(msg.text[i]));
        }
    }
    if (msg.hasE || msg.hasF) {
        SightlineFraming::appendU16Le(payload, msg.e);
        if (msg.hasF) {
            SightlineFraming::appendU16Le(payload, msg.f);
        }
    }
    return SightlineFraming::buildPacket(MessageId::DrawOverlay, payload);
}

std::vector<std::uint8_t> SightlineOverlayBuilder::buildDrawOverlayBatch(const std::vector<MsgDrawOverlay>& objects)
{
    std::vector<std::uint8_t> out {};
    out.reserve(objects.size() * 32U);
    for (const auto& obj : objects) {
        const auto pkt { buildDrawOverlay(obj) };
        out.insert(out.end(), pkt.begin(), pkt.end());
    }
    return out;
}

MsgDrawOverlay SightlineOverlayBuilder::makeCrossOverlay(std::uint8_t cameraIndex, std::uint8_t objectId,
    std::int16_t centerX, std::int16_t centerY, std::uint16_t size, OverlayPaletteColor fgColor,
    std::uint16_t thickness, bool originUpperLeft)
{
    MsgDrawOverlay msg {};
    msg.cameraIndex = cameraIndex;
    msg.objectId = objectId;
    msg.action = OverlayActionFlags::Create;
    msg.propertyFlags = OverlayPropertyFlags::CoordDisplayStatic;
    if (originUpperLeft) {
        msg.propertyFlags |= OverlayPropertyFlags::OriginUpperLeft;
    }
    msg.type = OverlayObjectType::Cross;
    msg.a = static_cast<std::uint16_t>(centerX);
    msg.b = static_cast<std::uint16_t>(centerY);
    msg.c = size;
    msg.d = 0U;
    msg.backgroundColor = static_cast<std::uint8_t>(((static_cast<std::uint8_t>(fgColor) & 0x0FU) << 4U)
        | (static_cast<std::uint8_t>(OverlayPaletteColor::TransparentBgOrTurquoiseFg) & 0x0FU));
    msg.text.clear();
    msg.e = thickness;
    msg.hasE = true;
    msg.hasF = false;
    return msg;
}

MsgDrawOverlay SightlineOverlayBuilder::makeRectangleOverlay(std::uint8_t cameraIndex, std::uint8_t objectId,
    std::int16_t x, std::int16_t y, std::uint16_t width, std::uint16_t height, bool filled, OverlayPaletteColor fgColor,
    OverlayPaletteColor bgColor, std::uint8_t alpha, std::uint16_t thickness, bool originUpperLeft)
{
    MsgDrawOverlay msg {};
    msg.cameraIndex = cameraIndex;
    msg.objectId = objectId;
    const auto alphaBits = static_cast<std::uint8_t>((alpha & 0x1FU) << 3U);
    msg.action = static_cast<std::uint8_t>(OverlayActionFlags::Create | alphaBits);
    msg.propertyFlags = OverlayPropertyFlags::CoordDisplayStatic;
    if (originUpperLeft) {
        msg.propertyFlags |= OverlayPropertyFlags::OriginUpperLeft;
    }
    msg.type = filled ? OverlayObjectType::FilledRectangle : OverlayObjectType::Rectangle;
    msg.a = static_cast<std::uint16_t>(x);
    msg.b = static_cast<std::uint16_t>(y);
    msg.c = width;
    msg.d = height;
    msg.backgroundColor = static_cast<std::uint8_t>(
        ((static_cast<std::uint8_t>(fgColor) & 0x0FU) << 4U) | (static_cast<std::uint8_t>(bgColor) & 0x0FU));
    msg.text.clear();
    msg.e = thickness;
    msg.hasE = true;
    msg.hasF = false;
    return msg;
}

MsgDrawOverlay SightlineOverlayBuilder::makeTextOverlay(std::uint8_t cameraIndex, std::uint8_t objectId, std::int16_t x,
    std::int16_t y, const std::string& text, OverlayFontId fontId, OverlayPaletteColor fgColor,
    OverlayPaletteColor bgColor, std::uint8_t hScale, std::uint8_t vScale, bool originUpperLeft)
{
    MsgDrawOverlay msg {};
    msg.cameraIndex = cameraIndex;
    msg.objectId = objectId;
    msg.action = OverlayActionFlags::Create;
    msg.propertyFlags = OverlayPropertyFlags::CoordDisplayStatic;
    if (originUpperLeft) {
        msg.propertyFlags |= OverlayPropertyFlags::OriginUpperLeft;
    }
    msg.type = OverlayObjectType::TextExtended;
    msg.a = static_cast<std::uint16_t>(x);
    msg.b = static_cast<std::uint16_t>(y);
    msg.c = static_cast<std::uint16_t>((static_cast<std::uint16_t>(vScale) << 8U) | static_cast<std::uint16_t>(hScale));
    msg.d = static_cast<std::uint16_t>(static_cast<std::uint8_t>(fontId) & 0x1FU);
    msg.backgroundColor = static_cast<std::uint8_t>(
        ((static_cast<std::uint8_t>(fgColor) & 0x0FU) << 4U) | (static_cast<std::uint8_t>(bgColor) & 0x0FU));
    msg.text = text;
    msg.e = 0U;
    msg.hasE = true;
    msg.hasF = false;
    return msg;
}

MsgDrawOverlay SightlineOverlayBuilder::makeKlvFieldOverlay(std::uint8_t cameraIndex, std::uint8_t objectId,
    std::int16_t x, std::int16_t y, KlvFieldTag fieldTag, KlvFormatType formatType, const std::string& formatString,
    OverlayFontId fontId, OverlayPaletteColor fgColor, bool originUpperLeft)
{
    MsgDrawOverlay msg {};
    msg.cameraIndex = cameraIndex;
    msg.objectId = objectId;
    msg.action = OverlayActionFlags::Create;
    msg.propertyFlags = OverlayPropertyFlags::CoordDisplayStatic;
    if (originUpperLeft) {
        msg.propertyFlags |= OverlayPropertyFlags::OriginUpperLeft;
    }
    msg.type = OverlayObjectType::KlvField;
    msg.a = static_cast<std::uint16_t>(x);
    msg.b = static_cast<std::uint16_t>(y);
    msg.c = static_cast<std::uint16_t>((32U << 8U) | (static_cast<std::uint8_t>(fontId) & 0x1FU));
    msg.d = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(formatType) << 8U) | (static_cast<std::uint8_t>(fieldTag) & 0xFFU));
    msg.backgroundColor = static_cast<std::uint8_t>(((static_cast<std::uint8_t>(fgColor) & 0x0FU) << 4U)
        | (static_cast<std::uint8_t>(OverlayPaletteColor::TransparentBgOrTurquoiseFg) & 0x0FU));
    msg.text = formatString;
    msg.e = 0U;
    msg.hasE = false;
    msg.hasF = false;
    return msg;
}

MsgDrawOverlay SightlineOverlayBuilder::makeBlackoutOverlay(
    std::uint8_t cameraIndex, std::uint8_t objectId, std::uint16_t width, std::uint16_t height)
{
    MsgDrawOverlay msg {};
    msg.cameraIndex = cameraIndex;
    msg.objectId = objectId;
    msg.action = OverlayActionFlags::Create;
    msg.propertyFlags = OverlayPropertyFlags::OriginUpperLeft | OverlayPropertyFlags::CoordDisplayStatic;
    msg.type = OverlayObjectType::FilledRectangle;
    msg.a = 1U;
    msg.b = 1U;
    msg.c = width;
    msg.d = height;
    msg.backgroundColor = static_cast<std::uint8_t>((static_cast<std::uint8_t>(OverlayPaletteColor::Black) << 4U)
        | (static_cast<std::uint8_t>(OverlayPaletteColor::Black) & 0x0FU));
    msg.text.clear();
    msg.e = 1U;
    msg.hasE = true;
    msg.hasF = false;
    return msg;
}

MsgDrawOverlay SightlineOverlayBuilder::makeDestroyOverlay(std::uint8_t cameraIndex, std::uint8_t objectId)
{
    MsgDrawOverlay msg {};
    msg.cameraIndex = cameraIndex;
    msg.objectId = objectId;
    msg.action = OverlayActionFlags::Destroy;
    msg.propertyFlags = OverlayPropertyFlags::CoordDisplayStatic;
    msg.type = OverlayObjectType::Circle;
    msg.a = 0U;
    msg.b = 0U;
    msg.c = 0U;
    msg.d = 0U;
    msg.backgroundColor = 0U;
    msg.text.clear();
    msg.e = 0U;
    msg.hasE = false;
    msg.hasF = false;
    return msg;
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
        for (std::size_t i { 0U }; i < nameLen; ++i) {
            payload.push_back(static_cast<std::uint8_t>(msg.fontFileName[i]));
        }
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
    return SightlineKlvBuilder::buildAncillaryTextMetadata(msg);
}

} // namespace Sightline
