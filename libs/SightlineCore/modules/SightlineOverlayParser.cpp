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

    out.displayIndex = payload[0U];
    out.reticleMode = payload[1U];
    out.trackingBoxMode = payload[2U];
    out.telemetryTextMode = payload[3U];
    return true;
}

bool SightlineOverlayParser::parseDrawObject(ByteView packet, MsgDrawObject& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::DrawObject) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 15U) {
        return false;
    }

    out.displayIndex = payload[0U];
    out.objectId = payload[1U];
    out.shapeType = payload[2U];
    out.x = SightlineFraming::readU16Le(payload.data() + 3U);
    out.y = SightlineFraming::readU16Le(payload.data() + 5U);
    out.width = SightlineFraming::readU16Le(payload.data() + 7U);
    out.height = SightlineFraming::readU16Le(payload.data() + 9U);
    out.colorRgba = SightlineFraming::readU32Le(payload.data() + 11U);

    out.text.clear();
    if (payload.size() > 15U) {
        auto strLen { payload.size() - 15U };
        if (payload[payload.size() - 1U] == 0U) {
            --strLen;
        }
        out.text.assign(reinterpret_cast<const char*>(payload.data() + 15U), strLen);
    }
    return true;
}

bool SightlineOverlayParser::parseDrawOverlay(ByteView packet, MsgDrawOverlay& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::DrawOverlay) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 3U) {
        return false;
    }

    out.displayIndex = payload[0U];
    out.clearDisplay = payload[1U];
    const auto numObjects { payload[2U] };
    out.objects.clear();
    out.objects.reserve(numObjects);

    std::size_t offset { 3U };
    for (std::uint8_t i { 0U }; i < numObjects && offset + 14U <= payload.size(); ++i) {
        MsgDrawObject obj {};
        obj.displayIndex = out.displayIndex;
        obj.objectId = payload[offset];
        obj.shapeType = payload[offset + 1U];
        obj.x = SightlineFraming::readU16Le(payload.data() + offset + 2U);
        obj.y = SightlineFraming::readU16Le(payload.data() + offset + 4U);
        obj.width = SightlineFraming::readU16Le(payload.data() + offset + 6U);
        obj.height = SightlineFraming::readU16Le(payload.data() + offset + 8U);
        obj.colorRgba = SightlineFraming::readU32Le(payload.data() + offset + 10U);
        offset += 14U;

        std::size_t strEnd { offset };
        while (strEnd < payload.size() && payload[strEnd] != 0U) {
            ++strEnd;
        }
        obj.text.assign(reinterpret_cast<const char*>(payload.data() + offset), strEnd - offset);
        if (strEnd < payload.size() && payload[strEnd] == 0U) {
            offset = strEnd + 1U;
        } else {
            offset = strEnd;
        }
        out.objects.push_back(std::move(obj));
    }
    return true;
}

bool SightlineOverlayParser::parseLogoParameters(ByteView packet, MsgLogoParameters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::LogoParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 9U) {
        return false;
    }

    out.displayIndex = payload[0U];
    out.logoIndex = payload[1U];
    out.enable = payload[2U];
    out.x = SightlineFraming::readU16Le(payload.data() + 3U);
    out.y = SightlineFraming::readU16Le(payload.data() + 5U);
    out.opacity = payload[7U];
    out.scale = payload[8U];
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

    out.displayIndex = payload[0U];
    out.lineIndex = payload[1U];
    out.x = SightlineFraming::readU16Le(payload.data() + 2U);
    out.y = SightlineFraming::readU16Le(payload.data() + 4U);
    out.fontId = payload[6U];
    out.colorRgba = SightlineFraming::readU32Le(payload.data() + 7U);

    out.text.clear();
    if (payload.size() > 11U) {
        auto strLen { payload.size() - 11U };
        if (payload[payload.size() - 1U] == 0U) {
            --strLen;
        }
        out.text.assign(reinterpret_cast<const char*>(payload.data() + 11U), strLen);
    }
    return true;
}

bool SightlineOverlayParser::parseUserFont(ByteView packet, MsgUserFont& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::UserFont) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 5U) {
        return false;
    }

    out.fontId = payload[0U];
    out.charWidth = payload[1U];
    out.charHeight = payload[2U];
    out.firstChar = payload[3U];
    out.numChars = payload[4U];

    if (payload.size() > 5U) {
        out.glyphData.assign(payload.begin() + 5U, payload.end());
    } else {
        out.glyphData.clear();
    }
    return true;
}

} // namespace Sightline
