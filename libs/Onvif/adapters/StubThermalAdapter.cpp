#include "StubThermalAdapter.h"

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <PelcoDCore/PelcoDDevice.h>

namespace Onvif {

StubThermalAdapter::StubThermalAdapter(std::shared_ptr<PelcoD::PelcoDDevice> device)
    : m_device(std::move(device))
{
}

void StubThermalAdapter::setEventPublisher(EventCallback publisher)
{
    std::scoped_lock lock(m_mutex);
    m_eventPublisher = std::move(publisher);
}

RadiometryConfig StubThermalAdapter::handleGetRadiometryConfig(const std::string& /*videoSourceToken*/)
{
    std::scoped_lock lock(m_mutex);
    return m_radiometryConfig;
}

bool StubThermalAdapter::handleSetRadiometryConfig(
    const std::string& /*videoSourceToken*/, const RadiometryConfig& config)
{
    std::scoped_lock lock(m_mutex);
    m_radiometryConfig = config;
    return true;
}

std::vector<RadiometrySpot> StubThermalAdapter::handleGetRadiometrySpots(const std::string& /*videoSourceToken*/)
{
    std::scoped_lock lock(m_mutex);
    return m_radiometrySpots;
}

bool StubThermalAdapter::handleSetRadiometrySpots(
    const std::string& /*videoSourceToken*/, const std::vector<RadiometrySpot>& spots)
{
    std::scoped_lock lock(m_mutex);
    m_radiometrySpots = spots;
    return true;
}

std::vector<RadiometryBox> StubThermalAdapter::handleGetRadiometryBoxes(const std::string& /*videoSourceToken*/)
{
    std::scoped_lock lock(m_mutex);
    return m_radiometryBoxes;
}

bool StubThermalAdapter::handleSetRadiometryBoxes(
    const std::string& /*videoSourceToken*/, const std::vector<RadiometryBox>& boxes)
{
    EventCallback publisher { };
    std::vector<OnvifEvent> pendingEvents { };

    {
        std::scoped_lock lock(m_mutex);
        m_radiometryBoxes = boxes;
        publisher = m_eventPublisher;

        if (publisher) {
            for (const auto& box : m_radiometryBoxes) {
                for (const auto& alarm : m_radiometryAlarms) {
                    if (alarm.enabled && alarm.token == box.token) {
                        if (box.maxTemperature >= alarm.thresholdTemperature) {
                            OnvifEvent ev { };
                            ev.topic = "tns1:Thermal/Radiometry/HighTemperatureAlarm";
                            ev.sourceName = "BoxToken";
                            ev.sourceValue = box.token;
                            ev.dataName = "State";
                            ev.dataValue = "true";
                            pendingEvents.push_back(ev);
                        }
                    }
                }
            }
        }
    }

    if (publisher) {
        for (const auto& ev : pendingEvents) {
            publisher(ev);
        }
    }

    return true;
}

std::vector<ColorPalette> StubThermalAdapter::handleGetColorPalettes(const std::string& /*videoSourceToken*/)
{
    std::scoped_lock lock(m_mutex);
    return m_colorPalettes;
}

bool StubThermalAdapter::handleSetColorPalette(const std::string& /*videoSourceToken*/, const std::string& paletteToken)
{
    std::scoped_lock lock(m_mutex);
    bool found = false;
    for (auto& pal : m_colorPalettes) {
        if (pal.token == paletteToken) {
            pal.isDefault = true;
            m_activeColorPalette = pal.token;
            found = true;
        } else {
            pal.isDefault = false;
        }
    }
    return found;
}

bool StubThermalAdapter::handleTriggerNuc(const std::string& /*videoSourceToken*/)
{
    if (m_device) {
        static_cast<void>(m_device->setAuxiliary(5U));
    }
    return true;
}

} // namespace Onvif
