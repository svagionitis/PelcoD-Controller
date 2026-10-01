#pragma once

/// @file SightlineSerialParser.h
/// @brief Parser deserializing raw Sightline SLA serial port, passthrough, and GPIO frames.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineSerialParser
/// @brief Deserializes serial port settings, transparent passthrough chunks, and GPIO states.
class SightlineSerialParser {
public:
    /// @brief Parses active port configuration (Message ID 0x53 / 0x3E).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized port configuration structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parsePortConfiguration(
        const std::vector<std::uint8_t>& packet, MsgSetPortConfiguration& out);

    /// @brief Parses transparent pass-through data packet (Message ID 0x3D).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized passthrough structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseCommandPassThrough(
        const std::vector<std::uint8_t>& packet, MsgCommandPassThrough& out);

    /// @brief Parses GPIO pin state telemetry (Message ID 0xB6).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized GPIO structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseGPIO(const std::vector<std::uint8_t>& packet, MsgGPIO& out);

    /// @brief Parses master I2C response / telemetry packet (Message ID 0x94).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized I2C command structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseI2CCommand(ByteView packet, MsgI2CCommand& out);
};

} // namespace Sightline
