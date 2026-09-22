#pragma once

/// @file OnvifCameraTab.h
/// @brief Dashboard tab for discovering, connecting to, and controlling ONVIF Profile S IP cameras.

#include "QOnvifDevice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace PelcoDApp {

class VideoStreamTab;

/// @class OnvifCameraTab
/// @brief Dashboard tab providing ONVIF discovery, profile selection, PTZ, and preset management.
class OnvifCameraTab : public QWidget {
    Q_OBJECT

public:
    explicit OnvifCameraTab(
        PelcoD::Qt::QOnvifDevice* onvifDevice, VideoStreamTab* videoTab = nullptr, QWidget* parent = nullptr);
    ~OnvifCameraTab() override = default;

signals:
    void streamUriSelected(const QString& rtspUri);

private slots:
    // Discovery & Connection
    void handleStartDiscovery();
    void handleDiscoveryFinished(const QList<Onvif::DiscoveredDevice>& devices);
    void handleSelectDiscovered(int index);
    void handleConnect();
    void handleDisconnect();
    void handleDeviceConnected(const QString& endpoint, const QString& model);
    void handleDeviceDisconnected();
    void handleErrorOccurred(const QString& message);

    // Profiles & Video
    void handleProfileSelected(int index);
    void handleStreamUriResolved(const QString& uri);
    void handleSnapshotUriResolved(const QString& uri);
    void handleSendToVideoTab();
    void handleCopyRtsp();

    // Motion & Presets
    void handleContinuousPanTilt(double pan, double tilt);
    void handleContinuousZoom(int dir);
    void handleStopMotion();
    void handleRelativeMove();
    void handleGotoHome();
    void handleSetHome();
    void handleSendWiperOn();
    void handleSendWiperOff();
    void handleSendWasher();
    void handleSendIrOn();
    void handleSendIrOff();
    void handleSendCustomAux();
    void handleAuxiliaryCompleted(bool success, const QString& response);
    void handleRefreshPresets();
    void handleGotoPreset();
    void handleSavePreset();
    void handleDeletePreset();
    void handlePresetsUpdated(const std::vector<Onvif::PtzPreset>& presets);
    void handleStatusUpdated(const Onvif::PtzStatus& status);
    void handleRefreshGeoLocation();
    void handleSaveGeoLocation();
    void handleExecuteGeoMove();
    void handleExecuteAbsoluteSpherical();
    void handleUpdateLiveGeoTargetReadout();
    void handleGeoLocationUpdated(const Onvif::LocationEntity& location);
    void handleGeoMoveCompleted(bool success);

    // Preset Tours / Patrols
    void handleRefreshTours();
    void handleTourSelected(int index);
    void handleStartTour();
    void handlePauseTour();
    void handleStopTour();
    void handleAddTourStep();
    void handleRemoveTourStep();
    void handleSaveTour();
    void handleToursUpdated(const std::vector<Onvif::PresetTour>& tours);

    // Maintenance
    void handleRebootCamera();
    void handleRebootCompleted(bool success);

    // Profile T: Imaging & Events
    void handleRefreshImaging();
    void handleApplyImaging();
    void handleFocusNear();
    void handleFocusFar();
    void handleFocusStop();
    void handleToggleEvents(bool enable);
    void handleClearEvents();
    void handleImagingSettingsUpdated(const Onvif::ImagingSettings& settings);
    void handleEventReceived(const Onvif::OnvifEvent& event);

    // Profile T: On-Screen Display (OSD)
    void handleRefreshOsds();
    void handleCreateOsd();
    void handleSetOsd();
    void handleDeleteOsd();
    void handleOsdsUpdated(const std::vector<Onvif::OsdConfig>& osds);
    void handleOsdSelectionChanged();

    // Profile T: Privacy Masks
    void handleRefreshMasks();
    void handleAddMask();
    void handleUpdateMask();
    void handleDeleteMask();
    void handleMasksUpdated(const std::vector<Onvif::PrivacyMask>& masks);
    void handleMaskSelectionChanged();

    // Profile T: Video Source Modes
    void handleRefreshVideoSourceModes();
    void handleApplyVideoSourceMode();
    void handleVideoSourceModesUpdated(const std::vector<Onvif::VideoSourceMode>& modes);
    void handleVideoSourceModeChanged(const QString& modeToken, bool rebootRequired);

    // Device Management: Users & Security
    void handleRefreshUsers();
    void handleAddUser();
    void handleUpdateUser();
    void handleDeleteUser();
    void handleUsersUpdated(const std::vector<Onvif::OnvifUser>& users);
    void handleUserSelectionChanged();

    // Device Management: Network & System
    void handleRefreshNetwork();
    void handleApplyNetwork();
    void handleRefreshDns();
    void handleApplyDns();
    void handleRefreshNtp();
    void handleApplyNtp();
    void handleSyncPcTime();
    void handleFactoryDefaultSoft();
    void handleFactoryDefaultHard();
    void handleNetworkUpdated(const std::vector<Onvif::NetworkInterfaceConfig>& ifaces);
    void handleGatewayUpdated(const QString& gateway);
    void handleDnsUpdated(const Onvif::DnsConfig& dns);
    void handleNtpUpdated(const Onvif::NtpConfig& ntp);
    void handleFactoryDefaultCompleted(bool success);

    // Profile T: Imaging Presets & Focus Status
    void handleRecallImagingPreset();
    void handleFocusStatusUpdated(const Onvif::FocusStatus20& status);
    void handleImagingPresetsUpdated(const std::vector<Onvif::ImagingPreset>& presets);

    // Profile S/T: Device I/O & Relay Outputs
    void handleRefreshRelays();
    void handleActivateRelay();
    void handleDeactivateRelay();
    void handleApplyRelaySettings();
    void handleRefreshInputs();
    void handleRelaysUpdated(const std::vector<Onvif::RelayOutputConfig>& relays);
    void handleDigitalInputsUpdated(const std::vector<Onvif::DigitalInputConfig>& inputs);
    void handleRelaySelectionChanged();

    // Profile T & M: Metadata & Analytics
    void handleRefreshMetadataConfigs();
    void handleApplyMetadataConfig();
    void handleToggleMetadataStream(bool start);
    void handlePollMetadataOnce();
    void handleMetadataConfigsUpdated(const std::vector<Onvif::MetadataConfiguration>& configs);
    void handleMetadataReceived(const Onvif::MetadataStreamPayload& payload);

    // Profile M & T: Video Analytics Rule Engine & Modules
    void handleRefreshRules();
    void handleAddRule();
    void handleDeleteRule();
    void handleRulesUpdated(const std::vector<Onvif::AnalyticsRule>& rules);
    void handleSupportedRulesUpdated(const std::vector<Onvif::AnalyticsRuleDescription>& rules);
    void handleRefreshAnalyticsModules();
    void handleAnalyticsModulesUpdated(const std::vector<Onvif::AnalyticsModule>& modules);

    // Maintenance & System Logs Extensions
    void handleFetchSystemLog();
    void handleFetchAccessLog();
    void handleSystemLogReceived(Onvif::SystemLogType logType, const QString& logData);
    void handleFetchSupportInfo();
    void handleSystemSupportInfoReceived(const Onvif::SystemSupportInfo& info);
    void handleDownloadBackup();
    void handleSystemBackupReceived(const QString& backupData);
    void handleRestoreBackup();
    void handleSystemRestoreCompleted(bool success);
    void handleFetchEndpointReference();
    void handleEndpointReferenceReceived(const QString& endpointReference);

    // Profile G: Recordings & Replay
    void handleRefreshRecordings();
    void handleCreateRecording();
    void handleDeleteRecording();
    void handleCreateTrack();
    void handleDeleteTrack();
    void handleRefreshRecordingJobs();
    void handleCreateRecordingJob();
    void handleToggleJobMode();
    void handleDeleteRecordingJob();
    void handleRefreshRecordingSummary();
    void handleFindRecordings();
    void handleFindEvents();
    void handleResolveReplayUri();
    void handlePlayInVideoStreamTab();
    void handleRecordingsUpdated(const std::vector<Onvif::RecordingConfig>& recordings);
    void handleRecordingJobsUpdated(const std::vector<Onvif::RecordingJob>& jobs);
    void handleRecordingSummaryUpdated(const Onvif::RecordingSummary& summary);
    void handleRecordingSearchResultsReceived(
        const QString& searchToken, const std::vector<Onvif::RecordingSearchResult>& results);
    void handleEventSearchResultsReceived(
        const QString& searchToken, const std::vector<Onvif::RecordedEventResult>& results);
    void handleReplayUriResolved(const QString& recordingToken, const QString& uri);

    // PKI Certificates & HTTPS/TLS Security
    void handleRefreshCertificates();
    void handleCreateSelfSignedCert();
    void handleGenerateCsr();
    void handleDeleteCertificate();
    void handleApplyClientCertMode();
    void handleCertificatesUpdated(const std::vector<Onvif::OnvifCertificate>& certs);
    void handleCertificateInfoReceived(const Onvif::CertificateInformation& info);
    void handlePkcs10CsrReceived(const Onvif::Pkcs10Request& csr);
    void handleClientCertModeUpdated(Onvif::ClientCertificateMode mode);

    // Thermal & Radiometry Service
    void handleRefreshRadiometry();
    void handleApplyRadiometry();
    void handleRefreshPalettes();
    void handleSetPalette();
    void handleTriggerNuc();
    void handleRefreshMeasurements();
    void handleAddMeasurement();
    void handleDeleteMeasurement();
    void handleRadiometryConfigUpdated(const Onvif::RadiometryConfig& config);
    void handleRadiometrySpotsUpdated(const std::vector<Onvif::RadiometrySpot>& spots);
    void handleRadiometryBoxesUpdated(const std::vector<Onvif::RadiometryBox>& boxes);
    void handleColorPalettesUpdated(const std::vector<Onvif::ColorPalette>& palettes);
    void handleNucTriggered(bool success);

private:
    void setupUi();
    void updateConnectionUi(bool connected);
    void initConnectionWidgets();
    void registerConnectionWidget(QWidget* w);

    /// @brief Returns true if m_onvifDevice is non-null. Used as a single-line guard in slots.
    [[nodiscard]] bool requireDevice() const noexcept
    {
        return m_onvifDevice != nullptr;
    }

    /// @brief Configures a QTableWidget with standard read-only, single-row-select settings.
    static void configureTable(QTableWidget* table, const QStringList& headers);

    /// @brief Maps a cmbOsdPosition combo index to the corresponding OsdPositionType enum value.
    [[nodiscard]] static Onvif::OsdPositionType osdPositionFromIndex(int index) noexcept;

    PelcoD::Qt::QOnvifDevice* m_onvifDevice { nullptr };
    VideoStreamTab* m_videoTab { nullptr };
    QList<Onvif::DiscoveredDevice> m_discoveredList {};

    QList<QWidget*> m_connectionWidgets;

    QTabWidget* m_cameraTabs { nullptr };

    // Discovery & Connection widgets
    QPushButton* btnDiscover { nullptr };
    QComboBox* cmbDiscovered { nullptr };
    QLineEdit* editEndpoint { nullptr };
    QLineEdit* editUsername { nullptr };
    QLineEdit* editPassword { nullptr };
    QPushButton* btnConnect { nullptr };
    QPushButton* btnDisconnect { nullptr };
    QLabel* lblConnectionStatus { nullptr };

    // Device Information widgets
    QLabel* lblManufacturer { nullptr };
    QLabel* lblModel { nullptr };
    QLabel* lblFirmware { nullptr };
    QLabel* lblSerial { nullptr };
    QLabel* lblHardwareId { nullptr };

    // Media & Streaming widgets
    QComboBox* cmbProfiles { nullptr };
    QLineEdit* editRtspUri { nullptr };
    QPushButton* btnCopyRtsp { nullptr };
    QPushButton* btnStreamInVideoTab { nullptr };
    QLineEdit* editSnapshotUri { nullptr };

    // PTZ Controls
    QSlider* sliderSpeed { nullptr };
    QLabel* lblSpeedVal { nullptr };
    QDoubleSpinBox* spinRelPan { nullptr };
    QDoubleSpinBox* spinRelTilt { nullptr };
    QDoubleSpinBox* spinRelZoom { nullptr };
    QPushButton* btnRelMove { nullptr };
    QPushButton* btnGotoHome = nullptr;
    QPushButton* btnSetHome = nullptr;
    QPushButton* btnWiperOn = nullptr;
    QPushButton* btnWiperOff = nullptr;
    QPushButton* btnWasher = nullptr;
    QPushButton* btnIrOn = nullptr;
    QPushButton* btnIrOff = nullptr;
    QLineEdit* editCustomAux = nullptr;
    QPushButton* btnSendAux = nullptr;
    QLabel* lblTelemetryPanTilt = nullptr;
    QLabel* lblTelemetryZoom { nullptr };
    QLabel* lblTelemetryMoving { nullptr };

    // Presets
    QTableWidget* tablePresets { nullptr };
    QLineEdit* editPresetName { nullptr };
    QPushButton* btnRefreshPresets { nullptr };
    QPushButton* btnGotoPreset { nullptr };
    QPushButton* btnSavePreset { nullptr };
    QPushButton* btnDeletePreset { nullptr };

    // Preset Tours / Patrols
    QComboBox* cmbPresetTours { nullptr };
    QPushButton* btnRefreshTours { nullptr };
    QPushButton* btnStartTour { nullptr };
    QPushButton* btnPauseTour { nullptr };
    QPushButton* btnStopTour { nullptr };
    QLabel* lblTourStatus { nullptr };
    QTableWidget* tableTourSpots { nullptr };
    QPushButton* btnAddTourStep { nullptr };
    QPushButton* btnRemoveTourStep { nullptr };
    QPushButton* btnSaveTour { nullptr };

    // Profile T: Optical & Imaging widgets
    QSlider* sliderBrightness { nullptr };
    QLabel* lblBrightnessVal { nullptr };
    QSlider* sliderContrast { nullptr };
    QLabel* lblContrastVal { nullptr };
    QSlider* sliderSaturation { nullptr };
    QLabel* lblSaturationVal { nullptr };
    QSlider* sliderSharpness { nullptr };
    QLabel* lblSharpnessVal { nullptr };
    QComboBox* cmbIrFilter { nullptr };
    QCheckBox* chkBacklight { nullptr };
    QCheckBox* chkWdr { nullptr };
    QComboBox* cmbAutoFocus { nullptr };
    QPushButton* btnFocusNear { nullptr };
    QPushButton* btnFocusFar { nullptr };
    QLabel* lblFocusStatus { nullptr };
    QComboBox* cmbImagingPresets { nullptr };
    QPushButton* btnRecallImagingPreset { nullptr };
    QPushButton* btnRefreshImaging { nullptr };
    QPushButton* btnApplyImaging { nullptr };

    // Profile S/T: Device I/O & Relay Outputs widgets
    QTableWidget* tableRelays { nullptr };
    QLineEdit* editRelayToken { nullptr };
    QComboBox* cmbRelayMode { nullptr };
    QDoubleSpinBox* spinRelayDelay { nullptr };
    QComboBox* cmbRelayIdleState { nullptr };
    QPushButton* btnRefreshRelays { nullptr };
    QPushButton* btnActivateRelay { nullptr };
    QPushButton* btnDeactivateRelay { nullptr };
    QPushButton* btnApplyRelaySettings { nullptr };
    QTableWidget* tableDigitalInputs { nullptr };
    QPushButton* btnRefreshInputs { nullptr };

    // Profile T: Live Events widgets
    QPushButton* btnToggleEvents { nullptr };
    QTableWidget* tableEvents { nullptr };
    QPushButton* btnClearEvents { nullptr };

    // Profile T: On-Screen Display (OSD) widgets
    QTableWidget* tableOsds { nullptr };
    QLineEdit* editOsdText { nullptr };
    QComboBox* cmbOsdPosition { nullptr };
    QCheckBox* chkOsdDateTime { nullptr };
    QSpinBox* spinOsdFontSize { nullptr };
    QPushButton* btnRefreshOsds { nullptr };
    QPushButton* btnAddOsd { nullptr };
    QPushButton* btnUpdateOsd { nullptr };
    QPushButton* btnDeleteOsd { nullptr };

    // Profile T: Privacy Masks widgets
    QTableWidget* tableMasks { nullptr };
    QLineEdit* editMaskToken { nullptr };
    QComboBox* cmbMaskType { nullptr };
    QSpinBox* spinMaskColorR { nullptr };
    QSpinBox* spinMaskColorG { nullptr };
    QSpinBox* spinMaskColorB { nullptr };
    QCheckBox* chkMaskEnabled { nullptr };
    QPushButton* btnRefreshMasks { nullptr };
    QPushButton* btnAddMask { nullptr };
    QPushButton* btnUpdateMask { nullptr };
    QPushButton* btnDeleteMask { nullptr };

    // Profile T: Video Source Modes widgets
    QComboBox* cmbVideoSourceModes { nullptr };
    QLabel* lblVideoSourceModeInfo { nullptr };
    QPushButton* btnRefreshVideoSourceModes { nullptr };
    QPushButton* btnApplyVideoSourceMode { nullptr };

    // Maintenance
    QPushButton* btnReboot { nullptr };

    // Device Management: Users & Security widgets
    QTableWidget* tableUsers { nullptr };
    QLineEdit* editUserUsername { nullptr };
    QLineEdit* editUserPassword { nullptr };
    QComboBox* cmbUserLevel { nullptr };
    QPushButton* btnRefreshUsers { nullptr };
    QPushButton* btnAddUser { nullptr };
    QPushButton* btnUpdateUser { nullptr };
    QPushButton* btnDeleteUser { nullptr };

    // Device Management: Network & System widgets
    QLineEdit* editNetToken { nullptr };
    QCheckBox* chkNetEnabled { nullptr };
    QCheckBox* chkNetDhcp { nullptr };
    QLineEdit* editNetIp { nullptr };
    QSpinBox* spinNetPrefix { nullptr };
    QLineEdit* editNetGateway { nullptr };
    QPushButton* btnRefreshNetwork { nullptr };
    QPushButton* btnApplyNetwork { nullptr };

    QCheckBox* chkDnsDhcp { nullptr };
    QLineEdit* editDnsServers { nullptr };
    QPushButton* btnRefreshDns { nullptr };
    QPushButton* btnApplyDns { nullptr };

    QCheckBox* chkNtpDhcp { nullptr };
    QLineEdit* editNtpServers { nullptr };
    QPushButton* btnRefreshNtp { nullptr };
    QPushButton* btnApplyNtp { nullptr };

    QPushButton* btnSyncPcTime { nullptr };
    QPushButton* btnFactoryDefaultSoft { nullptr };
    QPushButton* btnFactoryDefaultHard { nullptr };

    // Enhanced Network & Maintenance widgets (Sub-Tab 8)
    QPushButton* btnFetchSystemLog { nullptr };
    QPushButton* btnFetchAccessLog { nullptr };
    QTextEdit* txtSystemLogs { nullptr };
    QPushButton* btnFetchSupportInfo { nullptr };
    QPushButton* btnDownloadBackup { nullptr };
    QPushButton* btnRestoreBackup { nullptr };
    QLineEdit* editBackupPayload { nullptr };
    QPushButton* btnFetchEndpointRef { nullptr };
    QLabel* lblEndpointRef { nullptr };

    // Profile T & M: Metadata & Analytics widgets (Sub-Tab 10)
    QComboBox* cmbMetaConfigs { nullptr };
    QCheckBox* chkMetaPtzStatus { nullptr };
    QCheckBox* chkMetaAnalytics { nullptr };
    QCheckBox* chkMetaEvents { nullptr };
    QCheckBox* chkMetaGeo { nullptr };
    QPushButton* btnRefreshMetaConfigs { nullptr };
    QPushButton* btnApplyMetaConfig { nullptr };
    QPushButton* btnToggleMetaStream { nullptr };
    QPushButton* btnPollMetaOnce { nullptr };
    QLabel* lblMetaTelemetry { nullptr };
    QTableWidget* tableMetaObjects { nullptr };

    // Profile M & T: Video Analytics Rule Engine widgets
    QTableWidget* tableRules { nullptr };
    QPushButton* btnRefreshRules { nullptr };
    QLineEdit* editRuleName { nullptr };
    QComboBox* cmbRuleType { nullptr };
    QLineEdit* editRuleClasses { nullptr };
    QDoubleSpinBox* spinRuleMinConf { nullptr };
    QDoubleSpinBox* spinRuleDwellTime { nullptr };
    QPushButton* btnAddRule { nullptr };
    QPushButton* btnDeleteRule { nullptr };

    // PKI Certificates & TLS Security widgets (Sub-Tab 8)
    QTableWidget* tableCertificates { nullptr };
    QPushButton* btnRefreshCerts { nullptr };
    QLineEdit* editNewCertId { nullptr };
    QLineEdit* editNewCertSubject { nullptr };
    QSpinBox* spinNewCertDays { nullptr };
    QPushButton* btnCreateSelfSignedCert { nullptr };
    QPushButton* btnGenerateCsr { nullptr };
    QPushButton* btnDeleteCert { nullptr };
    QComboBox* cmbClientCertMode { nullptr };
    QPushButton* btnApplyClientCertMode { nullptr };

    // Profile G: Recordings & Replay widgets (Sub-Tab 11)
    QLabel* lblRecordingSummary { nullptr };
    QPushButton* btnRefreshRecordingSummary { nullptr };
    QTableWidget* tableRecordings { nullptr };
    QLineEdit* editNewRecordingSource { nullptr };
    QLineEdit* editNewRecordingContent { nullptr };
    QPushButton* btnCreateRecording { nullptr };
    QPushButton* btnDeleteRecording { nullptr };
    QPushButton* btnRefreshRecordings { nullptr };

    QComboBox* cmbTrackType { nullptr };
    QLineEdit* editTrackDesc { nullptr };
    QPushButton* btnCreateTrack { nullptr };
    QPushButton* btnDeleteTrack { nullptr };

    QTableWidget* tableRecordingJobs { nullptr };
    QLineEdit* editJobRecordingToken { nullptr };
    QLineEdit* editJobSourceToken { nullptr };
    QSpinBox* spinJobPriority { nullptr };
    QComboBox* cmbJobMode { nullptr };
    QPushButton* btnCreateJob { nullptr };
    QPushButton* btnToggleJobMode { nullptr };
    QPushButton* btnDeleteJob { nullptr };
    QPushButton* btnRefreshRecordingJobs { nullptr };

    QLineEdit* editSearchScope { nullptr };
    QPushButton* btnFindRecordings { nullptr };
    QTableWidget* tableSearchResults { nullptr };

    QLineEdit* editEventStartUtc { nullptr };
    QLineEdit* editEventEndUtc { nullptr };
    QPushButton* btnFindEvents { nullptr };
    QTableWidget* tableEventSearchResults { nullptr };

    QLineEdit* editReplayUri { nullptr };
    QPushButton* btnResolveReplayUri { nullptr };
    QPushButton* btnPlayReplayUri { nullptr };

    // Geolocation & GeoMove UI controls
    QDoubleSpinBox* spinCameraLat { nullptr };
    QDoubleSpinBox* spinCameraLon { nullptr };
    QDoubleSpinBox* spinCameraElev { nullptr };
    QDoubleSpinBox* spinCameraYaw { nullptr };
    QDoubleSpinBox* spinCameraPitch { nullptr };
    QPushButton* btnRefreshGeoLoc { nullptr };
    QPushButton* btnSaveGeoLoc { nullptr };

    QDoubleSpinBox* spinTargetLat { nullptr };
    QDoubleSpinBox* spinTargetLon { nullptr };
    QDoubleSpinBox* spinTargetElev { nullptr };
    QDoubleSpinBox* spinTargetWidth { nullptr };
    QDoubleSpinBox* spinTargetHeight { nullptr };
    QLabel* lblComputedGeoBearing { nullptr };
    QLabel* lblComputedGeoTilt { nullptr };
    QLabel* lblComputedGeoDistance { nullptr };
    QPushButton* btnExecuteGeoMove { nullptr };

    QDoubleSpinBox* spinSphericalAzimuth { nullptr };
    QDoubleSpinBox* spinSphericalElevation { nullptr };
    QDoubleSpinBox* spinSphericalZoom { nullptr };
    QPushButton* btnExecuteSphericalMove { nullptr };

    // Thermal & Radiometry UI controls (Sub-Tab 13)
    QDoubleSpinBox* spinEmissivity { nullptr };
    QDoubleSpinBox* spinTargetDistance { nullptr };
    QDoubleSpinBox* spinReflectedTemp { nullptr };
    QDoubleSpinBox* spinAtmosphericTemp { nullptr };
    QDoubleSpinBox* spinRelativeHumidity { nullptr };
    QDoubleSpinBox* spinWindowTransmission { nullptr };
    QPushButton* btnRefreshRadiometry { nullptr };
    QPushButton* btnApplyRadiometry { nullptr };

    QComboBox* cmbThermalPalettes { nullptr };
    QPushButton* btnSetPalette { nullptr };
    QPushButton* btnRefreshPalettes { nullptr };
    QPushButton* btnTriggerNuc { nullptr };
    QLabel* lblNucStatus { nullptr };

    QTableWidget* tableRadiometry { nullptr };
    QComboBox* cmbRadType { nullptr };
    QLineEdit* editRadToken { nullptr };
    QLineEdit* editRadLabel { nullptr };
    QDoubleSpinBox* spinRadX1 { nullptr };
    QDoubleSpinBox* spinRadY1 { nullptr };
    QDoubleSpinBox* spinRadX2 { nullptr };
    QDoubleSpinBox* spinRadY2 { nullptr };
    QPushButton* btnAddMeasurement { nullptr };
    QPushButton* btnDeleteMeasurement { nullptr };
    QPushButton* btnRefreshMeasurements { nullptr };

    QDoubleSpinBox* spinAlarmThreshold { nullptr };
    QLabel* lblThermalAlarmStatus { nullptr };
};

} // namespace PelcoDApp
