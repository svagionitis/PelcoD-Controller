#pragma once

/// @file SightlineNucParser.h
/// @brief Parser deserializing raw Sightline SLA NUC and dead pixel configuration telemetry packets.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineNucParser
/// @brief Deserializes NUC parameters and dead pixel replacement status frames.
class SightlineNucParser {
public:
    /// @brief Parses NUC calibration parameters (Message ID 0x35).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized NUC structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseNucParameters(
        const std::vector<std::uint8_t>& packet, MsgNucParameters& out);

    /// @brief Parses dead pixel replacement configuration (Message ID 0xA8).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized dead pixel structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseDeadPixel(
        const std::vector<std::uint8_t>& packet, MsgDeadPixel& out);
};

} // namespace Sightline
