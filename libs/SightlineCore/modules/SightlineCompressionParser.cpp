/// @file SightlineCompressionParser.cpp
/// @brief Implementation of Sightline compression frame deserializers.

#include "SightlineCompressionParser.h"

namespace Sightline {

bool SightlineCompressionParser::parseH264Parameters(const std::vector<std::uint8_t>& packet, MsgSetH264Parameters& out)
{
    const auto id { SightlineFraming::identifyMessage(packet) };
    if (id != MessageId::CurrentH264Parameters && id != MessageId::SetH264Parameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 13U) {
        return false;
    }

    out.targetBitrateBps = SightlineFraming::readU32Le(payload.data());
    out.intraFrameInterval = payload[4U];
    out.lfDisableIdc = payload[5U];
    out.airMbPeriod = payload[6U];
    out.sliceRefreshRowNumber = payload[7U];
    out.flags = payload[8U];
    out.displayId = SightlineFraming::readU16Le(payload.data() + 9U);
    out.minQp = payload[11U];
    out.maxQp = payload[12U];
    return true;
}

bool SightlineCompressionParser::parseStreamingControl(
    const std::vector<std::uint8_t>& packet, MsgStreamingControl& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::StreamingControl) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 2U) {
        return false;
    }

    out.streamIndex = payload[0U];
    out.action = payload[1U];
    return true;
}

bool SightlineCompressionParser::parseDecoderParameters(ByteView packet, MsgDecoderParameters& out)
{
    if (SightlineFraming::identifyMessage(packet) != MessageId::DecoderParameters) {
        return false;
    }

    const auto payload { SightlineFraming::extractPayload(packet) };
    if (payload.size() < 11U) {
        return false;
    }

    out.decoderIndex = payload[0U];
    out.enable = payload[1U];
    out.codec = payload[2U];
    out.networkPort = SightlineFraming::readU16Le(payload.data() + 3U);
    out.bufferDepthMs = SightlineFraming::readU16Le(payload.data() + 5U);
    out.multicastIp = SightlineFraming::readU32Le(payload.data() + 7U);
    return true;
}

} // namespace Sightline
