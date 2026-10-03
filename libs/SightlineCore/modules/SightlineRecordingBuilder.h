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

    /// @brief Encodes command acknowledgment message (Message ID 0xC3).
    /// @param[in] msg CommandAck structure.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCmdAck(
        const MsgCommandAck& msg);

    /// @brief Encodes hardened video recording parameters with sequence tracking (Message ID 0xC4).
    /// @param[in] msg Hardened recording parameters structure.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetFileRecordingV2(
        const MsgSetFileRecordingParamsV2& msg);

    /// @brief Encodes hardened snapshot capture command (Message ID 0xC5).
    /// @param[in] msg Hardened snapshot request structure.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDoSnapShotV2(
        const MsgDoSnapShotV2& msg);

    /// @brief Encodes push-based file recording event notification (Message ID 0xC6).
    /// @param[in] msg File recording event structure.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildRecordingEvent(
        const MsgFileRecordingEvent& msg);

    /// @brief Encodes comprehensive recording health telemetry (Message ID 0xC7).
    /// @param[in] msg Recording health telemetry structure.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildRecordingStatusV2(
        const MsgCurrentRecordingStatusV2& msg);
};

} // namespace Sightline
