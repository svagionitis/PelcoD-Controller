#pragma once

/// @file MotionSafetyGuard.h
/// @brief Monitors PTZ motion lifetime, manages dead-man timeouts, and enforces fail-safe stops.

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace PelcoD {

/// @class MotionSafetyGuard
/// @brief Thread-safe watchdog and state monitor enforcing motion limits and automatic fail-safe stop.
/// @details Operates a monotonic deadline-based background timer. When a motion command arrives, the
///          dead-man timer is armed (if timeout > 0). If no subsequent motion refresh or explicit stop
///          arrives within the timeout window, the guard triggers an emergency stop callback.
class MotionSafetyGuard final {
public:
    using StopTriggerCallback = std::function<void()>;

    /// @brief Constructs a motion safety guard.
    /// @param[in] stopCb Callback invoked when the dead-man timer expires to issue a stop command.
    explicit MotionSafetyGuard(StopTriggerCallback stopCb);

    /// @brief Destructor; terminates the watchdog thread safely.
    ~MotionSafetyGuard();

    MotionSafetyGuard(const MotionSafetyGuard&) = delete;
    MotionSafetyGuard& operator=(const MotionSafetyGuard&) = delete;
    MotionSafetyGuard(MotionSafetyGuard&&) = delete;
    MotionSafetyGuard& operator=(MotionSafetyGuard&&) = delete;

    /// @brief Configures the dead-man watchdog timeout.
    /// @param[in] timeout Duration before motion auto-stops (0ms disables watchdog).
    void setDeadManTimeout(std::chrono::milliseconds timeout) noexcept;

    /// @brief Retrieves the configured dead-man watchdog timeout.
    /// @return Configured duration.
    [[nodiscard]] std::chrono::milliseconds getDeadManTimeout() const noexcept;

    /// @brief Reports whether the watchdog is armed and actively monitoring motion.
    /// @return True if armed.
    [[nodiscard]] bool isArmed() const noexcept;

    /// @brief Reports whether the device is currently in a commanded moving state.
    /// @return True if moving.
    [[nodiscard]] bool isMoving() const noexcept;

    /// @brief Informs the guard that a motion command was issued.
    /// @details Arms or resets the dead-man timeout deadline.
    /// @param[in] frame Raw Pelco-D frame bytes.
    void onMotionCommand(const std::vector<std::uint8_t>& frame);

    /// @brief Informs the guard that an explicit stop command was issued.
    /// @details Disarms the dead-man watchdog and marks the device as stationary.
    void onStopCommand() noexcept;

    /// @brief Disarms the guard and resets motion state on transport disconnect or error.
    void onDisconnect() noexcept;

    /// @brief Shuts down the internal watchdog thread.
    void shutdown() noexcept;

private:
    void watchdogLoop();

    StopTriggerCallback m_stopCb {};
    std::chrono::milliseconds m_timeout { 0 };
    std::chrono::steady_clock::time_point m_deadline {};
    bool m_moving { false };
    bool m_armed { false };
    bool m_stopRequested { false };
    mutable std::mutex m_mutex {};
    std::condition_variable m_cv {};
    std::thread m_worker {};
};

} // namespace PelcoD
