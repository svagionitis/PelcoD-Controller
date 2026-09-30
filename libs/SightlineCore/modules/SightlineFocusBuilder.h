#pragma once

/// @file SightlineFocusBuilder.h
/// @brief Serializer for Sightline lens auto-focus, motorized zoom, and calibration commands (IDD Focus module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineFocusBuilder
/// @brief Encodes motorized lens commands, focus modes/ROIs, and optical calibration parameters.
class SightlineFocusBuilder {
public:
    /// @brief Encodes motorized lens command (Message ID 0x28 / 0xB2).
    /// @param[in] msg Lens command parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildLensCommand(
        const MsgLensCommand& msg);

    /// @brief Encodes focus mode and ROI (Message ID 0x54 / 0xB3).
    /// @param[in] msg Focus parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildFocusParameters(
        const MsgFocusParameters& msg);

    /// @brief Encodes optical calibration parameters (Message ID 0x57 / 0xB1).
    /// @param[in] msg Lens calibration parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetLensParameters(
        const MsgSetLensParameters& msg);

    /// @brief Encodes query for focus parameters (Message ID 0x28 query 0xB3).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetFocusParameters(
        std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for lens parameters (Message ID 0x28 query 0xB1).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetLensParameters(
        std::uint8_t cameraIndex = 0U);
};

} // namespace Sightline
