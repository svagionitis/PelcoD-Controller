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

    /// @brief Gets snapshot HTTP URI resolved for current profile.
    /// @return Snapshot URI.
    [[nodiscard]] QString snapshotUri() const;

    /// @brief Gets discovered media profiles.
    /// @return List of MediaProfile records.
    [[nodiscard]] std::vector<PelcoD::Onvif::MediaProfile> profiles() const;

    /// @brief Gets cached PTZ presets for current profile.
    /// @return List of PtzPreset records.
    [[nodiscard]] std::vector<PelcoD::Onvif::PtzPreset> presets() const;

    /// @brief Gets cached preset tours for current profile.
    /// @return List of PresetTour records.
    [[nodiscard]] std::vector<PelcoD::Onvif::PresetTour> presetTours() const
    {
        return m_presetTours;
    }

    /// @brief Gets cached OSD overlays for active video source.
    /// @return List of OsdConfig records.
    [[nodiscard]] std::vector<PelcoD::Onvif::OsdConfig> osds() const
    {
        return m_osds;
    }

    /// @brief Gets cached ONVIF user accounts.
    /// @return List of OnvifUser records.
    [[nodiscard]] std::vector<PelcoD::Onvif::OnvifUser> users() const
    {
        return m_users;
    }

    /// @brief Gets cached network interface configurations.
    /// @return List of NetworkInterfaceConfig records.
    [[nodiscard]] std::vector<PelcoD::Onvif::NetworkInterfaceConfig> networkInterfaces() const
    {
        return m_networkInterfaces;
    }

    /// @brief Gets cached DNS configuration.
    /// @return DnsConfig struct.
    [[nodiscard]] PelcoD::Onvif::DnsConfig dnsConfig() const
    {
        return m_dnsConfig;
    }

    /// @brief Gets cached NTP configuration.
    /// @return NtpConfig struct.
    [[nodiscard]] PelcoD::Onvif::NtpConfig ntpConfig() const
    {
        return m_ntpConfig;
    }

    /// @brief Gets cached device gateway.
    /// @return Gateway IP address.
    [[nodiscard]] QString networkGateway() const
    {
        return m_networkGateway;
    }

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

    /// @brief Moves PTZ head relatively by translation offset delta.
    /// @param[in] pan Pan step delta [-1.0, 1.0].
    /// @param[in] tilt Tilt step delta [-1.0, 1.0].
    /// @param[in] zoom Zoom step delta [-1.0, 1.0].
    void relativeMove(double pan, double tilt, double zoom = 0.0);

    /// @brief Commands camera head to move to configured home position.
    void gotoHomePosition();

    /// @brief Sets current camera head position as home position.
    void setHomePosition();

    /// @brief Sends an auxiliary command (e.g. "tt:Wiper|On", "tt:Washer|On", "Aux1On").
    /// @param[in] auxiliaryData Command string token.
    /// @return Returned auxiliary response or empty on failure.
    QString sendAuxiliaryCommand(const QString& auxiliaryData);

    /// @brief Refreshes PTZ presets from camera for active profile.
    void refreshPresets();

    /// @brief Recalls and navigates to preset.
    /// @param[in] presetToken Preset identifier token.
    /// @param[in] speed Normalized speed [0.0, 1.0].
    /// @return True on success.
    bool gotoPreset(const QString& presetToken, double speed = 1.0);

    /// @brief Saves or updates preset.
    /// @param[in] presetName Friendly preset label.
    /// @param[in] presetToken Optional token to overwrite.
    /// @return True on success.
    bool setPreset(const QString& presetName, const QString& presetToken = QString());

    /// @brief Deletes preset from camera.
    /// @param[in] presetToken Preset identifier token.
    /// @return True on success.
    bool removePreset(const QString& presetToken);

    /// @brief Queries list of configured preset tours for active profile.
    void refreshPresetTours();

    /// @brief Operates a preset tour (Start, Stop, Pause).
    /// @param[in] tourToken Tour identifier token.
    /// @param[in] operation "Start", "Stop", or "Pause".
    /// @return True on success.
    bool operatePresetTour(const QString& tourToken, const QString& operation);

    /// @brief Operates a preset tour using typed operation enum.
    /// @param[in] tourToken Tour identifier token.
    /// @param[in] operation PresetTourOperation enum.
    /// @return True on success.
    bool operatePresetTour(const QString& tourToken, PelcoD::Onvif::PresetTourOperation operation);

    /// @brief Modifies a preset tour sequence.
    /// @param[in] tour Preset tour details and spots.
    /// @return True on success.
    bool modifyPresetTour(const PelcoD::Onvif::PresetTour& tour);

    /// @brief Deletes a preset tour.
    /// @param[in] tourToken Tour identifier token.
    /// @return True on success.
    bool removePresetTour(const QString& tourToken);

    /// @brief Resolves HTTP snapshot URI for active profile.
    /// @return Resolved URI string.
    QString resolveSnapshotUri();

    /// @brief Sends reboot command to camera.
    /// @return True on success.
    bool rebootCamera();

    /// @brief Refreshes and emits current PTZ kinematics status.
    void refreshStatus();

    // =========================================================================
    // Profile T: Imaging & Optical Controls
    // =========================================================================

    /// @brief Queries and emits current optical/imaging settings.
    /// @param[in] videoSourceToken Optional token (defaults to active profile's video source).
    void refreshImagingSettings(const QString& videoSourceToken = QString());

    /// @brief Applies updated optical/imaging parameters to the camera.
    /// @param[in] settings Updated imaging parameters.
    /// @param[in] videoSourceToken Optional token (defaults to active profile's video source).
    /// @return True on success.
    bool setImagingSettings(
        const PelcoD::Onvif::ImagingSettings& settings, const QString& videoSourceToken = QString());

    /// @brief Starts continuous motorized optical focus movement.
    /// @param[in] speed Normalized speed [-1.0 (near) to +1.0 (far)].
    /// @param[in] videoSourceToken Optional token (defaults to active profile's video source).
    void focusContinuous(float speed, const QString& videoSourceToken = QString());

    /// @brief Stops motorized optical focus movement.
    /// @param[in] videoSourceToken Optional token (defaults to active profile's video source).
    void focusStop(const QString& videoSourceToken = QString());

    // =========================================================================
    // Profile T: PullPoint Event Service
    // =========================================================================

    /// @brief Starts background PullPoint event subscription and polling loop.
    /// @param[in] pollIntervalMs Interval between event pulls in milliseconds.
    void startEventSubscription(int pollIntervalMs = 2000);

    /// @brief Stops background PullPoint event subscription and unregisters from camera.
    void stopEventSubscription();

    /// @brief Checks if event subscription is currently active.
    [[nodiscard]] bool isEventSubscriptionActive() const;

    // =========================================================================
    // Profile T: On-Screen Display (OSD) Overlays
    // =========================================================================

    /// @brief Queries list of configured OSD overlays for active video source.
    void refreshOSDs();

    /// @brief Creates a new OSD overlay configuration on camera.
    /// @param[in] osd OSD configuration parameters.
    /// @return Assigned token or empty on failure.
    QString createOSD(const PelcoD::Onvif::OsdConfig& osd);

    /// @brief Modifies an existing OSD overlay.
    /// @param[in] osd Updated OSD configuration.
    /// @return True on success.
    bool setOSD(const PelcoD::Onvif::OsdConfig& osd);

    /// @brief Deletes an OSD overlay.
    /// @param[in] osdToken Token of the OSD to delete.
    /// @return True if removed successfully.
    bool deleteOSD(const QString& osdToken);

    // =========================================================================
    // Device Management & Security
    // =========================================================================

    /// @brief Queries list of configured ONVIF user accounts.
    void refreshUsers();

    /// @brief Creates new ONVIF user account on camera.
    /// @param[in] user New user parameters.
    /// @return True on success.
    bool createUser(const PelcoD::Onvif::OnvifUser& user);

    /// @brief Updates an existing ONVIF user's password and/or role.
    /// @param[in] user Updated user parameters.
    /// @return True on success.
    bool setUser(const PelcoD::Onvif::OnvifUser& user);

    /// @brief Deletes ONVIF user account by username.
    /// @param[in] username Username to delete.
    /// @return True on success.
    bool deleteUser(const QString& username);

    /// @brief Queries network adapter interface configurations.
    void refreshNetworkInterfaces();

    /// @brief Updates network interface configuration.
    /// @param[in] config Updated interface settings.
    /// @return True on success.
    bool setNetworkInterface(const PelcoD::Onvif::NetworkInterfaceConfig& config);

    /// @brief Queries default gateway IP.
    void refreshNetworkGateway();

    /// @brief Sets default gateway address.
    /// @param[in] gateway Gateway IP address.
    /// @return True on success.
    bool setNetworkGateway(const QString& gateway);

    /// @brief Queries DNS server configuration.
    void refreshDNS();

    /// @brief Sets DNS server configuration.
    /// @param[in] dns Updated DNS settings.
    /// @return True on success.
    bool setDNS(const PelcoD::Onvif::DnsConfig& dns);

    /// @brief Queries NTP server configuration.
    void refreshNTP();

    /// @brief Sets NTP server configuration.
    /// @param[in] ntp Updated NTP settings.
    /// @return True on success.
    bool setNTP(const PelcoD::Onvif::NtpConfig& ntp);

    /// @brief Updates camera system date, time, and timezone.
    /// @param[in] dt Date/time parameters.
    /// @return True on success.
    bool setSystemDateAndTime(const PelcoD::Onvif::SystemDateTimeConfig& dt);

    /// @brief Resets device to factory default settings.
    /// @param[in] hard True for hard reset, false for soft.
    /// @return True on success.
    bool setSystemFactoryDefault(bool hard = false);

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

    /// @brief Emitted when active snapshot URI is resolved.
    /// @param[in] uri JPEG snapshot URL.
    void snapshotUriResolved(const QString& uri);

    /// @brief Emitted when PTZ position/status is refreshed.
    /// @param[in] status Current kinematics state.
    void statusUpdated(const PelcoD::Onvif::PtzStatus& status);

    /// @brief Emitted when preset list is refreshed.
    /// @param[in] presets List of camera presets.
    void presetsUpdated(const std::vector<PelcoD::Onvif::PtzPreset>& presets);

    /// @brief Emitted when preset tours list is refreshed.
    /// @param[in] tours List of camera preset tours.
    void presetToursUpdated(const std::vector<PelcoD::Onvif::PresetTour>& tours);

    /// @brief Emitted when reboot request completes.
    /// @param[in] success True if reboot was accepted.
    void rebootCompleted(bool success);

    /// @brief Emitted when optical imaging settings are refreshed.
    /// @param[in] settings Current camera imaging settings.
    void imagingSettingsUpdated(const PelcoD::Onvif::ImagingSettings& settings);

    /// @brief Emitted when an auxiliary command finishes.
    /// @param[in] success True if accepted.
    /// @param[in] response Server response payload.
    void auxiliaryCommandCompleted(bool success, const QString& response);

    /// @brief Emitted when an event notification message is pulled.
    /// @param[in] event Event notification details.
    void eventReceived(const PelcoD::Onvif::OnvifEvent& event);

    /// @brief Emitted when OSD overlay configuration list is refreshed.
    /// @param[in] osds List of camera OSD configurations.
    void osdsUpdated(const std::vector<PelcoD::Onvif::OsdConfig>& osds);

    /// @brief Emitted when ONVIF users list is refreshed.
    /// @param[in] users List of camera user accounts.
    void usersUpdated(const std::vector<PelcoD::Onvif::OnvifUser>& users);

    /// @brief Emitted when network interface configuration is refreshed.
    /// @param[in] ifaces List of network interface configs.
    void networkInterfacesUpdated(const std::vector<PelcoD::Onvif::NetworkInterfaceConfig>& ifaces);

    /// @brief Emitted when default gateway is refreshed.
    /// @param[in] gateway Gateway address string.
    void networkGatewayUpdated(const QString& gateway);

    /// @brief Emitted when DNS configuration is refreshed.
    /// @param[in] dns Current DNS settings.
    void dnsUpdated(const PelcoD::Onvif::DnsConfig& dns);

    /// @brief Emitted when NTP configuration is refreshed.
    /// @param[in] ntp Current NTP settings.
    void ntpUpdated(const PelcoD::Onvif::NtpConfig& ntp);

    /// @brief Emitted when factory default reset command finishes.
    /// @param[in] success True if accepted.
    void factoryDefaultCompleted(bool success);

    /// @brief Emitted when an operation fails.
    /// @param[in] message Diagnostic error message.
    void errorOccurred(const QString& message);

    /// @brief Emitted when asynchronous discovery finishes.
    /// @param[in] devices List of discovered cameras.
    void discoveryFinished(const QList<PelcoD::Onvif::DiscoveredDevice>& devices);

private:
    void pollEvents();

    std::unique_ptr<PelcoD::Onvif::OnvifClient> m_client {};
    bool m_connected { false };
    QString m_endpoint {};
    QString m_activeProfileToken {};
    QString m_activeVideoSourceToken {};
    QString m_rtspStreamUri {};
    QString m_snapshotUri {};
    std::vector<PelcoD::Onvif::MediaProfile> m_profiles {};
    std::vector<PelcoD::Onvif::PtzPreset> m_presets {};
    std::vector<PelcoD::Onvif::PresetTour> m_presetTours {};
    std::vector<PelcoD::Onvif::OsdConfig> m_osds {};
    std::vector<PelcoD::Onvif::OnvifUser> m_users {};
    std::vector<PelcoD::Onvif::NetworkInterfaceConfig> m_networkInterfaces {};
    QString m_networkGateway {};
    PelcoD::Onvif::DnsConfig m_dnsConfig {};
    PelcoD::Onvif::NtpConfig m_ntpConfig {};
    PelcoD::Onvif::DeviceInformation m_deviceInfo {};
    PelcoD::Onvif::ImagingSettings m_imagingSettings {};
    QString m_eventSubscriptionUrl {};
    bool m_eventSubActive { false };
};

} // namespace PelcoD::Qt
