#include "PelcoDPtzAdapter.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "XmlUtils.h"
#include <PelcoDCore/PatrolController.h>

namespace Onvif {

std::string PelcoDPtzAdapter::handleSetPreset(const std::string& name, const std::string& token)
{
    std::scoped_lock lock(m_mutex);

    std::string assignedToken = token;
    if (assignedToken.empty()) {
        assignedToken = std::to_string(m_nextPresetId++);
    }

    PtzPreset preset { };
    preset.token = assignedToken;
    preset.name = name.empty() ? ("Preset " + assignedToken) : name;

    try {
        const int id = std::stoi(assignedToken);
        if (m_device && id >= 1 && id <= 255) {
            static_cast<void>(m_device->setPreset(static_cast<std::uint8_t>(id)));
        }
    } catch (const std::exception&) {
        // Non-integer token, ignore device preset index mapping
    }

    m_presets[assignedToken] = preset;
    return assignedToken;
}

bool PelcoDPtzAdapter::handleGotoPreset(const std::string& token)
{
    EventCallback publisher { };
    bool dispatched = false;

    {
        std::scoped_lock lock(m_mutex);
        try {
            const int id = std::stoi(token);
            if (m_device && id >= 1 && id <= 255) {
                static_cast<void>(m_device->goToPreset(static_cast<std::uint8_t>(id)));
                dispatched = true;
            }
        } catch (const std::exception&) {
            // Fall through
        }

        if (!dispatched) {
            dispatched = (m_presets.find(token) != m_presets.end());
        }

        if (dispatched) {
            publisher = m_eventPublisher;
        }
    }

    if (dispatched && publisher) {
        OnvifEvent ev { };
        ev.topic = "tns1:PTZController/PTZPresets/Reached";
        ev.sourceName = "PresetToken";
        ev.sourceValue = token;
        ev.dataName = "State";
        ev.dataValue = "true";
        publisher(ev);
    }

    return dispatched;
}

bool PelcoDPtzAdapter::handleRemovePreset(const std::string& token)
{
    std::scoped_lock lock(m_mutex);

    try {
        const int id = std::stoi(token);
        if (m_device && id >= 1 && id <= 255) {
            static_cast<void>(m_device->clearPreset(static_cast<std::uint8_t>(id)));
        }
    } catch (const std::exception&) {
        // Fall through
    }

    return m_presets.erase(token) > 0;
}

std::vector<PtzPreset> PelcoDPtzAdapter::handleGetPresets()
{
    std::scoped_lock lock(m_mutex);
    std::vector<PtzPreset> result { };
    result.reserve(m_presets.size());
    for (const auto& [tok, preset] : m_presets) {
        result.push_back(preset);
    }
    return result;
}

PtzStatus PelcoDPtzAdapter::handleGetStatus()
{
    PtzStatus status { };
    if (!m_device) {
        return status;
    }

    const auto devStatus = m_device->getStatus();
    status.pan = devStatus.panDegrees();
    status.tilt = devStatus.tiltDegrees();
    status.zoom = static_cast<double>(devStatus.zoomPosition) / 65535.0;
    status.isMoving = m_isMoving.load();
    return status;
}

void PelcoDPtzAdapter::setPatrolController(std::shared_ptr<PelcoD::PatrolController> patrol)
{
    std::scoped_lock lock(m_mutex);
    m_sharedPatrol = std::move(patrol);
    m_patrol = m_sharedPatrol.get();
}

void PelcoDPtzAdapter::setPatrolController(PelcoD::PatrolController* patrol)
{
    std::scoped_lock lock(m_mutex);
    m_patrol = patrol;
}

void PelcoDPtzAdapter::setPersistencePath(const std::string& path)
{
    std::scoped_lock lock(m_mutex);
    m_persistencePath = path;
    loadTours();
}

std::vector<PresetTour> PelcoDPtzAdapter::handleGetPresetTours()
{
    std::scoped_lock lock(m_mutex);
    std::vector<PresetTour> result { };
    result.reserve(m_tours.size());
    for (const auto& [tok, tour] : m_tours) {
        result.push_back(tour);
    }
    return result;
}

std::optional<PresetTour> PelcoDPtzAdapter::handleGetPresetTour(const std::string& tourToken)
{
    std::scoped_lock lock(m_mutex);
    const auto it = m_tours.find(tourToken);
    if (it != m_tours.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string PelcoDPtzAdapter::handleCreatePresetTour()
{
    std::scoped_lock lock(m_mutex);
    std::string token = "Tour_" + std::to_string(m_nextTourId++);
    PresetTour tour { };
    tour.token = token;
    tour.name = "Patrol Tour " + std::to_string(m_nextTourId - 1U);
    tour.status = PresetTourState::Idle;
    tour.autoStart = false;
    m_tours[token] = tour;
    saveTours();
    return token;
}

bool PelcoDPtzAdapter::handleModifyPresetTour(const PresetTour& tour)
{
    std::scoped_lock lock(m_mutex);
    m_tours[tour.token] = tour;
    saveTours();
    return true;
}

bool PelcoDPtzAdapter::handleOperatePresetTour(const std::string& tourToken, PresetTourOperation op)
{
    std::scoped_lock lock(m_mutex);
    auto it = m_tours.find(tourToken);
    if (it == m_tours.end()) {
        return false;
    }

    auto& tour = it->second;
    PelcoD::PatrolController* patrol = m_patrol ? m_patrol : m_sharedPatrol.get();

    if (op == PresetTourOperation::Start) {
        tour.status = PresetTourState::Touring;
        if (patrol) {
            std::vector<PelcoD::PatrolStep> steps { };
            for (const auto& spot : tour.spots) {
                PelcoD::PatrolStep step { };
                try {
                    step.presetId = static_cast<std::uint8_t>(std::stoul(spot.presetToken));
                } catch (const std::exception&) {
                    step.presetId = 1U;
                }
                step.dwellTimeSeconds = spot.stayTimeSeconds;
                step.speed
                    = static_cast<std::uint8_t>(std::clamp(Xml::safeFloat(spot.speed, 1.0f) * 63.0f, 0.0f, 63.0f));
                steps.push_back(step);
            }
            if (!steps.empty()) {
                patrol->setSteps(steps);
                patrol->setLoop(true);
                if (!patrol->start()) {
                    tour.status = PresetTourState::Idle;
                    return false;
                }
            }
        }
        return true;
    }
    if (op == PresetTourOperation::Stop) {
        tour.status = PresetTourState::Idle;
        if (patrol) {
            patrol->stop();
        }
        return true;
    }
    if (op == PresetTourOperation::Pause) {
        tour.status = PresetTourState::Paused;
        if (patrol) {
            patrol->pause();
        }
        return true;
    }
    return false;
}

bool PelcoDPtzAdapter::handleRemovePresetTour(const std::string& tourToken)
{
    std::scoped_lock lock(m_mutex);
    const bool removed = m_tours.erase(tourToken) > 0;
    if (removed) {
        saveTours();
    }
    return removed;
}

void PelcoDPtzAdapter::saveTours()
{
    if (m_persistencePath.empty()) {
        return;
    }
    std::ofstream out(m_persistencePath);
    if (!out.is_open()) {
        return;
    }
    out << "[\n";
    bool firstTour = true;
    for (const auto& [token, tour] : m_tours) {
        if (!firstTour) {
            out << ",\n";
        }
        firstTour = false;
        out << "  {\n"
            << "    \"token\": \"" << tour.token << "\",\n"
            << "    \"name\": \"" << tour.name << "\",\n"
            << "    \"autoStart\": " << (tour.autoStart ? "true" : "false") << ",\n"
            << "    \"spots\": [\n";
        bool firstSpot = true;
        for (const auto& spot : tour.spots) {
            if (!firstSpot) {
                out << ",\n";
            }
            firstSpot = false;
            out << "      { \"presetToken\": \"" << spot.presetToken << "\", \"speed\": " << spot.speed
                << ", \"stayTime\": " << spot.stayTimeSeconds << " }";
        }
        out << "\n    ]\n  }";
    }
    out << "\n]\n";
}

void PelcoDPtzAdapter::loadTours()
{
    if (m_persistencePath.empty()) {
        return;
    }
    std::ifstream in(m_persistencePath);
    if (!in.is_open()) {
        return;
    }
    std::ostringstream ss { };
    ss << in.rdbuf();
    const std::string content { ss.str() };
    if (content.empty()) {
        return;
    }

    std::size_t pos = 0;
    while ((pos = content.find("\"token\":", pos)) != std::string::npos) {
        pos += 8;
        auto q1 = content.find('"', pos);
        if (q1 == std::string::npos) {
            break;
        }
        auto q2 = content.find('"', q1 + 1);
        if (q2 == std::string::npos) {
            break;
        }
        std::string token = content.substr(q1 + 1, q2 - q1 - 1);
        pos = q2 + 1;

        PresetTour tour { };
        tour.token = token;

        auto nextTokPos = content.find("\"token\":", pos);

        auto nPos = content.find("\"name\":", pos);
        if (nPos != std::string::npos && (nextTokPos == std::string::npos || nPos < nextTokPos)) {
            auto nq1 = content.find('"', nPos + 7);
            if (nq1 != std::string::npos) {
                auto nq2 = content.find('"', nq1 + 1);
                if (nq2 != std::string::npos) {
                    tour.name = content.substr(nq1 + 1, nq2 - nq1 - 1);
                }
            }
        }

        auto spotsPos = content.find("\"spots\":", pos);
        if (spotsPos != std::string::npos && (nextTokPos == std::string::npos || spotsPos < nextTokPos)) {
            auto closeBracket = content.find(']', spotsPos);
            std::size_t spotPos = spotsPos;
            while ((spotPos = content.find("\"presetToken\":", spotPos)) != std::string::npos
                && (closeBracket == std::string::npos || spotPos < closeBracket)) {
                spotPos += 14;
                auto sq1 = content.find('"', spotPos);
                if (sq1 == std::string::npos) {
                    break;
                }
                auto sq2 = content.find('"', sq1 + 1);
                if (sq2 == std::string::npos) {
                    break;
                }
                PresetTourSpot spot { };
                spot.presetToken = content.substr(sq1 + 1, sq2 - sq1 - 1);
                spotPos = sq2 + 1;

                auto spdPos = content.find("\"speed\":", spotPos);
                if (spdPos != std::string::npos && (closeBracket == std::string::npos || spdPos < closeBracket)) {
                    spot.speed = std::stof(content.substr(spdPos + 8));
                }
                auto stayPos = content.find("\"stayTime\":", spotPos);
                if (stayPos != std::string::npos && (closeBracket == std::string::npos || stayPos < closeBracket)) {
                    spot.stayTimeSeconds = static_cast<std::uint32_t>(std::stoul(content.substr(stayPos + 11)));
                }
                tour.spots.push_back(std::move(spot));
            }
        }

        m_tours[tour.token] = tour;
    }
}

} // namespace Onvif
