/// @file CallbackGate.cpp
/// @brief Implementation of the CallbackGate admission barrier.

#include "CallbackGate.h"

#include <chrono>
#include <thread>

namespace PelcoD {

namespace {

/// @brief Innermost gate entered on the current thread and its nesting depth on that thread.
struct ActiveGate {
    const CallbackGate* gate { nullptr };
    std::uint32_t depth { 0U };
};

thread_local ActiveGate t_activeGate {};

} // namespace

CallbackGate::Pass::Pass(CallbackGate& gate) noexcept
    : m_gate { gate }
    , m_prevGate { t_activeGate.gate }
    , m_prevDepth { t_activeGate.depth }
    , m_entered { gate.enter() }
{
    if (m_entered) {
        const std::uint32_t depth { (m_prevGate == &gate) ? (m_prevDepth + 1U) : 1U };
        t_activeGate = ActiveGate { &gate, depth };
    }
}

CallbackGate::Pass::~Pass()
{
    if (m_entered) {
        t_activeGate = ActiveGate { m_prevGate, m_prevDepth };
        m_gate.leave();
    }
}

bool CallbackGate::enter() noexcept
{
    std::uint32_t current { m_state.load(std::memory_order_acquire) };
    do {
        if (((current & kClosedBit) != 0U) || ((current & kCountMask) == kCountMask)) {
            return false;
        }
    } while (!m_state.compare_exchange_weak(current, current + 1U, std::memory_order_acq_rel, std::memory_order_acquire));
    return true;
}

void CallbackGate::leave() noexcept
{
    static_cast<void>(m_state.fetch_sub(1U, std::memory_order_acq_rel));
}

void CallbackGate::close()
{
    static_cast<void>(m_state.fetch_or(kClosedBit, std::memory_order_acq_rel));

    // Frames of this gate already on the calling thread cannot finish until close() returns.
    const std::uint32_t ownFrames { (t_activeGate.gate == this) ? t_activeGate.depth : 0U };
    while ((m_state.load(std::memory_order_acquire) & kCountMask) > ownFrames) {
        std::this_thread::sleep_for(std::chrono::milliseconds { 1 });
    }
}

bool CallbackGate::isClosed() const noexcept
{
    return (m_state.load(std::memory_order_acquire) & kClosedBit) != 0U;
}

std::uint32_t CallbackGate::inFlight() const noexcept
{
    return m_state.load(std::memory_order_acquire) & kCountMask;
}

} // namespace PelcoD
