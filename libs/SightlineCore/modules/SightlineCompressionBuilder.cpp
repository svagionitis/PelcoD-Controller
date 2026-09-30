/// @file SightlineCompressionBuilder.cpp
/// @brief Implementation of Sightline compression command serializers.

#include "SightlineCompressionBuilder.h"

namespace Sightline {

std::vector<std::uint8_t> SightlineCompressionBuilder::buildSetH264Parameters(
    const MsgSetH264Parameters& msg)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(13U);
    SightlineFraming::appendU32Le(payload, msg.targetBitrateBps);
    payload.push_back(msg.intraFrameInterval);
    payload.push_back(msg.lfDisableIdc);
    payload.push_back(msg.airMbPeriod);
    payload.push_back(msg.sliceRefreshRowNumber);
    payload.push_back(msg.flags);
    SightlineFraming::appendU16Le(payload, msg.displayId);
    payload.push_back(msg.minQp);
    payload.push_back(msg.maxQp);
    return SightlineFraming::buildPacket(MessageId::SetH264Parameters, payload);
}

std::vector<std::uint8_t> SightlineCompressionBuilder::buildStreamingControl(
    const MsgStreamingControl& msg)
{
    const std::vector<std::uint8_t> payload { msg.streamIndex, msg.action };
    return SightlineFraming::buildPacket(MessageId::StreamingControl, payload);
}

std::vector<std::uint8_t> SightlineCompressionBuilder::buildGetH264Parameters(
    std::uint16_t displayId)
{
    std::vector<std::uint8_t> payload {};
    payload.reserve(2U);
    SightlineFraming::appendU16Le(payload, displayId);
    return SightlineFraming::buildPacket(MessageId::GetH264Parameters, payload);
}

std::vector<std::uint8_t> SightlineCompressionBuilder::buildGetStreamingControl(
    std::uint8_t streamIndex)
{
    const std::vector<std::uint8_t> payload {
        static_cast<std::uint8_t>(MessageId::StreamingControl), streamIndex
    };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
