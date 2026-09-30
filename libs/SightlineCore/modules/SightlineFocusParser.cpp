/// @file SightlineFocusParser.cpp
/// @brief Implementation of Sightline focus frame deserializers.

#include "SightlineFocusParser.h"

namespace Sightline {

bool SightlineFocusParser::parseFocusStats(
    const std::vector<std::uint8_t>& packet, MsgFocusParameters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::FocusStats) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 10U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.focusMode = payload[1U];
    out.roiX = SightlineFraming::readU16Le(payload.data() + 2U);
    out.roiY = SightlineFraming::readU16Le(payload.data() + 4U);
    out.roiWidth = SightlineFraming::readU16Le(payload.data() + 6U);
    out.roiHeight = SightlineFraming::readU16Le(payload.data() + 8U);
    return true;
}

bool SightlineFocusParser::parseLensParameters(
    const std::vector<std::uint8_t>& packet, MsgSetLensParameters& out)
{
    const auto msgId { SightlineFraming::identifyMessage(packet) };
    if (msgId != MessageId::SetLensParameters &&
        msgId != MessageId::CurrentLensParameters &&
        msgId != MessageId::LensParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 33U) {
        return false;
    }

    out.cameraIndex = payload[0U];
    out.minFocalLengthMm = SightlineFraming::readDouble64Le(payload.data() + 1U);
    out.maxFocalLengthMm = SightlineFraming::readDouble64Le(payload.data() + 9U);
    out.horizontalFovWideDeg = SightlineFraming::readDouble64Le(payload.data() + 17U);
    out.horizontalFovTeleDeg = SightlineFraming::readDouble64Le(payload.data() + 25U);
    return true;
}

} // namespace Sightline
