#pragma once

/// @file MockTransport.h
/// @brief Google Mock implementation of the Pelco-D streaming transport interface.

#include "BaseTransport.h"
#include "ITransport.h"

#if __has_include(<gmock/gmock.h>)
#include <gmock/gmock.h>

#include <cstdint>
#include <string>
#include <vector>

namespace PelcoDTest {

/// @class MockTransport
/// @brief Google Mock transport implementation inheriting BaseTransport.
/// @details Enables precise mock expectations (EXPECT_CALL) on frame transmission, channel lifecycle,
///          and deterministic injection of simulated incoming bytes or connection state changes.
class MockTransport : public PelcoD::BaseTransport {
public:
    MockTransport() = default;
    ~MockTransport() override = default;

    // Non-copyable, non-movable per BaseTransport semantics
    MockTransport(const MockTransport&) = delete;
    MockTransport& operator=(const MockTransport&) = delete;
    MockTransport(MockTransport&&) = delete;
    MockTransport& operator=(MockTransport&&) = delete;

    MOCK_METHOD(bool, open, (), (override));
    MOCK_METHOD(void, close, (), (override));
    MOCK_METHOD(bool, isOpen, (), (const, noexcept, override));
    MOCK_METHOD(bool, sendData, (const std::vector<std::uint8_t>& data), (override));
    MOCK_METHOD(bool, setBaudRate, (std::uint32_t baudRate), (override));
    MOCK_METHOD(std::uint32_t, getBaudRate, (), (const, noexcept, override));

    /// @brief Simulates incoming byte traffic from the remote device.
    /// @param[in] data Received raw byte buffer to dispatch to registered callback.
    void simulateIncomingData(const std::vector<std::uint8_t>& data)
    {
        invokeDataCallback(data);
    }

    /// @brief Simulates a transport connection state transition.
    /// @param[in] state Target transport state (e.g. Connected, Disconnected, Error).
    /// @param[in] errorMsg Diagnostic message associated with the state change.
    void simulateStateChange(PelcoD::TransportState state, const std::string& errorMsg = {})
    {
        notifyState(state, errorMsg);
    }
};

/// @class MockITransport
/// @brief Pure virtual Google Mock implementation of the ITransport interface.
class MockITransport : public PelcoD::ITransport {
public:
    MockITransport() = default;
    ~MockITransport() override = default;

    MockITransport(const MockITransport&) = delete;
    MockITransport& operator=(const MockITransport&) = delete;
    MockITransport(MockITransport&&) = delete;
    MockITransport& operator=(MockITransport&&) = delete;

    MOCK_METHOD(bool, open, (), (override));
    MOCK_METHOD(void, close, (), (override));
    MOCK_METHOD(bool, isOpen, (), (const, noexcept, override));
    MOCK_METHOD(bool, sendData, (const std::vector<std::uint8_t>& data), (override));
    MOCK_METHOD(void, setDataCallback, (DataReceivedCallback callback), (override));
    MOCK_METHOD(void, setStateCallback, (StateChangedCallback callback), (override));
    MOCK_METHOD(bool, setBaudRate, (std::uint32_t baudRate), (override));
    MOCK_METHOD(std::uint32_t, getBaudRate, (), (const, noexcept, override));
};

} // namespace PelcoDTest

#endif // __has_include(<gmock/gmock.h>)
