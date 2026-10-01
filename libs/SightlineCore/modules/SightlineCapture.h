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

/// @struct MsgCameraSwitch
/// @brief Rapid switching of active camera input channels (Message ID 0x82).
struct MsgCameraSwitch {
    std::uint8_t cameraIndex { 0U }; ///< Target camera index (0..3)
    std::uint8_t switchType { 0U }; ///< 0: Instant, 1: Smooth dissolve
    std::uint8_t flags { 0U }; ///< Reserved / options
};

/// @struct MsgAdvancedCaptureParameters
/// @brief MIPI CSI-2 / SDI deserializer register parameters (Message ID 0x7B).
struct MsgAdvancedCaptureParameters {
    std::uint8_t cameraIndex { 0U }; ///< Camera channel index
    std::uint8_t bitDepth { 8U }; ///< Bit depth (8, 10, 12, 14, 16)
    std::uint8_t laneCount { 2U }; ///< MIPI CSI data lanes (1, 2, 4)
    std::uint32_t pixelClockHz { 0U }; ///< Pixel clock frequency in Hz
    std::uint8_t syncFlags { 0U }; ///< HSync / VSync active polarities
};

/// @struct MsgDigitalVideoParserParameters
/// @brief Hardware framing decoder parameters for digital video inputs (Message ID 0x91).
struct MsgDigitalVideoParserParameters {
    std::uint8_t cameraIndex { 0U }; ///< Camera channel index
    std::uint8_t videoStandard { 1U }; ///< 1: BT.656, 2: BT.1120, 3: SMPTE-296M, 4: SMPTE-274M
    std::uint8_t embeddedSync { 1U }; ///< 0: Discrete sync lines, 1: Embedded SAV/EAV words
    std::uint8_t clockEdge { 0U }; ///< 0: Rising edge, 1: Falling edge
    std::uint8_t flags { 0U }; ///< Additional configuration flags
};

/// @struct MsgCameraCapabilities
/// @brief Reports sensor capabilities, maximum resolution, and zoom limits (Message ID 0xBB).
struct MsgCameraCapabilities {
    std::uint8_t cameraIndex { 0U };
    std::uint16_t maxWidth { 1920U };
    std::uint16_t maxHeight { 1080U };
    std::uint8_t maxFrameRate { 60U };
    std::uint8_t supportsZoom { 1U };
    std::uint8_t flags { 0U };
};

} // namespace Sightline
