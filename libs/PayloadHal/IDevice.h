#pragma once

/// @file IDevice.h
/// @brief Base polymorphic interface for all physical and simulated payload devices.

#include "PayloadTypes.h"

#include <functional>
#include <string>

namespace PayloadHal {

/// @class IDevice
/// @brief Fundamental interface implemented by every payload subsystem, establishing
///        lifecycle, health monitoring, connection management, and device identification.
class IDevice {
public:
    virtual ~IDevice() = default;

    /// @brief Opens communications and initializes the underlying hardware.
    /// @return True if connection was established and initialization succeeded.
    virtual bool connect() = 0;

    /// @brief Gracefully terminates communications and transitions hardware to safe standby.
    virtual void disconnect() = 0;

    /// @brief Checks whether communication with the physical device is active.
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;

    /// @brief Retrieves the current operational state of the device.
    [[nodiscard]] virtual DeviceState state() const noexcept = 0;

    /// @brief Retrieves hardware identification, manufacturer, and firmware metadata.
    [[nodiscard]] virtual DeviceInfo info() const noexcept = 0;

    /// @brief Callback signature for device state transitions.
    using StateCallback = std::function<void(DeviceState newState, const std::string& reason)>;

    /// @brief Registers an observer callback for asynchronous state and health changes.
    /// @param[in] cb Callable invoked upon state transition.
    virtual void registerStateCallback(StateCallback cb) = 0;
};

} // namespace PayloadHal
