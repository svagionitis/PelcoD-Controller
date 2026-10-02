/// @file SightlineOverlayParser.cpp
/// @brief Implementation of Sightline Overlays module response deserializer.

#include "SightlineOverlayParser.h"

namespace Sightline {

bool SightlineOverlayParser::parseOverlayMode(ByteView packet, MsgSetOverlayMode& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::CurrentOverlayMode && id != MessageId::SetOverlayMode) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 4U) {
        return false;
    }

    out.primaryReticle = payload[0U];
    out.secondaryReticle = payload[1U];
    out.graphics = SightlineFraming::readU16Le(payload.data() + 2U);

    if (payload.size() >= 5U) {
        out.mtiColor = payload[4U];
    }
    if (payload.size() >= 6U) {
        out.mtiSelectableColor = payload[5U];
    }
    if (payload.size() >= 7U) {
        out.cameraIndex = payload[6U];
    }
    if (payload.size() >= 8U) {
        out.selectedReticle = payload[7U];
    }
    if (payload.size() >= 9U) {
        out.personReticle = payload[8U];
    }
    if (payload.size() >= 10U) {
        out.cursorReticle = payload[9U];
    }
    if (payload.size() >= 11U) {
        out.lineThickness = payload[10U];
    }
    if (payload.size() >= 12U) {
        out.fontScale = payload[11U];
    }
    if (payload.size() >= 13U) {
        out.fontId = payload[12U];
    }
    if (payload.size() >= 14U) {
        out.extraLabels = payload[13U];
    }
    if (payload.size() >= 15U) {
        out.modernMode = payload[14U];
    }
    if (payload.size() >= 16U) {
        out.mtiReticle = payload[15U];
    }
    if (payload.size() >= 17U) {
        out.mtiLabelAdv = payload[16U];
    }
    if (payload.size() >= 18U) {
        out.mtiMultiColor = payload[17U];
    }
    if (payload.size() >= 19U) {
        out.detCounterColor = payload[18U];
    }
    if (payload.size() >= 20U) {
        out.mtiDisplayLabelLimit = payload[19U];
    }
    if (payload.size() >= 21U) {
        out.detCounterPos = payload[20U];
    }
    if (payload.size() >= 22U) {
        out.mtiLabelDeconflict = payload[21U];
    }

    return true;
}

bool SightlineOverlayParser::parseDrawOverlay(ByteView packet, MsgDrawOverlay& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::DrawOverlay) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 15U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.objectId = payload[1U];
    out.action = payload[2U];
    out.propertyFlags = payload[3U];
    out.type = static_cast<OverlayObjectType>(payload[4U]);
    out.a = SightlineFraming::readU16Le(payload.data() + 5U);
    out.b = SightlineFraming::readU16Le(payload.data() + 7U);
    out.c = SightlineFraming::readU16Le(payload.data() + 9U);
    out.d = SightlineFraming::readU16Le(payload.data() + 11U);
    out.backgroundColor = payload[13U];

    const auto textLen = static_cast<std::size_t>(payload[14U]);
    if (payload.size() < 15U + textLen) {
        return false;
    }

    out.text.assign(reinterpret_cast<const char*>(payload.data() + 15U), textLen);
    const std::size_t optOffset { 15U + textLen };

    out.hasE = false;
    out.hasF = false;
    if (payload.size() >= optOffset + 2U) {
        out.e = SightlineFraming::readU16Le(payload.data() + optOffset);
        out.hasE = true;
        if (payload.size() >= optOffset + 4U) {
            out.f = SightlineFraming::readU16Le(payload.data() + optOffset + 2U);
            out.hasF = true;
        }
    }
    return true;
}

bool SightlineOverlayParser::parseDrawObject(ByteView packet, MsgDrawObject& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::DrawObject) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 13U) {
        return false;
    }

    out.objectId = payload[0U];
    out.action = payload[1U];
    out.propertyFlags = payload[2U];
    out.shapeType = payload[3U];
    out.a = SightlineFraming::readU16Le(payload.data() + 4U);
    out.b = SightlineFraming::readU16Le(payload.data() + 6U);
    out.c = SightlineFraming::readU16Le(payload.data() + 8U);
    out.d = SightlineFraming::readU16Le(payload.data() + 10U);
    out.color = payload[12U];

    out.text.clear();
    if (payload.size() > 13U) {
        auto strLen { payload.size() - 13U };
        if (payload[payload.size() - 1U] == 0U) {
            --strLen;
        }
        out.text.assign(reinterpret_cast<const char*>(payload.data() + 13U), strLen);
    }
    return true;
}

bool SightlineOverlayParser::parseLogoParameters(ByteView packet, MsgLogoParameters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::LogoParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 6U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.logoOpacity = payload[1U];
    out.offsetX = SightlineFraming::readU16Le(payload.data() + 2U);
    out.offsetY = SightlineFraming::readU16Le(payload.data() + 4U);
    return true;
}

bool SightlineOverlayParser::parseUserFont(ByteView packet, MsgUserFont& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::UserFont) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 2U) {
        return false;
    }

    out.userFontIndex = payload[0U];
    const auto nameLen = static_cast<std::size_t>(payload[1U]);
    if (payload.size() < 2U + nameLen) {
        return false;
    }
    out.fontFileName.assign(reinterpret_cast<const char*>(payload.data() + 2U), nameLen);
    return true;
}

bool SightlineOverlayParser::parseOverlayObjectsIds(ByteView packet, MsgCurrentOverlayObjectsIds& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CurrentOverlayObjectsIds) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 32U) {
        return false;
    }

    out.idMask[0U] = SightlineFraming::readU64Le(payload.data());
    out.idMask[1U] = SightlineFraming::readU64Le(payload.data() + 8U);
    out.idMask[2U] = SightlineFraming::readU64Le(payload.data() + 16U);
    out.idMask[3U] = SightlineFraming::readU64Le(payload.data() + 24U);
    return true;
}

bool SightlineOverlayParser::parseOverlayObjectParams(ByteView packet, MsgCurrentOverlayObjectParameters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::CurrentOverlayObjectParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 13U) {
        return false;
    }

    out.type = payload[0U];
    out.objectId = payload[1U];
    out.flags = payload[2U];
    out.staticObject = payload[3U];
    out.a = SightlineFraming::readU16Le(payload.data() + 4U);
    out.b = SightlineFraming::readU16Le(payload.data() + 6U);
    out.c = SightlineFraming::readU16Le(payload.data() + 8U);
    out.d = SightlineFraming::readU16Le(payload.data() + 10U);
    out.color = payload[12U];

    out.text.clear();
    if (payload.size() > 13U) {
        auto strLen { payload.size() - 13U };
        if (payload[payload.size() - 1U] == 0U) {
            --strLen;
        }
        out.text.assign(reinterpret_cast<const char*>(payload.data() + 13U), strLen);
    }
    return true;
}

bool SightlineOverlayParser::parseAncillaryTextMetadata(ByteView packet, MsgAncillaryTextMetadata& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::AncillaryTextMetadata) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 11U) {
        return false;
    }

    out.creationTime = SightlineFraming::readU64Le(payload.data());
    std::size_t offset { 8U };

    const auto srcLen = static_cast<std::size_t>(payload[offset++]);
    if (offset + srcLen > payload.size()) {
        return false;
    }
    out.source.assign(reinterpret_cast<const char*>(payload.data() + offset), srcLen);
    offset += srcLen;

    if (offset >= payload.size()) {
        return false;
    }
    const auto origLen = static_cast<std::size_t>(payload[offset++]);
    if (offset + origLen > payload.size()) {
        return false;
    }
    out.originator.assign(reinterpret_cast<const char*>(payload.data() + offset), origLen);
    offset += origLen;

    if (offset >= payload.size()) {
        return false;
    }
    const auto bodyLen = static_cast<std::size_t>(payload[offset++]);
    if (offset + bodyLen > payload.size()) {
        return false;
    }
    out.messageBody.assign(reinterpret_cast<const char*>(payload.data() + offset), bodyLen);
    offset += bodyLen;

    if (offset + 2U <= payload.size()) {
        out.displayId = SightlineFraming::readU16Le(payload.data() + offset);
    } else {
        out.displayId = 0x0002U;
    }
    return true;
}

} // namespace Sightline
