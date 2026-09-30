#pragma once

/// @file SightlineStabilizationParser.h
/// @brief Parser deserializing raw Sightline SLA stabilization frames into typed POD structures.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineStabilizationParser
/// @brief Deserializes electronic stabilization parameters from SLA telemetry.
class SightlineStabilizationParser {
public:
    /// @brief Parses active stabilization settings (Message ID 0x41 / 0x02).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized stabilization parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseStabilizationParams(
        const std::vector<std::uint8_t>& packet, MsgSetStabilizationParameters& out);

    /// @brief Parses stabilization bias settings (Message ID 0x12).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized stabilization bias.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseStabilizationBias(
        const std::vector<std::uint8_t>& packet, MsgSetStabilizationBias& out);

    /// @brief Parses frame registration parameters (Message ID 0x0E).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized registration parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseRegistration(
        const std::vector<std::uint8_t>& packet, MsgSetRegistrationParameters& out);
};

} // namespace Sightline
