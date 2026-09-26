#pragma once

/// @file LocalPresetManager.h
/// @brief In-memory implementation of IPtzPresetManager for physical and simulated PTZ stations.

#include "ICameraPayload.h"
#include "IPanTiltUnit.h"
#include "IPtzPresetManager.h"

#include <map>
#include <memory>
#include <mutex>

namespace PayloadHal {

/// @class LocalPresetManager
/// @brief Thread-safe local preset repository coordinating gimbal pan/tilt angles and optical camera zoom/focus.
class LocalPresetManager : public IPtzPresetManager {
public:
    explicit LocalPresetManager(
        std::shared_ptr<IPanTiltUnit> ptu = nullptr, std::shared_ptr<ICameraPayload> camera = nullptr);
    ~LocalPresetManager() override = default;

    bool savePreset(const PtzPreset& preset) override;
    bool saveCurrentPosition(std::uint32_t id, const std::string& name = "") override;
    bool recallPreset(std::uint32_t id, float speedRatio = 1.0f) override;
    bool clearPreset(std::uint32_t id) override;

    [[nodiscard]] std::optional<PtzPreset> getPreset(std::uint32_t id) const override;
    [[nodiscard]] std::vector<PtzPreset> listPresets() const override;

    void setHomePresetId(std::uint32_t id) override;
    [[nodiscard]] std::optional<std::uint32_t> getHomePresetId() const override;
    bool goHome(float speedRatio = 1.0f) override;

    void setPanTiltUnit(std::shared_ptr<IPanTiltUnit> ptu);
    void setCamera(std::shared_ptr<ICameraPayload> camera);

private:
    std::shared_ptr<IPanTiltUnit> m_ptu {};
    std::shared_ptr<ICameraPayload> m_camera {};

    mutable std::mutex m_mutex {};
    std::map<std::uint32_t, PtzPreset> m_presets {};
    std::optional<std::uint32_t> m_homePresetId {};
};

} // namespace PayloadHal
