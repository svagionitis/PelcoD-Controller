/// @file LocalPresetManager.cpp
/// @brief Implementation of the in-memory local PTZ preset manager.

#include "LocalPresetManager.h"

#include <algorithm>

namespace PayloadHal {

LocalPresetManager::LocalPresetManager(std::shared_ptr<IPanTiltUnit> ptu, std::shared_ptr<ICameraPayload> camera)
    : m_ptu(std::move(ptu))
    , m_camera(std::move(camera))
{
}

bool LocalPresetManager::savePreset(const PtzPreset& preset)
{
    if (preset.id == 0U) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_presets[preset.id] = preset;
    return true;
}

bool LocalPresetManager::saveCurrentPosition(std::uint32_t id, const std::string& name)
{
    if (id == 0U) {
        return false;
    }

    std::shared_ptr<IPanTiltUnit> ptu;
    std::shared_ptr<ICameraPayload> camera;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ptu = m_ptu;
        camera = m_camera;
    }

    if (!ptu) {
        return false;
    }

    const auto ptuTelem = ptu->currentTelemetry();
    CameraTelemetry camTelem {};
    if (camera) {
        camTelem = camera->currentTelemetry();
    }

    PtzPreset p {};
    p.id = id;
    p.name = name.empty() ? ("Preset " + std::to_string(id)) : name;
    p.panAngleDeg = ptuTelem.panAngleDeg;
    p.tiltAngleDeg = ptuTelem.tiltAngleDeg;
    p.opticalZoomFactor = camTelem.opticalZoomFactor;
    p.focusDistanceNormalized = camTelem.focusDistanceNormalized;
    p.preferredSpectrum = camera ? camera->spectrum() : CameraSpectrum::DaylightVisible;
    p.timestamp = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_presets[id] = p;
    }
    return true;
}

bool LocalPresetManager::recallPreset(std::uint32_t id, float speedRatio)
{
    (void)speedRatio;
    PtzPreset preset {};
    std::shared_ptr<IPanTiltUnit> ptu;
    std::shared_ptr<ICameraPayload> camera;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_presets.find(id);
        if (it == m_presets.end()) {
            return false;
        }
        preset = it->second;
        ptu = m_ptu;
        camera = m_camera;
    }

    bool success = true;
    if (ptu) {
        if (!ptu->setAbsoluteAngles(preset.panAngleDeg, preset.tiltAngleDeg)) {
            success = false;
        }
    }

    if (camera) {
        if (preset.opticalZoomFactor >= 1.0) {
            const double normZoom = std::clamp((preset.opticalZoomFactor - 1.0) / 39.0, 0.0, 1.0);
            camera->setZoomNormalized(normZoom);
        }
        if (preset.focusDistanceNormalized > 0.0) {
            camera->setFocusNormalized(preset.focusDistanceNormalized);
        }
    }

    return success;
}

bool LocalPresetManager::clearPreset(std::uint32_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto erased = m_presets.erase(id);
    if (m_homePresetId.has_value() && *m_homePresetId == id) {
        m_homePresetId.reset();
    }
    return erased > 0U;
}

std::optional<PtzPreset> LocalPresetManager::getPreset(std::uint32_t id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_presets.find(id);
    if (it != m_presets.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<PtzPreset> LocalPresetManager::listPresets() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<PtzPreset> list;
    list.reserve(m_presets.size());
    for (const auto& [id, preset] : m_presets) {
        list.push_back(preset);
    }
    return list;
}

void LocalPresetManager::setHomePresetId(std::uint32_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_homePresetId = id;
}

std::optional<std::uint32_t> LocalPresetManager::getHomePresetId() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_homePresetId;
}

bool LocalPresetManager::goHome(float speedRatio)
{
    std::optional<std::uint32_t> homeId;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        homeId = m_homePresetId;
    }
    if (!homeId.has_value()) {
        return false;
    }
    return recallPreset(*homeId, speedRatio);
}

void LocalPresetManager::setPanTiltUnit(std::shared_ptr<IPanTiltUnit> ptu)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ptu = std::move(ptu);
}

void LocalPresetManager::setCamera(std::shared_ptr<ICameraPayload> camera)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_camera = std::move(camera);
}

} // namespace PayloadHal
