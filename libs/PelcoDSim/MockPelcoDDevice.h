#pragma once

/// @file MockPelcoDDevice.h
/// @brief In-memory Pelco-D device emulator implementing ITransport for offline simulation.

#include "ITransport.h"
#include "KinematicsSimulator.h"
#include "LatencyPipeline.h"
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

    std::uint32_t baudRate { 9600U };
    bool filterByBaudRate { false };
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

    /// @brief Update PTZ kinematics simulation parameters.
    /// @param[in] config Kinematics settings.
    void setKinematicsConfig(const KinematicsConfig& config);

    /// @brief Retrieve current PTZ kinematics simulation parameters.
    /// @return Active KinematicsConfig.
    [[nodiscard]] KinematicsConfig getKinematicsConfig() const;

    /// @brief Update link delay, jitter, and packet loss parameters.
    /// @param[in] config Latency settings.
    void setLatencyConfig(const LatencyConfig& config);

    /// @brief Retrieve current latency configuration.
    /// @return Active LatencyConfig.
    [[nodiscard]] LatencyConfig getLatencyConfig() const;

    /// @brief Check if camera head or lens is actively slewing or moving.
    /// @return true if non-zero velocity or slewing in progress.
    [[nodiscard]] bool isMoving() const;

    // ITransport interface
    [[nodiscard]] bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const noexcept override;
    [[nodiscard]] bool sendData(const std::vector<std::uint8_t>& data) override;
    void setDataCallback(DataReceivedCallback callback) override;
    void setStateCallback(StateChangedCallback callback) override;

    bool setBaudRate(std::uint32_t baudRate) override;
    [[nodiscard]] std::uint32_t getBaudRate() const noexcept override;

    /// @brief Injects simulated incoming byte stream to controller RX.
    /// @param[in] data Raw byte buffer.
    void injectRxData(const std::vector<std::uint8_t>& data);

private:
    void processFrame(const std::vector<std::uint8_t>& frame);
    void dispatchResponse(std::vector<std::uint8_t> response);
    void sendGeneralReply(std::uint8_t cmdChecksum);
    void sendExtendedReply(std::uint8_t resp1, std::uint8_t resp2, std::uint8_t d1, std::uint8_t d2);
    void sendQueryReply(std::uint8_t cmdChecksum);

    std::uint8_t m_address { 1U };
    std::atomic<bool> m_open { false };
    std::atomic<std::uint32_t> m_currentBaudRate { 9600U };

    mutable std::mutex m_stateMutex;
    mutable MockDeviceState m_state {};

    mutable std::mutex m_callbackMutex;
    DataReceivedCallback m_dataCallback;
    StateChangedCallback m_stateCallback;

    mutable KinematicsSimulator m_kinematics;
    LatencyPipeline m_latency;
};

} // namespace PelcoD
