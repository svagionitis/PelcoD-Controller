#pragma once

/// @file SightlineGeneralParser.h
/// @brief Deserializer for Sightline general system responses and status messages (IDD General module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineGeneralParser
/// @brief Parses version numbers, system status, user warnings, and current hardware configuration.
class SightlineGeneralParser {
public:
    /// @brief Parses system version information reply (Message ID 0x40 / 0x00).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized version structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseVersionNumber(
        const std::vector<std::uint8_t>& packet, MsgVersionNumber& out);

    /// @brief Parses diagnostic user warning message (Message ID 0x86).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized warning structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseUserWarning(
        const std::vector<std::uint8_t>& packet, MsgUserWarningMessage& out);

    /// @brief Parses periodic system health status message (Message ID 0x87).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized status structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseSystemStatus(
        const std::vector<std::uint8_t>& packet, MsgSystemStatusMessage& out);

    /// @brief Parses hardware camera and display configuration reply (Message ID 0x8E).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized configuration structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseCurrentConfiguration(
        const std::vector<std::uint8_t>& packet, MsgCurrentConfiguration& out);

    /// @brief Parses system status mode reply (Message ID 0x80).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized status mode structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseSystemStatusMode(
        const std::vector<std::uint8_t>& packet, MsgSystemStatusMode& out);
};

} // namespace Sightline
