#pragma once

/// @file SightlineCompressionBuilder.h
/// @brief Serializer for Sightline video compression, encoding, and bitrate control (IDD Compression module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineCompressionBuilder
/// @brief Encodes H.264/H.265 compression bitrate, intra-frame interval, QP limits, and slice refresh.
class SightlineCompressionBuilder {
public:
    /// @brief Encodes H.264 compression bitrate and GOP (Message ID 0x23).
    /// @param[in] msg Encoder parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetH264Parameters(
        const MsgSetH264Parameters& msg);

    /// @brief Encodes video stream start/stop control (Message ID 0x90).
    /// @param[in] msg Streaming control parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildStreamingControl(
        const MsgStreamingControl& msg);

    /// @brief Encodes query for active H.264 parameters (Message ID 0x24).
    /// @param[in] displayId Network display mask ID (e.g. 0x0002 for Net0).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetH264Parameters(
        std::uint16_t displayId = 0x0002U);

    /// @brief Encodes query for active streaming control parameters (Message ID 0x28 query 0x90).
    /// @param[in] streamIndex Target stream index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetStreamingControl(
        std::uint8_t streamIndex = 0U);
};

} // namespace Sightline
