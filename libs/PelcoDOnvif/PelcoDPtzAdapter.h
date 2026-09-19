#pragma once

/// @file PelcoDPtzAdapter.h
/// @brief Adapter translating ONVIF PTZ and Imaging commands to PelcoDDevice hardware control.

#include "OnvifServerTypes.h"
#include <cstdint>
#include <PelcoDCore/PatrolController.h>
#include <PelcoDCore/PelcoDDevice.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace PelcoD::Onvif {

/// @class PelcoDPtzAdapter
/// @brief Implements IPtzHandler and IImagingHandler by delegating commands to a PelcoDDevice instance.
/// @details Converts normalized velocity, focus, and angle ranges to Pelco-D protocol parameters and emits ONVIF
/// events.
class PelcoDPtzAdapter : public IPtzHandler,
                         public IImagingHandler,
                         public IDeviceIoHandler,
                         public IMetadataHandler,
                         public IDeviceManagementHandler,
                         public IRecordingHandler,
                         public ISearchHandler,
                         public IReplayHandler,
                         public IAnalyticsHandler,
                         public IMaskHandler,
                         public IVideoSourceModeHandler,
                         public IThermalHandler {
public:
    /// @brief Constructs adapter wrapping an existing PelcoDDevice.
    /// @param[in] device Shared pointer to initialized PelcoDDevice instance.
    explicit PelcoDPtzAdapter(std::shared_ptr<PelcoD::PelcoDDevice> device);

    ~PelcoDPtzAdapter() override;

    // Non-copyable, non-movable
    PelcoDPtzAdapter(const PelcoDPtzAdapter&) = delete;
    PelcoDPtzAdapter& operator=(const PelcoDPtzAdapter&) = delete;
    PelcoDPtzAdapter(PelcoDPtzAdapter&&) = delete;
    PelcoDPtzAdapter& operator=(PelcoDPtzAdapter&&) = delete;

    /// @brief Registers an event publisher callback to receive generated ONVIF events.
    /// @param[in] publisher Callback function invoked when alarms or presets trigger.
    void setEventPublisher(EventCallback publisher);

    /// @brief Links a PatrolController for executing Preset Tours.
    /// @param[in] patrol Pointer to initialized PatrolController.
    void setPatrolController(std::shared_ptr<PelcoD::PatrolController> patrol);

    /// @brief Links a PatrolController for executing Preset Tours.
    /// @param[in] patrol Raw pointer to initialized PatrolController.
    void setPatrolController(PelcoD::PatrolController* patrol);

    /// @brief Sets file path for persisting tours across server restarts.
    /// @param[in] path File path for JSON tour database.
    void setPersistencePath(const std::string& path);

    // =========================================================================
    // IPtzHandler Implementation (Profile S & T)
    // =========================================================================

    void handleContinuousMove(float panSpeed, float tiltSpeed, float zoomSpeed) override;
    void handleAbsoluteMove(float pan, float tilt, float zoom) override;
    void handleStop(bool stopPanTilt, bool stopZoom) override;
    [[nodiscard]] std::string handleSetPreset(const std::string& name, const std::string& token) override;
    [[nodiscard]] bool handleGotoPreset(const std::string& token) override;
    [[nodiscard]] bool handleRemovePreset(const std::string& token) override;
    [[nodiscard]] std::vector<PtzPreset> handleGetPresets() override;
    [[nodiscard]] PtzStatus handleGetStatus() override;

    [[nodiscard]] std::vector<PresetTour> handleGetPresetTours() override;
    [[nodiscard]] std::optional<PresetTour> handleGetPresetTour(const std::string& tourToken) override;
    [[nodiscard]] std::string handleCreatePresetTour() override;
    [[nodiscard]] bool handleModifyPresetTour(const PresetTour& tour) override;
    [[nodiscard]] bool handleOperatePresetTour(const std::string& tourToken, PresetTourOperation op) override;
    [[nodiscard]] bool handleRemovePresetTour(const std::string& tourToken) override;

    [[nodiscard]] bool handleRelativeMove(float pan, float tilt, float zoom, float speed = 1.0f) override;
    [[nodiscard]] bool handleGotoHomePosition(float speed = 1.0f) override;
    [[nodiscard]] bool handleSetHomePosition() override;
    [[nodiscard]] std::string handleSendAuxiliaryCommand(const std::string& auxiliaryData) override;
    bool handleGeoMove(const std::string& profileToken, const GeoMoveTarget& target) override;
    void handleAbsoluteMoveSpherical(float azimuthDeg, float elevationDeg, float zoom) override;

    // =========================================================================
    // IImagingHandler Implementation (Profile T)
    // =========================================================================

    [[nodiscard]] ImagingSettings handleGetImagingSettings(const std::string& videoSourceToken) override;
    [[nodiscard]] bool handleSetImagingSettings(
        const std::string& videoSourceToken, const ImagingSettings& settings) override;
    void handleMoveFocus(const std::string& videoSourceToken, float speed) override;
    void handleStopFocus(const std::string& videoSourceToken) override;
    [[nodiscard]] FocusStatus20 handleGetFocusStatus(const std::string& videoSourceToken) override;
    [[nodiscard]] bool handleMoveFocusAdvanced(const std::string& videoSourceToken, const FocusMove& move) override;
    [[nodiscard]] std::vector<ImagingPreset> handleGetImagingPresets(const std::string& videoSourceToken) override;
    [[nodiscard]] bool handleSetCurrentImagingPreset(
        const std::string& videoSourceToken, const std::string& presetToken) override;

    // =========================================================================
    // IDeviceIoHandler Implementation (Profile S & T)
    // =========================================================================

    [[nodiscard]] std::vector<RelayOutputConfig> handleGetRelayOutputs() override;
    [[nodiscard]] std::vector<std::string> handleGetRelayOutputOptions(const std::string& token) override;
    [[nodiscard]] bool handleSetRelayOutputSettings(
        const std::string& token, const RelayOutputConfig& settings) override;
    [[nodiscard]] bool handleSetRelayOutputState(const std::string& token, RelayLogicalState state) override;
    [[nodiscard]] std::vector<DigitalInputConfig> handleGetDigitalInputs() override;

    // =========================================================================
    // IMetadataHandler Implementation (Profile T & M)
    // =========================================================================

    [[nodiscard]] std::vector<MetadataConfiguration> handleGetMetadataConfigurations() override;
    [[nodiscard]] std::optional<MetadataConfiguration> handleGetMetadataConfiguration(
        const std::string& token) override;
    [[nodiscard]] bool handleSetMetadataConfiguration(const MetadataConfiguration& config) override;
    [[nodiscard]] MetadataConfigurationOptions handleGetMetadataConfigurationOptions(
        const std::string& configToken, const std::string& profileToken = "") override;
    [[nodiscard]] MetadataStreamPayload handleGetCurrentMetadata(const std::string& profileToken = "") override;

    /// @brief Injects or updates active analytics detected objects for metadata streaming.
    /// @param[in] objects List of detected objects.
    void setDetectedObjects(std::vector<AnalyticsObject> objects);

    /// @brief Appends a single detected analytics object.
    /// @param[in] object AnalyticsObject to track.
    void addDetectedObject(const AnalyticsObject& object);

    /// @brief Clears current active detected analytics objects.
    void clearDetectedObjects();

    // =========================================================================
    // IDeviceManagementHandler Implementation (Profile S/T/M/PKI)
    // =========================================================================

    [[nodiscard]] std::string handleGetSystemLog(SystemLogType logType) override;
    [[nodiscard]] SystemSupportInfo handleGetSystemSupportInformation() override;
    [[nodiscard]] std::string handleGetSystemBackup() override;
    [[nodiscard]] bool handleRestoreSystem(const std::string& backupData) override;
    [[nodiscard]] std::string handleGetEndpointReference() override;

    [[nodiscard]] std::vector<OnvifCertificate> handleGetCertificates() override;
    [[nodiscard]] std::optional<CertificateInformation> handleGetCertificateInformation(
        const std::string& certificateId) override;
    [[nodiscard]] OnvifCertificate handleCreateCertificate(
        const std::string& certificateId, const std::string& subject, int daysValid = 365) override;
    [[nodiscard]] Pkcs10Request handleGetPkcs10Request(
        const std::string& certificateId, const std::string& subject) override;
    [[nodiscard]] bool handleLoadCertificates(const std::vector<OnvifCertificate>& certificates) override;
    [[nodiscard]] bool handleDeleteCertificate(const std::string& certificateId) override;
    [[nodiscard]] ClientCertificateMode handleGetClientCertificateMode() override;
    [[nodiscard]] bool handleSetClientCertificateMode(ClientCertificateMode mode) override;

    [[nodiscard]] std::optional<LocationEntity> handleGetGeoLocation(const std::string& entityToken) override;
    [[nodiscard]] bool handleSetGeoLocation(const LocationEntity& location) override;
    [[nodiscard]] bool handleDeleteGeoLocation(const std::string& entityToken) override;

    /// @brief Gets the configured camera mounting location.
    [[nodiscard]] LocationEntity cameraLocation() const;

    /// @brief Sets the camera mounting location and orientation.
    void setCameraLocation(const LocationEntity& location);

    // =========================================================================
    // IRecordingHandler Implementation (Profile G)
    // =========================================================================

    [[nodiscard]] std::string handleCreateRecording(const RecordingConfig& config) override;
    [[nodiscard]] std::vector<RecordingConfig> handleGetRecordings() override;
    [[nodiscard]] std::optional<RecordingConfig> handleGetRecordingConfiguration(
        const std::string& recordingToken) override;
    [[nodiscard]] bool handleSetRecordingConfiguration(
        const std::string& recordingToken, const RecordingConfig& config) override;
    [[nodiscard]] bool handleDeleteRecording(const std::string& recordingToken) override;
    [[nodiscard]] std::vector<RecordingJob> handleGetRecordingJobs() override;
    [[nodiscard]] std::string handleCreateRecordingJob(const RecordingJob& job) override;
    [[nodiscard]] bool handleSetRecordingJobMode(const std::string& jobToken, RecordingJobMode mode) override;
    [[nodiscard]] bool handleDeleteRecordingJob(const std::string& jobToken) override;
    [[nodiscard]] RecordingSummary handleGetRecordingSummary() override;
    [[nodiscard]] std::vector<RecordingTrack> handleGetTracks(const std::string& recordingToken) override;
    [[nodiscard]] std::string handleCreateTrack(
        const std::string& recordingToken, const RecordingTrack& track) override;
    [[nodiscard]] bool handleDeleteTrack(const std::string& recordingToken, const std::string& trackToken) override;

    // =========================================================================
    // ISearchHandler Implementation (Profile G)
    // =========================================================================

    [[nodiscard]] std::string handleFindRecordings(
        const std::string& scope, int maxMatches, const std::string& keepAliveTime) override;
    [[nodiscard]] std::vector<RecordingSearchResult> handleGetRecordingSearchResults(
        const std::string& searchToken) override;
    [[nodiscard]] std::string handleFindEvents(
        const std::string& startUtc, const std::string& endUtc, int maxMatches) override;
    [[nodiscard]] std::vector<RecordedEventResult> handleGetEventSearchResults(const std::string& searchToken) override;
    [[nodiscard]] bool handleEndSearch(const std::string& searchToken) override;

    // =========================================================================
    // IReplayHandler Implementation (Profile G)
    // =========================================================================

    [[nodiscard]] std::string handleGetReplayUri(
        const std::string& recordingToken, const std::string& trackToken) override;
    [[nodiscard]] ReplayConfiguration handleGetReplayConfiguration() override;
    [[nodiscard]] bool handleSetReplayConfiguration(const ReplayConfiguration& config) override;

    // =========================================================================
    // IAnalyticsHandler Implementation (Profile M & Profile T)
    // =========================================================================

    [[nodiscard]] std::vector<AnalyticsRuleDescription> handleGetSupportedRules(
        const std::string& configToken) override;
    [[nodiscard]] std::vector<AnalyticsRule> handleGetRules(const std::string& configToken) override;
    bool handleCreateRules(const std::string& configToken, const std::vector<AnalyticsRule>& rules) override;
    bool handleModifyRules(const std::string& configToken, const std::vector<AnalyticsRule>& rules) override;
    bool handleDeleteRules(const std::string& configToken, const std::vector<std::string>& ruleNames) override;

    [[nodiscard]] std::vector<AnalyticsModuleDescription> handleGetSupportedAnalyticsModules(
        const std::string& configToken) override;
    [[nodiscard]] std::vector<AnalyticsModule> handleGetAnalyticsModules(const std::string& configToken) override;
    bool handleCreateAnalyticsModules(
        const std::string& configToken, const std::vector<AnalyticsModule>& modules) override;
    bool handleModifyAnalyticsModules(
        const std::string& configToken, const std::vector<AnalyticsModule>& modules) override;
    bool handleDeleteAnalyticsModules(
        const std::string& configToken, const std::vector<std::string>& moduleNames) override;

    // =========================================================================
    // IMaskHandler Implementation (Profile T / Media2)
    // =========================================================================

    [[nodiscard]] MaskOptions handleGetMaskOptions(const std::string& configToken) override;
    [[nodiscard]] std::vector<PrivacyMask> handleGetMasks(const std::string& configToken) override;
    [[nodiscard]] std::optional<PrivacyMask> handleGetMask(const std::string& maskToken) override;
    bool handleSetMask(const PrivacyMask& mask) override;
    [[nodiscard]] std::string handleCreateMask(const PrivacyMask& mask) override;
    bool handleDeleteMask(const std::string& maskToken) override;

    // =========================================================================
    // IVideoSourceModeHandler Implementation (Profile T / Media2)
    // =========================================================================

    [[nodiscard]] std::vector<VideoSourceMode> handleGetVideoSourceModes(const std::string& videoSourceToken) override;
    bool handleSetVideoSourceMode(
        const std::string& videoSourceToken, const std::string& modeToken, bool& outRebootNeeded) override;

    // =========================================================================
    // IThermalHandler Implementation (ver10/thermal/wsdl)
    // =========================================================================

    [[nodiscard]] RadiometryConfig handleGetRadiometryConfiguration(
        const std::string& videoSourceToken) override;
    bool handleSetRadiometryConfiguration(
        const std::string& videoSourceToken, const RadiometryConfig& config) override;
    [[nodiscard]] std::vector<RadiometrySpot> handleGetRadiometrySpots(
        const std::string& videoSourceToken) override;
    bool handleSetRadiometrySpots(
        const std::string& videoSourceToken, const std::vector<RadiometrySpot>& spots) override;
    [[nodiscard]] std::vector<RadiometryBox> handleGetRadiometryBoxes(
        const std::string& videoSourceToken) override;
    bool handleSetRadiometryBoxes(
        const std::string& videoSourceToken, const std::vector<RadiometryBox>& boxes) override;
    [[nodiscard]] std::vector<ColorPalette> handleGetColorPalettes(
        const std::string& videoSourceToken) override;
    bool handleSetColorPalette(
        const std::string& videoSourceToken, const std::string& paletteToken) override;
    bool handleTriggerNuc(const std::string& videoSourceToken) override;

private:
    void onDeviceStatusUpdated(const PelcoD::DeviceStatus& status);
    void saveTours();
    void loadTours();

    std::shared_ptr<PelcoD::PelcoDDevice> m_device;
    std::shared_ptr<PelcoD::PatrolController> m_sharedPatrol {};
    PelcoD::PatrolController* m_patrol { nullptr };

    mutable std::mutex m_mutex {};
    std::map<std::string, PtzPreset> m_presets {};
    std::uint32_t m_nextPresetId { 1 };
    std::map<std::string, PresetTour> m_tours {};
    std::uint32_t m_nextTourId { 1 };
    std::string m_persistencePath { "onvif_tours.json" };
    std::atomic<bool> m_isMoving { false };

    EventCallback m_eventPublisher {};
    PelcoD::Connection m_statusConn {};
    std::uint8_t m_lastAlarms { 0x00U };
    ImagingSettings m_imagingSettings {};
    std::string m_homePresetToken { "1" };
    float m_homePan { 0.0f };
    float m_homeTilt { 0.0f };
    float m_homeZoom { 0.0f };
    bool m_hasHomeCoordinates { false };
    FocusStatus20 m_focusStatus {};
    std::vector<ImagingPreset> m_imagingPresets { { "Preset_Clear", "Clear", "Clear Daylight" },
        { "Preset_BW", "B/W", "Night Vision B/W" } };
    std::string m_currentImagingPresetToken { "Preset_Clear" };
    std::vector<RelayOutputConfig> m_relays { { "Relay_1", RelayMode::Bistable, 0.0f, RelayIdleState::Open,
                                                  RelayLogicalState::Inactive },
        { "Relay_2", RelayMode::Bistable, 0.0f, RelayIdleState::Open, RelayLogicalState::Inactive } };
    std::vector<DigitalInputConfig> m_digitalInputs { { "Input_1", RelayIdleState::Open, "Alarm", false } };

    std::vector<MetadataConfiguration> m_metadataConfigs { { "MetadataConfig_1", "DefaultMetadataConfig", 1, "PT60S",
        true, true, true, true } };
    std::vector<AnalyticsObject> m_detectedObjects {};
    std::chrono::steady_clock::time_point m_startTime { std::chrono::steady_clock::now() };

    std::vector<OnvifCertificate> m_certificates {};
    ClientCertificateMode m_clientCertMode { ClientCertificateMode::Off };
    std::vector<RecordingConfig> m_recordings {};
    std::vector<RecordingJob> m_recordingJobs {};
    ReplayConfiguration m_replayConfig {};
    std::map<std::string, std::vector<RecordingSearchResult>> m_recordingSearches {};
    std::map<std::string, std::vector<RecordedEventResult>> m_eventSearches {};
    std::uint32_t m_nextSearchSessionId { 1 };

    void evaluateRulesForObject(const AnalyticsObject& prevObj, const AnalyticsObject& currentObj);
    void evaluateRulesForFrame();

    struct ObjectTrackState {
        AnalyticsObject lastObject {};
        std::chrono::steady_clock::time_point firstSeen {};
        std::chrono::steady_clock::time_point lastSeen {};
        std::map<std::string, bool> triggeredRules {}; // ruleName -> bool
    };

    std::vector<AnalyticsRule> m_rules {};
    std::vector<AnalyticsModule> m_analyticsModules {};
    std::map<int, ObjectTrackState> m_objectTracks {};

    LocationEntity m_cameraLocation { "Device", "Location_1", true, { 37.7749, -122.4194, 10.0 }, { 0.0, 0.0, 0.0 } };

    mutable std::mutex m_maskMutex {};
    std::vector<PrivacyMask> m_masks {
        { "Mask_1", "VideoSource_1", { { 0.1f, 0.1f }, { 0.3f, 0.1f }, { 0.3f, 0.3f }, { 0.1f, 0.3f } },
          MaskType::Color, { 0, 0, 0, "RGB" }, true }
    };
    MaskOptions m_maskOptions {};
    std::uint32_t m_nextMaskId { 2 };

    mutable std::mutex m_videoSourceModeMutex {};
    std::vector<VideoSourceMode> m_videoSourceModes {};

    mutable std::mutex m_thermalMutex {};
    RadiometryConfig m_radiometryConfig {};
    std::vector<RadiometrySpot> m_radiometrySpots {
        { "Spot_1", { 0.5f, 0.5f }, "Center Spot", 24.5f }
    };
    std::vector<RadiometryBox> m_radiometryBoxes {
        { "Box_1", { 0.2f, 0.2f }, { 0.8f, 0.8f }, "Central Target Zone", 21.0f, 36.8f, 28.4f }
    };
    std::vector<RadiometryAlarmConfig> m_radiometryAlarms {
        { "Box_1", 50.0f, 2.0f, RadiometryAlarmType::HighTemperature, true }
    };
    std::vector<ColorPalette> m_colorPalettes {
        { "WhiteHot", "White Hot", true },
        { "BlackHot", "Black Hot", false },
        { "Ironbow", "Ironbow", false },
        { "Rainbow", "Rainbow", false },
        { "Sepia", "Sepia", false },
        { "Fire", "Fire", false }
    };
    std::string m_activeColorPalette { "WhiteHot" };
};

} // namespace PelcoD::Onvif
