#include "PelcoDPtzAdapter.h"
#include <PelcoDCore/PatrolController.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

namespace PelcoD::Onvif {

PelcoDPtzAdapter::PelcoDPtzAdapter(std::shared_ptr<PelcoD::PelcoDDevice> device)
    : m_device(std::move(device))
{
    if (m_device) {
        m_statusConn = m_device->addStatusCallback(
            [this](const PelcoD::DeviceStatus& status) { onDeviceStatusUpdated(status); });
    }
    loadTours();
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

void PelcoDPtzAdapter::setPatrolController(std::shared_ptr<PelcoD::PatrolController> patrol)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sharedPatrol = std::move(patrol);
    m_patrol = m_sharedPatrol.get();
}

void PelcoDPtzAdapter::setPatrolController(PelcoD::PatrolController* patrol)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_patrol = patrol;
}

void PelcoDPtzAdapter::setPersistencePath(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_persistencePath = path;
    loadTours();
}

std::vector<PresetTour> PelcoDPtzAdapter::handleGetPresetTours()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<PresetTour> result {};
    result.reserve(m_tours.size());
    for (const auto& [tok, tour] : m_tours) {
        result.push_back(tour);
    }
    return result;
}

std::optional<PresetTour> PelcoDPtzAdapter::handleGetPresetTour(const std::string& tourToken)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_tours.find(tourToken);
    if (it != m_tours.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string PelcoDPtzAdapter::handleCreatePresetTour()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string token = "Tour_" + std::to_string(m_nextTourId++);
    PresetTour tour {};
    tour.token = token;
    tour.name = "Patrol Tour " + std::to_string(m_nextTourId - 1);
    tour.status = PresetTourState::Idle;
    tour.autoStart = false;
    m_tours[token] = tour;
    saveTours();
    return token;
}

bool PelcoDPtzAdapter::handleModifyPresetTour(const PresetTour& tour)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tours[tour.token] = tour;
    saveTours();
    return true;
}

bool PelcoDPtzAdapter::handleOperatePresetTour(const std::string& tourToken, PresetTourOperation op)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tours.find(tourToken);
    if (it == m_tours.end()) {
        return false;
    }

    auto& tour = it->second;
    PelcoD::PatrolController* patrol = m_patrol ? m_patrol : m_sharedPatrol.get();

    if (op == PresetTourOperation::Start) {
        tour.status = PresetTourState::Touring;
        if (patrol) {
            std::vector<PelcoD::PatrolStep> steps {};
            for (const auto& spot : tour.spots) {
                PelcoD::PatrolStep step {};
                try {
                    step.presetId = static_cast<std::uint8_t>(std::stoul(spot.presetToken));
                } catch (...) {
                    step.presetId = 1U;
                }
                step.dwellTimeSeconds = spot.stayTimeSeconds;
                step.speed = static_cast<std::uint8_t>(std::clamp(spot.speed * 63.0f, 0.0f, 63.0f));
                steps.push_back(step);
            }
            if (!steps.empty()) {
                patrol->setSteps(steps);
                patrol->setLoop(true);
                patrol->start();
            }
        }
        return true;
    } else if (op == PresetTourOperation::Stop) {
        tour.status = PresetTourState::Idle;
        if (patrol) {
            patrol->stop();
        }
        return true;
    } else if (op == PresetTourOperation::Pause) {
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
    std::lock_guard<std::mutex> lock(m_mutex);
    const bool removed = m_tours.erase(tourToken) > 0;
    if (removed) {
        saveTours();
    }
    return removed;
}

bool PelcoDPtzAdapter::handleRelativeMove(float pan, float tilt, float zoom, float /*speed*/)
{
    if (!m_device) {
        return false;
    }

    const auto currentStatus = m_device->getStatus();

    if (std::abs(pan) > 0.0001f) {
        float currentPan = static_cast<float>(currentStatus.panDegrees());
        float newPan = std::fmod(currentPan + pan + 360.0f, 360.0f);
        if (newPan < 0.0f) {
            newPan += 360.0f;
        }
        const auto cDeg = static_cast<std::uint16_t>(std::clamp(newPan * 100.0f, 0.0f, 35999.0f));
        m_device->setPanAngle(cDeg);
    }

    if (std::abs(tilt) > 0.0001f) {
        float currentTilt = static_cast<float>(currentStatus.tiltDegrees());
        float newTilt = std::clamp(currentTilt + tilt, 0.0f, 90.0f);
        const auto cDeg = static_cast<std::uint16_t>(std::clamp(newTilt * 100.0f, 0.0f, 35999.0f));
        m_device->setTiltAngle(cDeg);
    }

    if (std::abs(zoom) > 0.0001f) {
        float currentZoomNorm = static_cast<float>(currentStatus.zoomPosition) / 65535.0f;
        float newZoom = std::clamp(currentZoomNorm + zoom, 0.0f, 1.0f);
        const auto pos = static_cast<std::uint16_t>(newZoom * 65535.0f);
        m_device->setZoomPosition(pos);
    }

    return true;
}

bool PelcoDPtzAdapter::handleGotoHomePosition(float /*speed*/)
{
    if (!m_device) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_hasHomeCoordinates) {
        handleAbsoluteMove(m_homePan, m_homeTilt, m_homeZoom);
        return true;
    }
    if (!m_homePresetToken.empty()) {
        try {
            const auto presetId = static_cast<std::uint8_t>(std::stoul(m_homePresetToken));
            if (presetId > 0U) {
                m_device->goToPreset(presetId);
                return true;
            }
        } catch (...) {
        }
    }
    handleAbsoluteMove(0.0f, 0.0f, 0.0f);
    return true;
}

bool PelcoDPtzAdapter::handleSetHomePosition()
{
    if (!m_device) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto status = m_device->getStatus();
    m_homePan = static_cast<float>(status.panDegrees());
    m_homeTilt = static_cast<float>(status.tiltDegrees());
    m_homeZoom = static_cast<float>(status.zoomPosition) / 65535.0f;
    m_hasHomeCoordinates = true;

    try {
        const auto presetId = static_cast<std::uint8_t>(std::stoul(m_homePresetToken));
        if (presetId > 0U) {
            m_device->setPreset(presetId);
        }
    } catch (...) {
    }
    return true;
}

std::string PelcoDPtzAdapter::handleSendAuxiliaryCommand(const std::string& auxiliaryData)
{
    if (!m_device || auxiliaryData.empty()) {
        return {};
    }

    if (auxiliaryData.find("Wiper") != std::string::npos || auxiliaryData.find("wiper") != std::string::npos) {
        const bool isOff
            = auxiliaryData.find("Off") != std::string::npos || auxiliaryData.find("off") != std::string::npos;
        if (isOff) {
            m_device->clearAuxiliary(1U);
        } else {
            m_device->setAuxiliary(1U);
        }
        return auxiliaryData;
    }

    if (auxiliaryData.find("Washer") != std::string::npos || auxiliaryData.find("washer") != std::string::npos) {
        const bool isOff
            = auxiliaryData.find("Off") != std::string::npos || auxiliaryData.find("off") != std::string::npos;
        if (isOff) {
            m_device->clearAuxiliary(2U);
        } else {
            m_device->setAuxiliary(2U);
        }
        return auxiliaryData;
    }

    if (auxiliaryData.find("IR") != std::string::npos || auxiliaryData.find("ir") != std::string::npos
        || auxiliaryData.find("Ir") != std::string::npos) {
        const bool isOff
            = auxiliaryData.find("Off") != std::string::npos || auxiliaryData.find("off") != std::string::npos;
        if (isOff) {
            m_device->clearAuxiliary(3U);
        } else {
            m_device->setAuxiliary(3U);
        }
        return auxiliaryData;
    }

    auto auxPos = auxiliaryData.find("Aux");
    if (auxPos == std::string::npos) {
        auxPos = auxiliaryData.find("aux");
    }
    if (auxPos != std::string::npos && auxPos + 3 < auxiliaryData.size()
        && std::isdigit(static_cast<unsigned char>(auxiliaryData[auxPos + 3]))) {
        const auto auxId = static_cast<std::uint8_t>(auxiliaryData[auxPos + 3] - '0');
        if (auxId >= 1U && auxId <= 8U) {
            const bool isOff
                = auxiliaryData.find("Off") != std::string::npos || auxiliaryData.find("off") != std::string::npos;
            if (isOff) {
                m_device->clearAuxiliary(auxId);
            } else {
                m_device->setAuxiliary(auxId);
            }
            return auxiliaryData;
        }
    }

    return auxiliaryData;
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
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
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

        PresetTour tour {};
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
                PresetTourSpot spot {};
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
    std::lock_guard<std::mutex> lock(m_mutex);
    m_focusStatus.moveStatus = "IDLE";
    if (m_device) {
        m_device->focusStop();
    }
}

FocusStatus20 PelcoDPtzAdapter::handleGetFocusStatus(const std::string& /*videoSourceToken*/)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_focusStatus;
}

bool PelcoDPtzAdapter::handleMoveFocusAdvanced(const std::string& videoSourceToken, const FocusMove& move)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_focusStatus.moveStatus = "MOVING";

    if (move.mode == FocusMoveMode::Continuous) {
        handleMoveFocus(videoSourceToken, move.continuousSpeed);
    } else if (move.mode == FocusMoveMode::Absolute) {
        m_focusStatus.position = std::clamp(move.absolutePosition, 0.0f, 1.0f);
        if (m_device) {
            if (move.absolutePosition > 0.5f) {
                m_device->focusFar();
            } else {
                m_device->focusNear();
            }
        }
    } else if (move.mode == FocusMoveMode::Relative) {
        m_focusStatus.position = std::clamp(m_focusStatus.position + move.relativeDistance, 0.0f, 1.0f);
        if (m_device) {
            if (move.relativeDistance > 0.0f) {
                m_device->focusFar();
            } else {
                m_device->focusNear();
            }
        }
    }
    return true;
}

std::vector<ImagingPreset> PelcoDPtzAdapter::handleGetImagingPresets(const std::string& /*videoSourceToken*/)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_imagingPresets;
}

bool PelcoDPtzAdapter::handleSetCurrentImagingPreset(
    const std::string& /*videoSourceToken*/, const std::string& presetToken)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& p : m_imagingPresets) {
        if (p.token == presetToken) {
            m_currentImagingPresetToken = presetToken;
            return true;
        }
    }
    return false;
}

std::vector<RelayOutputConfig> PelcoDPtzAdapter::handleGetRelayOutputs()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_relays;
}

std::vector<std::string> PelcoDPtzAdapter::handleGetRelayOutputOptions(const std::string& /*token*/)
{
    return { "Bistable", "Monostable" };
}

bool PelcoDPtzAdapter::handleSetRelayOutputSettings(const std::string& token, const RelayOutputConfig& settings)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& r : m_relays) {
        if (r.token == token) {
            r.mode = settings.mode;
            r.delayTimeSeconds = settings.delayTimeSeconds;
            r.idleState = settings.idleState;
            return true;
        }
    }
    return false;
}

bool PelcoDPtzAdapter::handleSetRelayOutputState(const std::string& token, RelayLogicalState state)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& r : m_relays) {
        if (r.token == token) {
            r.logicalState = state;

            // Extract numeric suffix for Aux index (e.g. "Relay_1" -> Aux 1)
            std::uint8_t auxId = 1U;
            const auto underPos = token.find('_');
            if (underPos != std::string::npos && underPos + 1 < token.length()) {
                try {
                    auxId = static_cast<std::uint8_t>(std::stoi(token.substr(underPos + 1)));
                } catch (...) {
                    auxId = 1U;
                }
            }

            if (m_device) {
                if (state == RelayLogicalState::Active) {
                    m_device->setAuxiliary(auxId);
                } else {
                    m_device->clearAuxiliary(auxId);
                }
            }

            // If monostable pulse, auto-reset after delay
            if (r.mode == RelayMode::Monostable && state == RelayLogicalState::Active && r.delayTimeSeconds > 0.0f) {
                const auto delayMs = static_cast<int>(r.delayTimeSeconds * 1000.0f);
                std::thread([this, token, auxId, delayMs]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
                    {
                        std::lock_guard<std::mutex> lk(m_mutex);
                        for (auto& rel : m_relays) {
                            if (rel.token == token) {
                                rel.logicalState = RelayLogicalState::Inactive;
                                break;
                            }
                        }
                    }
                    if (m_device) {
                        m_device->clearAuxiliary(auxId);
                    }
                }).detach();
            }

            return true;
        }
    }
    return false;
}

std::vector<DigitalInputConfig> PelcoDPtzAdapter::handleGetDigitalInputs()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_digitalInputs;
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

// =========================================================================
// IMetadataHandler Implementation
// =========================================================================

std::vector<MetadataConfiguration> PelcoDPtzAdapter::handleGetMetadataConfigurations()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metadataConfigs;
}

std::optional<MetadataConfiguration> PelcoDPtzAdapter::handleGetMetadataConfiguration(const std::string& token)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& cfg : m_metadataConfigs) {
        if (cfg.token == token) {
            return cfg;
        }
    }
    return std::nullopt;
}

bool PelcoDPtzAdapter::handleSetMetadataConfiguration(const MetadataConfiguration& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& cfg : m_metadataConfigs) {
        if (cfg.token == config.token) {
            cfg = config;
            return true;
        }
    }
    m_metadataConfigs.push_back(config);
    return true;
}

MetadataConfigurationOptions PelcoDPtzAdapter::handleGetMetadataConfigurationOptions(
    const std::string& /*configToken*/, const std::string& /*profileToken*/)
{
    MetadataConfigurationOptions opts {};
    opts.ptzStatusSupported = true;
    opts.analyticsSupported = true;
    opts.eventsSupported = true;
    return opts;
}

MetadataStreamPayload PelcoDPtzAdapter::handleGetCurrentMetadata(const std::string& /*profileToken*/)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    MetadataStreamPayload payload {};
    payload.ptzStatus = handleGetStatus();

    AnalyticsFrame frame {};
    frame.objects = m_detectedObjects;
    payload.analyticsFrame = frame;

    return payload;
}

void PelcoDPtzAdapter::setDetectedObjects(std::vector<AnalyticsObject> objects)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_detectedObjects = std::move(objects);
}

void PelcoDPtzAdapter::addDetectedObject(const AnalyticsObject& object)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_detectedObjects.push_back(object);
}

void PelcoDPtzAdapter::clearDetectedObjects()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_detectedObjects.clear();
}

// =========================================================================
// IDeviceManagementHandler Implementation
// =========================================================================

std::string PelcoDPtzAdapter::handleGetSystemLog(SystemLogType logType)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream ss;
    if (logType == SystemLogType::Access) {
        ss << "[ACCESS LOG] Pelco-D PTZ Adapter active\n";
    } else {
        ss << "[SYSTEM LOG] Pelco-D Hardware State: " << (m_device ? "Connected" : "Disconnected") << "\n";
        ss << "[SYSTEM LOG] Presets configured: " << m_presets.size() << "\n";
        ss << "[SYSTEM LOG] Tours configured: " << m_tours.size() << "\n";
    }
    return ss.str();
}

SystemSupportInfo PelcoDPtzAdapter::handleGetSystemSupportInformation()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    SystemSupportInfo info {};
    const auto uptime
        = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m_startTime).count();
    info.uptimeSeconds = static_cast<uint64_t>(uptime);
    info.cpuLoadPercent = 5.0f;
    info.memoryUsedMb = 32;
    info.memoryTotalMb = 512;
    info.activeConnections = 1;
    info.storageState = "OK";

    std::ostringstream ss;
    ss << "Pelco-D Controller Diagnostics\n"
       << "------------------------------\n"
       << "Device Connected: " << (m_device ? "Yes" : "No") << "\n"
       << "Presets Count: " << m_presets.size() << "\n"
       << "Tours Count: " << m_tours.size() << "\n"
       << "Relays Count: " << m_relays.size() << "\n"
       << "Digital Inputs Count: " << m_digitalInputs.size() << "\n"
       << "Uptime: " << uptime << " seconds\n";
    info.rawDiagnostics = ss.str();
    return info;
}

std::string PelcoDPtzAdapter::handleGetSystemBackup()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream ss;
    ss << "{\n"
       << "  \"presets_count\": " << m_presets.size() << ",\n"
       << "  \"tours_count\": " << m_tours.size() << ",\n"
       << "  \"imaging\": {\n"
       << "    \"brightness\": " << m_imagingSettings.brightness << ",\n"
       << "    \"colorSaturation\": " << m_imagingSettings.colorSaturation << ",\n"
       << "    \"contrast\": " << m_imagingSettings.contrast << "\n"
       << "  }\n"
       << "}";
    return ss.str();
}

bool PelcoDPtzAdapter::handleRestoreSystem(const std::string& backupData)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return !backupData.empty();
}

std::string PelcoDPtzAdapter::handleGetEndpointReference()
{
    return "urn:uuid:pelco-d-ptz-controller-adapter-01";
}

} // namespace PelcoD::Onvif
