#pragma once

/// @file DeviceStatus.h
/// @brief Telemetry and operational state representation for a Pelco-D device.

#include "PelcoDTypes.h"

#include <chrono>
#include <cstdint>

namespace PelcoD {

/// @struct DeviceStatus
/// @brief Snapshot of live device status, angles, alarms, and configurations.
struct DeviceStatus {
    bool connected { false };
    std::uint8_t address { 1U };

    // Pan & Tilt positions in centidegrees (0.01 deg, range 0 - 35999)
    std::uint16_t panCentidegrees { 0U };
    std::uint16_t tiltCentidegrees { 0U };

    // Zoom & Focus positions
    std::uint16_t zoomPosition { 0U };
    std::uint16_t focusPosition { 0U };
    std::uint16_t irisPosition { 0U };

    // Active speed settings (0 - 3)
    std::uint8_t zoomSpeed { 0U };
    std::uint8_t focusSpeed { 0U };

    // Alarms byte bitmask (Bit 0 = Alarm 1 ... Bit 7 = Alarm 8)
    std::uint8_t alarms { 0x00U };

    // Optical and camera states
    AutoMode autoFocus { AutoMode::Off };
    AutoMode autoIris { AutoMode::Off };
    AutoMode agc { AutoMode::Off };
    SwitchState backlightComp { SwitchState::Off };
    SwitchState autoWhiteBalance { SwitchState::Off };

    // Active preset tracking (0 if none active)
    std::uint8_t activePreset { 0U };

    // Magnification (raw device value from 0x63 response)
    std::uint16_t magnification { 0U };

    // ACK/NAK tracking (from 0x01 Standard Extended Response)
    bool lastAckOk { false };
    std::uint8_t lastAckOpcode { 0x00U };

    // Diagnostic telemetry (from 0x71 response)
    std::uint8_t diagnosticTemp { 0U };
    std::uint8_t diagnosticSensorId { 0U };

    // Timestamp of last received message
    std::chrono::steady_clock::time_point lastRxTime {};

    /// @brief Gets pan angle in floating point degrees (0.00 to 359.99).
    /// @return Pan angle in degrees.
    [[nodiscard]] double panDegrees() const noexcept
    {
        return static_cast<double>(panCentidegrees) / 100.0;
    }

    /// @brief Gets tilt angle in floating point degrees (0.00 to 359.99).
    /// @return Tilt angle in degrees.
    [[nodiscard]] double tiltDegrees() const noexcept
    {
        return static_cast<double>(tiltCentidegrees) / 100.0;
    }

    /// @brief Checks if a specific alarm bit is active (1 to 8).
    /// @param[in] alarmIndex Alarm number from 1 to 8.
    /// @return True if alarm flag is asserted.
    [[nodiscard]] bool isAlarmActive(std::uint8_t alarmIndex) const noexcept
    {
        if (alarmIndex < 1U || alarmIndex > 8U) {
            return false;
        }
        const std::uint8_t mask = static_cast<std::uint8_t>(1U << (alarmIndex - 1U));
        return (alarms & mask) != 0U;
    }
};

} // namespace PelcoD
