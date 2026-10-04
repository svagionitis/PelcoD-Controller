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

/// @enum AutoGainMode
/// @brief Sensor auto-gain and high-bit-depth dynamic range compression modes (Message ID 0x70).
enum class AutoGainMode : std::uint8_t {
    HighBitDepthAuto = 0U, ///< 10 to 16-bit high bit depth automatic gain control
    Manual = 1U,           ///< Manual gain: agHoldmax and agHoldmin used for clamping
    SightLineAuto = 2U,    ///< SLA AGC adjusts sensor registers (4000/17xx)
    CameraAuto = 3U,       ///< Camera internal AGC controls gain/exposure (4000/17xx)
    Unknown = 255U
};

/// @struct MsgDigitalCameraParameters
/// @brief Digital camera high-bit-depth auto gain and dynamic range parameters (Message ID 0x70 / 0x71).
/// @details Official Sightline SLASetDigitalCameraParameters_t / SLACurrentDigitalCameraParameters_t.
///          Dynamically adjusts a 10 to 16 bit digital camera input to an 8 bit image.
struct MsgDigitalCameraParameters {
    std::uint8_t cameraIndex { 0U };         ///< Target camera index (0-based)
    AutoGainMode mode { AutoGainMode::HighBitDepthAuto }; ///< Gain mode (0: HighBitDepth, 1: Manual, 2: SightLine, 3: Camera)
    std::uint16_t agHoldmax { 65535U };      ///< Autogain max value mapped to 255 in 8-bit output
    std::uint16_t agHoldmin { 0U };          ///< Autogain min value mapped to 0 in 8-bit output
    std::uint8_t rowROIPct { 0U };           ///< Row offset in % of image height (255 = 100%, default 0)
    std::uint8_t colROIPct { 0U };           ///< Col offset in % of image width (255 = 100%, default 0)
    std::uint8_t highROIPct { 255U };        ///< Height in % of image height (255 = 100%, default 255)
    std::uint8_t wideROIPct { 255U };        ///< Width in % of image width (255 = 100%, default 255)
    std::uint16_t minAGRange { 200U };       ///< Min spread to prevent over-gaining flat scenes (default 200)
    std::uint8_t agRate8 { 32U };            ///< Smoothing filter time constant (1..255, default 32 ~= 1 sec)
    std::uint8_t minExp { 0U };              ///< Minimum exposure / integration time (0 = ignored)
    std::uint8_t maxExp { 0U };              ///< Maximum exposure / integration time (0 = ignored)
    std::uint8_t rejectDarkTail { 0U };      ///< Left/dark tail exclusion (0 = none, 10 = 1.0%, 255 = 25.5%)
    std::uint8_t rejectBrightTail { 0U };    ///< Right/bright tail exclusion (0 = none, 10 = 1.0%, 255 = 25.5%)
    std::uint8_t midpoint { 128U };          ///< Center output value correspondence (0..255, default 128)
    std::uint32_t highBitDepthFlags { 0U };  ///< Bit 0: Contrast stretching from saturation %
};

using MsgSetDigitalCameraParameters = MsgDigitalCameraParameters;

} // namespace Sightline
