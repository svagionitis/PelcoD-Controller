#pragma once

/// @file SightlineDetectionParser.h
/// @brief Parser deserializing raw Sightline SLA detection telemetry frames.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineDetectionParser
/// @brief Deserializes active MTI detection parameters (Message ID 0x54 / 0x2D).
class SightlineDetectionParser {
public:
    /// @brief Parses active detection parameters (Message ID 0x54 / 0x2D).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized detection parameters structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseDetectionParams(
        const std::vector<std::uint8_t>& packet, MsgSetDetectionParameters& out);
};

} // namespace Sightline
