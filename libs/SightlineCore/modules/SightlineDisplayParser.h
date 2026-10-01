#pragma once

/// @file SightlineDisplayParser.h
/// @brief Parser deserializing raw Sightline SLA display parameters (IDD Display module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineDisplayParser
/// @brief Deserializes video output display parameters and dimensions.
class SightlineDisplayParser {
public:
    /// @brief Parses active display parameters (Message ID 0x57 / 0x16).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized display parameters structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseDisplayParameters(ByteView packet, MsgSetDisplayParameters& out);

    /// @brief Parses multi-channel video display routing and aspect ratio (Message ID 0xA4).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized video display structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseVideoDisplay(ByteView packet, MsgVideoDisplay& out);

    /// @brief Parses multi-display split screen and PiP window routing (Message ID 0xA5).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized multi-display structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseMultiDisplay(ByteView packet, MsgMultiDisplay& out);
};

} // namespace Sightline
