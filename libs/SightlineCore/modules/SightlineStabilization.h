#pragma once

/// @file SightlineStabilization.h
/// @brief Sightline SLA Stabilization Module (Electronic image stabilization and registration).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__stabilize.html
/// @see https://knowledge.sightlineintelligence.com/wp-content/uploads/EAN-Stabilization.pdf

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @enum StabilizationPreset
/// @brief Preconfigured stabilization profiles defined in EAN-Stabilization.
enum class StabilizationPreset : std::uint8_t {
    AirborneGimbal = 0U, ///< Section 2.2.1: Airborne camera gimbal
    FixedMountPtz = 1U, ///< Section 2.2.2: Fixed or ground mounted PTZ
    MovingVehicle = 2U ///< Section 2.2.3: Moving vehicle mounted camera
};

/// @struct MsgSetStabilizationParameters
/// @brief Electronic video image stabilization control (Message ID 0x02) and telemetry (0x41).
/// @details Conforms to official Sightline SLACurrentStabilizationParameters_t / SLASetStabilizationParameters_t.
struct MsgSetStabilizationParameters {
    std::uint8_t mode { 1U }; ///< Bit0: On/Off, Bit1: Disable all, Bits2&4: Edge mode, Bit5: Disable zoom-to-track
                              ///< pan/tilt, Bit6: Disable registration
    std::uint8_t rate { 50U }; ///< Stabilization recentering rate 0..255 (default: 50, 5% per frame)
    std::uint8_t translationLimit { 0U }; ///< Maximum display grey edge limit in pixels (0: no limit)
    std::uint8_t angleLimit { 0U }; ///< Maximum rotational stabilization limit in degrees (0: none/fastest, 5: typical)
    std::uint8_t cameraIndex { 0U }; ///< Camera channel index (0..3, or 255 for all)
    std::uint8_t maxStabOff { 0U }; ///< Maximum stabilization offset in pixels (0: no limit)
    std::uint8_t edgeY { 0x10U }; ///< Edge border fill Y luma
    std::uint8_t edgeU { 0x80U }; ///< Edge border fill U chroma
    std::uint8_t edgeV { 0x80U }; ///< Edge border fill V chroma
};

/// @struct MsgResetStabilizationParameters
/// @brief Reset internal stabilization motion smoothing filters (Message ID 0x04).
/// @details Conforms to official Sightline SLAResetStabilizationParameters_t struct layout.
struct MsgResetStabilizationParameters {
    std::uint8_t resetType { 0U }; ///< 0: Reset all filters (default), 1: Display filter only, 2: Auto bias only
    std::uint8_t cameraIndex { 0U }; ///< Camera index (0..3, or 255 for all)
};

/// @struct MsgSetStabilizationBias
/// @brief Motion bias correction values for stabilization (Message ID 0x9F).
/// @details Conforms to official Sightline SLAStabilizationBias_t struct layout.
struct MsgSetStabilizationBias {
    std::uint8_t cameraIndex { 0U }; ///< Camera index (0..3, or 255 for all)
    std::int16_t biasCol { 0 }; ///< Per-frame column adjustment (bias) in pixels
    std::int16_t biasRow { 0 }; ///< Per-frame row adjustment (bias) in pixels
    std::uint8_t autoBias { 1U }; ///< 1: Enable auto bias (combined auto + manual), 0: Manual bias only
    std::uint8_t updateRate { 50U }; ///< Auto bias update rate 0..255 (default 50)
    std::int16_t biasRotation { 0 }; ///< Deprecated legacy alias for backward compatibility
};

/// @struct MsgSetRegistrationParameters
/// @brief Frame-to-frame image registration parameters (Message ID 0x9E).
/// @details Conforms to official Sightline SLARegistrationParameters_t struct layout.
struct MsgSetRegistrationParameters {
    std::uint8_t cameraIndex { 0U }; ///< Camera index (0..3)
    std::uint16_t maxTranslation { 0U }; ///< Maximum translation pixels/frame (0: default 1/4 image height)
    std::uint8_t maxRotation { 5U }; ///< Maximum rotation range in degrees/frame: 0..10 (default: 5)
    std::uint8_t zoomRange { 0U }; ///< Maximum zoom range in percent zoom/frame: 0..10 (default: 0)
    std::uint16_t left { 0U }; ///< Left band of edge pixels to ignore
    std::uint16_t right { 0U }; ///< Right band of edge pixels to ignore
    std::uint16_t top { 0U }; ///< Top band of edge pixels to ignore
    std::uint16_t bottom { 0U }; ///< Bottom band of edge pixels to ignore
    std::uint8_t updateRate { 100U }; ///< Model update rate 0..100% (100: moving camera, 10: ground staring)
    std::uint8_t flags { 0U }; ///< Bit 1: 0 = Software registration, 2 = Passed-in registration
    std::uint8_t searchRange { 32U }; ///< Deprecated legacy alias for backward compatibility
    std::uint8_t pyramidLevels { 3U }; ///< Deprecated legacy alias for backward compatibility
};

} // namespace Sightline
