#pragma once

/// @file SightlineSerialBuilder.h
/// @brief Serializer for Sightline serial port configuration, passthrough tunnels, and GPIO.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineSerialBuilder
/// @brief Encodes serial port baud rates, transparent command pass-through, and GPIO pin states.
class SightlineSerialBuilder {
public:
    /// @brief Encodes serial port configuration (Message ID 0x3E).
    /// @param[in] msg Port parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetPortConfiguration(
        const MsgSetPortConfiguration& msg);

    /// @brief Encodes transparent pass-through data packet (Message ID 0x3D).
    /// @param[in] msg Passthrough payload.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCommandPassThrough(
        const MsgCommandPassThrough& msg);

    /// @brief Encodes GPIO pin state and direction (Message ID 0xB6).
    /// @param[in] msg GPIO parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGPIO(
        const MsgGPIO& msg);

    /// @brief Encodes query for active port configuration (Message ID 0x3F).
    /// @param[in] port Port index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetPortConfiguration(
        std::uint8_t port = 0U);

    /// @brief Encodes query for GPIO pin states (Message ID 0x28 query 0xB6).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetGPIO();
};

} // namespace Sightline
