#pragma once

/// @file SightlineLandingParser.h
/// @brief Parser deserializing raw Sightline SLA landing aid telemetry packets.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineLandingParser
/// @brief Deserializes visual landing aid commands and relative position telemetry.
class SightlineLandingParser {
public:
    /// @brief Parses landing aid configuration parameters (Message ID 0x81).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized landing aid structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseLandingAid(ByteView packet, MsgLandingAid& out);

    /// @brief Parses landing target relative coordinates and orientation (Message ID 0x83).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized landing position structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseLandingPosition(ByteView packet, MsgLandingPosition& out);
};

} // namespace Sightline
