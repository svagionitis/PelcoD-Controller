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
/// @brief Deserializes two-channel video blending parameters (Message ID 0x4D / 0x2F).
class SightlineBlendingParser {
public:
    /// @brief Parses active blending parameters (Message ID 0x4D / 0x2F).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized blend parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseBlendParameters(ByteView packet, MsgSetBlendParameters& out);

    /// @brief Parses 4-point projective homography points (Message ID 0x95).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized four align points structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseFourAlignPoints(ByteView packet, MsgFourAlignPoints& out);

    /// @brief Parses blend alignment offsets and registration mode (Message ID 0xB9).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized blend align structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseBlendAlign(ByteView packet, MsgBlendAlign& out);
};

} // namespace Sightline
