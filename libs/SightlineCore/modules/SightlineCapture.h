#pragma once

/// @file SightlineCapture.h
/// @brief Sightline SLA Capture Module (Camera acquisition and ADC controls).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__capture.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgSetVideoParameters
/// @brief Video input chop and deinterlace configuration (Message ID 0x10) and telemetry (0x46).
/// @details Conforms to official Sightline SLASetVideoParameters_t / SLACurrentVideoParameters_t.
struct MsgSetVideoParameters {
    std::uint8_t autoChop { 0U }; ///< 0: Manual chop, 1: Automatically detect boundary pixels to remove
    std::uint8_t chopTop { 0U }; ///< Top pixels to remove (8 to 64)
    std::uint8_t chopBottom { 0U }; ///< Bottom pixels to remove (8 to 64)
    std::uint8_t chopLeft { 0U }; ///< Left pixels to remove (8 to 128)
    std::uint8_t chopRight { 0U }; ///< Right pixels to remove (8 to 128)
    std::uint8_t deinterlace { 1U }; ///< 0: No deinterlacing, 1: Digital deinterlacing
    std::uint8_t autoReset { 1U }; ///< 0: Never reset, 1: Auto reset decoder on frame sync loss
    std::uint8_t cameraIndex { 0U }; ///< Camera channel index
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
