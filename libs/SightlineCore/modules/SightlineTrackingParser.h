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
};

} // namespace Sightline
