#include "PelcoDPtzAdapter.h"

#include <algorithm>
#include <cmath>

namespace PelcoD::Onvif {

PelcoDPtzAdapter::PelcoDPtzAdapter(std::shared_ptr<PelcoD::PelcoDDevice> device)
    : m_device(std::move(device))
{
}

void PelcoDPtzAdapter::handleContinuousMove(float panSpeed, float tiltSpeed, float zoomSpeed)
{
    if (!m_device) {
        return;
    }

    const bool panIdle = std::abs(panSpeed) < 0.02f;
    const bool tiltIdle = std::abs(tiltSpeed) < 0.02f;

    if (panIdle && tiltIdle) {
        m_device->stopMotion();
        if (std::abs(zoomSpeed) <= 0.05f) {
            m_isMoving = false;
        }
    } else {
        m_isMoving = true;
        PanDirection panDir = PanDirection::Stop;
        if (panSpeed > 0.02f) {
            panDir = PanDirection::Right;
        } else if (panSpeed < -0.02f) {
            panDir = PanDirection::Left;
        }

        TiltDirection tiltDir = TiltDirection::Stop;
        if (tiltSpeed > 0.02f) {
            tiltDir = TiltDirection::Up;
        } else if (tiltSpeed < -0.02f) {
            tiltDir = TiltDirection::Down;
        }

        const auto pSpeed = static_cast<std::uint8_t>(std::clamp(std::abs(panSpeed) * 63.0f, 0.0f, 63.0f));
        const auto tSpeed = static_cast<std::uint8_t>(std::clamp(std::abs(tiltSpeed) * 63.0f, 0.0f, 63.0f));

        m_device->move(panDir, pSpeed, tiltDir, tSpeed);
    }

    if (zoomSpeed > 0.05f) {
        m_isMoving = true;
        m_device->zoomTele();
    } else if (zoomSpeed < -0.05f) {
        m_isMoving = true;
        m_device->zoomWide();
    } else {
        m_device->zoomStop();
    }
}

void PelcoDPtzAdapter::handleAbsoluteMove(float pan, float tilt, float zoom)
{
    if (!m_device) {
        return;
    }

    // Convert degrees to centidegrees (1/100 of a degree)
    if (pan >= 0.0f) {
        const auto cDeg = static_cast<std::uint16_t>(std::clamp(pan * 100.0f, 0.0f, 35999.0f));
        m_device->setPanAngle(cDeg);
    }

    if (tilt >= 0.0f) {
        const auto cDeg = static_cast<std::uint16_t>(std::clamp(tilt * 100.0f, 0.0f, 35999.0f));
        m_device->setTiltAngle(cDeg);
    }

    if (zoom >= 0.0f) {
        const auto pos = static_cast<std::uint16_t>(std::clamp(zoom * 65535.0f, 0.0f, 65535.0f));
        m_device->setZoomPosition(pos);
    }
}

void PelcoDPtzAdapter::handleStop(bool stopPanTilt, bool stopZoom)
{
    if (!m_device) {
        return;
    }

    if (stopPanTilt) {
        m_device->stopMotion();
    }

    if (stopZoom) {
        m_device->zoomStop();
    }

    if (stopPanTilt && stopZoom) {
        m_isMoving = false;
    }
}

std::string PelcoDPtzAdapter::handleSetPreset(const std::string& name, const std::string& token)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string assignedToken = token;
    if (assignedToken.empty()) {
        assignedToken = std::to_string(m_nextPresetId++);
    }

    PtzPreset preset {};
    preset.token = assignedToken;
    preset.name = name.empty() ? ("Preset " + assignedToken) : name;

    try {
        const int id = std::stoi(assignedToken);
        if (m_device && id >= 1 && id <= 255) {
            m_device->setPreset(static_cast<std::uint8_t>(id));
        }
    } catch (...) {
        // Non-integer token, ignore device preset index mapping
    }

    m_presets[assignedToken] = preset;
    return assignedToken;
}

bool PelcoDPtzAdapter::handleGotoPreset(const std::string& token)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        const int id = std::stoi(token);
        if (m_device && id >= 1 && id <= 255) {
            m_device->goToPreset(static_cast<std::uint8_t>(id));
            return true;
        }
    } catch (...) {
        // Fall through
    }

    return m_presets.find(token) != m_presets.end();
}

bool PelcoDPtzAdapter::handleRemovePreset(const std::string& token)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        const int id = std::stoi(token);
        if (m_device && id >= 1 && id <= 255) {
            m_device->clearPreset(static_cast<std::uint8_t>(id));
        }
    } catch (...) {
        // Fall through
    }

    return m_presets.erase(token) > 0;
}

std::vector<PtzPreset> PelcoDPtzAdapter::handleGetPresets()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<PtzPreset> result {};
    result.reserve(m_presets.size());
    for (const auto& [tok, preset] : m_presets) {
        result.push_back(preset);
    }
    return result;
}

PtzStatus PelcoDPtzAdapter::handleGetStatus()
{
    PtzStatus status {};
    if (!m_device) {
        return status;
    }

    const auto devStatus = m_device->getStatus();
    status.pan = static_cast<float>(devStatus.panDegrees());
    status.tilt = static_cast<float>(devStatus.tiltDegrees());
    status.zoom = static_cast<float>(devStatus.zoomPosition) / 65535.0f;
    status.isMoving = m_isMoving.load();
    return status;
}

} // namespace PelcoD::Onvif
