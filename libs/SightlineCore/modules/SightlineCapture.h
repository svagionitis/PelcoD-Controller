#pragma once

/// @file SightlineCapture.h
/// @brief Sightline SLA Capture Module (Camera acquisition and ADC controls).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__capture.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgSetVideoParameters
/// @brief Video input standard and capture configuration (Message ID 0x10).
struct MsgSetVideoParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t inputFormat { 0U }; // 0: Auto, 1: 1080p, 2: 720p, 3: NTSC, 4: PAL
    std::uint16_t width { 1920U };
    std::uint16_t height { 1080U };
    std::uint8_t frameRate { 30U };
};

/// @struct MsgSetVideoMode
/// @brief Controls video freeze, zoom, and orientation (Message ID 0x1F).
struct MsgSetVideoMode {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t freeze { 0U };
    std::uint8_t digitalZoom { 100U }; // 100 = 1.0x, 200 = 2.0x
    std::uint8_t mirror { 0U };
    std::uint8_t flip { 0U };
};

} // namespace Sightline
