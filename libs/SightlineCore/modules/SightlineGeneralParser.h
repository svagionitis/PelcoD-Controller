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
    [[nodiscard]] static bool parseVersionNumber(const std::vector<std::uint8_t>& packet, MsgVersionNumber& out);

    /// @brief Parses diagnostic user warning message (Message ID 0x86).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized warning structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseUserWarning(const std::vector<std::uint8_t>& packet, MsgUserWarningMessage& out);

    /// @brief Parses periodic system health status message (Message ID 0x87).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized status structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseSystemStatus(const std::vector<std::uint8_t>& packet, MsgSystemStatusMessage& out);

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
    [[nodiscard]] static bool parseSystemStatusMode(const std::vector<std::uint8_t>& packet, MsgSystemStatusMode& out);

    /// @brief Parses system value register reply (Message ID 0x92 / 0x93).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized system value structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseSystemValue(ByteView packet, MsgSystemValue& out);

    /// @brief Parses custom binary tag data frame (Message ID 0x96).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized tag data structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseTagData(ByteView packet, MsgTagData& out);

    /// @brief Parses tag data rate reply (Message ID 0x97).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized tag data rate structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseTagDataRate(ByteView packet, MsgTagDataRate& out);

    /// @brief Parses tag source selector reply (Message ID 0x98).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized tag source selector structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseTagSourceSelector(ByteView packet, MsgTagSourceSelector& out);

    /// @brief Parses detailed latency timing telemetry (Message ID 0x88).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized latency measurements.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseDetailedTiming(ByteView packet, MsgDetailedTiming& out);

    /// @brief Parses appended metadata configuration reply (Message ID 0x89).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized appended metadata settings.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseAppendedMetadata(ByteView packet, MsgAppendedMetadata& out);

    /// @brief Parses frame index and timestamp telemetry (Message ID 0x8A).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized frame index and timestamp.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseFrameIndex(ByteView packet, MsgFrameIndex& out);
};

} // namespace Sightline
