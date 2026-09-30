#pragma once

/// @file SightlineDetection.h
/// @brief Sightline SLA Detection Module (Automatic object detection, MTI, marine).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__detect.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgSetDetectionParameters
/// @brief Moving Target Indication (MTI) sensitivity and thresholds (Message ID 0x2D).
struct MsgSetDetectionParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t mode { 0U }; // 0: Off, 1: MTI, 2: Vehicle, 3: Person, 4: Maritime
    std::uint8_t threshold { 20U };
    std::uint16_t minTargetSize { 4U };
    std::uint16_t maxTargetSize { 200U };
};

} // namespace Sightline
