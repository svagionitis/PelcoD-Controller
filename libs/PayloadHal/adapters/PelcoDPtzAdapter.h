#pragma once

/// @file PelcoDPtzAdapter.h
/// @brief Hardware Abstraction Layer adapter mapping IPanTiltUnit to PelcoDDevice.

#include "IPanTiltUnit.h"
#include "PelcoDCore/PelcoDDevice.h"

#include <memory>
#include <mutex>

namespace PayloadHal {

/// @class PelcoDPtzAdapter
/// @brief Bridges a Pelco-D PTZ camera/head (PelcoDDevice) to the IPanTiltUnit HAL interface.
class PelcoDPtzAdapter : public IPanTiltUnit {
public:
    /// @brief Constructs an adapter wrapping an existing PelcoDDevice instance.
    /// @param[in] device Shared pointer to the underlying PelcoDDevice.
    explicit PelcoDPtzAdapter(std::shared_ptr<PelcoD::PelcoDDevice> device);
    ~PelcoDPtzAdapter() override;

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

    /// @brief Provides access to the underlying PelcoDDevice.
    [[nodiscard]] std::shared_ptr<PelcoD::PelcoDDevice> underlyingDevice() const noexcept
    {
        return m_device;
    }

private:
    void handleDeviceStatus(const PelcoD::DeviceStatus& status);

    std::shared_ptr<PelcoD::PelcoDDevice> m_device;
    mutable std::mutex m_mutex;
    StateCallback m_stateCallback {};
    TelemetryCallback m_telemetryCallback {};
    PelcoD::Connection m_statusConnection {};
    GimbalTelemetry m_lastTelemetry {};
    DeviceState m_currentState { DeviceState::Disconnected };
};

} // namespace PayloadHal
