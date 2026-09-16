#pragma once

/// @file QOnvifDevice.h
/// @brief Qt QObject adapter bridging ONVIF Profile S IP camera client.

#include "PelcoDOnvif/OnvifClient.h"
#include "PelcoDOnvif/OnvifDiscovery.h"
#include "PelcoDOnvif/OnvifTypes.h"

#include <QList>
#include <QObject>
#include <QString>

#include <memory>

namespace PelcoD::Qt {

/// @class QOnvifDevice
/// @brief Qt wrapper managing an ONVIF Profile S IP camera session.
/// @details Exposes signals/slots for device discovery, capability negotiation,
/// RTSP stream URI retrieval, and PTZ continuous/absolute motion control.
class QOnvifDevice : public QObject {
    Q_OBJECT

public:
    /// @brief Constructs a QOnvifDevice instance.
    /// @param[in] parent Optional parent QObject.
    explicit QOnvifDevice(QObject* parent = nullptr);

    /// @brief Destructor.
    ~QOnvifDevice() override;

    /// @brief Checks if device is currently connected and authenticated.
    /// @return True if connected.
    [[nodiscard]] bool isConnected() const;

    /// @brief Gets active media profile token.
    /// @return Current profile token string.
    [[nodiscard]] QString activeProfileToken() const;

    /// @brief Gets active RTSP stream URI resolved for current profile.
    /// @return RTSP URI.
    [[nodiscard]] QString rtspStreamUri() const;

    /// @brief Gets discovered media profiles.
    /// @return List of MediaProfile records.
    [[nodiscard]] std::vector<PelcoD::Onvif::MediaProfile> profiles() const;

    /// @brief Gets camera hardware identification metadata.
    /// @return DeviceInformation struct.
    [[nodiscard]] PelcoD::Onvif::DeviceInformation deviceInformation() const;

    /// @brief Executes synchronous discovery of ONVIF cameras on LAN.
    /// @param[in] timeoutMs Multicast listening timeout in milliseconds.
    /// @return List of discovered cameras.
    [[nodiscard]] static QList<PelcoD::Onvif::DiscoveredDevice> discoverCameras(int timeoutMs = 2000);

    /// @brief Starts asynchronous discovery of ONVIF cameras on a worker thread.
    /// @param[in] timeoutMs Multicast listening timeout in milliseconds.
    void discoverCamerasAsync(int timeoutMs = 2000);

public Q_SLOTS:
    /// @brief Connects to an ONVIF camera endpoint and queries capabilities/profiles.
    /// @param[in] endpoint Device service URL.
    /// @param[in] username Authentication username.
    /// @param[in] password Authentication plaintext password.
    /// @return True on success.
    bool connectToCamera(
        const QString& endpoint, const QString& username = QString(), const QString& password = QString());

    /// @brief Disconnects from current camera and resets state.
    void disconnectFromCamera();

    /// @brief Sets active media profile by token and resolves its RTSP stream URI.
    /// @param[in] token Media profile token.
    /// @return True if profile was found and stream URI retrieved.
    bool setActiveProfile(const QString& token);

    /// @brief Sends continuous pan/tilt velocity command.
    /// @param[in] panSpeed Normalized horizontal velocity [-1.0, 1.0].
    /// @param[in] tiltSpeed Normalized vertical velocity [-1.0, 1.0].
    void move(double panSpeed, double tiltSpeed);

    /// @brief Sends continuous zoom velocity command.
    /// @param[in] direction Positive (+1) for tele/zoom in, negative (-1) for wide/zoom out, 0 to stop.
    /// @param[in] speed Normalized zoom speed [0.0, 1.0].
    void zoom(int direction, double speed = 1.0);

    /// @brief Stops motorized pan/tilt and/or zoom motion.
    /// @param[in] stopPanTilt True to halt pan and tilt.
    /// @param[in] stopZoom True to halt zoom.
    void stopMotion(bool stopPanTilt = true, bool stopZoom = true);

    /// @brief Moves PTZ head to absolute normalized coordinates.
    /// @param[in] pan Normalized pan [-1.0, 1.0].
    /// @param[in] tilt Normalized tilt [-1.0, 1.0].
    /// @param[in] zoom Normalized zoom [0.0, 1.0].
    void absoluteMove(double pan, double tilt, double zoom);

    /// @brief Refreshes and emits current PTZ kinematics status.
    void refreshStatus();

Q_SIGNALS:
    /// @brief Emitted when device connection succeeds.
    /// @param[in] endpoint Connected service URL.
    /// @param[in] model Camera model name.
    void connected(const QString& endpoint, const QString& model);

    /// @brief Emitted when camera session is disconnected.
    void disconnected();

    /// @brief Emitted when active RTSP stream URI is resolved.
    /// @param[in] uri RTSP stream URL ready for playback.
    void streamUriResolved(const QString& uri);

    /// @brief Emitted when PTZ position/status is refreshed.
    /// @param[in] status Current kinematics state.
    void statusUpdated(const PelcoD::Onvif::PtzStatus& status);

    /// @brief Emitted when an operation fails.
    /// @param[in] message Diagnostic error message.
    void errorOccurred(const QString& message);

    /// @brief Emitted when asynchronous discovery finishes.
    /// @param[in] devices List of discovered cameras.
    void discoveryFinished(const QList<PelcoD::Onvif::DiscoveredDevice>& devices);

private:
    std::unique_ptr<PelcoD::Onvif::OnvifClient> m_client {};
    bool m_connected { false };
    QString m_endpoint {};
    QString m_activeProfileToken {};
    QString m_rtspStreamUri {};
    std::vector<PelcoD::Onvif::MediaProfile> m_profiles {};
    PelcoD::Onvif::DeviceInformation m_deviceInfo {};
};

} // namespace PelcoD::Qt
