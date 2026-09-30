#pragma once

/// @file SightlineGeneralBuilder.h
/// @brief Serializer for Sightline general system configuration, resets, and status modes (IDD General module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineGeneralBuilder
/// @brief Encodes version queries, parameter queries, resets, flash saves, and status reporting modes.
class SightlineGeneralBuilder {
public:
    /// @brief Encodes get version number query (Message ID 0x00).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVersionNumber();

    /// @brief Encodes a generic parameter query (Message ID 0x28).
    /// @param[in] queryId Target setter Message ID to query.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetParameters(std::uint8_t queryId);

    /// @brief Encodes system reset command (Message ID 0x01).
    /// @param[in] msg Reset parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildResetAllParameters(
        const MsgResetAllParameters& msg);

    /// @brief Encodes save to flash command (Message ID 0x25).
    /// @param[in] msg Save parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSaveParameters(
        const MsgSaveParameters& msg);

    /// @brief Encodes system status mode configuration (Message ID 0x80).
    /// @param[in] msg Status mode settings.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSystemStatusMode(
        const MsgSystemStatusMode& msg);

    /// @brief Encodes hardware ID query (Message ID 0x50).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetHardwareId();

    /// @brief Encodes query for system status mode (Message ID 0x28 query 0x80).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetSystemStatusMode();

    /// @brief Encodes query for hardware configuration (Message ID 0x28 query 0x8E).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCurrentConfig();
};

} // namespace Sightline
