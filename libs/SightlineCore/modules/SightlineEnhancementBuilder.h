#pragma once

/// @file SightlineEnhancementBuilder.h
/// @brief Serializer for Sightline video contrast, brightness, and CLAHE enhancements (IDD Enhancement module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineEnhancementBuilder
/// @brief Encodes contrast, brightness, sharpening, and CLAHE adaptive histogram equalization (Message ID 0x21).
class SightlineEnhancementBuilder {
public:
    /// @brief Encodes contrast/brightness/CLAHE enhancement (Message ID 0x21).
    /// @param[in] msg Enhancement parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVideoEnhance(
        const MsgSetVideoEnhancement& msg);

    /// @brief Encodes complete SLA video enhancement parameters (Message ID 0x21).
    /// @param[in] msg Full enhancement parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVideoEnhanceFull(
        const MsgSetVideoEnhancementFull& msg);

    /// @brief Encodes false color palette selection (Message ID 0x16).
    /// @param[in] cameraIndex Target camera index (0-based, or 255 for all).
    /// @param[in] palette Predefined false color palette mode.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetFalseColor(
        std::uint8_t cameraIndex, FalseColorPalette palette);

    /// @brief Encodes query for active video enhancement parameters (Message ID 0x22).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVideoEnhance(
        std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for 3D noise statistics (Message ID 0x28 query 0xAF).
    /// @details 0xAF is read-only. Trigger the calculation first with MsgNucParameters
    ///          (NucRunMode::Noise3DStats), then query; the reply is a MsgNoise3D.
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetNoise3D(
        std::uint8_t cameraIndex = 0U);
};

} // namespace Sightline
