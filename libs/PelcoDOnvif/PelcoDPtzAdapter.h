#pragma once

/// @file PelcoDPtzAdapter.h
/// @brief Adapter translating ONVIF PTZ commands to PelcoDDevice hardware control.

#include "OnvifServerTypes.h"
#include <PelcoDCore/PelcoDDevice.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace PelcoD::Onvif {

/// @class PelcoDPtzAdapter
/// @brief Implements IPtzHandler by delegating commands to a PelcoDDevice instance.
/// @details Converts normalized velocity and angle ranges to Pelco-D protocol parameters.
class PelcoDPtzAdapter : public IPtzHandler {
public:
    /// @brief Constructs adapter wrapping an existing PelcoDDevice.
    /// @param[in] device Shared pointer to initialized PelcoDDevice instance.
    explicit PelcoDPtzAdapter(std::shared_ptr<PelcoD::PelcoDDevice> device);

    ~PelcoDPtzAdapter() override = default;

    /// @brief Translates continuous pan/tilt/zoom speeds to Pelco-D move commands.
    void handleContinuousMove(float panSpeed, float tiltSpeed, float zoomSpeed) override;

    /// @brief Translates absolute positioning coordinates.
    void handleAbsoluteMove(float pan, float tilt, float zoom) override;

    /// @brief Issues stop commands to PelcoDDevice.
    void handleStop(bool stopPanTilt, bool stopZoom) override;

    /// @brief Stores current position as preset.
    [[nodiscard]] std::string handleSetPreset(const std::string& name, const std::string& token) override;

    /// @brief Recalls preset on PelcoDDevice.
    [[nodiscard]] bool handleGotoPreset(const std::string& token) override;

    /// @brief Clears preset on PelcoDDevice.
    [[nodiscard]] bool handleRemovePreset(const std::string& token) override;

    /// @brief Retrieves list of configured presets.
    [[nodiscard]] std::vector<PtzPreset> handleGetPresets() override;

    /// @brief Retrieves current status and telemetry snapshot.
    [[nodiscard]] PtzStatus handleGetStatus() override;

private:
    std::shared_ptr<PelcoD::PelcoDDevice> m_device;
    mutable std::mutex m_mutex {};
    std::map<std::string, PtzPreset> m_presets {};
    uint32_t m_nextPresetId { 1 };
    std::atomic<bool> m_isMoving { false };
};

} // namespace PelcoD::Onvif
