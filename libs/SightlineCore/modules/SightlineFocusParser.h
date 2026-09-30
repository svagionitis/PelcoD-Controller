#pragma once

/// @file SightlineFocusParser.h
/// @brief Parser deserializing raw Sightline SLA auto-focus statistics (IDD Focus module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineFocusParser
/// @brief Deserializes auto-focus statistics and feedback parameters.
class SightlineFocusParser {
public:
    /// @brief Parses active auto-focus statistics (Message ID 0x55).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized focus parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseFocusStats(
        const std::vector<std::uint8_t>& packet, MsgFocusParameters& out);

    /// @brief Parses lens optical calibration parameters (Message ID 0x6E / 0x6F / 0xB1).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized lens parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseLensParameters(
        const std::vector<std::uint8_t>& packet, MsgSetLensParameters& out);
};

} // namespace Sightline
