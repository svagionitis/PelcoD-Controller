#pragma once

/// @file SightlineEnhancementParser.h
/// @brief Parser deserializing raw Sightline SLA enhancement parameters (IDD Enhancement module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineEnhancementParser
/// @brief Deserializes video contrast, brightness, and CLAHE enhancement parameters.
class SightlineEnhancementParser {
public:
    /// @brief Parses active video enhancement parameters (Message ID 0x4A / 0x21).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized enhancement structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseVideoEnhance(
        const std::vector<std::uint8_t>& packet, MsgSetVideoEnhancement& out);

    /// @brief Parses complete SLA video enhancement parameters (Message ID 0x4A / 0x21).
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized full enhancement structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseVideoEnhanceFull(
        const std::vector<std::uint8_t>& packet, MsgSetVideoEnhancementFull& out);

    /// @brief Parses the 3D noise statistics reply (Message ID 0xAF, IDD SLANoise3D_t).
    /// @details Decodes cameraIndex plus 8 little-endian u16 values scaled by kNoise3DScale.
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized noise statistics.
    /// @return True on successful parse.
    /// @retval false Wrong message ID or payload shorter than 17 bytes.
    [[nodiscard]] static bool parseNoise3D(
        const std::vector<std::uint8_t>& packet, MsgNoise3D& out);
};

} // namespace Sightline
