#pragma once

/// @file LensPayloadAdapter.h
/// @brief Payload HAL adapter implementing ICameraPayload over LensController.

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "PayloadHal/ICameraPayload.h"
#include "PayloadHal/IDevice.h"
#include "PayloadHal/PayloadTypes.h"
#include "PayloadHal/VideoStreamTypes.h"

#include "LensController.h"
#include "LensTypes.h"

namespace LensControl {

/// @struct OpticalSensorConfig
/// @brief Sensor dimensions and focal range for pinhole FOV telemetry calculation.
struct OpticalSensorConfig {
    double minFocalLengthMm { 4.3 }; ///< Wide optical focal length in mm
    double maxFocalLengthMm { 129.0 }; ///< Tele optical focal length in mm
    double sensorWidthMm { 5.568 }; ///< Physical sensor width in mm (1/2.8" format)
    double sensorHeightMm { 3.132 }; ///< Physical sensor height in mm
    double maxOpticalZoom { 30.0 }; ///< Maximum optical magnification factor (e.g. 30x)
    PayloadHal::CameraSpectrum spectrum { PayloadHal::CameraSpectrum::DaylightVisible };
};

/// @class LensPayloadAdapter
/// @brief Adapts a LensController camera channel to PayloadHal::ICameraPayload.
class LensPayloadAdapter : public PayloadHal::ICameraPayload {
public:
    /// @brief Constructs an adapter binding a LensController channel to ICameraPayload.
    /// @param controller Shared pointer to master lens controller.
    /// @param channel Optical camera channel index [0..3].
    /// @param config Optical sensor dimensions and zoom specifications.
    explicit LensPayloadAdapter(std::shared_ptr<LensController> controller, CameraIndex channel = CameraIndex { 0U },
        OpticalSensorConfig config = { });

    ~LensPayloadAdapter() override = default;

    LensPayloadAdapter(const LensPayloadAdapter&) = delete;
    LensPayloadAdapter& operator=(const LensPayloadAdapter&) = delete;
    LensPayloadAdapter(LensPayloadAdapter&&) = delete;
    LensPayloadAdapter& operator=(LensPayloadAdapter&&) = delete;

    // --- IDevice Lifecycle Interface ---
    bool connect() override;
    void disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    [[nodiscard]] PayloadHal::DeviceState state() const noexcept override;
    [[nodiscard]] PayloadHal::DeviceInfo info() const noexcept override;
    void registerStateCallback(StateCallback cb) override;

    // --- ICameraPayload Spectral Band ---
    [[nodiscard]] PayloadHal::CameraSpectrum spectrum() const noexcept override;

    // --- Optical Zoom Controls ---
    bool setZoomNormalized(double zoom01) override;
    bool zoomContinuous(float velocity) override;
    bool zoomStop() override;

    // --- Focus Controls ---
    bool setFocusAuto(bool enable) override;
    bool setFocusNormalized(double focus01) override;
    bool focusContinuous(float velocity) override;
    bool focusStop() override;
    bool triggerOnePushFocus() override;

    // --- Iris Controls (Gated / Pass-Through) ---
    bool setIrisAuto(bool enable) override;
    bool setIrisNormalized(double iris01) override;
    bool irisContinuous(float velocity) override;
    bool irisStop() override;

    // --- Sensor Enhancement Controls ---
    bool setDayNightIcr(bool nightMode) override;
    bool setDefog(bool enable) override;
    bool setStabilizer(bool enable) override;

    // --- Telemetry Reporting ---
    void registerTelemetryCallback(TelemetryCallback cb) override;
    [[nodiscard]] PayloadHal::CameraTelemetry currentTelemetry() const override;

    // --- Video Stream Binding ---
    [[nodiscard]] std::string videoStreamUri(
        PayloadHal::VideoStreamProfile profile = PayloadHal::VideoStreamProfile::Primary) const override;
    bool setVideoStreamUri(const std::string& uri,
        PayloadHal::VideoStreamProfile profile = PayloadHal::VideoStreamProfile::Primary) override;
    [[nodiscard]] std::vector<PayloadHal::VideoStreamDescriptor> availableStreams() const override;

    // --- Telemetry Polling Tick ---
    /// @brief Polls underlying LensController, computes optics model, and notifies observers.
    void updateTelemetry();

private:
    [[nodiscard]] double computeFocalLength(double normZoom) const noexcept;
    [[nodiscard]] double computeHorizontalFov(double focalLengthMm) const noexcept;
    [[nodiscard]] double computeVerticalFov(double focalLengthMm) const noexcept;
    void updateTelemetryLocked();

    std::shared_ptr<LensController> m_controller;
    CameraIndex m_channel { 0U };
    OpticalSensorConfig m_config { };

    mutable std::mutex m_mutex { };
    bool m_connected { false };
    PayloadHal::DeviceState m_deviceState { PayloadHal::DeviceState::Disconnected };
    StateCallback m_stateCallback { };
    TelemetryCallback m_telemetryCallback { };
    PayloadHal::CameraTelemetry m_telemetry { };
    std::map<PayloadHal::VideoStreamProfile, std::string> m_streamUris { };

    double m_cachedNormalizedZoom { 0.0 };
    double m_cachedNormalizedFocus { 0.0 };
    double m_cachedIrisNormalized { 0.5 };
    bool m_autoIrisActive { true };
    bool m_dayNightIcrActive { false };
    bool m_defogActive { false };
    bool m_stabilizerActive { false };
};

} // namespace LensControl
