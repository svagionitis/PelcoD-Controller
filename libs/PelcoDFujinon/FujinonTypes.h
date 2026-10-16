#pragma once

/// @file FujinonTypes.h
/// @brief Type definitions and enumerations for Fujinon SX800 / SX801 camera extensions.

#include "DeviceStatus.h"

#include <cstdint>
#include <string>

namespace PelcoD {

/// @enum FujinonOISMode
/// @brief Optical and Electronic Image Stabilization modes for Fujinon SX800.
enum class FujinonOISMode : std::uint8_t {
    Auto = 0x01U, ///< Automatic stabilization switching
    OisOn = 0x02U, ///< Optical Image Stabilization enabled only
    EisOn = 0x03U, ///< Electronic Image Stabilization enabled only
    Off = 0x04U ///< Stabilization disabled
};

/// @enum FujinonDayNightMode
/// @brief Day/Night IR cut filter switching modes.
enum class FujinonDayNightMode : std::uint8_t {
    Auto = 0x01U, ///< Automatic day/night switching based on luminance
    Day = 0x02U, ///< Day mode (IR cut filter engaged, color)
    Night = 0x03U, ///< Night mode (IR cut filter retracted, B&W / IR sensitive)
    External = 0x04U ///< Switched via external trigger port
};

/// @enum FujinonIRWavelength
/// @brief Infrared wavelength tuning for night surveillance.
enum class FujinonIRWavelength : std::uint8_t {
    Visible = 0x00U, ///< Visible light band
    W950nm = 0x01U, ///< 950 nm bandpass
    W940nm = 0x02U, ///< 940 nm bandpass
    W850nm = 0x03U, ///< 850 nm bandpass
    W808nm = 0x04U ///< 808 nm bandpass
};

/// @enum FujinonDefogLevel
/// @brief Optical and digital defogging intensity.
enum class FujinonDefogLevel : std::uint8_t { Off = 0x00U, Level1 = 0x01U, Level2 = 0x02U, Level3 = 0x03U };

/// @enum FujinonHeatHazeLevel
/// @brief De-heat haze image stabilization level.
enum class FujinonHeatHazeLevel : std::uint8_t { Off = 0x00U, Level1 = 0x01U, Level2 = 0x02U };

/// @enum FujinonWDRLevel
/// @brief Wide Dynamic Range (WDR) processing level.
enum class FujinonWDRLevel : std::uint8_t { Off = 0x00U, Level1 = 0x01U, Level2 = 0x02U, Level3 = 0x03U };

/// @enum FujinonWBMode
/// @brief White balance mode presets.
enum class FujinonWBMode : std::uint8_t {
    Auto = 0x01U,
    Custom1 = 0x02U,
    Custom2 = 0x03U,
    Day = 0x04U,
    Cloud = 0x05U,
    ColorTemp = 0x06U
};

/// @enum FujinonColorTemp
/// @brief Manual color temperature presets when WBMode is ColorTemp.
enum class FujinonColorTemp : std::uint8_t {
    K3000 = 0x01U, ///< 3000 K
    K5000 = 0x02U, ///< 5000 K (default)
    K9000 = 0x03U ///< 9000 K
};

/// @enum FujinonDigitalZoom
/// @brief Digital zoom magnification factor.
enum class FujinonDigitalZoom : std::uint8_t { Off = 0x00U, X1_25 = 0x01U, X1_5 = 0x02U, X1_75 = 0x03U, X2 = 0x04U };

/// @enum FujinonOSDPosition
/// @brief 4-corner screen positioning for on-screen display text elements.
enum class FujinonOSDPosition : std::uint8_t {
    TopLeft = 0x01U,
    BottomLeft = 0x02U,
    TopRight = 0x03U,
    BottomRight = 0x04U
};

/// @enum FujinonVideoMode
/// @brief Base analog/SDI video broadcast standard.
enum class FujinonVideoMode : std::uint8_t { NTSC = 0x00U, PAL = 0x01U };

/// @enum FujinonHDFormat
/// @brief Full HD video framerate and scanning format.
enum class FujinonHDFormat : std::uint8_t { F1080p60 = 0x00U, F1080p50 = 0x01U, F1080p30 = 0x02U, F1080p25 = 0x03U };

/// @enum FujinonMenuDirection
/// @brief On-screen display (OSD) navigation cursor direction.
enum class FujinonMenuDirection : std::uint8_t { Up = 0x01U, Down = 0x02U, Left = 0x03U, Right = 0x04U };

/// @enum FujinonDigitalZoomMode
/// @brief Digital zoom operation mode (v2.12.0).
enum class FujinonDigitalZoomMode : std::uint8_t {
    Off = 0x00U, ///< Digital zoom disabled
    DigitalZoom = 0x01U, ///< Magnification up to 1.25x..2x
    CropMode = 0x02U ///< Crop mode (maintains resolution via sensor crop)
};

/// @enum FujinonDayNightModeEx
/// @brief Extended Day/Night switching mode (v2.12.0).
enum class FujinonDayNightModeEx : std::uint8_t {
    Auto = 0x00U, ///< Auto switching
    AutoAndScheduled = 0x01U, ///< Auto & Scheduled switching
    Scheduled = 0x02U, ///< Scheduled switching based on start times
    Day = 0x03U, ///< Day mode fixed
    Night = 0x04U ///< Night mode fixed
};

/// @enum FujinonLanguage
/// @brief On-screen display language selection (v2.12.0).
enum class FujinonLanguage : std::uint8_t { English = 0x00U, French = 0x01U, Japanese = 0x03U };

/// @enum FujinonAutoFocusMode
/// @brief Auto Focus operating mode.
enum class FujinonAutoFocusMode : std::uint8_t { AFOn = 0x00U, AFOff = 0x01U, QuickAF = 0x02U };

/// @enum FujinonAutoIrisMode
/// @brief Auto Iris operating mode.
enum class FujinonAutoIrisMode : std::uint8_t { Manual = 0x00U, Auto = 0x01U };

/// @enum FujinonAGCMode
/// @brief Automatic Gain Control mode.
enum class FujinonAGCMode : std::uint8_t { Standard = 0x00U, Off = 0x01U, Hyper = 0x02U };

/// @enum FujinonBLCMode
/// @brief Backlight Compensation mode.
enum class FujinonBLCMode : std::uint8_t { Off = 0x00U, Soft = 0x01U, Hard = 0x02U };

/// @enum FujinonShutterLimitAuto
/// @brief Lower shutter limit for automatic exposure.
enum class FujinonShutterLimitAuto : std::uint8_t {
    Manual = 0x00U,
    S1_8 = 0x01U,
    S1_15 = 0x02U,
    S1_30 = 0x03U,
    S1_60 = 0x04U,
    S1_125 = 0x05U,
    S1_250 = 0x06U,
    S1_500 = 0x07U,
    S1_1000 = 0x08U,
    S1_2000 = 0x09U,
    S1_4000 = 0x0AU,
    S1_10000 = 0x0BU
};

/// @enum FujinonPhotoModePreset
/// @brief Camera photo scene mode preset.
enum class FujinonPhotoModePreset : std::uint8_t { Surveillance = 0x01U, Movie = 0x02U };

/// @enum FujinonExternalDayNight
/// @brief External contact trigger Day/Night control.
enum class FujinonExternalDayNight : std::uint8_t { OnVLC = 0x01U, OnClear = 0x02U, Off = 0x03U };

/// @enum FujinonTimeFormat
/// @brief OSD time display format mode.
enum class FujinonTimeFormat : std::uint8_t { Mode24H = 0x01U, Mode12H = 0x02U };

/// @enum FujinonDateFormat
/// @brief OSD calendar date display format.
enum class FujinonDateFormat : std::uint8_t {
    YMD_Hyphen = 0x01U,
    MDY_Hyphen = 0x02U,
    DMY_Hyphen = 0x03U,
    YMD_Slash = 0x04U,
    MDY_Slash = 0x05U,
    DMY_Slash = 0x06U
};

/// @enum FujinonVideoDisplayMode
/// @brief Video aspect framing display mode.
enum class FujinonVideoDisplayMode : std::uint8_t { SideCut = 0x01U, LetterBox = 0x02U, Squeeze = 0x03U };

/// @enum FujinonSDRecordMode
/// @brief SD card recording behavior when storage is full.
enum class FujinonSDRecordMode : std::uint8_t { Overwrite = 0x01U, Stop = 0x02U };

/// @enum FujinonVideoEncoding
/// @brief SD card video recording codec.
enum class FujinonVideoEncoding : std::uint8_t { H264 = 0x01U, MJPEG = 0x02U, MPEG4 = 0x03U };

/// @enum FujinonMoviePlayMode
/// @brief SD card recorded video playback state.
enum class FujinonMoviePlayMode : std::uint8_t { Stop = 0x00U, Step = 0x01U, Continuous = 0x02U };

/// @enum FujinonLogType
/// @brief Internal diagnostic log category.
enum class FujinonLogType : std::uint8_t { Zoom = 0x01U, Focus = 0x02U, PowerOnTime = 0x03U, OIS = 0x04U };

/// @struct FujinonPhotoSettings
/// @brief Snapshot decoded from 18-byte Query Photo Setting response.
struct FujinonPhotoSettings {
    std::uint8_t afArea { 0U };
    std::uint8_t afSensitivity { 0U };
    FujinonDayNightMode dayNightMode { FujinonDayNightMode::Auto };
    FujinonIRWavelength irWavelength { FujinonIRWavelength::Visible };
    FujinonOISMode oisMode { FujinonOISMode::Auto };
};

/// @struct FujinonImageQualitySettings
/// @brief Snapshot decoded from 18-byte Query Image Quality response.
struct FujinonImageQualitySettings {
    bool vlcFilter { false };
    FujinonWDRLevel wdr { FujinonWDRLevel::Off };
    FujinonHeatHazeLevel heatHaze { FujinonHeatHazeLevel::Off };
    FujinonDefogLevel defog { FujinonDefogLevel::Off };
    std::uint8_t brightness { 11U }; ///< 1 (darkest) to 21 (brightest), default 11
    std::uint8_t contrast { 3U }; ///< 1 (lowest) to 5 (highest), default 3
    std::uint8_t saturation { 3U }; ///< 1 (lowest) to 5 (highest), default 3
    std::uint8_t sharpness { 4U }; ///< 1 (softest) to 5 (hardest), default 4
    FujinonDigitalZoom digitalZoom { FujinonDigitalZoom::Off };
    std::uint8_t noiseReduction { 3U };
};

/// @struct FujinonFineImageSettings
/// @brief Fine image quality adjustments (1..100, default 50) introduced in v2.12.0.
struct FujinonFineImageSettings {
    std::uint8_t brightness { 50U }; ///< 1 (darkest) to 100 (brightest), default 50
    std::uint8_t contrast { 50U }; ///< 1 (lowest) to 100 (highest), default 50
    std::uint8_t saturation { 50U }; ///< 1 (lowest) to 100 (highest), default 50
    std::uint8_t sharpness { 50U }; ///< 1 (softest) to 100 (hardest), default 50
    std::uint8_t wbShiftRed { 50U }; ///< 1 to 100, default 50
    std::uint8_t wbShiftBlue { 50U }; ///< 1 to 100, default 50
};

/// @struct FujinonDayNightExSettings
/// @brief Extended Day/Night schedule, threshold, and optical filter parameters (v2.12.0).
struct FujinonDayNightExSettings {
    FujinonDayNightModeEx mode { FujinonDayNightModeEx::Auto };
    std::uint8_t dayToNightThreshold { 0U }; ///< 0..255
    std::uint8_t nightToDayThreshold { 0U }; ///< 0..255
    std::uint8_t autoDelaySeconds { 0U }; ///< Delay in seconds (0..60)
    std::uint8_t dayStartHour { 0U }; ///< 0..23
    std::uint8_t dayStartMinute { 0U }; ///< 0..59
    std::uint8_t nightStartHour { 0U }; ///< 0..23
    std::uint8_t nightStartMinute { 0U }; ///< 0..59
    bool opticalFilterDayIRPass { false }; ///< false = IR cut (Day), true = IR pass
    bool opticalFilterNightIRPass { true }; ///< false = IR cut, true = IR pass (Night)
};

/// @struct FujinonZoomFocusExSettings
/// @brief Extended Zoom and Focus speeds and digital zoom mode (v2.12.0).
struct FujinonZoomFocusExSettings {
    std::uint8_t zoomSpeedEx { 1U }; ///< Speed step 1..8 (1:4s, 2:6s, 3:8s, 4:10s, 5:15s, 6:20s, 7:30s, 8:60s)
    std::uint8_t focusSpeedEx { 1U }; ///< Speed step 1..5
    FujinonDigitalZoomMode digitalZoomMode { FujinonDigitalZoomMode::Off };
};

/// @struct FujinonManualSettings
/// @brief Exposure and sensor manual overrides.
struct FujinonManualSettings {
    std::uint8_t manualIris { 0U };
    std::uint8_t manualShutter { 0U };
    std::uint8_t manualISO { 0U };
};

/// @struct FujinonStatus
/// @brief Complete operational status of Fujinon SX800 camera including standard Pelco-D telemetry.
struct FujinonStatus {
    DeviceStatus baseStatus {};

    // Stabilization & Enhancements
    FujinonOISMode oisMode { FujinonOISMode::Auto };
    FujinonDayNightMode dayNightMode { FujinonDayNightMode::Auto };
    FujinonIRWavelength irWavelength { FujinonIRWavelength::Visible };
    FujinonDefogLevel defogLevel { FujinonDefogLevel::Off };
    FujinonHeatHazeLevel heatHazeLevel { FujinonHeatHazeLevel::Off };
    FujinonWDRLevel wdrLevel { FujinonWDRLevel::Off };
    bool vlcFilter { false };

    // Image Parameters
    std::uint8_t brightness { 11U };
    std::uint8_t contrast { 3U };
    std::uint8_t saturation { 3U };
    std::uint8_t sharpness { 4U };
    FujinonWBMode wbMode { FujinonWBMode::Auto };
    FujinonColorTemp colorTemp { FujinonColorTemp::K5000 };
    FujinonDigitalZoom digitalZoom { FujinonDigitalZoom::Off };
    std::uint8_t noiseReduction { 3U };

    // Optics & Sensor
    FujinonAutoFocusMode autoFocusMode { FujinonAutoFocusMode::AFOn };
    std::uint8_t afArea { 0U };
    std::uint8_t afSensitivity { 2U };
    std::uint16_t absoluteFocusPosition { 0U };
    std::uint16_t absoluteZoomPosition { 0U };
    std::uint8_t manualIris { 0U };
    std::uint8_t manualShutter { 0U };
    std::uint8_t manualISO { 0U };

    // OSD Display Settings
    bool timeDisplay { false };
    FujinonOSDPosition timePosition { FujinonOSDPosition::TopLeft };
    bool titleDisplay { false };
    FujinonOSDPosition titlePosition { FujinonOSDPosition::TopLeft };
    bool idDisplay { false };
    FujinonOSDPosition idPosition { FujinonOSDPosition::TopLeft };
    bool reticleDisplay { false };

    // System Telemetry
    std::string serialNumber {};
    std::string firmwareVersion {};
    std::uint8_t lensStatus { 0U };
    bool rs485Termination { false };

    // v2.12.0 Extended Settings
    FujinonFineImageSettings fineImageSettings {};
    FujinonDayNightExSettings dayNightExSettings {};
    FujinonZoomFocusExSettings zoomFocusExSettings {};
    bool antialiasing { false };
    FujinonLanguage language { FujinonLanguage::English };
    double focalLengthMm { 20.0 }; ///< Optical focal length in millimeters (20.0mm wide to 800.0mm tele)
};

} // namespace PelcoD
