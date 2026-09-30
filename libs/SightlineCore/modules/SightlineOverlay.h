#pragma once

/// @file SightlineOverlay.h
/// @brief Sightline SLA Overlay Module (Reticles, dynamic graphic primitives, and overlays).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__overlay.html

#include "../SightlineTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Sightline {

/// @struct MsgSetOverlayMode
/// @brief Configures overlay graphics rendering options (Message ID 0x06).
struct MsgSetOverlayMode {
    std::uint8_t displayIndex { 0U };
    std::uint8_t reticleMode { 1U }; // 0: None, 1: Crosshair, 2: Box, 3: Custom
    std::uint8_t trackingBoxMode { 1U }; // 0: Off, 1: Bounding Box, 2: Centroid
    std::uint8_t telemetryTextMode { 1U };
};

/// @struct MsgDrawObject
/// @brief Dynamic custom graphics object rendering on video overlay (Message ID 0x3B).
struct MsgDrawObject {
    std::uint8_t displayIndex { 0U };
    std::uint8_t objectId { 0U };
    std::uint8_t shapeType { 0U }; // 0: Line, 1: Rectangle, 2: Circle, 3: Text
    std::uint16_t x { 0U };
    std::uint16_t y { 0U };
    std::uint16_t width { 0U };
    std::uint16_t height { 0U };
    std::uint32_t colorRgba { 0x00FF00FFU }; // Green opaque default
    std::string text {};
};

/// @struct MsgDrawOverlay
/// @brief Advanced multi-primitive overlay graphics update (Message ID 0x9C).
struct MsgDrawOverlay {
    std::uint8_t displayIndex { 0U };
    std::uint8_t clearDisplay { 0U };
    std::vector<MsgDrawObject> objects {};
};

} // namespace Sightline
