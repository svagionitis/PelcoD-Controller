/// @file Connection.cpp
/// @brief Implementation of Connection class methods.

#include "Connection.h"

namespace PelcoD {

Connection::Connection(std::function<void()> disconnectFn)
{
    if (disconnectFn) {
        m_state = std::make_shared<SharedState>(std::move(disconnectFn));
    }
}

void Connection::disconnect() noexcept
{
    if (!m_state) {
        return;
    }
    std::function<void()> fn {};
    {
        std::scoped_lock lock { m_state->mutex };
        if (!m_state->connected.load(std::memory_order_relaxed)) {
            return;
        }
        m_state->connected.store(false, std::memory_order_release);
        fn = std::move(m_state->disconnectFn);
    }
    if (fn) {
        try {
            fn();
        } catch (...) {
            // MISRA / CERT: prevent exceptions escaping cleanup routines
        }
    }
}

bool Connection::isConnected() const noexcept
{
    return m_state && m_state->connected.load(std::memory_order_acquire);
}

} // namespace PelcoD
