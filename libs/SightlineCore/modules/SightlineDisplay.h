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

} // namespace Sightline
