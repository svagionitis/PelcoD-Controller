#pragma once

/// @file SightlineOverlayBuilder.h
/// @brief Serializer for Sightline overlay modes, reticles, and dynamic graphic objects (IDD Overlays module).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__overlay.html

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineOverlayBuilder
/// @brief Encodes video overlay modes, reticle crosshairs, dynamic graphic objects, and batch overlays.
class SightlineOverlayBuilder {
public:
    /// @brief Encodes overlay display mode configuration (Message ID 0x06).
    /// @param[in] msg Overlay mode settings.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetOverlayMode(
        const MsgSetOverlayMode& msg);

    /// @brief Encodes single custom graphic object command (Message ID 0x3B).
    /// @param[in] msg Graphic object parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDrawObject(
        const MsgDrawObject& msg);

    /// @brief Encodes multiple graphic primitives update (Message ID 0x9C).
    /// @param[in] msg Draw overlay parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDrawOverlay(
        const MsgDrawOverlay& msg);

    /// @brief Encodes query for active overlay mode (Message ID 0x07).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetOverlayMode(
        std::uint8_t cameraIndex = 0U);
};

} // namespace Sightline
