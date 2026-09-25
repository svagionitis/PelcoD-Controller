#pragma once

/// @file OnvifPayloadAdapter.h
/// @brief Hardware Abstraction Layer adapter for ONVIF Profile S/T cameras.

#include "ICameraPayload.h"
#include "IPanTiltUnit.h"
#include "IPayload.h"
#include "Onvif/OnvifClient.h"

#include <memory>
#include <mutex>
#include <string>

namespace PayloadHal {

/// @class OnvifPtuAdapter
/// @brief Bridges an ONVIF Profile S PTZ service to the IPanTiltUnit HAL interface.
class OnvifPtuAdapter : public IPanTiltUnit {
public:
    /// @brief Constructs PTU adapter wrapping an existing OnvifClient.
    /// @param[in] client Shared pointer to configured OnvifClient.
    /// @param[in] profileToken Optional profile token (auto-detected if empty).
    explicit OnvifPtuAdapter(std::shared_ptr<Onvif::OnvifClient> client, std::string profileToken = "");
    ~OnvifPtuAdapter() override = default;

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
    bool savePreset(std::uint8_t presetId, const std::string& name = "") override;
    bool recallPreset(std::uint8_t presetId) override;

    void registerTelemetryCallback(TelemetryCallback cb) override;
    [[nodiscard]] GimbalTelemetry currentTelemetry() const override;

    /// @brief Polls camera for current PTZ status and updates internal telemetry.
    void updateTelemetry();

    /// @brief Sets active media profile token.
    /// @param[in] profileToken Media profile token.
    void setProfileToken(std::string profileToken);

    /// @brief Gets active media profile token.
    /// @return Profile token string.
    [[nodiscard]] const std::string& profileToken() const noexcept;

    /// @brief Accesses underlying OnvifClient.
    /// @return Shared pointer to OnvifClient.
    [[nodiscard]] std::shared_ptr<Onvif::OnvifClient> underlyingClient() const noexcept
    {
        return m_client;
    }

private:
    std::shared_ptr<Onvif::OnvifClient> m_client;
    std::string m_profileToken {};
    mutable std::mutex m_mutex;
    StateCallback m_stateCallback {};
    TelemetryCallback m_telemetryCallback {};
    GimbalTelemetry m_telemetry {};
    bool m_connected { false };
    bool m_ptzSupported { true };
};

/// @class OnvifCameraAdapter
/// @brief Bridges ONVIF Media & Imaging services to the ICameraPayload HAL interface.
class OnvifCameraAdapter : public ICameraPayload {
public:
    /// @brief Constructs camera adapter wrapping an existing OnvifClient.
    /// @param[in] client Shared pointer to configured OnvifClient.
    /// @param[in] profileToken Optional profile token (auto-detected if empty).
    /// @param[in] videoSourceToken Optional video source token (auto-detected if empty).
    explicit OnvifCameraAdapter(
        std::shared_ptr<Onvif::OnvifClient> client, std::string profileToken = "", std::string videoSourceToken = "");
    ~OnvifCameraAdapter() override = default;

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

    /// @brief Retrieves the live RTSP streaming URI.
    /// @return Full RTSP stream URL.
    [[nodiscard]] std::string videoStreamUri() const;

    /// @brief Polls camera for optical and imaging settings and updates internal telemetry.
    void updateTelemetry();

    /// @brief Sets active media profile token.
    /// @param[in] profileToken Media profile token.
    void setProfileToken(std::string profileToken);

    /// @brief Gets active media profile token.
    /// @return Profile token string.
    [[nodiscard]] const std::string& profileToken() const noexcept;

    /// @brief Sets active video source token for imaging services.
    /// @param[in] videoSourceToken Video source token.
    void setVideoSourceToken(std::string videoSourceToken);

    /// @brief Gets active video source token.
    /// @return Video source token string.
    [[nodiscard]] const std::string& videoSourceToken() const noexcept;

    /// @brief Accesses underlying OnvifClient.
    /// @return Shared pointer to OnvifClient.
    [[nodiscard]] std::shared_ptr<Onvif::OnvifClient> underlyingClient() const noexcept
    {
        return m_client;
    }

private:
    std::shared_ptr<Onvif::OnvifClient> m_client;
    std::string m_profileToken {};
    std::string m_videoSourceToken {};
    mutable std::mutex m_mutex;
    StateCallback m_stateCallback {};
    TelemetryCallback m_telemetryCallback {};
    CameraTelemetry m_telemetry {};
    mutable std::string m_streamUri {};
    bool m_connected { false };
};

/// @class OnvifPayloadAdapter
/// @brief Multi-sensor composite IPayload station integrating ONVIF PTZ, optics, and imaging.
class OnvifPayloadAdapter : public IPayload {
public:
    /// @brief Constructs composite payload adapter wrapping an existing OnvifClient.
    /// @param[in] client Shared pointer to configured OnvifClient.
    /// @param[in] profileToken Optional profile token (auto-detected if empty).
    explicit OnvifPayloadAdapter(std::shared_ptr<Onvif::OnvifClient> client, std::string profileToken = "");

    /// @brief Constructs composite payload adapter from endpoint URL and credentials.
    /// @param[in] deviceEndpoint Camera device service URL.
    /// @param[in] credentials Authentication credentials.
    explicit OnvifPayloadAdapter(const std::string& deviceEndpoint, const Onvif::SecurityCredentials& credentials = {});

    ~OnvifPayloadAdapter() override;

    // --- IDevice Lifecycle ---
    bool connect() override;
    void disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    [[nodiscard]] DeviceState state() const noexcept override;
    [[nodiscard]] DeviceInfo info() const noexcept override;
    void registerStateCallback(StateCallback cb) override;

    // --- IPayload Subsystem Accessors ---
    [[nodiscard]] std::shared_ptr<IPanTiltUnit> panTilt() const noexcept override;
    [[nodiscard]] std::shared_ptr<ICameraPayload> primaryCamera() const noexcept override;
    [[nodiscard]] std::shared_ptr<ICameraPayload> secondaryCamera() const noexcept override;
    [[nodiscard]] std::shared_ptr<ILaserRangeFinder> lrf() const noexcept override;

    [[nodiscard]] std::optional<Klv::GeoPoint2D> calculateTargetCoordinates(
        const Klv::GeoPoint2D& platformGps, double platformHeadingDeg, double platformAltMeters) const override;

    /// @brief Convenience accessor for RTSP video stream URI from primary camera.
    /// @return RTSP stream URL string.
    [[nodiscard]] std::string videoStreamUri() const;

    /// @brief Accesses underlying OnvifClient.
    /// @return Shared pointer to OnvifClient.
    [[nodiscard]] std::shared_ptr<Onvif::OnvifClient> underlyingClient() const noexcept
    {
        return m_client;
    }

private:
    std::shared_ptr<Onvif::OnvifClient> m_client;
    std::shared_ptr<OnvifPtuAdapter> m_ptu;
    std::shared_ptr<OnvifCameraAdapter> m_camera;
    mutable std::mutex m_mutex;
    StateCallback m_stateCallback {};
};

} // namespace PayloadHal
