#pragma once

/// @file PelcoDPtzAdapter.h
/// @brief Adapter translating ONVIF PTZ and Imaging commands to PelcoDDevice hardware control.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <PelcoDCore/PatrolController.h>
#include <PelcoDCore/PelcoDDevice.h>

#include "OnvifServerTypes.h"

namespace Onvif {

/// @class PelcoDPtzAdapter
/// @brief Implements IPtzHandler and IImagingHandler by delegating commands to a PelcoDDevice instance.
/// @details Converts normalized velocity, focus, and angle ranges to Pelco-D protocol parameters and emits ONVIF
/// events. Strictly adheres to Interface Segregation Principle (ISP).
class PelcoDPtzAdapter : public IPtzHandler, public IImagingHandler {
public:
    /// @brief Constructs adapter wrapping an existing PelcoDDevice.
    /// @param[in] device Shared pointer to initialized PelcoDDevice instance.
    explicit PelcoDPtzAdapter(std::shared_ptr<PelcoD::PelcoDDevice> device);

    ~PelcoDPtzAdapter() override;

    // Non-copyable, non-movable per MISRA C++:2023
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

    void handleContinuousMove(float panSpeed, float tiltSpeed, float zoomSpeed,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) override;

    void handleAbsoluteMove(std::optional<float> pan, std::optional<float> tilt, std::optional<float> zoom,
        const std::string& spaceUri = "") override;

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

    [[nodiscard]] bool handleRelativeMove(std::optional<float> pan, std::optional<float> tilt,
        std::optional<float> zoom, float speed = 1.0f, const std::string& spaceUri = "") override;
    [[nodiscard]] bool handleGotoHomePosition(float speed = 1.0f) override;
    [[nodiscard]] bool handleSetHomePosition() override;
    [[nodiscard]] std::string handleSendAuxiliaryCommand(const std::string& auxiliaryData) override;
    [[nodiscard]] bool handleGeoMove(const std::string& profileToken, const GeoMoveTarget& target) override;
    void handleAbsoluteMoveSpherical(float azimuthDeg, float elevationDeg, float zoom) override;

    /// @brief Gets active camera mounting and geographic orientation.
    /// @return Active LocationEntity.
    [[nodiscard]] LocationEntity cameraLocation() const;

    /// @brief Configures camera mounting and geographic orientation.
    /// @param[in] location New LocationEntity.
    void setCameraLocation(const LocationEntity& location);

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

private:
    void onDeviceStatusUpdated(const PelcoD::DeviceStatus& status);
    void saveTours();
    void loadTours();

    void timerLoop();
    void armAutoStop(std::chrono::milliseconds timeout);
    void cancelAutoStop();

    std::atomic<bool> m_timerRunning { false };
    std::condition_variable m_timerCv { };
    std::mutex m_timerMutex { };
    std::thread m_timerThread { };
    std::optional<std::chrono::steady_clock::time_point> m_autoStopDeadline { std::nullopt };

    std::shared_ptr<PelcoD::PelcoDDevice> m_device;
    std::shared_ptr<PelcoD::PatrolController> m_sharedPatrol { };
    PelcoD::PatrolController* m_patrol { nullptr };

    mutable std::mutex m_mutex { };
    std::map<std::string, PtzPreset> m_presets { };
    std::uint32_t m_nextPresetId { 1U };
    std::map<std::string, PresetTour> m_tours { };
    std::uint32_t m_nextTourId { 1U };
    std::string m_persistencePath { "onvif_tours.json" };
    std::atomic<bool> m_isMoving { false };

    EventCallback m_eventPublisher { };
    PelcoD::Connection m_statusConn { };
    std::uint8_t m_lastAlarms { 0x00U };
    ImagingSettings m_imagingSettings { };
    std::string m_homePresetToken { "1" };
    float m_homePan { 0.0f };
    float m_homeTilt { 0.0f };
    float m_homeZoom { 0.0f };
    bool m_hasHomeCoordinates { false };
    LocationEntity m_cameraLocation { "Device", "Location_1", true, { 37.7749, -122.4194, 10.0 }, { 0.0, 0.0, 0.0 } };
    FocusStatus20 m_focusStatus { };
    std::vector<ImagingPreset> m_imagingPresets { { "Preset_Clear", "Clear", "Clear Daylight" },
        { "Preset_BW", "B/W", "Night Vision B/W" } };
    std::string m_currentImagingPresetToken { "Preset_Clear" };
};

} // namespace Onvif
