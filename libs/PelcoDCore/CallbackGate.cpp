/// @file CallbackGate.cpp
/// @brief Implementation of the CallbackGate admission barrier.

#include "CallbackGate.h"

#include <chrono>
#include <thread>

namespace PelcoD {

namespace {

thread_local const CallbackGate::Pass* t_topPass { nullptr };

} // namespace

CallbackGate::Pass::Pass(CallbackGate& gate) noexcept
    : m_gate { gate }
    , m_prevPass { t_topPass }
    , m_entered { gate.enter() }
{
    if (m_entered) {
        t_topPass = this;
    }
}

CallbackGate::Pass::~Pass()
{
    if (m_entered) {
        t_topPass = m_prevPass;
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
    std::uint32_t ownFrames { 0U };
    for (const Pass* p { t_topPass }; p != nullptr; p = p->m_prevPass) {
        if ((&p->m_gate == this) && p->m_entered) {
            ++ownFrames;
        }
    }

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
