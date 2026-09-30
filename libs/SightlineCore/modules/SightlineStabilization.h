#pragma once

/// @file SightlineStabilization.h
/// @brief Sightline SLA Stabilization Module (Electronic image stabilization and registration).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__stabilize.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgSetStabilizationParameters
/// @brief Electronic video image stabilization control (Message ID 0x02) and telemetry (0x41).
/// @details Conforms to official Sightline SLACurrentStabilizationParameters_t / SLASetStabilizationParameters_t.
struct MsgSetStabilizationParameters {
    std::uint8_t mode { 1U }; ///< 0: Off, 1: On, 2: Auto
    std::uint8_t rate { 0U }; ///< Stabilization update rate
    std::uint8_t translationLimit { 0U }; ///< Max translation limit
    std::uint8_t angleLimit { 0U }; ///< Max rotation angle limit
    std::uint8_t cameraIndex { 0U }; ///< Camera channel index
    std::uint8_t maxStabOff { 0U }; ///< Maximum stabilization offset
    std::uint8_t autoBias { 1U }; ///< Auto-bias enable
    std::uint8_t maxShift { 64U }; ///< Maximum pixel shift limit
    std::uint8_t flags { 0U }; ///< Control / algorithm flags
};

/// @struct MsgResetStabilizationParameters
/// @brief Reset internal stabilization motion smoothing filters (Message ID 0x04).
/// @details Conforms to official Sightline SLAResetStabilizationParameters_t struct layout.
struct MsgResetStabilizationParameters {
    std::uint8_t resetType { 0U }; // 0: Reset all (default), 1: Display filter, 2: Auto bias
    std::uint8_t cameraIndex { 0U };
};

/// @struct MsgSetStabilizationBias
/// @brief Motion bias correction values for stabilization (Message ID 0x12).
struct MsgSetStabilizationBias {
    std::uint8_t cameraIndex { 0U };
    std::int16_t biasCol { 0 };
    std::int16_t biasRow { 0 };
    std::int16_t biasRotation { 0 };
};

/// @struct MsgSetRegistrationParameters
/// @brief Frame-to-frame image registration parameters (Message ID 0x0E).
struct MsgSetRegistrationParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t searchRange { 32U };
    std::uint8_t pyramidLevels { 3U };
    std::uint8_t flags { 0U };
};

} // namespace Sightline
