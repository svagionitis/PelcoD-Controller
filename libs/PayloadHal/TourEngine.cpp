/// @file TourEngine.cpp
/// @brief Implementation of the automated cyclical guard patrol and tour execution engine.

#include "TourEngine.h"

#include <cmath>

namespace PayloadHal {

namespace {

double normalizeAngleDeg(double angle)
{
    while (angle > 180.0) {
        angle -= 360.0;
    }
    while (angle < -180.0) {
        angle += 360.0;
    }
    return angle;
}

double angularDiffDeg(double a, double b)
{
    return std::abs(normalizeAngleDeg(a - b));
}

} // namespace

TourEngine::TourEngine(std::shared_ptr<IPanTiltUnit> ptu,
                       std::shared_ptr<ICameraPayload> camera,
                       std::shared_ptr<IPtzPresetManager> presetMgr)
    : m_ptu(std::move(ptu))
    , m_camera(std::move(camera))
    , m_presetMgr(std::move(presetMgr))
{
    m_running = true;
    m_workerThread = std::thread(&TourEngine::executionLoop, this);
}

TourEngine::~TourEngine()
{
    stopTour();
    m_running = false;
    m_cv.notify_all();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

bool TourEngine::registerTour(const TourDefinition& tour)
{
    if (tour.tourId.empty() || tour.waypoints.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tours[tour.tourId] = tour;
    return true;
}

bool TourEngine::removeTour(const std::string& tourId)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_status.activeTourId == tourId && m_status.state != TourState::Idle) {
        lock.unlock();
        stopTour();
        lock.lock();
    }
    return m_tours.erase(tourId) > 0U;
}

std::optional<TourDefinition> TourEngine::getTour(const std::string& tourId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_tours.find(tourId);
    if (it != m_tours.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<TourDefinition> TourEngine::listTours() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<TourDefinition> list;
    list.reserve(m_tours.size());
    for (const auto& [id, tour] : m_tours) {
        list.push_back(tour);
    }
    return list;
}

bool TourEngine::startTour(const std::string& tourId)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    const auto it = m_tours.find(tourId);
    if (it == m_tours.end() || it->second.waypoints.empty()) {
        return false;
    }

    const auto& tour = it->second;
    m_status.state = TourState::SlewingToWaypoint;
    m_status.activeTourId = tourId;
    m_status.currentWaypointIndex = 0U;
    m_status.currentPresetId = tour.waypoints[0].presetId;
    m_status.completedLoops = 0U;
    m_status.remainingDwell = tour.waypoints[0].dwellTime;
    m_status.statusMessage = "Starting tour: " + tour.name;

    const auto presetId = tour.waypoints[0].presetId;
    const auto speed = tour.waypoints[0].slewSpeedRatio;
    auto mgr = m_presetMgr;
    auto cb = m_callback;
    auto stCopy = m_status;
    lock.unlock();

    if (mgr) {
        mgr->recallPreset(presetId, speed);
    }
    if (cb) {
        cb(stCopy);
    }

    m_cv.notify_all();
    return true;
}

bool TourEngine::pauseTour()
{
    StatusCallback cb;
    TourStatus stCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_status.state != TourState::SlewingToWaypoint && m_status.state != TourState::DwellingAtWaypoint) {
            return false;
        }
        m_status.state = TourState::Paused;
        m_status.statusMessage = "Tour paused by user or override";
        cb = m_callback;
        stCopy = m_status;
    }

    if (cb) {
        cb(stCopy);
    }
    return true;
}

bool TourEngine::resumeTour()
{
    std::uint32_t presetId = 0U;
    std::shared_ptr<IPtzPresetManager> mgr;
    StatusCallback cb;
    TourStatus stCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_status.state != TourState::Paused) {
            return false;
        }
        m_status.state = TourState::SlewingToWaypoint;
        m_status.statusMessage = "Tour resumed";
        presetId = m_status.currentPresetId;
        mgr = m_presetMgr;
        cb = m_callback;
        stCopy = m_status;
    }

    if (mgr && presetId > 0U) {
        mgr->recallPreset(presetId, 1.0f);
    }
    if (cb) {
        cb(stCopy);
    }

    m_cv.notify_all();
    return true;
}

void TourEngine::stopTour()
{
    StatusCallback cb;
    TourStatus stCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_status.state == TourState::Idle) {
            return;
        }
        m_status.state = TourState::Idle;
        m_status.activeTourId.clear();
        m_status.statusMessage = "Tour stopped";
        cb = m_callback;
        stCopy = m_status;
    }

    if (cb) {
        cb(stCopy);
    }
    m_cv.notify_all();
}

void TourEngine::notifyManualIntervention()
{
    StatusCallback cb;
    TourStatus stCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_status.state != TourState::SlewingToWaypoint && m_status.state != TourState::DwellingAtWaypoint) {
            return;
        }
        m_status.state = TourState::Paused;
        m_status.statusMessage = "Tour paused: manual operator intervention detected";
        cb = m_callback;
        stCopy = m_status;
    }

    if (cb) {
        cb(stCopy);
    }
}

TourStatus TourEngine::status() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

void TourEngine::registerStatusCallback(StatusCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = std::move(cb);
}

void TourEngine::setArrivalThresholdDeg(double degrees) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_arrivalThresholdDeg = std::max(0.05, degrees);
}

double TourEngine::arrivalThresholdDeg() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_arrivalThresholdDeg;
}

bool TourEngine::checkArrival(const PtzPreset& target)
{
    if (!m_ptu) {
        return true;
    }
    const auto telem = m_ptu->currentTelemetry();
    const double panErr = angularDiffDeg(telem.panAngleDeg, target.panAngleDeg);
    const double tiltErr = std::abs(telem.tiltAngleDeg - target.tiltAngleDeg);

    return (panErr <= m_arrivalThresholdDeg && tiltErr <= m_arrivalThresholdDeg);
}

void TourEngine::executionLoop()
{
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait_for(lock, std::chrono::milliseconds(50), [this] {
            return !m_running || (m_status.state != TourState::Idle && m_status.state != TourState::Paused
                                  && m_status.state != TourState::Completed);
        });

        if (!m_running) {
            break;
        }

        if (m_status.state == TourState::Idle || m_status.state == TourState::Paused
            || m_status.state == TourState::Completed) {
            continue;
        }

        const auto tourIt = m_tours.find(m_status.activeTourId);
        if (tourIt == m_tours.end() || tourIt->second.waypoints.empty()) {
            m_status.state = TourState::Fault;
            m_status.statusMessage = "Tour definition missing";
            continue;
        }

        const auto& tour = tourIt->second;
        if (m_status.currentWaypointIndex >= tour.waypoints.size()) {
            m_status.currentWaypointIndex = 0U;
        }
        const auto& currentWp = tour.waypoints[m_status.currentWaypointIndex];

        // 1. Slewing State: Check if waypoint arrived
        if (m_status.state == TourState::SlewingToWaypoint) {
            auto mgr = m_presetMgr;
            const auto targetPresetOpt = mgr ? mgr->getPreset(currentWp.presetId) : std::nullopt;
            if (!targetPresetOpt.has_value()) {
                m_status.state = TourState::Fault;
                m_status.statusMessage = "Preset not found: " + std::to_string(currentWp.presetId);
                continue;
            }

            const bool arrived = checkArrival(*targetPresetOpt);
            if (arrived) {
                m_status.state = TourState::DwellingAtWaypoint;
                m_dwellStartTime = std::chrono::steady_clock::now();
                m_status.remainingDwell = currentWp.dwellTime;
                m_status.statusMessage = "Dwelling at waypoint " + std::to_string(m_status.currentWaypointIndex + 1);

                const auto cb = m_callback;
                const auto stCopy = m_status;
                lock.unlock();
                if (cb) {
                    cb(stCopy);
                }
            }
            continue;
        }

        // 2. Dwelling State: Countdown dwell hold timer
        if (m_status.state == TourState::DwellingAtWaypoint) {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_dwellStartTime);
            if (elapsed >= currentWp.dwellTime) {
                // Advance to next waypoint
                m_status.currentWaypointIndex++;
                if (m_status.currentWaypointIndex >= tour.waypoints.size()) {
                    m_status.completedLoops++;
                    if (!tour.loop || (tour.maxLoops > 0U && m_status.completedLoops >= tour.maxLoops)) {
                        m_status.state = TourState::Completed;
                        m_status.statusMessage = "Tour completed successfully";
                        const auto cb = m_callback;
                        const auto stCopy = m_status;
                        lock.unlock();
                        if (cb) {
                            cb(stCopy);
                        }
                        continue;
                    }
                    m_status.currentWaypointIndex = 0U;
                }

                const auto& nextWp = tour.waypoints[m_status.currentWaypointIndex];
                m_status.currentPresetId = nextWp.presetId;
                m_status.state = TourState::SlewingToWaypoint;
                m_status.remainingDwell = nextWp.dwellTime;
                m_status.statusMessage = "Slewing to waypoint " + std::to_string(m_status.currentWaypointIndex + 1);

                const auto nextPresetId = nextWp.presetId;
                const auto nextSpeed = nextWp.slewSpeedRatio;
                auto mgr = m_presetMgr;
                const auto cb = m_callback;
                const auto stCopy = m_status;
                lock.unlock();

                if (mgr) {
                    mgr->recallPreset(nextPresetId, nextSpeed);
                }
                if (cb) {
                    cb(stCopy);
                }
            } else {
                m_status.remainingDwell = currentWp.dwellTime - elapsed;
            }
        }
    }
}

} // namespace PayloadHal
