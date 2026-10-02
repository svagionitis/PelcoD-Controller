#pragma once

/// @file SightlineTrackingParser.h
/// @brief Parser deserializing raw Sightline SLA tracking packets into typed POD structures.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineTrackingParser
/// @brief Deserializes target tracking coordinates, multi-target telemetry, and trails.
class SightlineTrackingParser {
public:
    /// @brief Parses single primary track coordinate and scene motion (Message ID 0x43).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized tracking position structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseTrackingPosition(
        const std::vector<std::uint8_t>& packet, MsgTrackingPosition& out);

    /// @brief Parses multi-target position and velocity telemetry (Message ID 0x51).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized tracking positions structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseTrackingPositions(
        const std::vector<std::uint8_t>& packet, MsgTrackingPositions& out);

    /// @brief Parses multi-target report with AI classifier labels (Message ID 0xA0).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized extended tracking positions structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parsePositionsExtended(
        const std::vector<std::uint8_t>& packet, MsgTrackingPositionsExtended& out);

    /// @brief Parses target history trails (Message ID 0x9D).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized track trails structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseTrackTrails(
        const std::vector<std::uint8_t>& packet, MsgTrackTrails& out);

    /// @brief Parses tracking parameters reply (Message ID 0x44 / 0x0C).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized tracking parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseTrackingParameters(
        const std::vector<std::uint8_t>& packet, MsgSetTrackingParameters& out);

    /// @brief Parses acquisition start command (Message ID 0x08).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized start tracking parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseStartTracking(
        const std::vector<std::uint8_t>& packet, MsgStartTracking& out);

    /// @brief Parses active track modification command (Message ID 0x05).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized modify tracking parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseModifyTracking(
        const std::vector<std::uint8_t>& packet, MsgModifyTracking& out);

    /// @brief Parses track index modification command (Message ID 0x17).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized modify track index parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseModifyTrackIndex(
        const std::vector<std::uint8_t>& packet, MsgModifyTrackIndex& out);

    /// @brief Parses sub-pixel tracking nudge command (Message ID 0x0A).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized nudge tracking parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseNudgeTracking(
        const std::vector<std::uint8_t>& packet, MsgNudgeTrackingCoordinate& out);

    /// @brief Parses track gate shift command (Message ID 0x33).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized shift selected track parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseShiftSelectedTrack(
        const std::vector<std::uint8_t>& packet, MsgShiftSelectedTrack& out);
};

} // namespace Sightline
