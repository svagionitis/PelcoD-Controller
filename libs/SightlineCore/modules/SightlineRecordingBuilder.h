#pragma once

/// @file SightlineRecordingBuilder.h
/// @brief Serializer for Sightline SD card video recording and snapshots (IDD Recording module).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__record.html

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineRecordingBuilder
/// @brief Encodes onboard SD card video recording, snapshot, and file prefix commands.
class SightlineRecordingBuilder {
public:
    /// @brief Encodes onboard SD card recording start/stop/snapshot (Message ID 0x1E).
    /// @param[in] msg SD recording parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetSDRecording(
        const MsgSetSDRecordingParameters& msg);

    /// @brief Encodes query for active snapshot state (Message ID 0x5F).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetSnapShot(
        std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active SD recording parameters (Message ID 0x28 query 0x1E).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetSDRecording();
};

} // namespace Sightline
