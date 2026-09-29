#pragma once

/// @file FlirPfecPtzAdapter.h
/// @brief Hardware Abstraction Layer adapter mapping IPanTiltUnit to FlirPfecDevice.

#include "IPanTiltUnit.h"
#include "Nmea/FlirPfecDevice.h"

#include <memory>
#include <mutex>
#include <string>

namespace PayloadHal {

/// @class FlirPfecPtzAdapter
/// @brief Bridges a FLIR M-Series marine PTZ camera (FlirPfecDevice) to the IPanTiltUnit HAL interface.
class FlirPfecPtzAdapter : public IPanTiltUnit {
public:
    /// @brief Constructs an adapter wrapping an existing FlirPfecDevice instance.
    /// @param[in] device Shared pointer to underlying FlirPfecDevice.
    explicit FlirPfecPtzAdapter(std::shared_ptr<Nmea::FlirPfecDevice> device);
    ~FlirPfecPtzAdapter() override;

    // --- IDevice Interface ---
    bool connect() override;
    void disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    [[nodiscard]] DeviceState state() const noexcept override;
    [[nodiscard]] DeviceInfo info() const noexcept override;
    void registerStateCallback(StateCallback cb) override;

    // --- IPanTiltUnit Interface ---
    bool setRate(double panDegPerSec, double tiltDegPerSec) override;
    bool setNormalizedVelocity(float panVel, float tiltVel) override;
    bool setAbsoluteAngles(double panDeg, double tiltDeg) override;
    bool setRelativeNudge(double deltaPanDeg, double deltaTiltDeg) override;
    bool stopMotion() override;

    [[nodiscard]] bool supportsStabilization() const noexcept override;
    bool setStabilizationMode(StabilizationMode mode) override;
    [[nodiscard]] StabilizationMode stabilizationMode() const noexcept override;
    bool zeroGyroDrift() override;

    bool getLimits(double& minPan, double& maxPan, double& minTilt, double& maxTilt) const override;
    bool savePreset(uint8_t presetId, const std::string& name = "") override;
    bool recallPreset(uint8_t presetId) override;

    void registerTelemetryCallback(TelemetryCallback cb) override;
    [[nodiscard]] GimbalTelemetry currentTelemetry() const override;

    /// @brief Provides access to the underlying FlirPfecDevice.
    [[nodiscard]] std::shared_ptr<Nmea::FlirPfecDevice> underlyingDevice() const noexcept
    {
        return m_device;
    }

private:
    void handlePositionReport(const Nmea::PfecGimbalPosition& pos);

    std::shared_ptr<Nmea::FlirPfecDevice> m_device;
    std::size_t m_posSubscriptionId { 0U };

    mutable std::mutex m_mutex;
    StateCallback m_stateCallback {};
    TelemetryCallback m_telemetryCallback {};
    GimbalTelemetry m_lastTelemetry {};
    DeviceState m_currentState { DeviceState::Disconnected };
};

} // namespace PayloadHal
