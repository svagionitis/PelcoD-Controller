#pragma once

/// @file SerialLrfAdapter.h
/// @brief Physical hardware driver bridging serial/stream transports to ILaserRangeFinder.

#include "LrfProtocols.h"
#include "PayloadHal/ILaserRangeFinder.h"
#include "Transport/ITransport.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace PayloadHal {

/// @class SerialLrfAdapter
/// @brief Hardware adapter implementing the ILaserRangeFinder interface over any Transport::ITransport,
///        featuring ANSI Z136 eye-safety interlocks, inactivity watchdog auto-disarm, multi-protocol
///        framing (NMEA, ASCII, Binary), and range gating.
class SerialLrfAdapter : public ILaserRangeFinder,
                         public std::enable_shared_from_this<SerialLrfAdapter> {
public:
    /// @brief Constructs adapter wrapping a physical or virtual stream transport.
    /// @param[in] transport Shared pointer to underlying ITransport channel.
    /// @param[in] config Operational, safety, and protocol configuration.
    explicit SerialLrfAdapter(
        std::shared_ptr<Transport::ITransport> transport, SerialLrfConfig config = {});

    ~SerialLrfAdapter() override;

    // --- IDevice Interface Implementation ---
    bool connect() override;
    void disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    [[nodiscard]] DeviceState state() const noexcept override;
    [[nodiscard]] DeviceInfo info() const noexcept override;
    void registerStateCallback(StateCallback cb) override;

    // --- ILaserRangeFinder Interface Implementation ---
    bool armLaser() override;
    bool disarmLaser() override;
    [[nodiscard]] bool isArmed() const noexcept override;

    bool triggerSingleMeasurement() override;
    bool setContinuousMode(LrfMode mode) override;
    bool stopRanging() override;

    bool setRangeGating(double minRangeMeters, double maxRangeMeters) override;
    void registerMeasurementCallback(MeasurementCallback cb) override;
    [[nodiscard]] std::optional<LrfTargetMeasurement> lastMeasurement() const override;

    // --- Extended Hardware Driver Inspection ---

    /// @brief Retrieves the active operational configuration.
    [[nodiscard]] const SerialLrfConfig& config() const noexcept;

    /// @brief Retrieves the active continuous pulse repetition mode.
    [[nodiscard]] LrfMode activeMode() const noexcept;

    /// @brief Retrieves the configured range gate bounds [minMeters, maxMeters].
    [[nodiscard]] std::pair<double, double> rangeGate() const noexcept;

    /// @brief Accesses the underlying transport instance.
    [[nodiscard]] std::shared_ptr<Transport::ITransport> transport() const noexcept;

private:
    void handleIncomingBytes(const std::vector<std::uint8_t>& data);
    void handleTransportState(Transport::TransportState state, const std::string& errorMsg);
    void workerLoop();
    void disarmLaserLocked();

    std::shared_ptr<Transport::ITransport> m_transport;
    SerialLrfConfig m_config;
    std::unique_ptr<ILrfProtocolParser> m_parser;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;

    bool m_connected { false };
    bool m_armed { false };
    LrfMode m_mode { LrfMode::Standby };
    double m_gateMin { 0.5 };
    double m_gateMax { 30000.0 };

    std::chrono::steady_clock::time_point m_lastActivityTime {};
    std::chrono::steady_clock::time_point m_lastContinuousPulseTime {};

    std::optional<LrfTargetMeasurement> m_lastMeasurement {};
    MeasurementCallback m_measurementCb {};
    StateCallback m_stateCb {};

    std::atomic<bool> m_running { false };
    std::thread m_workerThread;
};

} // namespace PayloadHal
