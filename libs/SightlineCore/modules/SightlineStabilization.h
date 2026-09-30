#pragma once

/// @file SightlineStabilization.h
/// @brief Sightline SLA Stabilization Module (Electronic image stabilization and registration).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__stabilize.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgSetStabilizationParameters
/// @brief Electronic video image stabilization control (Message ID 0x02).
struct MsgSetStabilizationParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t mode { 1U }; // 0: Off, 1: On, 2: Auto
    std::uint8_t autoBias { 1U };
    std::uint8_t maxShift { 64U };
    std::uint8_t flags { 0U };
};

/// @struct MsgResetStabilizationParameters
/// @brief Reset internal stabilization motion smoothing filters (Message ID 0x04).
struct MsgResetStabilizationParameters {
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
