/// @file SightlinePaletteParser.cpp
/// @brief Implementation of the Sightline user palette deserializer.

#include "SightlinePaletteParser.h"

namespace Sightline {

namespace {
    /// @brief Size of a full 256-entry YUV LUT.
    constexpr std::size_t kFullLutSize { 768U };
} // namespace

bool SightlinePaletteParser::parseUserPalette(ByteView packet, MsgUserPalette& out)
{
    const MessageId id { SightlineFraming::identifyMessage(packet) };
    if ((id != MessageId::SetUserPalette) && (id != MessageId::CurrentUserPalette)) {
        return false;
    }

    const ByteView payload { SightlineFraming::extractPayload(packet) };
    if (payload.empty()) {
        return false;
    }

    if (payload.size() == kFullLutSize) {
        out.paletteIndex = 0U;
        out.lutData.assign(payload.begin(), payload.end());
    } else {
        out.paletteIndex = payload[0U];
        out.lutData.assign(payload.begin() + 1, payload.end());
    }
    return true;
}

} // namespace Sightline
