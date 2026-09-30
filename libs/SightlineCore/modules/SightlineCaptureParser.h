#pragma once

/// @file SightlineCaptureParser.h
/// @brief Parser deserializing raw Sightline SLA camera acquisition and video mode telemetry frames.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineCaptureParser
/// @brief Deserializes video acquisition parameters and mode configurations.
class SightlineCaptureParser {
public:
    /// @brief Parses active video capture parameters (Message ID 0x46).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized video parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseVideoParameters(
        const std::vector<std::uint8_t>& packet, MsgSetVideoParameters& out);

    /// @brief Parses active video mode parameters (Message ID 0x4B).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized video mode structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseVideoMode(
        const std::vector<std::uint8_t>& packet, MsgSetVideoMode& out);
};

} // namespace Sightline
