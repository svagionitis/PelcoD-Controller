/// @file SightlineOverlayParser.cpp
/// @brief Implementation of Sightline Overlays module response deserializer.

#include "SightlineOverlayParser.h"

namespace Sightline {

bool SightlineOverlayParser::parseOverlayMode(
    const std::vector<std::uint8_t>& packet, MsgSetOverlayMode& out)
{
    const auto id = SightlineFraming::identifyMessage(packet);
    if (id != MessageId::CurrentOverlayMode && id != MessageId::SetOverlayMode) {
        return false;
    }

    const auto payload = SightlineFraming::extractPayload(packet);
    if (payload.size() < 4U) {
        return false;
    }

    out.displayIndex = payload[0U];
    out.reticleMode = payload[1U];
    out.trackingBoxMode = payload[2U];
    out.telemetryTextMode = payload[3U];
    return true;
}

} // namespace Sightline
