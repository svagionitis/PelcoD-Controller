/// @file SightlinePaletteBuilder.cpp
/// @brief Implementation of the Sightline user palette serializer.

#include "SightlinePaletteBuilder.h"

namespace Sightline {

namespace {
    /// @brief Size of a full 256-entry YUV LUT.
    constexpr std::size_t kFullLutSize { 768U };
} // namespace

std::vector<std::uint8_t> SightlinePaletteBuilder::buildSetUserPalette(const MsgUserPalette& msg)
{
    std::vector<std::uint8_t> payload {};
    if ((msg.lutData.size() == kFullLutSize) && (msg.paletteIndex == 0U)) {
        payload = msg.lutData;
    } else {
        payload.reserve(1U + msg.lutData.size());
        payload.push_back(msg.paletteIndex);
        payload.insert(payload.end(), msg.lutData.begin(), msg.lutData.end());
    }
    return SightlineFraming::buildPacket(MessageId::SetUserPalette, payload);
}

std::vector<std::uint8_t> SightlinePaletteBuilder::buildGetUserPalette(std::uint8_t paletteIndex)
{
    const std::vector<std::uint8_t> payload { static_cast<std::uint8_t>(MessageId::SetUserPalette), paletteIndex };
    return SightlineFraming::buildPacket(MessageId::GetParameters, payload);
}

} // namespace Sightline
