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
    if (!m_device || auxiliaryData.empty()) {
        return { };
    }

    if (auxiliaryData.find("Wiper") != std::string::npos || auxiliaryData.find("wiper") != std::string::npos) {
        const bool isOff
            = auxiliaryData.find("Off") != std::string::npos || auxiliaryData.find("off") != std::string::npos;
        if (isOff) {
            static_cast<void>(m_device->clearAuxiliary(1U));
        } else {
            static_cast<void>(m_device->setAuxiliary(1U));
        }
        return auxiliaryData;
    }

    if (auxiliaryData.find("Washer") != std::string::npos || auxiliaryData.find("washer") != std::string::npos) {
        const bool isOff
            = auxiliaryData.find("Off") != std::string::npos || auxiliaryData.find("off") != std::string::npos;
        if (isOff) {
            static_cast<void>(m_device->clearAuxiliary(2U));
        } else {
            static_cast<void>(m_device->setAuxiliary(2U));
        }
        return auxiliaryData;
    }

    if (auxiliaryData.find("IR") != std::string::npos || auxiliaryData.find("ir") != std::string::npos
        || auxiliaryData.find("Ir") != std::string::npos) {
        const bool isOff
            = auxiliaryData.find("Off") != std::string::npos || auxiliaryData.find("off") != std::string::npos;
        if (isOff) {
            static_cast<void>(m_device->clearAuxiliary(3U));
        } else {
            static_cast<void>(m_device->setAuxiliary(3U));
        }
        return auxiliaryData;
    }

    auto auxPos = auxiliaryData.find("Aux");
    if (auxPos == std::string::npos) {
        auxPos = auxiliaryData.find("aux");
    }
    if (auxPos != std::string::npos && auxPos + 3U < auxiliaryData.size() && auxiliaryData[auxPos + 3U] >= '1'
        && auxiliaryData[auxPos + 3U] <= '8') {
        const auto auxId = static_cast<std::uint8_t>(auxiliaryData[auxPos + 3U] - '0');
        const bool isOff
            = auxiliaryData.find("Off") != std::string::npos || auxiliaryData.find("off") != std::string::npos;
        if (isOff) {
            static_cast<void>(m_device->clearAuxiliary(auxId));
        } else {
            static_cast<void>(m_device->setAuxiliary(auxId));
        }
        return auxiliaryData;
    }

    return auxiliaryData;
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
        // Standard Pelco-D protocol does not support direct absolute optical focus positioning.
        return false;
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
