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
};

} // namespace Sightline
