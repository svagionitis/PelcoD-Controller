#pragma once

/// @file ViscaSonyAdapter.h
/// @brief Hardware Abstraction Layer adapter for Sony FCB block cameras using VISCA.

#include "ICameraPayload.h"
#include "SonyFCBDevice.h"

#include <memory>
#include <mutex>

namespace PayloadHal {

/// @class ViscaSonyAdapter
/// @brief Adapts a Sony FCB series block camera (SonyFCBDevice) to the ICameraPayload HAL interface.
class ViscaSonyAdapter : public ICameraPayload {
public:
    /// @brief Constructs an adapter wrapping an existing SonyFCBDevice instance.
    /// @param[in] device Shared pointer to SonyFCBDevice.
    explicit ViscaSonyAdapter(std::shared_ptr<Visca::Sony::SonyFCBDevice> device);
    ~ViscaSonyAdapter() override = default;

    // --- IDevice Interface ---
    bool connect() override;
    void disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    [[nodiscard]] DeviceState state() const noexcept override;
    [[nodiscard]] DeviceInfo info() const noexcept override;
    void registerStateCallback(StateCallback cb) override;

    // --- ICameraPayload Interface ---
    [[nodiscard]] CameraSpectrum spectrum() const noexcept override;

    bool setZoomNormalized(double zoom01) override;
    bool zoomContinuous(float velocity) override;
    bool zoomStop() override;

    bool setFocusAuto(bool enable) override;
    bool setFocusNormalized(double focus01) override;
    bool triggerOnePushFocus() override;

    bool setDayNightIcr(bool nightMode) override;
    bool setDefog(bool enable) override;
    bool setStabilizer(bool enable) override;

    void registerTelemetryCallback(TelemetryCallback cb) override;
    [[nodiscard]] CameraTelemetry currentTelemetry() const override;

    /// @brief Polls device status and updates internal telemetry.
    void updateTelemetry();

    [[nodiscard]] std::shared_ptr<Visca::Sony::SonyFCBDevice> underlyingDevice() const noexcept
    {
        return m_device;
    }

private:
    std::shared_ptr<Visca::Sony::SonyFCBDevice> m_device;
    mutable std::mutex m_mutex;
    StateCallback m_stateCallback {};
    TelemetryCallback m_telemetryCallback {};
    CameraTelemetry m_telemetry {};
    bool m_connected { false };
};

} // namespace PayloadHal
