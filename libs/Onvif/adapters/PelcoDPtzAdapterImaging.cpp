#include "PelcoDPtzAdapter.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace Onvif {

std::string PelcoDPtzAdapter::handleSendAuxiliaryCommand(const std::string& auxiliaryData)
{
    if (!m_device) {
        return { };
    }

    const std::string trimmed = auxiliaryData;
    if (trimmed.find("WiperOn") != std::string::npos || trimmed.find("tt:Wiper|On") != std::string::npos) {
        static_cast<void>(m_device->setAuxiliary(1U));
        return "tt:Wiper|On";
    }
    if (trimmed.find("WiperOff") != std::string::npos || trimmed.find("tt:Wiper|Off") != std::string::npos) {
        static_cast<void>(m_device->clearAuxiliary(1U));
        return "tt:Wiper|Off";
    }
    if (trimmed.find("WasherOn") != std::string::npos || trimmed.find("tt:Washer|On") != std::string::npos) {
        static_cast<void>(m_device->setAuxiliary(2U));
        return "tt:Washer|On";
    }
    if (trimmed.find("WasherOff") != std::string::npos || trimmed.find("tt:Washer|Off") != std::string::npos) {
        static_cast<void>(m_device->clearAuxiliary(2U));
        return "tt:Washer|Off";
    }
    if (trimmed.find("IRLampOn") != std::string::npos || trimmed.find("tt:IRLamp|On") != std::string::npos) {
        static_cast<void>(m_device->setAuxiliary(3U));
        return "tt:IRLamp|On";
    }
    if (trimmed.find("IRLampOff") != std::string::npos || trimmed.find("tt:IRLamp|Off") != std::string::npos) {
        static_cast<void>(m_device->clearAuxiliary(3U));
        return "tt:IRLamp|Off";
    }

    return { };
}

// =========================================================================
// IImagingHandler Implementation (Profile T)
// =========================================================================

ImagingSettings PelcoDPtzAdapter::handleGetImagingSettings(const std::string& /*videoSourceToken*/)
{
    std::scoped_lock lock(m_mutex);
    return m_imagingSettings;
}

bool PelcoDPtzAdapter::handleSetImagingSettings(
    const std::string& /*videoSourceToken*/, const ImagingSettings& settings)
{
    std::scoped_lock lock(m_mutex);
    m_imagingSettings = settings;
    return true;
}

void PelcoDPtzAdapter::handleMoveFocus(const std::string& /*videoSourceToken*/, float speed)
{
    if (!m_device) {
        return;
    }
    if (!std::isfinite(speed)) {
        return;
    }

    if (speed > 0.05f) {
        static_cast<void>(m_device->focusFar());
    } else if (speed < -0.05f) {
        static_cast<void>(m_device->focusNear());
    } else {
        static_cast<void>(m_device->focusStop());
    }
}

void PelcoDPtzAdapter::handleStopFocus(const std::string& /*videoSourceToken*/)
{
    if (m_device) {
        static_cast<void>(m_device->focusStop());
    }
}

FocusStatus20 PelcoDPtzAdapter::handleGetFocusStatus(const std::string& /*videoSourceToken*/)
{
    std::scoped_lock lock(m_mutex);
    return m_focusStatus;
}

bool PelcoDPtzAdapter::handleMoveFocusAdvanced(const std::string& videoSourceToken, const FocusMove& move)
{
    if (move.mode == FocusMoveMode::Continuous) {
        handleMoveFocus(videoSourceToken, move.continuousSpeed);
        return true;
    }
    if (move.mode == FocusMoveMode::Absolute) {
        if (!m_device || !std::isfinite(move.absolutePosition)) {
            return false;
        }
        const auto pos = static_cast<std::uint16_t>(std::clamp(move.absolutePosition, 0.0f, 1.0f) * 65535.0f);
        static_cast<void>(m_device->setFocusPosition(pos));
        return true;
    }
    if (move.mode == FocusMoveMode::Relative) {
        if (!m_device || !std::isfinite(move.relativeDistance)) {
            return false;
        }
        if (move.relativeDistance > 0.0f) {
            static_cast<void>(m_device->focusFar());
        } else {
            static_cast<void>(m_device->focusNear());
        }
        return true;
    }
    return false;
}

std::vector<ImagingPreset> PelcoDPtzAdapter::handleGetImagingPresets(const std::string& /*videoSourceToken*/)
{
    std::scoped_lock lock(m_mutex);
    return m_imagingPresets;
}

bool PelcoDPtzAdapter::handleSetCurrentImagingPreset(
    const std::string& /*videoSourceToken*/, const std::string& presetToken)
{
    std::scoped_lock lock(m_mutex);
    for (const auto& p : m_imagingPresets) {
        if (p.token == presetToken) {
            m_currentImagingPresetToken = presetToken;
            return true;
        }
    }
    return false;
}

void PelcoDPtzAdapter::onDeviceStatusUpdated(const PelcoD::DeviceStatus& status)
{
    EventCallback publisher { };
    std::uint8_t newAlarms = 0x00U;
    std::uint8_t oldAlarms = 0x00U;

    {
        std::scoped_lock lock(m_mutex);
        newAlarms = status.alarms;
        oldAlarms = m_lastAlarms;
        m_lastAlarms = newAlarms;
        publisher = m_eventPublisher;
    }

    if (publisher && newAlarms != oldAlarms) {
        for (std::uint8_t i = 0U; i < 8U; ++i) {
            const std::uint8_t mask = static_cast<std::uint8_t>(1U << i);
            const bool oldState = (oldAlarms & mask) != 0U;
            const bool newState = (newAlarms & mask) != 0U;
            if (oldState != newState) {
                OnvifEvent ev { };
                ev.topic = "tns1:Device/Trigger/DigitalInput";
                ev.sourceName = "InputToken";
                ev.sourceValue = "DigitalInput_" + std::to_string(i + 1U);
                ev.dataName = "LogicalState";
                ev.dataValue = newState ? "true" : "false";
                publisher(ev);
            }
        }
    }
}

} // namespace Onvif
