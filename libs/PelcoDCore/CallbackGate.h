#pragma once

/// @file CallbackGate.h
/// @brief Lock-free admission gate that lets an owner wait for in-flight foreign-thread callbacks.

#include <atomic>
#include <cstdint>

namespace PelcoD {

/// @class CallbackGate
/// @brief Admission gate guarding callbacks that foreign threads (e.g. transports) invoke on an owner.
/// @details Transports copy their registered callback under a lock and invoke the copy outside that
///          lock, so clearing the callback is not a synchronisation barrier: an invocation can still be
///          executing (or about to start) after the owner believes it has unsubscribed. Each callback
///          body enters the gate through a CallbackGate::Pass; the owner calls close() before tearing
///          down state. After close() returns:
///          - every later enter() is rejected, and
///          - no callback that entered earlier is still executing (except the caller's own frames when
///            close() is invoked re-entrantly from inside a guarded callback).
///
///          The admission path (enter/leave) is lock-free and noexcept. close() polls with a 1 ms sleep;
///          it is a rare, owner-side operation, so polling avoids a mutex on the hot RX path.
/// @note Thread-safe. A gate cannot be reopened; owners create a fresh gate per session so that stale
///       callbacks from an earlier session stay rejected after a restart.
/// @warning Destroying the owner from inside one of its own guarded callbacks remains undefined
///          behaviour: close() cannot wait for the frame that is executing the destructor.
class CallbackGate final {
public:
    /// @class Pass
    /// @brief RAII admission token: enters the gate on construction and leaves it on destruction.
    /// @details Also records the gate in a thread-local slot so that a re-entrant close() on the same
    ///          thread does not wait for itself.
    class Pass final {
    public:
        /// @brief Attempts to enter @p gate.
        /// @param[in] gate Gate to enter. Must outlive this Pass.
        explicit Pass(CallbackGate& gate) noexcept;

        /// @brief Leaves the gate if it was entered and restores the previous thread-local slot.
        ~Pass();

        Pass(const Pass&) = delete;
        Pass& operator=(const Pass&) = delete;
        Pass(Pass&&) = delete;
        Pass& operator=(Pass&&) = delete;

        /// @brief Reports whether admission succeeded.
        /// @return True if the guarded callback may proceed; false if the gate was closed.
        [[nodiscard]] explicit operator bool() const noexcept
        {
            return m_entered;
        }

    private:
        friend class CallbackGate;
        CallbackGate& m_gate;
        const Pass* m_prevPass { nullptr };
        bool m_entered { false };
    };

    CallbackGate() noexcept = default;
    ~CallbackGate() = default;

    CallbackGate(const CallbackGate&) = delete;
    CallbackGate& operator=(const CallbackGate&) = delete;
    CallbackGate(CallbackGate&&) = delete;
    CallbackGate& operator=(CallbackGate&&) = delete;

    /// @brief Closes the gate and waits until no admitted callback is still executing.
    /// @details Idempotent. When called from inside one or more Pass frames of this same gate on the
    ///          current thread, waits only for the *other* in-flight callbacks.
    /// @note Blocks; never call while holding a lock that a guarded callback may need.
    void close();

    /// @brief Reports whether close() has been called.
    /// @return True once the gate rejects new admissions.
    [[nodiscard]] bool isClosed() const noexcept;

    /// @brief Number of callbacks currently admitted (diagnostic / test use).
    /// @return In-flight admission count.
    [[nodiscard]] std::uint32_t inFlight() const noexcept;

private:
    /// @brief Attempts to register one admission.
    /// @return True if admitted; false if the gate is closed or the counter is saturated.
    [[nodiscard]] bool enter() noexcept;

    /// @brief Releases one admission previously granted by enter().
    void leave() noexcept;

    static constexpr std::uint32_t kClosedBit { 0x80000000U };
    static constexpr std::uint32_t kCountMask { 0x7FFFFFFFU };

    std::atomic<std::uint32_t> m_state { 0U };
};

} // namespace PelcoD
