#pragma once

/// @file CapturingTransport.h
/// @brief Test double that retains every transport callback it was ever given.

#include "ITransport.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace PelcoD::Test {

/// @class CapturingTransport
/// @brief ITransport double that keeps stale callbacks reachable after they are replaced or cleared.
/// @details Real transports copy the registered callback under a lock and invoke the copy outside the
///          lock, so a callback may run after `setDataCallback(nullptr)` returns. This double makes that
///          window deterministic: tests can grab the callback registered by a session and invoke it
///          after `PelcoDDevice::stop()` (or a restart) to verify that stale invocations are rejected.
/// @note Thread-safe: all members are guarded by an internal mutex or are atomic.
class CapturingTransport final : public ITransport {
public:
    /// @brief Marks the transport open.
    /// @return Always true.
    [[nodiscard]] bool open() override
    {
        m_open.store(true);
        return true;
    }

    /// @brief Marks the transport closed. Does not invoke any callback.
    void close() override
    {
        m_open.store(false);
    }

    /// @brief Reports whether open() was called more recently than close().
    /// @return True if open.
    [[nodiscard]] bool isOpen() const noexcept override
    {
        return m_open.load();
    }

    /// @brief Accepts and discards outbound bytes.
    /// @param[in] data Ignored payload.
    /// @return True while the transport is open.
    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data) override
    {
        static_cast<void>(data);
        return m_open.load();
    }

    /// @brief Records the callback; non-empty callbacks are kept as the latest data callback.
    /// @param[in] callback Data callback (may be empty to "clear").
    void setDataCallback(DataReceivedCallback callback) override
    {
        std::scoped_lock lock { m_mutex };
        if (callback) {
            m_lastData = std::move(callback);
        }
    }

    /// @brief Records the callback; non-empty callbacks are kept as the latest state callback.
    /// @param[in] callback State callback (may be empty to "clear").
    void setStateCallback(StateChangedCallback callback) override
    {
        std::scoped_lock lock { m_mutex };
        if (callback) {
            m_lastState = std::move(callback);
        }
    }

    /// @brief Returns the most recent non-empty data callback, even if it was later cleared.
    /// @return Copy of the retained data callback.
    [[nodiscard]] DataReceivedCallback lastDataCb() const
    {
        std::scoped_lock lock { m_mutex };
        return m_lastData;
    }

    /// @brief Returns the most recent non-empty state callback, even if it was later cleared.
    /// @return Copy of the retained state callback.
    [[nodiscard]] StateChangedCallback lastStateCb() const
    {
        std::scoped_lock lock { m_mutex };
        return m_lastState;
    }

private:
    mutable std::mutex m_mutex {};
    std::atomic<bool> m_open { false };
    DataReceivedCallback m_lastData {};
    StateChangedCallback m_lastState {};
};

} // namespace PelcoD::Test
