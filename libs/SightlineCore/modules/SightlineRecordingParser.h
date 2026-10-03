#pragma once

/// @file SightlineRecordingParser.h
/// @brief Deserializer for Sightline recording responses (IDD Recording module).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__record.html

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineRecordingParser
/// @brief Parses SD card recording status and parameters.
class SightlineRecordingParser {
public:
    /// @brief Parses SD recording parameters message (Message ID 0x1E).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized recording parameters structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseSDRecording(
        const std::vector<std::uint8_t>& packet, MsgSetSDRecordingParameters& out);

    /// @brief Parses snapshot status and path reply (Message ID 0x5D / 0x5F).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized snapshot structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseSnapShot(
        const std::vector<std::uint8_t>& packet, MsgCurrentSnapShot& out);

    /// @brief Parses command acknowledgment message (Message ID 0xC3).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized CommandAck structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseCmdAck(
        const std::vector<std::uint8_t>& packet, MsgCommandAck& out);

    /// @brief Parses hardened video recording parameters (Message ID 0xC4).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized recording parameters structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseSetFileRecordingV2(
        const std::vector<std::uint8_t>& packet, MsgSetFileRecordingParamsV2& out);

    /// @brief Parses hardened snapshot request (Message ID 0xC5).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized snapshot request structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseDoSnapShotV2(
        const std::vector<std::uint8_t>& packet, MsgDoSnapShotV2& out);

    /// @brief Parses push-based file recording event notification (Message ID 0xC6).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized event structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseRecordingEvent(
        const std::vector<std::uint8_t>& packet, MsgFileRecordingEvent& out);

    /// @brief Parses comprehensive recording health telemetry (Message ID 0xC7).
    /// @param[in] packet Raw packet buffer.
    /// @param[out] out Deserialized telemetry structure.
    /// @return True if parsing succeeded.
    [[nodiscard]] static bool parseRecordingStatusV2(
        const std::vector<std::uint8_t>& packet, MsgCurrentRecordingStatusV2& out);
};

} // namespace Sightline
