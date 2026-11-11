#pragma once

/// @file ILensDriver.h
/// @brief Abstract zero-heap interface for motorized camera lens actuators.

#include <cstddef>
#include <cstdint>

#include "LensTypes.h"

namespace LensControl {

/// @class ILensDriver
/// @brief Zero-heap polymorphic hardware abstraction for motorized lens actuators.
class ILensDriver {
public:
    virtual ~ILensDriver() = default;

    /// @brief Initialize serial communication port for physical lens.
    /// @param port Physical UART index.
    /// @param baud Baud rate in bps.
    /// @return True on success, false otherwise.
    [[nodiscard]] virtual bool initialize(std::uint8_t port, std::uint32_t baud) noexcept = 0;

    /// @brief Relative focus step displacement.
    /// @param steps Encoder offset (negative = Near, positive = Far).
    /// @param speed Actuator speed.
    virtual void moveFocus(std::int16_t steps, MotorSpeed speed) noexcept = 0;

    /// @brief Relative zoom step displacement.
    /// @param steps Encoder offset (negative = Wide, positive = Tele).
    /// @param speed Actuator speed.
    virtual void moveZoom(std::int16_t steps, MotorSpeed speed) noexcept = 0;

    /// @brief Absolute focus position command.
    /// @param pos Target focus encoder position.
    /// @param speed Actuator speed.
    virtual void setFocusPosition(MotorPosition pos, MotorSpeed speed) noexcept = 0;

    /// @brief Absolute zoom position command.
    /// @param pos Target zoom encoder position.
    /// @param speed Actuator speed.
    virtual void setZoomPosition(MotorPosition pos, MotorSpeed speed) noexcept = 0;

    /// @brief Immediately stop all motorized optical motion.
    virtual void stopMotion() noexcept = 0;

    /// @brief Query active focus position.
    [[nodiscard]] virtual MotorPosition currentFocus() const noexcept = 0;

    /// @brief Query active zoom position.
    [[nodiscard]] virtual MotorPosition currentZoom() const noexcept = 0;

    /// @brief Query bitmask of tripped limit switches.
    [[nodiscard]] virtual std::uint8_t limitFlags() const noexcept = 0;

    /// @brief Send raw command bytes directly to lens actuator.
    virtual void sendRawCommand(const std::uint8_t* data, std::size_t len) noexcept = 0;

    /// @brief Checks if physical lens has native autonomous autofocus.
    [[nodiscard]] virtual bool hasBuiltinAutofocus() const noexcept = 0;

    /// @brief Commands lens to execute one-push hardware autofocus.
    virtual bool triggerBuiltinAutofocus() noexcept = 0;

    /// @brief Checks if physical lens handles focus tracking across zoom.
    [[nodiscard]] virtual bool hasBuiltinZoomTracking() const noexcept = 0;

    /// @brief Configures physical lens internal zoom-focus tracking.
    virtual bool setBuiltinZoomTracking(bool enable) noexcept = 0;

    /// @brief Retrieves physical travel ranges.
    [[nodiscard]] virtual LensRanges lensRanges() const noexcept = 0;
};

} // namespace LensControl
