#pragma once

/// @file SightlineEnhancement.h
/// @brief Sightline SLA Enhancement Module (Contrast, CLAHE, 3D Noise reduction).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__enhance.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgSetVideoEnhancement
/// @brief Contrast, brightness, sharpening, and CLAHE (Message ID 0x21).
struct MsgSetVideoEnhancement {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t contrast { 50U };
    std::uint8_t brightness { 50U };
    std::uint8_t sharpening { 0U };
    std::uint8_t claheEnable { 0U };
};

/// @struct MsgNoise3D
/// @brief 3D Spatio-temporal noise reduction filter settings (Message ID 0xAF).
struct MsgNoise3D {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t enable { 1U };
    std::uint8_t temporalStrength { 50U };
    std::uint8_t spatialStrength { 30U };
};

} // namespace Sightline
