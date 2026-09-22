#pragma once

#include <cstdint>
#include <string_view>

namespace Visca::Sony {

/// @brief Sony FCB camera exposure modes.
enum class SonyExposureMode : uint8_t {
    FullAuto = 0x00, ///< Full Automatic exposure
    Manual = 0x03, ///< Full Manual exposure (shutter, iris, gain)
    ShutterPriority = 0x0A, ///< Shutter Priority AE
    IrisPriority = 0x0B, ///< Iris Priority AE
    Bright = 0x0D ///< Bright mode (combined iris + gain control)
};

/// @brief Sony FCB white balance modes.
enum class SonyWhiteBalanceMode : uint8_t {
    Auto = 0x00, ///< Auto White Balance
    Indoor = 0x01, ///< Indoor preset (3200K)
    Outdoor = 0x02, ///< Outdoor preset (5800K)
    OnePush = 0x03, ///< One-push trigger WB
    ATW = 0x04, ///< Auto Tracing White Balance
    Manual = 0x05, ///< Manual R/B Gain
    SodiumIndoor = 0x06, ///< Sodium lamp indoor
    SodiumOutdoor = 0x07, ///< Sodium lamp outdoor
    SodiumAuto = 0x08 ///< Sodium lamp auto
};

/// @brief Image stabilizer modes supported by FCB cameras.
enum class SonyStabilizerMode : uint8_t {
    Off = 0x00, ///< Stabilizer disabled
    Normal = 0x01, ///< Normal optical/electronic stabilizer
    Super = 0x02, ///< Super stabilization mode
    SuperPlus = 0x03 ///< Super+ enhanced vibration suppression
};

/// @brief Wide Dynamic Range / Visibility Enhancer modes.
enum class SonyWideDMode : uint8_t {
    Off = 0x00, ///< Disabled
    WideD = 0x01, ///< Wide-D enabled
    VisibilityEnhancer = 0x02 ///< Visibility Enhancer (VE) enabled
};

/// @brief Defog processing intensity levels.
enum class SonyDefogMode : uint8_t { Off = 0x00, Low = 0x01, Mid = 0x02, High = 0x03 };

/// @brief Sony FCB internal register addresses.
namespace Register {
    inline constexpr uint8_t kBaudRate { 0x00 }; ///< Serial Baud Rate (9600..115200)
    inline constexpr uint8_t kOpticalAxisGap { 0x47 }; ///< Optical Axis Gap Compensation (EV9520L)
    inline constexpr uint8_t kDistortionComp { 0x57 }; ///< Distortion Compensation (EV9520L)
    inline constexpr uint8_t kDigitalOutput { 0x60 }; ///< Digital Output Mode / TMDS / HDMI (EW9500H)
    inline constexpr uint8_t kOperatingMode { 0x72 }; ///< Video output resolution & frame rate (EW9500H)
    inline constexpr uint8_t kLvdsMode { 0x74 }; ///< LVDS Serial Mode Single/Double (EV9520L)
    inline constexpr uint8_t kMonitorMode { 0x7E }; ///< Monitor output mode
} // namespace Register

/// @struct SonyFCBStatus
/// @brief Aggregated real-time camera state decoded from Block Inquiries 00 through 05.
struct SonyFCBStatus {
    // --- Lens Control (Block 00) ---
    uint16_t zoomPosition { 0x0000 }; ///< Raw zoom position (0x0000 wide, 0x4000 optical tele, 0x7000 dzoom)
    uint16_t focusPosition { 0x1000 }; ///< Raw focus position (0x1000 near to 0xF000 far)
    uint16_t focusNearLimit { 0x1000 }; ///< Minimum near limit distance code
    bool focusAuto { true }; ///< True: Auto Focus, False: Manual Focus
    bool digitalZoomOn { false }; ///< Digital zoom enabled
    bool digitalZoomCombine { true }; ///< True: Combine mode, False: Separate mode
    bool lowContrastDetected { false }; ///< Low contrast warning flag
    bool zoomCommandExecuting { false }; ///< Zoom motor actively moving
    bool focusCommandExecuting { false }; ///< Focus motor actively moving

    // --- Camera Control (Block 01) ---
    SonyExposureMode exposureMode { SonyExposureMode::FullAuto }; ///< Exposure control mode
    uint8_t shutterPosition { 0 }; ///< Shutter step index
    uint8_t irisPosition { 0 }; ///< Iris step index
    uint8_t gainPosition { 0 }; ///< Gain step index (dB)
    SonyWhiteBalanceMode wbMode { SonyWhiteBalanceMode::Auto }; ///< White balance mode
    uint16_t rGain { 0 }; ///< Red channel manual gain
    uint16_t bGain { 0 }; ///< Blue channel manual gain
    uint8_t apertureGain { 0 }; ///< Aperture / sharpness level (0x0..0xF)
    bool slowShutterOn { false }; ///< Auto slow shutter enabled
    bool exposureCompOn { false }; ///< Exposure compensation enabled
    uint8_t exposureCompPosition { 0 }; ///< Exposure compensation level (0..0xE)
    bool backlightOn { false }; ///< Backlight compensation enabled
    bool spotAeOn { false }; ///< Spot AE coordinate window active
    bool spotFocusOn { false }; ///< Spot Focus coordinate window active
    bool spotAwbOn { false }; ///< Spot AWB coordinate window active
    bool veOn { false }; ///< Visibility Enhancer active

    // --- Other Inquiry (Block 02) ---
    bool powerOn { true }; ///< Camera power status
    bool icrColor { true }; ///< ICR optical mode (true = Color, false = BW)
    bool icrOn { false }; ///< IR cut filter retracted (Night mode)
    bool autoIcrOn { false }; ///< Auto ICR mode active
    bool stabilizerOn { false }; ///< Image stabilizer active
    SonyStabilizerMode stabilizerLevel { SonyStabilizerMode::Normal }; ///< Stabilizer strength
    bool freezeOn { false }; ///< Video frame freeze active
    bool lrReverseOn { false }; ///< Left/Right image inversion (mirror)
    uint32_t cameraId { 0 }; ///< Unique Camera ID
    bool system50Hz { false }; ///< System frequency (true: 50Hz/25fps, false: 60Hz/30fps)

    // --- Extended Functions 1-3 (Blocks 03..05) ---
    uint16_t digitalZoomPosition { 0 }; ///< Digital zoom magnification position
    uint8_t gamma { 0 }; ///< Gamma curve selection
    uint8_t nrLevel { 0 }; ///< 2D/3D Noise reduction level
    uint8_t colorGain { 0 }; ///< Color saturation gain
    uint8_t chromaSuppress { 0 }; ///< Low light chroma suppression
    bool defogOn { false }; ///< Defog mode active
    SonyDefogMode defogLevel { SonyDefogMode::Off }; ///< Defog processing intensity
    SonyWideDMode wideDMode { SonyWideDMode::Off }; ///< Dynamic range mode
    uint8_t veCompensationLevel { 0 }; ///< VE compensation level (0: Low, 1: Mid, 2: High)
    uint8_t veBrightnessCompensation { 0 }; ///< VE brightness compensation selection
};

} // namespace Visca::Sony
