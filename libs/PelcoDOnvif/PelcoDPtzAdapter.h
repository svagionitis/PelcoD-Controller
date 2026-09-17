#pragma once

/// @file PelcoDPtzAdapter.h
/// @brief Adapter translating ONVIF PTZ and Imaging commands to PelcoDDevice hardware control.

#include "OnvifServerTypes.h"
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
                         public IDeviceManagementHandler {
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
    // IDeviceManagementHandler Implementation (Profile S/T/M)
    // =========================================================================

    [[nodiscard]] std::string handleGetSystemLog(SystemLogType logType) override;
    [[nodiscard]] SystemSupportInfo handleGetSystemSupportInformation() override;
    [[nodiscard]] std::string handleGetSystemBackup() override;
    [[nodiscard]] bool handleRestoreSystem(const std::string& backupData) override;
    [[nodiscard]] std::string handleGetEndpointReference() override;

private:
    void onDeviceStatusUpdated(const PelcoD::DeviceStatus& status);
    void saveTours();
    void loadTours();

    std::shared_ptr<PelcoD::PelcoDDevice> m_device;
    std::shared_ptr<PelcoD::PatrolController> m_sharedPatrol {};
    PelcoD::PatrolController* m_patrol { nullptr };

    mutable std::mutex m_mutex {};
    std::map<std::string, PtzPreset> m_presets {};
    uint32_t m_nextPresetId { 1 };
    std::map<std::string, PresetTour> m_tours {};
    uint32_t m_nextTourId { 1 };
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
};

} // namespace PelcoD::Onvif
