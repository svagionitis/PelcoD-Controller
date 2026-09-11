#pragma once

/// @file MockPelcoDDevice.h
/// @brief In-memory Pelco-D device emulator implementing ITransport for offline simulation.

#include "ITransport.h"
#include "PelcoDTypes.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace PelcoD {

/// @struct PresetPosition
/// @brief Saved coordinates for a preset.
struct PresetPosition {
    std::uint16_t pan { 0U };
    std::uint16_t tilt { 0U };
    std::uint16_t zoom { 0U };
};

/// @struct MockDeviceState
/// @brief Internal state variables of simulated device.
struct MockDeviceState {
    std::uint16_t panCentidegrees { 0U };
    std::uint16_t tiltCentidegrees { 0U };
    std::uint16_t zoomPosition { 1000U };
    std::uint16_t focusPosition { 2000U };
    std::uint16_t irisPosition { 500U };
    std::uint16_t magnification { 100U };
    std::uint8_t diagnosticTemp { 28U };
    std::uint8_t diagnosticSensorId { 0x05U };

    std::uint8_t zoomSpeed { 0U };
    std::uint8_t focusSpeed { 0U };
    std::uint8_t alarms { 0x00U };

    AutoMode autoFocus { AutoMode::Off };
    AutoMode autoIris { AutoMode::Off };
    AutoMode agc { AutoMode::Off };
    SwitchState blc { SwitchState::Off };
    SwitchState awb { SwitchState::Off };

    std::uint8_t swType { 0x05U };
    std::uint8_t hwType { 0x01U };
    std::string modelName { "PELCO-D-SIM" };

    std::map<std::uint8_t, PresetPosition> presets {};
    std::array<bool, 8> auxStates { false, false, false, false, false, false, false, false };
};

/// @class MockPelcoDDevice
/// @brief Simulated Pelco-D device executing the protocol state machine in memory.
class MockPelcoDDevice : public ITransport {
public:
    explicit MockPelcoDDevice(std::uint8_t address = 1U) noexcept;
    ~MockPelcoDDevice() override = default;

    // Non-copyable, non-movable
    MockPelcoDDevice(const MockPelcoDDevice&) = delete;
    MockPelcoDDevice& operator=(const MockPelcoDDevice&) = delete;
    MockPelcoDDevice(MockPelcoDDevice&&) = delete;
    MockPelcoDDevice& operator=(MockPelcoDDevice&&) = delete;

    [[nodiscard]] MockDeviceState getInternalState() const;
    void setInternalState(const MockDeviceState& state);

    // ITransport interface
    [[nodiscard]] bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const noexcept override;
    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data) override;
    void setDataCallback(DataReceivedCallback callback) override;
    void setStateCallback(StateChangedCallback callback) override;

    /// @brief Injects simulated incoming byte stream to controller RX.
    /// @param[in] data Raw byte buffer.
    void injectRxData(const std::vector<std::uint8_t>& data);

private:
    void processFrame(const std::vector<std::uint8_t>& frame);
    void sendGeneralReply(std::uint8_t cmdChecksum);
    void sendExtendedReply(std::uint8_t resp1, std::uint8_t resp2, std::uint8_t d1, std::uint8_t d2);
    void sendQueryReply(std::uint8_t cmdChecksum);

    std::uint8_t m_address { 1U };
    std::atomic<bool> m_open { false };

    mutable std::mutex m_stateMutex;
    MockDeviceState m_state {};

    mutable std::mutex m_callbackMutex;
    DataReceivedCallback m_dataCallback;
    StateChangedCallback m_stateCallback;
};

} // namespace PelcoD
