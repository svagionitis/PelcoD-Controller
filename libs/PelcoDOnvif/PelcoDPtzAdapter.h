#pragma once

/// @file PelcoDPtzAdapter.h
/// @brief Adapter translating ONVIF PTZ and Imaging commands to PelcoDDevice hardware control.

#include "OnvifServerTypes.h"
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
class PelcoDPtzAdapter : public IPtzHandler, public IImagingHandler {
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

    // =========================================================================
    // IImagingHandler Implementation (Profile T)
    // =========================================================================

    [[nodiscard]] ImagingSettings handleGetImagingSettings(const std::string& videoSourceToken) override;
    [[nodiscard]] bool handleSetImagingSettings(
        const std::string& videoSourceToken, const ImagingSettings& settings) override;
    void handleMoveFocus(const std::string& videoSourceToken, float speed) override;
    void handleStopFocus(const std::string& videoSourceToken) override;

private:
    void onDeviceStatusUpdated(const PelcoD::DeviceStatus& status);

    std::shared_ptr<PelcoD::PelcoDDevice> m_device;
    mutable std::mutex m_mutex {};
    std::map<std::string, PtzPreset> m_presets {};
    uint32_t m_nextPresetId { 1 };
    std::atomic<bool> m_isMoving { false };

    EventCallback m_eventPublisher {};
    PelcoD::Connection m_statusConn {};
    std::uint8_t m_lastAlarms { 0x00U };
    ImagingSettings m_imagingSettings {};
};

} // namespace PelcoD::Onvif
