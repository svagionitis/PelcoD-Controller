#pragma once

/// @file SightlineDisplay.h
/// @brief Sightline SLA Display Module (Video output display controls and layouts).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__display.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgSetDisplayParameters
/// @brief Video output display scaling and rendering options (Message ID 0x16).
struct MsgSetDisplayParameters {
    std::uint8_t displayIndex { 0U };
    std::uint8_t cameraIndex { 0U };
    std::uint16_t xOffset { 0U };
    std::uint16_t yOffset { 0U };
    std::uint16_t displayWidth { 1920U };
    std::uint16_t displayHeight { 1080U };
};

/// @struct MsgVideoDisplay
/// @brief Multi-channel display aspect ratio, rotation, and mirror routing (Message ID 0xA4).
struct MsgVideoDisplay {
    std::uint8_t displayIndex { 0U };
    std::uint8_t cameraIndex { 0U };
    std::uint8_t aspectRatio { 0U }; ///< 0: Stretch, 1: 4:3, 2: 16:9, 3: 1:1
    std::uint8_t rotation { 0U }; ///< 0: 0 deg, 1: 90 deg, 2: 180 deg, 3: 270 deg
    std::uint8_t mirror { 0U }; ///< 0: Off, 1: Horiz, 2: Vert, 3: Both
};

/// @struct MsgMultiDisplay
/// @brief Split-screen PiP, side-by-side, and tiled layout routing (Message ID 0xA5).
struct MsgMultiDisplay {
    std::uint8_t displayIndex { 0U };
    std::uint8_t layout { 0U }; ///< 0: Single, 1: PiP, 2: Side-by-Side, 3: Quad, 4: Blended
    std::uint8_t pipCameraIndex { 1U }; ///< Secondary/PiP camera feed index
    std::uint16_t pipX { 0U }; ///< PiP window upper-left X
    std::uint16_t pipY { 0U }; ///< PiP window upper-left Y
    std::uint16_t pipWidth { 320U }; ///< PiP window width
    std::uint16_t pipHeight { 240U }; ///< PiP window height
};

} // namespace Sightline
