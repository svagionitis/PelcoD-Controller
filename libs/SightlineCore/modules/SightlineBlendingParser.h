#pragma once

/// @file SightlineBlendingParser.h
/// @brief Parser deserializing raw Sightline SLA two-channel video blending telemetry frames.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineBlendingParser
/// @brief Deserializes two-channel video blending parameters (Message ID 0x4D / 0x2F / 0x95 / 0xB9 / 0x75).
class SightlineBlendingParser {
public:
    /// @brief Parses blending command parameters (Message ID 0x2F) or mapped telemetry (0x4D).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized blend parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseBlendParameters(ByteView packet, MsgSetBlendParameters& out);

    /// @brief Parses full active blending telemetry snapshot (Message ID 0x4D).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized current blend parameters (19 bytes).
    /// @return True on successful parse.
    [[nodiscard]] static bool parseCurrentBlendParameters(ByteView packet, MsgCurrentBlendParameters& out);

    /// @brief Parses 4-point projective homography points (Message ID 0x95).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized four align points structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseFourAlignPoints(ByteView packet, MsgFourAlignPoints& out);

    /// @brief Parses blend alignment offsets and scaling (Message ID 0xB9).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized blend align structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseBlendAlign(ByteView packet, MsgBlendAlign& out);

    /// @brief Parses multiple alignment parameters (Message ID 0x74 / 0x75).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized multiple alignment structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseMultipleAlignment(ByteView packet, MsgSetMultipleAlignment& out);
};

} // namespace Sightline
