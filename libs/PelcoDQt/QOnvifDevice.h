#pragma once

/// @file QOnvifDevice.h
/// @brief Qt QObject adapter bridging ONVIF Profile S IP camera client.

#include "Onvif/OnvifClient.h"
#include "Onvif/OnvifDiscovery.h"
#include "Onvif/OnvifTypes.h"

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
    [[nodiscard]] std::vector<Onvif::MediaProfile> profiles() const;

    /// @brief Gets cached PTZ presets for current profile.
    /// @return List of PtzPreset records.
    [[nodiscard]] std::vector<Onvif::PtzPreset> presets() const;

    /// @brief Gets cached preset tours for current profile.
    /// @return List of PresetTour records.
    [[nodiscard]] std::vector<Onvif::PresetTour> presetTours() const
    {
        return m_presetTours;
    }

    /// @brief Gets cached OSD overlays for active video source.
    /// @return List of OsdConfig records.
    [[nodiscard]] std::vector<Onvif::OsdConfig> osds() const
    {
        return m_osds;
    }

    /// @brief Gets cached ONVIF user accounts.
    /// @return List of OnvifUser records.
    [[nodiscard]] std::vector<Onvif::OnvifUser> users() const
    {
        return m_users;
    }

    /// @brief Gets cached network interface configurations.
    /// @return List of NetworkInterfaceConfig records.
    [[nodiscard]] std::vector<Onvif::NetworkInterfaceConfig> networkInterfaces() const
    {
        return m_networkInterfaces;
    }

    /// @brief Gets cached DNS configuration.
    /// @return DnsConfig struct.
    [[nodiscard]] Onvif::DnsConfig dnsConfig() const
    {
        return m_dnsConfig;
    }

    /// @brief Gets cached NTP configuration.
    /// @return NtpConfig struct.
    [[nodiscard]] Onvif::NtpConfig ntpConfig() const
    {
        return m_ntpConfig;
    }

    /// @brief Gets cached device gateway.
    /// @return Gateway IP address.
    [[nodiscard]] QString networkGateway() const
    {
        return m_networkGateway;
    }

    /// @brief Gets cached optical focus status.
    /// @return FocusStatus20 struct.
    [[nodiscard]] Onvif::FocusStatus20 focusStatus() const
    {
        return m_focusStatus;
    }

    /// @brief Gets cached imaging presets.
    /// @return Vector of ImagingPreset.
    [[nodiscard]] std::vector<Onvif::ImagingPreset> imagingPresets() const
    {
        return m_imagingPresets;
    }

    /// @brief Gets cached relay outputs.
    /// @return Vector of RelayOutputConfig.
    [[nodiscard]] std::vector<Onvif::RelayOutputConfig> relayOutputs() const
    {
        return m_relayOutputs;
    }

    /// @brief Gets cached digital inputs.
    /// @return Vector of DigitalInputConfig.
    [[nodiscard]] std::vector<Onvif::DigitalInputConfig> digitalInputs() const
    {
        return m_digitalInputs;
    }

    /// @brief Gets cached installed X.509 certificates.
    [[nodiscard]] std::vector<Onvif::OnvifCertificate> certificates() const
    {
        return m_certificates;
    }

    /// @brief Gets cached recordings list (Profile G).
    [[nodiscard]] std::vector<Onvif::RecordingConfig> recordings() const
    {
        return m_recordings;
    }

    /// @brief Gets cached recording jobs list (Profile G).
    [[nodiscard]] std::vector<Onvif::RecordingJob> recordingJobs() const
    {
        return m_recordingJobs;
    }

    /// @brief Gets cached recording summary (Profile G).
    [[nodiscard]] std::optional<Onvif::RecordingSummary> recordingSummary() const
    {
        return m_recordingSummary;
    }

    /// @brief Gets cached replay configuration (Profile G).
    [[nodiscard]] std::optional<Onvif::ReplayConfiguration> replayConfiguration() const
    {
        return m_replayConfig;
    }

    /// @brief Gets cached video analytics rules (Profile M & T).
    [[nodiscard]] std::vector<Onvif::AnalyticsRule> rules() const
    {
        return m_rules;
    }

    /// @brief Gets cached supported video analytics rules descriptions.
    [[nodiscard]] std::vector<Onvif::AnalyticsRuleDescription> supportedRules() const
    {
        return m_supportedRules;
    }

    /// @brief Gets cached video analytics modules.
    [[nodiscard]] std::vector<Onvif::AnalyticsModule> analyticsModules() const
    {
        return m_analyticsModules;
    }

    /// @brief Gets cached camera geographic location and mounting orientation.
    [[nodiscard]] std::optional<Onvif::LocationEntity> geoLocation() const
    {
        return m_geoLocation;
    }

    /// @brief Gets cached privacy masks (Profile T / Media2).
    [[nodiscard]] std::vector<Onvif::PrivacyMask> masks() const
    {
        return m_masks;
    }

    /// @brief Gets cached privacy mask options (Profile T / Media2).
    [[nodiscard]] std::optional<Onvif::MaskOptions> maskOptions() const
    {
        return m_maskOptions;
    }

    /// @brief Gets cached video source modes (Profile T / Media2).
    [[nodiscard]] std::vector<Onvif::VideoSourceMode> videoSourceModes() const
    {
        return m_videoSourceModes;
    }

    /// @brief Gets cached radiometric environmental configuration.
    [[nodiscard]] Onvif::RadiometryConfig radiometryConfig() const
    {
        return m_radiometryConfig;
    }

    /// @brief Gets cached spotmeter measurements.
    [[nodiscard]] std::vector<Onvif::RadiometrySpot> radiometrySpots() const
    {
        return m_radiometrySpots;
    }

    /// @brief Gets cached radiometric zone/box measurements.
    [[nodiscard]] std::vector<Onvif::RadiometryBox> radiometryBoxes() const
    {
        return m_radiometryBoxes;
    }

    /// @brief Gets cached thermal color palettes.
    [[nodiscard]] std::vector<Onvif::ColorPalette> colorPalettes() const
    {
        return m_colorPalettes;
    }

    /// @brief Gets camera hardware identification metadata.
    /// @return DeviceInformation struct.
    [[nodiscard]] Onvif::DeviceInformation deviceInformation() const;

    /// @brief Executes synchronous discovery of ONVIF cameras on LAN.
    /// @param[in] timeoutMs Multicast listening timeout in milliseconds.
    /// @return List of discovered cameras.
    [[nodiscard]] static QList<Onvif::DiscoveredDevice> discoverCameras(int timeoutMs = 2000);

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

    /// @brief Moves PTZ head to absolute spherical angles in degrees.
    /// @param[in] azimuthDeg Azimuth angle in degrees [0.0, 360.0).
    /// @param[in] elevationDeg Elevation angle in degrees [-90.0, +90.0].
    /// @param[in] zoom Normalized zoom position [0.0, 1.0].
    void absoluteMoveSpherical(double azimuthDeg, double elevationDeg, double zoom = 0.0);

    /// @brief Directs camera PTZ to aim at a WGS84 geographic coordinate (ONVIF GeoMove).
    /// @param[in] lat Target latitude in degrees.
    /// @param[in] lon Target longitude in degrees.
    /// @param[in] elevation Target elevation in meters.
    /// @param[in] speed Optional speed ratio [0.0, 1.0].
    /// @param[in] areaWidth Optional target framing width in meters.
    /// @param[in] areaHeight Optional target framing height in meters.
    void geoMove(
        double lat, double lon, double elevation, double speed = 1.0, double areaWidth = 0.0, double areaHeight = 0.0);

    /// @brief Queries camera installation geographic location and mounting orientation.
    void refreshGeoLocation();

    /// @brief Updates camera installation geographic location and mounting orientation.
    /// @param[in] location Updated LocationEntity.
    void updateGeoLocation(const Onvif::LocationEntity& location);

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
    bool operatePresetTour(const QString& tourToken, Onvif::PresetTourOperation operation);

    /// @brief Modifies a preset tour sequence.
    /// @param[in] tour Preset tour details and spots.
    /// @return True on success.
    bool modifyPresetTour(const Onvif::PresetTour& tour);

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
    bool setImagingSettings(const Onvif::ImagingSettings& settings, const QString& videoSourceToken = QString());

    /// @brief Starts continuous motorized optical focus movement.
    /// @param[in] speed Normalized speed [-1.0 (near) to +1.0 (far)].
    /// @param[in] videoSourceToken Optional token (defaults to active profile's video source).
    void focusContinuous(float speed, const QString& videoSourceToken = QString());

    /// @brief Stops motorized optical focus movement.
    /// @param[in] videoSourceToken Optional token (defaults to active profile's video source).
    void focusStop(const QString& videoSourceToken = QString());

    /// @brief Queries current focus status and encoder position.
    /// @param[in] videoSourceToken Optional token.
    void refreshFocusStatus(const QString& videoSourceToken = QString());

    /// @brief Moves optical focus to absolute position.
    /// @param[in] position Target position [0.0 to 1.0].
    /// @param[in] speed Speed ratio.
    /// @param[in] videoSourceToken Optional token.
    void focusAbsolute(float position, float speed = 1.0f, const QString& videoSourceToken = QString());

    /// @brief Moves optical focus relative to current position.
    /// @param[in] distance Displacement [-1.0 to 1.0].
    /// @param[in] speed Speed ratio.
    /// @param[in] videoSourceToken Optional token.
    void focusRelative(float distance, float speed = 1.0f, const QString& videoSourceToken = QString());

    /// @brief Queries saved optical imaging presets.
    /// @param[in] videoSourceToken Optional token.
    void refreshImagingPresets(const QString& videoSourceToken = QString());

    /// @brief Recalls saved optical imaging preset.
    /// @param[in] presetToken Preset token.
    /// @param[in] videoSourceToken Optional token.
    /// @return True on success.
    bool setCurrentImagingPreset(const QString& presetToken, const QString& videoSourceToken = QString());

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
    QString createOSD(const Onvif::OsdConfig& osd);

    /// @brief Modifies an existing OSD overlay.
    /// @param[in] osd Updated OSD configuration.
    /// @return True on success.
    bool setOSD(const Onvif::OsdConfig& osd);

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
    bool createUser(const Onvif::OnvifUser& user);

    /// @brief Updates an existing ONVIF user's password and/or role.
    /// @param[in] user Updated user parameters.
    /// @return True on success.
    bool setUser(const Onvif::OnvifUser& user);

    /// @brief Deletes ONVIF user account by username.
    /// @param[in] username Username to delete.
    /// @return True on success.
    bool deleteUser(const QString& username);

    /// @brief Queries network adapter interface configurations.
    void refreshNetworkInterfaces();

    /// @brief Updates network interface configuration.
    /// @param[in] config Updated interface settings.
    /// @return True on success.
    bool setNetworkInterface(const Onvif::NetworkInterfaceConfig& config);

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
    bool setDNS(const Onvif::DnsConfig& dns);

    /// @brief Queries NTP server configuration.
    void refreshNTP();

    /// @brief Sets NTP server configuration.
    /// @param[in] ntp Updated NTP settings.
    /// @return True on success.
    bool setNTP(const Onvif::NtpConfig& ntp);

    /// @brief Updates camera system date, time, and timezone.
    /// @param[in] dt Date/time parameters.
    /// @return True on success.
    bool setSystemDateAndTime(const Onvif::SystemDateTimeConfig& dt);

    /// @brief Resets device to factory default settings.
    /// @param[in] hard True for hard reset, false for soft.
    /// @return True on success.
    bool setSystemFactoryDefault(bool hard = false);

    // =========================================================================
    // Profile S/T: Device I/O & Relay Outputs
    // =========================================================================

    /// @brief Queries list of configured relay outputs from camera.
    void refreshRelayOutputs();

    /// @brief Changes the logical state of a relay output.
    /// @param[in] relayToken Relay token (e.g. "Relay_1").
    /// @param[in] active True to activate, false to deactivate.
    /// @return True on success.
    bool setRelayOutputState(const QString& relayToken, bool active);

    /// @brief Configures relay parameters (mode, delay time, idle state).
    /// @param[in] relayToken Relay token.
    /// @param[in] settings Updated relay configuration.
    /// @return True on success.
    bool setRelayOutputSettings(const QString& relayToken, const Onvif::RelayOutputConfig& settings);

    /// @brief Queries list of configured digital inputs from camera.
    void refreshDigitalInputs();

    // =========================================================================
    // Profile T/M: Metadata Stream & Video Analytics
    // =========================================================================

    /// @brief Queries list of metadata configurations on the device.
    void refreshMetadataConfigurations();

    /// @brief Modifies an existing metadata configuration.
    /// @param[in] config Updated configuration.
    /// @return True on success.
    bool setMetadataConfiguration(const Onvif::MetadataConfiguration& config);

    /// @brief Starts background metadata polling/streaming loop.
    /// @param[in] intervalMs Polling interval in milliseconds.
    void startMetadataStreaming(int intervalMs = 1000);

    /// @brief Stops background metadata streaming loop.
    void stopMetadataStreaming();

    /// @brief Checks whether metadata streaming is active.
    [[nodiscard]] bool isMetadataStreamingActive() const;

    /// @brief Polls and emits current metadata snapshot once.
    void pollCurrentMetadata();

    // =========================================================================
    // Device Management: System Logs & Maintenance
    // =========================================================================

    /// @brief Queries system or access logs from camera.
    /// @param[in] logType Log type (System or Access).
    void fetchSystemLog(Onvif::SystemLogType logType = Onvif::SystemLogType::System);

    /// @brief Queries detailed system diagnostics and support information.
    void fetchSystemSupportInformation();

    /// @brief Downloads a system configuration backup archive.
    void downloadSystemBackup();

    /// @brief Restores system configuration using a backup archive payload.
    /// @param[in] backupData Backup archive payload.
    /// @return True on success.
    bool restoreSystem(const QString& backupData);

    /// @brief Queries unique endpoint reference GUID from device.
    void fetchEndpointReference();

    // =========================================================================
    // PKI Certificates & HTTPS/TLS Security Service
    // =========================================================================

    /// @brief Queries list of installed X.509 certificates from camera.
    void refreshCertificates();

    /// @brief Queries detailed information for a specific certificate ID.
    /// @param[in] certificateId Certificate token.
    void fetchCertificateInformation(const QString& certificateId);

    /// @brief Requests camera to create a self-signed X.509 certificate.
    /// @param[in] certificateId Certificate token.
    /// @param[in] subject Subject distinguished name.
    /// @param[in] daysValid Validity period in days.
    /// @return True on success.
    bool createCertificate(const QString& certificateId, const QString& subject, int daysValid = 365);

    /// @brief Requests camera to generate a PKCS#10 Certificate Signing Request (CSR).
    /// @param[in] certificateId Certificate token.
    /// @param[in] subject Subject distinguished name.
    /// @return True on success.
    bool createPkcs10Csr(const QString& certificateId, const QString& subject);

    /// @brief Uploads signed X.509 certificates to camera.
    /// @param[in] certificates List of certificates.
    /// @return True on success.
    bool loadCertificates(const std::vector<Onvif::OnvifCertificate>& certificates);

    /// @brief Deletes certificates from camera by token IDs.
    /// @param[in] certificateIds List of certificate IDs.
    /// @return True on success.
    bool deleteCertificates(const QStringList& certificateIds);

    /// @brief Queries TLS client certificate authentication mode.
    void refreshClientCertificateMode();

    /// @brief Sets TLS client certificate authentication mode.
    /// @param[in] mode Desired ClientCertificateMode.
    /// @return True on success.
    bool setClientCertificateMode(Onvif::ClientCertificateMode mode);

    // =========================================================================
    // Profile G: Recording Service
    // =========================================================================

    /// @brief Queries list of edge recordings on camera.
    void refreshRecordings();

    /// @brief Creates a new recording storage container on camera.
    /// @param[in] config Recording container configuration.
    /// @return Assigned recordingToken or empty on failure.
    QString createRecording(const Onvif::RecordingConfig& config);

    /// @brief Updates configuration of an existing recording container.
    /// @param[in] config Updated recording configuration.
    /// @return True on success.
    bool setRecordingConfiguration(const Onvif::RecordingConfig& config);

    /// @brief Deletes a recording container and its stored data.
    /// @param[in] recordingToken Target recording token.
    /// @return True on success.
    bool deleteRecording(const QString& recordingToken);

    /// @brief Adds a track to a recording container.
    /// @param[in] recordingToken Parent recording token.
    /// @param[in] track Track parameters.
    /// @return Assigned trackToken or empty on failure.
    QString createTrack(const QString& recordingToken, const Onvif::RecordingTrack& track);

    /// @brief Deletes a track from a recording container.
    /// @param[in] recordingToken Parent recording token.
    /// @param[in] trackToken Track token to delete.
    /// @return True on success.
    bool deleteTrack(const QString& recordingToken, const QString& trackToken);

    /// @brief Queries list of automated recording jobs.
    void refreshRecordingJobs();

    /// @brief Creates an automated recording job binding a source to a recording.
    /// @param[in] job Job configuration.
    /// @return Assigned jobToken or empty on failure.
    QString createRecordingJob(const Onvif::RecordingJob& job);

    /// @brief Sets the operational mode of a recording job (Active vs Idle).
    /// @param[in] jobToken Target job token.
    /// @param[in] mode Desired RecordingJobMode.
    /// @return True on success.
    bool setRecordingJobMode(const QString& jobToken, Onvif::RecordingJobMode mode);

    /// @brief Deletes a recording job.
    /// @param[in] jobToken Target job token.
    /// @return True on success.
    bool deleteRecordingJob(const QString& jobToken);

    /// @brief Queries overall storage and time range summary for recordings.
    void refreshRecordingSummary();

    // =========================================================================
    // Profile G: Search Service
    // =========================================================================

    /// @brief Initiates historical recording search query.
    /// @param[in] scope Search scope.
    /// @param[in] maxMatches Maximum results.
    /// @return Search session token or empty on failure.
    QString findRecordings(const QString& scope = QString(), int maxMatches = 10);

    /// @brief Polls results for an active recording search query.
    /// @param[in] searchToken Search session token.
    void refreshRecordingSearchResults(const QString& searchToken);

    /// @brief Initiates historical recorded events search query.
    /// @param[in] startUtc Start timestamp (ISO 8601 UTC).
    /// @param[in] endUtc End timestamp (ISO 8601 UTC).
    /// @param[in] maxMatches Maximum matches.
    /// @return Search session token or empty on failure.
    QString findEvents(const QString& startUtc, const QString& endUtc = QString(), int maxMatches = 10);

    /// @brief Polls results for an active event search query.
    /// @param[in] searchToken Search session token.
    void refreshEventSearchResults(const QString& searchToken);

    /// @brief Closes an active search query session.
    /// @param[in] searchToken Search session token.
    /// @return True on success.
    bool endSearch(const QString& searchToken);

    // =========================================================================
    // Profile G: Replay Service
    // =========================================================================

    /// @brief Resolves RTSP replay URI for playback of a recorded track.
    /// @param[in] recordingToken Target recording token.
    /// @param[in] streamType Stream transport type (e.g. "RTP-Unicast").
    void resolveReplayUri(const QString& recordingToken, const QString& streamType = "RTP-Unicast");

    /// @brief Queries current replay session parameters.
    void refreshReplayConfiguration();

    /// @brief Configures replay session timeouts.
    /// @param[in] config Desired replay configuration.
    /// @return True on success.
    bool setReplayConfiguration(const Onvif::ReplayConfiguration& config);

    // =========================================================================
    // Profile M & Profile T: Video Analytics Rule Engine & Modules
    // =========================================================================

    /// @brief Queries supported video analytics rules descriptions.
    void refreshSupportedRules();

    /// @brief Queries active video analytics rules from camera.
    void refreshRules();

    /// @brief Creates new video analytics rules.
    /// @param[in] rules List of rules to create.
    /// @return True on success.
    bool createRules(const std::vector<Onvif::AnalyticsRule>& rules);

    /// @brief Modifies existing video analytics rules.
    /// @param[in] rules List of updated rules.
    /// @return True on success.
    bool modifyRules(const std::vector<Onvif::AnalyticsRule>& rules);

    /// @brief Deletes video analytics rules by name.
    /// @param[in] ruleNames List of rule names to delete.
    /// @return True on success.
    bool deleteRules(const QStringList& ruleNames);

    /// @brief Queries supported video analytics modules descriptions.
    void refreshSupportedAnalyticsModules();

    /// @brief Queries active video analytics modules from camera.
    void refreshAnalyticsModules();

    /// @brief Creates new video analytics modules.
    /// @param[in] modules List of modules to create.
    /// @return True on success.
    bool createAnalyticsModules(const std::vector<Onvif::AnalyticsModule>& modules);

    /// @brief Modifies existing video analytics modules.
    /// @param[in] modules List of updated modules.
    /// @return True on success.
    bool modifyAnalyticsModules(const std::vector<Onvif::AnalyticsModule>& modules);

    /// @brief Deletes video analytics modules by name.
    /// @param[in] moduleNames List of module names to delete.
    /// @return True on success.
    bool deleteAnalyticsModules(const QStringList& moduleNames);

    // =========================================================================
    // Profile T: Privacy Masks & Video Source Modes
    // =========================================================================

    /// @brief Queries list of privacy masks configured on the device.
    /// @param[in] configToken Optional configuration token filter.
    void refreshMasks(const QString& configToken = QString());

    /// @brief Queries privacy mask configuration limits and options.
    /// @param[in] configToken VideoSourceConfiguration token.
    void refreshMaskOptions(const QString& configToken = "VideoSourceConfig_1");

    /// @brief Creates a new privacy mask on the camera.
    /// @param[in] mask Privacy mask definition.
    /// @return Created mask token on success, empty on failure.
    QString createMask(const Onvif::PrivacyMask& mask);

    /// @brief Updates an existing privacy mask on the camera.
    /// @param[in] mask Updated mask definition.
    /// @return True on success.
    bool setMask(const Onvif::PrivacyMask& mask);

    /// @brief Deletes a privacy mask by token.
    /// @param[in] maskToken Mask identifier token.
    /// @return True on success.
    bool deleteMask(const QString& maskToken);

    /// @brief Queries supported video capture modes from camera.
    /// @param[in] videoSourceToken Video source token.
    void refreshVideoSourceModes(const QString& videoSourceToken = "VideoSource_1");

    /// @brief Switches camera sensor capture mode.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] modeToken Desired mode token (e.g. "Mode_4K30").
    /// @return True if mode switch was accepted.
    bool setVideoSourceMode(const QString& videoSourceToken, const QString& modeToken);

    /// @brief Queries radiometric compensation parameters from camera.
    /// @param[in] videoSourceToken Video source token.
    void refreshRadiometryConfiguration(const QString& videoSourceToken = "VideoSource_1");

    /// @brief Updates radiometric compensation parameters on camera.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] config Updated configuration.
    /// @return True on success.
    bool setRadiometryConfiguration(const QString& videoSourceToken, const Onvif::RadiometryConfig& config);
    bool setRadiometryConfiguration(const Onvif::RadiometryConfig& config)
    {
        return setRadiometryConfiguration("VideoSource_1", config);
    }

    /// @brief Queries radiometric spotmeters and measurement boxes.
    /// @param[in] videoSourceToken Video source token.
    void refreshRadiometryMeasurements(const QString& videoSourceToken = "VideoSource_1");

    /// @brief Updates or replaces radiometric spotmeters.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] spots Vector of spots.
    /// @return True on success.
    bool setRadiometrySpots(const QString& videoSourceToken, const std::vector<Onvif::RadiometrySpot>& spots);
    bool setRadiometrySpots(const std::vector<Onvif::RadiometrySpot>& spots)
    {
        return setRadiometrySpots("VideoSource_1", spots);
    }

    /// @brief Updates or replaces radiometric measurement boxes.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] boxes Vector of boxes.
    /// @return True on success.
    bool setRadiometryBoxes(const QString& videoSourceToken, const std::vector<Onvif::RadiometryBox>& boxes);
    bool setRadiometryBoxes(const std::vector<Onvif::RadiometryBox>& boxes)
    {
        return setRadiometryBoxes("VideoSource_1", boxes);
    }

    /// @brief Queries available thermal false-color palettes.
    /// @param[in] videoSourceToken Video source token.
    void refreshColorPalettes(const QString& videoSourceToken = "VideoSource_1");

    /// @brief Switches active thermal false-color palette.
    /// @param[in] videoSourceToken Video source token.
    /// @param[in] paletteToken Palette token (e.g. "Ironbow").
    /// @return True on success.
    bool setColorPalette(const QString& videoSourceToken, const QString& paletteToken);
    bool setColorPalette(const QString& paletteToken)
    {
        return setColorPalette("VideoSource_1", paletteToken);
    }

    /// @brief Triggers Non-Uniformity Correction (NUC / shutter calibration).
    /// @param[in] videoSourceToken Video source token.
    /// @return True on success.
    bool triggerNuc(const QString& videoSourceToken = "VideoSource_1");

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
    void statusUpdated(const Onvif::PtzStatus& status);

    /// @brief Emitted when preset list is refreshed.
    /// @param[in] presets List of camera presets.
    void presetsUpdated(const std::vector<Onvif::PtzPreset>& presets);

    /// @brief Emitted when camera geographic location/orientation is retrieved.
    /// @param[in] location Configured LocationEntity.
    void geoLocationUpdated(const Onvif::LocationEntity& location);

    /// @brief Emitted when GeoMove command completes.
    /// @param[in] success True if command acknowledged.
    void geoMoveCompleted(bool success);

    /// @brief Emitted when preset tours list is refreshed.
    /// @param[in] tours List of camera preset tours.
    void presetToursUpdated(const std::vector<Onvif::PresetTour>& tours);

    /// @brief Emitted when reboot request completes.
    /// @param[in] success True if reboot was accepted.
    void rebootCompleted(bool success);

    /// @brief Emitted when optical imaging settings are refreshed.
    /// @param[in] settings Current camera imaging settings.
    void imagingSettingsUpdated(const Onvif::ImagingSettings& settings);

    /// @brief Emitted when an auxiliary command finishes.
    /// @param[in] success True if accepted.
    /// @param[in] response Server response payload.
    void auxiliaryCommandCompleted(bool success, const QString& response);

    /// @brief Emitted when an event notification message is pulled.
    /// @param[in] event Event notification details.
    void eventReceived(const Onvif::OnvifEvent& event);

    /// @brief Emitted when OSD overlay configuration list is refreshed.
    /// @param[in] osds List of camera OSD configurations.
    void osdsUpdated(const std::vector<Onvif::OsdConfig>& osds);

    /// @brief Emitted when ONVIF users list is refreshed.
    /// @param[in] users List of camera user accounts.
    void usersUpdated(const std::vector<Onvif::OnvifUser>& users);

    /// @brief Emitted when network interface configuration is refreshed.
    /// @param[in] ifaces List of network interface configs.
    void networkInterfacesUpdated(const std::vector<Onvif::NetworkInterfaceConfig>& ifaces);

    /// @brief Emitted when default gateway is refreshed.
    /// @param[in] gateway Gateway address string.
    void networkGatewayUpdated(const QString& gateway);

    /// @brief Emitted when DNS configuration is refreshed.
    /// @param[in] dns Current DNS settings.
    void dnsUpdated(const Onvif::DnsConfig& dns);

    /// @brief Emitted when NTP configuration is refreshed.
    /// @param[in] ntp Current NTP settings.
    void ntpUpdated(const Onvif::NtpConfig& ntp);

    /// @brief Emitted when factory default reset command finishes.
    /// @param[in] success True if accepted.
    void factoryDefaultCompleted(bool success);

    /// @brief Emitted when optical focus status is refreshed.
    /// @param[in] status Current focus status.
    void focusStatusUpdated(const Onvif::FocusStatus20& status);

    /// @brief Emitted when optical imaging presets are refreshed.
    /// @param[in] presets List of camera imaging presets.
    void imagingPresetsUpdated(const std::vector<Onvif::ImagingPreset>& presets);

    /// @brief Emitted when relay outputs list is refreshed.
    /// @param[in] relays List of camera relay outputs.
    void relayOutputsUpdated(const std::vector<Onvif::RelayOutputConfig>& relays);

    /// @brief Emitted when digital inputs list is refreshed.
    /// @param[in] inputs List of camera digital inputs.
    void digitalInputsUpdated(const std::vector<Onvif::DigitalInputConfig>& inputs);

    /// @brief Emitted when metadata configurations list is refreshed.
    /// @param[in] configs List of metadata configurations.
    void metadataConfigurationsUpdated(const std::vector<Onvif::MetadataConfiguration>& configs);

    /// @brief Emitted when a metadata stream packet (Profile T/M) is received.
    /// @param[in] payload Metadata stream payload.
    void metadataReceived(const Onvif::MetadataStreamPayload& payload);

    /// @brief Emitted when system or access log content is retrieved.
    /// @param[in] logType Log type.
    /// @param[in] logData Text content of log.
    void systemLogReceived(Onvif::SystemLogType logType, const QString& logData);

    /// @brief Emitted when system support information is retrieved.
    /// @param[in] info Diagnostics support structure.
    void systemSupportInfoReceived(const Onvif::SystemSupportInfo& info);

    /// @brief Emitted when system backup archive is retrieved.
    /// @param[in] backupData Backup archive payload.
    void systemBackupReceived(const QString& backupData);

    /// @brief Emitted when restore system command completes.
    /// @param[in] success True on success.
    void systemRestoreCompleted(bool success);

    /// @brief Emitted when endpoint reference is retrieved.
    /// @param[in] endpointReference GUID string.
    void endpointReferenceReceived(const QString& endpointReference);

    /// @brief Emitted when X.509 certificates list is refreshed.
    /// @param[in] certs List of certificates.
    void certificatesUpdated(const std::vector<Onvif::OnvifCertificate>& certs);

    /// @brief Emitted when detailed certificate information is retrieved.
    /// @param[in] info Certificate information.
    void certificateInfoReceived(const Onvif::CertificateInformation& info);

    /// @brief Emitted when a PKCS#10 CSR is generated.
    /// @param[in] csr PKCS#10 CSR request object.
    void pkcs10CsrReceived(const Onvif::Pkcs10Request& csr);

    /// @brief Emitted when client certificate authentication mode is updated.
    /// @param[in] mode Current ClientCertificateMode.
    void clientCertificateModeUpdated(Onvif::ClientCertificateMode mode);

    /// @brief Emitted when edge recordings list is refreshed.
    /// @param[in] recordings List of recordings.
    void recordingsUpdated(const std::vector<Onvif::RecordingConfig>& recordings);

    /// @brief Emitted when recording jobs list is refreshed.
    /// @param[in] jobs List of recording jobs.
    void recordingJobsUpdated(const std::vector<Onvif::RecordingJob>& jobs);

    /// @brief Emitted when recording storage summary is refreshed.
    /// @param[in] summary Recording summary structure.
    void recordingSummaryUpdated(const Onvif::RecordingSummary& summary);

    /// @brief Emitted when recording search query results are retrieved.
    /// @param[in] searchToken Search session token.
    /// @param[in] results Vector of search results.
    void recordingSearchResultsReceived(
        const QString& searchToken, const std::vector<Onvif::RecordingSearchResult>& results);

    /// @brief Emitted when recorded event search query results are retrieved.
    /// @param[in] searchToken Search session token.
    /// @param[in] results Vector of event search results.
    void eventSearchResultsReceived(const QString& searchToken, const std::vector<Onvif::RecordedEventResult>& results);

    /// @brief Emitted when RTSP replay URI is resolved.
    /// @param[in] recordingToken Recording token.
    /// @param[in] uri Replay RTSP URI string.
    void replayUriResolved(const QString& recordingToken, const QString& uri);

    /// @brief Emitted when replay configuration is updated.
    /// @param[in] config Replay configuration.
    void replayConfigurationUpdated(const Onvif::ReplayConfiguration& config);

    /// @brief Emitted when video analytics rules list is refreshed.
    /// @param[in] rules List of rules.
    void rulesUpdated(const std::vector<Onvif::AnalyticsRule>& rules);

    /// @brief Emitted when supported video analytics rules are refreshed.
    /// @param[in] rules List of supported rule descriptions.
    void supportedRulesUpdated(const std::vector<Onvif::AnalyticsRuleDescription>& rules);

    /// @brief Emitted when video analytics modules list is refreshed.
    /// @param[in] modules List of modules.
    void analyticsModulesUpdated(const std::vector<Onvif::AnalyticsModule>& modules);

    /// @brief Emitted when supported video analytics modules are refreshed.
    /// @param[in] modules List of supported module descriptions.
    void supportedAnalyticsModulesUpdated(const std::vector<Onvif::AnalyticsModuleDescription>& modules);

    /// @brief Emitted when privacy masks list is refreshed.
    /// @param[in] masks List of configured privacy masks.
    void masksUpdated(const std::vector<Onvif::PrivacyMask>& masks);

    /// @brief Emitted when privacy mask options are retrieved.
    /// @param[in] options Supported mask limits and features.
    void maskOptionsUpdated(const Onvif::MaskOptions& options);

    /// @brief Emitted when video source capture modes list is refreshed.
    /// @param[in] modes List of video source capture modes.
    void videoSourceModesUpdated(const std::vector<Onvif::VideoSourceMode>& modes);

    /// @brief Emitted when a video source mode switch completes.
    /// @param[in] modeToken Activated mode token.
    /// @param[in] rebootRequired True if device requires reboot.
    void videoSourceModeChanged(const QString& modeToken, bool rebootRequired);

    /// @brief Emitted when radiometric configuration is retrieved.
    /// @param[in] config Radiometric compensation configuration.
    void radiometryConfigurationUpdated(const Onvif::RadiometryConfig& config);

    /// @brief Emitted when radiometric spotmeters list is refreshed.
    /// @param[in] spots List of spotmeters.
    void radiometrySpotsUpdated(const std::vector<Onvif::RadiometrySpot>& spots);

    /// @brief Emitted when radiometric measurement boxes list is refreshed.
    /// @param[in] boxes List of measurement boxes.
    void radiometryBoxesUpdated(const std::vector<Onvif::RadiometryBox>& boxes);

    /// @brief Emitted when false-color palettes are refreshed.
    /// @param[in] palettes Available palettes.
    void colorPalettesUpdated(const std::vector<Onvif::ColorPalette>& palettes);

    /// @brief Emitted when NUC shutter calibration completes.
    /// @param[in] success True if command succeeded.
    void nucTriggered(bool success);

    /// @brief Emitted when an operation fails.
    /// @param[in] message Diagnostic error message.
    void errorOccurred(const QString& message);

    /// @brief Emitted when asynchronous discovery finishes.
    /// @param[in] devices List of discovered cameras.
    void discoveryFinished(const QList<Onvif::DiscoveredDevice>& devices);

private:
    void pollEvents();
    void pollMetadata();

    std::unique_ptr<Onvif::OnvifClient> m_client {};
    bool m_connected { false };
    QString m_endpoint {};
    QString m_activeProfileToken {};
    QString m_activeVideoSourceToken {};
    QString m_rtspStreamUri {};
    QString m_snapshotUri {};
    std::vector<Onvif::MediaProfile> m_profiles {};
    std::vector<Onvif::PtzPreset> m_presets {};
    std::vector<Onvif::PresetTour> m_presetTours {};
    std::vector<Onvif::OsdConfig> m_osds {};
    std::vector<Onvif::OnvifUser> m_users {};
    std::vector<Onvif::NetworkInterfaceConfig> m_networkInterfaces {};
    QString m_networkGateway {};
    Onvif::DnsConfig m_dnsConfig {};
    Onvif::NtpConfig m_ntpConfig {};
    Onvif::DeviceInformation m_deviceInfo {};
    Onvif::ImagingSettings m_imagingSettings {};
    Onvif::FocusStatus20 m_focusStatus {};
    std::vector<Onvif::ImagingPreset> m_imagingPresets {};
    std::vector<Onvif::RelayOutputConfig> m_relayOutputs {};
    std::vector<Onvif::DigitalInputConfig> m_digitalInputs {};
    QString m_eventSubscriptionUrl {};
    bool m_eventSubActive { false };
    std::vector<Onvif::MetadataConfiguration> m_metadataConfigs {};
    bool m_metadataStreamingActive { false };
    std::vector<Onvif::OnvifCertificate> m_certificates {};
    Onvif::ClientCertificateMode m_clientCertMode { Onvif::ClientCertificateMode::Off };
    std::vector<Onvif::RecordingConfig> m_recordings {};
    std::vector<Onvif::RecordingJob> m_recordingJobs {};
    std::optional<Onvif::RecordingSummary> m_recordingSummary {};
    std::optional<Onvif::ReplayConfiguration> m_replayConfig {};
    std::vector<Onvif::AnalyticsRule> m_rules {};
    std::vector<Onvif::AnalyticsRuleDescription> m_supportedRules {};
    std::vector<Onvif::AnalyticsModule> m_analyticsModules {};
    std::vector<Onvif::AnalyticsModuleDescription> m_supportedModules {};
    std::optional<Onvif::LocationEntity> m_geoLocation {};

    std::vector<Onvif::PrivacyMask> m_masks {};
    std::optional<Onvif::MaskOptions> m_maskOptions {};
    std::vector<Onvif::VideoSourceMode> m_videoSourceModes {};

    Onvif::RadiometryConfig m_radiometryConfig {};
    std::vector<Onvif::RadiometrySpot> m_radiometrySpots {};
    std::vector<Onvif::RadiometryBox> m_radiometryBoxes {};
    std::vector<Onvif::ColorPalette> m_colorPalettes {};
};

} // namespace PelcoD::Qt
