#pragma once

/// @file SightlineBlendingParser.h
/// @brief Parser deserializing raw Sightline SLA two-channel video blending telemetry frames.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineBlendingParser
/// @brief Deserializes two-channel video blending parameters (Message ID 0x4D / 0x2F).
class SightlineBlendingParser {
public:
    /// @brief Parses active blending parameters (Message ID 0x4D / 0x2F).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized blend parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseBlendParameters(
        const std::vector<std::uint8_t>& packet, MsgSetBlendParameters& out);
};

} // namespace Sightline
