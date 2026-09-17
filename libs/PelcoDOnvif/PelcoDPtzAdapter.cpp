#include "PelcoDPtzAdapter.h"

#include <algorithm>
#include <cmath>

namespace PelcoD::Onvif {

PelcoDPtzAdapter::PelcoDPtzAdapter(std::shared_ptr<PelcoD::PelcoDDevice> device)
    : m_device(std::move(device))
{
    if (m_device) {
        m_statusConn = m_device->addStatusCallback(
            [this](const PelcoD::DeviceStatus& status) { onDeviceStatusUpdated(status); });
    }
}

PelcoDPtzAdapter::~PelcoDPtzAdapter()
{
    m_statusConn.disconnect();
}

void PelcoDPtzAdapter::setEventPublisher(EventCallback publisher)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventPublisher = std::move(publisher);
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

    bool dispatched = false;
    try {
        const int id = std::stoi(token);
        if (m_device && id >= 1 && id <= 255) {
            m_device->goToPreset(static_cast<std::uint8_t>(id));
            dispatched = true;
        }
    } catch (...) {
        // Fall through
    }

    if (!dispatched) {
        dispatched = (m_presets.find(token) != m_presets.end());
    }

    if (dispatched && m_eventPublisher) {
        OnvifEvent ev {};
        ev.topic = "tns1:PTZController/PTZPresets/Reached";
        ev.sourceName = "PresetToken";
        ev.sourceValue = token;
        ev.dataName = "State";
        ev.dataValue = "true";
        m_eventPublisher(ev);
    }

    return dispatched;
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

ImagingSettings PelcoDPtzAdapter::handleGetImagingSettings(const std::string& /*videoSourceToken*/)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_imagingSettings;
}

bool PelcoDPtzAdapter::handleSetImagingSettings(
    const std::string& /*videoSourceToken*/, const ImagingSettings& settings)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_imagingSettings = settings;

    if (m_device) {
        m_device->setBacklightComp(settings.backlightCompensation ? SwitchState::On : SwitchState::Off);
    }
    return true;
}

void PelcoDPtzAdapter::handleMoveFocus(const std::string& /*videoSourceToken*/, float speed)
{
    if (!m_device) {
        return;
    }

    if (speed < -0.05f) {
        m_device->focusNear();
    } else if (speed > 0.05f) {
        m_device->focusFar();
    } else {
        m_device->focusStop();
    }
}

void PelcoDPtzAdapter::handleStopFocus(const std::string& /*videoSourceToken*/)
{
    if (m_device) {
        m_device->focusStop();
    }
}

void PelcoDPtzAdapter::onDeviceStatusUpdated(const PelcoD::DeviceStatus& status)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_eventPublisher) {
        return;
    }

    if (status.alarms != m_lastAlarms) {
        for (int i = 0; i < 8; ++i) {
            const auto mask = static_cast<std::uint8_t>(1U << i);
            const bool oldBit = (m_lastAlarms & mask) != 0U;
            const bool newBit = (status.alarms & mask) != 0U;
            if (oldBit != newBit) {
                OnvifEvent ev {};
                ev.topic = "tns1:Device/Trigger/DigitalInput";
                ev.sourceName = "InputToken";
                ev.sourceValue = "Alarm_" + std::to_string(i + 1);
                ev.dataName = "LogicalState";
                ev.dataValue = newBit ? "true" : "false";
                m_eventPublisher(ev);
            }
        }
        m_lastAlarms = status.alarms;
    }
}

} // namespace PelcoD::Onvif
