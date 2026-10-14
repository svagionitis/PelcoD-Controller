#pragma once

/// @file MotionSafetyGuard.h
/// @brief Multi-axis motion safety monitor, dead-man watchdog, and fail-safe enforcement layer.

#include "MotionSafetyPolicy.h"
#include "PelcoDTypes.h"
#include "TaskScheduler.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace PelcoD {

class PelcoDSession;

/// @enum MotionSafetyEvent
/// @brief Events emitted when the safety guard intervenes in motion execution.
enum class MotionSafetyEvent : std::uint8_t { DeadManTimeout, ContinuousMotionLimit, DisconnectStop, ShutdownStop };

/// @class MotionSafetyGuard
/// @brief Mediates command dispatch between PTZ facades and PelcoDSession to enforce functional safety.
/// @details Tracks active physical motion axes, coordinates dead-man watchdog timeouts via TaskScheduler,
///          enforces continuous runaway motion limits, and ensures fail-safe stops upon disconnect/shutdown.
class MotionSafetyGuard final : public std::enable_shared_from_this<MotionSafetyGuard> {
public:
    using StopTriggerCallback = std::function<void()>;
    using SafetyEventCallback = std::function<void(MotionSafetyEvent event, MotionAxis axes)>;

    /// @brief Primary constructor attaching the guard to a session, scheduler, and policy.
    /// @param[in] session Target communication session for command transmission and queue management.
    /// @param[in] scheduler Task scheduler for cooperative deadline timers (zero dedicated threads).
    /// @param[in] policy Initial safety configuration policy.
    explicit MotionSafetyGuard(std::shared_ptr<PelcoDSession> session,
        std::shared_ptr<TaskScheduler> scheduler = nullptr, MotionSafetyPolicy policy = {});

    /// @brief Backward-compatible constructor for standalone callbacks.
    /// @param[in] stopCb Callback invoked when the dead-man timer expires to issue a stop command.
    /// @param[in] scheduler Optional shared task scheduler.
    explicit MotionSafetyGuard(StopTriggerCallback stopCb, std::shared_ptr<TaskScheduler> scheduler = nullptr);

    /// @brief Destructor terminating watchdog timers and issuing shutdown stop if configured.
    ~MotionSafetyGuard();

    MotionSafetyGuard(const MotionSafetyGuard&) = delete;
    MotionSafetyGuard& operator=(const MotionSafetyGuard&) = delete;
    MotionSafetyGuard(MotionSafetyGuard&&) = delete;
    MotionSafetyGuard& operator=(MotionSafetyGuard&&) = delete;

    /// @brief Updates the motion safety policy.
    /// @param[in] policy New configuration policy.
    void setPolicy(const MotionSafetyPolicy& policy) noexcept;

    /// @brief Retrieves the active safety policy snapshot.
    /// @return Active policy.
    [[nodiscard]] MotionSafetyPolicy getPolicy() const noexcept;

    /// @brief Configures the dead-man watchdog timeout (convenience helper).
    /// @param[in] timeout Duration before motion auto-stops (0ms disables watchdog).
    void setDeadManTimeout(std::chrono::milliseconds timeout) noexcept;

    /// @brief Retrieves the configured dead-man watchdog timeout.
    /// @return Configured duration.
    [[nodiscard]] std::chrono::milliseconds getDeadManTimeout() const noexcept;

    /// @brief Registers a callback to receive motion safety intervention notifications.
    /// @param[in] cb Notification callback.
    void setSafetyEventCallback(SafetyEventCallback cb) noexcept;

    /// @brief Reports whether the watchdog is armed and actively monitoring motion.
    /// @return True if armed.
    [[nodiscard]] bool isArmed() const noexcept;

    /// @brief Reports whether any motion axis is currently active.
    /// @return True if one or more axes are in motion.
    [[nodiscard]] bool isMoving() const noexcept;

    /// @brief Reports whether a specific motion axis is currently active.
    /// @param[in] axis Axis to inspect.
    /// @return True if the specified axis is in motion.
    [[nodiscard]] bool isAxisMoving(MotionAxis axis) const noexcept;

    /// @brief Reports the active bitmask of all moving axes.
    /// @return Bitmask of active axes.
    [[nodiscard]] MotionAxis getActiveAxes() const noexcept;

    /// @brief Reports whether the safety interlock is active, blocking motion.
    /// @return True if interlocked due to runaway continuous motion limit.
    [[nodiscard]] bool isInterlocked() const noexcept;

    /// @brief Resets the safety interlock, allowing motion to resume.
    void resetInterlock() noexcept;

    /// @brief Intermediates and enqueues a standard frame, updating safety state only on success.
    /// @param[in] frame Standard 7-byte protocol array.
    /// @param[in] queryKind Associated query kind if applicable.
    /// @param[in] priority Command queue priority.
    /// @return SendResult from session enqueue.
    [[nodiscard]] SendResult dispatch(const StandardFrame& frame, QueryKind queryKind = QueryKind::None,
        CommandPriority priority = CommandPriority::Normal);

    /// @brief Intermediates and enqueues a standard frame with query tag.
    /// @param[in] frame Standard 7-byte protocol array.
    /// @param[in] queryTag Associated correlation string tag.
    /// @param[in] priority Command queue priority.
    /// @return SendResult from session enqueue.
    [[nodiscard]] SendResult dispatch(
        const StandardFrame& frame, const std::string& queryTag, CommandPriority priority = CommandPriority::Normal);

    /// @brief Intermediates and enqueues a raw frame vector, updating safety state only on success.
    /// @param[in] frame Raw frame bytes.
    /// @param[in] queryKind Associated query kind if applicable.
    /// @param[in] priority Command queue priority.
    /// @return SendResult from session enqueue.
    [[nodiscard]] SendResult dispatch(const std::vector<std::uint8_t>& frame, QueryKind queryKind = QueryKind::None,
        CommandPriority priority = CommandPriority::Normal);

    /// @brief Intermediates and enqueues a raw frame vector with query tag.
    /// @param[in] frame Raw frame bytes.
    /// @param[in] queryTag Correlation string tag.
    /// @param[in] priority Command queue priority.
    /// @return SendResult from session enqueue.
    [[nodiscard]] SendResult dispatch(const std::vector<std::uint8_t>& frame, const std::string& queryTag,
        CommandPriority priority = CommandPriority::Normal);

    /// @brief Triggers an immediate fail-safe emergency stop across all active axes.
    /// @param[in] reason Cause for the fail-safe stop.
    /// @return SendResult of the injected stop command.
    [[nodiscard]] SendResult triggerEmergencyStop(MotionSafetyEvent reason) noexcept;

    /// @brief Legacy hook: informs guard that a standard motion command was issued.
    /// @param[in] frame Standard 7-byte protocol array.
    void onMotionCommand(const StandardFrame& frame);

    /// @brief Legacy hook: informs guard that a raw motion command was issued.
    /// @param[in] frame Raw Pelco-D frame bytes.
    void onMotionCommand(const std::vector<std::uint8_t>& frame);

    /// @brief Informs guard that an explicit stop command was issued.
    void onStopCommand() noexcept;

    /// @brief Handles transport disconnection events, purging motion queue and emitting fail-safe stop.
    void handleDisconnect() noexcept;

    /// @brief Disarms the guard and resets motion state on transport disconnect or error (forwards to
    /// handleDisconnect).
    void onDisconnect() noexcept;

    /// @brief Shuts down watchdog timers and issues final fail-safe stop if configured.
    void shutdown() noexcept;

private:
    void rescheduleDeadManLocked();
    void cancelDeadManLocked();
    void rescheduleRunawayLocked();
    void cancelRunawayLocked();
    void updateMotionStateLocked(const MotionAxis axes, bool isStop);
    void watchdogLoop();
    void onDeadManTimeout();
    void onRunawayTimeout();

    std::weak_ptr<PelcoDSession> m_session {};
    StopTriggerCallback m_stopCb {};
    std::shared_ptr<TaskScheduler> m_scheduler {};
    MotionSafetyPolicy m_policy {};
    SafetyEventCallback m_eventCb {};

    TaskId m_scheduledTaskId { 0U };
    TaskId m_runawayTaskId { 0U };
    std::chrono::steady_clock::time_point m_deadline {};
    std::chrono::steady_clock::time_point m_continuousStart {};
    MotionAxis m_activeAxes { MotionAxis::None };
    bool m_moving { false };
    bool m_armed { false };
    bool m_stopRequested { false };
    bool m_isShuttingDown { false };
    bool m_interlocked { false };

    mutable std::mutex m_mutex {};
    std::condition_variable m_cv {};
    std::thread m_worker {};
};

} // namespace PelcoD
