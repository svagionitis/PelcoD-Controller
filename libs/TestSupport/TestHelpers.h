#pragma once

/// @file TestHelpers.h
/// @brief Shared test harness utilities, abort suppression, and mock transports for unit testing.

#include "BaseTransport.h"
#include "ITransport.h"
#include "MockTransport.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <crtdbg.h>
#include <cstdlib>
#endif

namespace PelcoDTest {

/// @brief Configures runtime abort behavior to write to STDERR rather than opening modal GUI error dialogs.
/// @details Essential for headless CI/CD execution on Windows with MSVC.
inline void initTestHarness()
{
#if defined(_MSC_VER)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
}

/// @class ControlledTransport
/// @brief Controllable transport for testing frame transmission, injection, and state changes.
class ControlledTransport final : public Transport::BaseTransport {
public:
    bool open() override
    {
        m_open.store(true);
        return true;
    }

    void close() override
    {
        m_open.store(false);
        stopReadThread();
    }

    [[nodiscard]] bool isOpen() const noexcept override
    {
        return m_open.load();
    }

    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sentFrames.push_back(data);
        return m_open.load();
    }

    void inject(const std::vector<std::uint8_t>& data)
    {
        invokeDataCallback(data);
    }

    [[nodiscard]] bool hasDataCallback() const
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        return static_cast<bool>(m_dataCallback);
    }

    [[nodiscard]] bool hasStateCallback() const
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        return static_cast<bool>(m_stateCallback);
    }

    void triggerState(Transport::TransportState state, const std::string& errorMsg)
    {
        notifyState(state, errorMsg);
    }

    [[nodiscard]] std::vector<std::vector<std::uint8_t>> getSentFrames() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_sentFrames;
    }

private:
    std::atomic<bool> m_open { false };
    mutable std::mutex m_mutex;
    std::vector<std::vector<std::uint8_t>> m_sentFrames;
};

/// @class FailingOpenTransport
/// @brief Mock transport whose open() always fails.
class FailingOpenTransport final : public Transport::BaseTransport {
public:
    bool open() override
    {
        return false;
    }
    void close() override
    {
    }
    [[nodiscard]] bool isOpen() const noexcept override
    {
        return false;
    }
    [[nodiscard]] bool sendData([[maybe_unused]] const std::vector<std::uint8_t>& data) override
    {
        return false;
    }

    [[nodiscard]] bool hasDataCallback() const
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        return static_cast<bool>(m_dataCallback);
    }

    [[nodiscard]] bool hasStateCallback() const
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        return static_cast<bool>(m_stateCallback);
    }
};

/// @class FailingSendTransport
/// @brief Mock transport that opens successfully but fails all sendData() calls.
class FailingSendTransport final : public Transport::BaseTransport {
public:
    bool open() override
    {
        m_open.store(true);
        return true;
    }
    void close() override
    {
        m_open.store(false);
    }
    [[nodiscard]] bool isOpen() const noexcept override
    {
        return m_open.load();
    }
    [[nodiscard]] bool sendData([[maybe_unused]] const std::vector<std::uint8_t>& data) override
    {
        return false;
    }

private:
    std::atomic<bool> m_open { false };
};

} // namespace PelcoDTest
