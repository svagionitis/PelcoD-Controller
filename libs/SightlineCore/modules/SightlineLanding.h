#pragma once

/// @file SightlineLanding.h
/// @brief Sightline SLA Landing Aid Module (Visual landing aid detection and tracking).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__landing.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgLandingAid
/// @brief Controls autonomous landing aid search, target acquisition, and tracking (Message ID 0x81).
struct MsgLandingAid {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t mode { 1U }; // 0: Disable, 1: Acquire & Track, 2: Calibrate
    std::uint8_t patternType { 0U }; // 0: Standard Fiducial, 1: High Contrast Ring
};

/// @struct MsgLandingPosition
/// @brief Telemetry containing landing target relative position and orientation (Message ID 0x83).
struct MsgLandingPosition {
    std::uint8_t cameraIndex { 0U };
    double relativeX { 0.0 };
    double relativeY { 0.0 };
    double relativeZ { 0.0 };
    double yawDeg { 0.0 };
    double pitchDeg { 0.0 };
    double rollDeg { 0.0 };
    std::uint8_t confidence { 0U };
};

} // namespace Sightline
