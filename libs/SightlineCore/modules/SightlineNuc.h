#pragma once

/// @file SightlineNuc.h
/// @brief Sightline SLA NUC Module (Non-Uniformity Correction and Dead Pixel Replacement).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__nuc.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgNucParameters
/// @brief Non-uniformity correction calibration and shutter state (Message ID 0x35).
struct MsgNucParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t nucAction { 0U }; // 0: Query, 1: Trigger 1-point NUC, 2: Trigger 2-point NUC
    std::uint8_t shutterMode { 1U }; // 0: Manual, 1: Auto
};

/// @struct MsgDeadPixel
/// @brief Dead pixel map detection and replacement table (Message ID 0xA8).
struct MsgDeadPixel {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t mode { 1U }; // 0: Disable, 1: Enable replacement, 2: Auto-detect
    std::uint16_t deadPixelCount { 0U };
};

} // namespace Sightline
