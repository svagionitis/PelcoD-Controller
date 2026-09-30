#pragma once

/// @file SightlineFocus.h
/// @brief Sightline SLA Focus Module (Lens motorized focus, zoom, iris, and optical parameters).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__focus.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgLensCommand
/// @brief Motorized lens positioning and focus commands (Message ID 0xB2).
struct MsgLensCommand {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t commandType { 0U }; // 0: Stop, 1: Focus Far, 2: Focus Near, 3: Zoom In, 4: Zoom Out
    std::int16_t rateOrPosition { 0 };
};

/// @struct MsgFocusParameters
/// @brief Auto-focus and region-of-interest tuning (Message ID 0xB3).
struct MsgFocusParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t focusMode { 0U };
    std::uint16_t roiX { 0U };
    std::uint16_t roiY { 0U };
    std::uint16_t roiWidth { 0U };
    std::uint16_t roiHeight { 0U };
};

/// @struct MsgSetLensParameters
/// @brief Calibrated focal length and optical distortion parameters (Message ID 0x6E / 0xB1).
struct MsgSetLensParameters {
    std::uint8_t cameraIndex { 0U };
    double minFocalLengthMm { 4.3 };
    double maxFocalLengthMm { 129.0 };
    double horizontalFovWideDeg { 65.0 };
    double horizontalFovTeleDeg { 2.3 };
};

} // namespace Sightline
