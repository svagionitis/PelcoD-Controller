#pragma once

/// @file SightlineCompressionParser.h
/// @brief Parser deserializing raw Sightline SLA compression parameters (IDD Compression module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineCompressionParser
/// @brief Deserializes H.264/H.265 compression bitrate, intra-frame rate, and QP status.
class SightlineCompressionParser {
public:
    /// @brief Parses active H.264 compression parameters (Message ID 0x56).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized H.264 parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseH264Parameters(
        const std::vector<std::uint8_t>& packet, MsgSetH264Parameters& out);

    /// @brief Parses streaming control command/telemetry (Message ID 0x90).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized streaming control structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseStreamingControl(
        const std::vector<std::uint8_t>& packet, MsgStreamingControl& out);
};

} // namespace Sightline
