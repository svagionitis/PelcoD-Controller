#include "NmeaSlavingBridge.h"

#include <cmath>

namespace PayloadHal {

namespace {
    constexpr double kEarthRadiusMeters = 6371000.0;
    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
    constexpr double kKnotsToMps = 0.5144444444444444;
} // namespace

NmeaSlavingBridge::NmeaSlavingBridge(std::shared_ptr<Nmea::NmeaDevice> nmeaDevice,
    std::shared_ptr<GeoLockController> geoLockController, std::shared_ptr<PlatformLeverArmCompensator> compensator)
    : m_nmeaDevice(std::move(nmeaDevice))
    , m_geoLockController(std::move(geoLockController))
    , m_compensator(std::move(compensator))
{
    if (m_nmeaDevice) {
        m_navSubId = m_nmeaDevice->addNavCallback([this](const Nmea::NmeaNavSnapshot& snap) { handleNavUpdate(snap); });
        m_radarSubId = m_nmeaDevice->addRadarCallback([this](const Nmea::TtmData& ttm) { handleRadarUpdate(ttm); });
        m_aisSubId
            = m_nmeaDevice->addAisCallback([this](const Nmea::AisVesselTarget& target) { handleAisUpdate(target); });

        // Query initial nav snapshot if available
        const auto snap = m_nmeaDevice->navSnapshot();
        if (snap.hasPosition && snap.hasHeading) {
            handleNavUpdate(snap);
        }
    }

    m_running.store(true);
    m_workerThread = std::thread(&NmeaSlavingBridge::workerLoop, this);
}

NmeaSlavingBridge::~NmeaSlavingBridge()
{
    m_running.store(false);
    {
        std::lock_guard<std::mutex> lock(m_workerMutex);
        m_cv.notify_all();
    }
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    if (m_nmeaDevice) {
        if (m_navSubId != 0U) {
            m_nmeaDevice->removeNavCallback(m_navSubId);
        }
        if (m_radarSubId != 0U) {
            m_nmeaDevice->removeRadarCallback(m_radarSubId);
        }
        if (m_aisSubId != 0U) {
            m_nmeaDevice->removeAisCallback(m_aisSubId);
        }
    }
}

bool NmeaSlavingBridge::slaveToRadarTarget(std::uint32_t targetNumber)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_targetType = MarineTargetType::RadarArpa;
    m_targetId = targetNumber;
    m_targetName.clear();

    if (m_nmeaDevice) {
        const auto targetOpt = m_nmeaDevice->radarTarget(targetNumber);
        if (targetOpt.has_value()) {
            const auto& ttm = *targetOpt;
            m_targetName = ttm.targetName;
            m_targetSogKnots = ttm.targetSpeedKnots;
            m_targetCogDegrees = ttm.targetCourseDegrees;
            m_lastContactTime = std::chrono::steady_clock::now();

            if (m_hasPlatformNav && m_geoLockController) {
                double trueBearing = ttm.bearingDegrees;
                if (ttm.bearingReference == Nmea::TtmReference::Relative) {
                    trueBearing = std::fmod(m_platformHeadingDeg + ttm.bearingDegrees, 360.0);
                    if (trueBearing < 0.0) {
                        trueBearing += 360.0;
                    }
                }
                const double rangeMeters = ttm.targetDistanceNmi * 1852.0;
                m_lastKnownTargetPos = projectTargetFromRadar(m_platformPos, rangeMeters, trueBearing);
                (void)m_geoLockController->engage(m_lastKnownTargetPos);
            }
        }
    }

    return true;
}

bool NmeaSlavingBridge::slaveToAisVessel(std::uint32_t mmsi)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_targetType = MarineTargetType::AisVessel;
    m_targetId = mmsi;
    m_targetName.clear();

    if (m_nmeaDevice) {
        const auto targetOpt = m_nmeaDevice->aisTarget(mmsi);
        if (targetOpt.has_value()) {
            const auto& ais = *targetOpt;
            m_targetName = ais.vesselName.empty() ? ais.callSign : ais.vesselName;
            m_targetSogKnots = ais.speedOverGroundKnots;
            m_targetCogDegrees = ais.courseOverGroundDegrees;
            m_lastContactTime = std::chrono::steady_clock::now();

            if (ais.positionValid && m_geoLockController) {
                m_lastKnownTargetPos.latitudeDeg = ais.coordinates.latitudeDeg;
                m_lastKnownTargetPos.longitudeDeg = ais.coordinates.longitudeDeg;
                m_lastKnownTargetPos.altitudeM = 0.0;
                (void)m_geoLockController->engage(m_lastKnownTargetPos);
            }
        }
    }

    return true;
}

bool NmeaSlavingBridge::slaveToGeodeticTarget(const Klv::GeoPoint3D& target)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_targetType = MarineTargetType::GeodeticManual;
    m_targetId = 0U;
    m_targetName = "Manual Geodetic Target";
    m_lastKnownTargetPos = target;
    m_lastContactTime = std::chrono::steady_clock::now();
    m_targetSogKnots = 0.0;
    m_targetCogDegrees = 0.0;

    if (m_geoLockController) {
        return m_geoLockController->engage(target);
    }
    return false;
}

void NmeaSlavingBridge::disengage()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_targetType = MarineTargetType::None;
    m_targetId = 0U;
    m_targetName.clear();

    if (m_geoLockController) {
        m_geoLockController->disengage();
    }
}

void NmeaSlavingBridge::setTargetLossPolicy(TargetLossPolicy policy) noexcept
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_lossPolicy = policy;
}

void NmeaSlavingBridge::setTargetTimeout(std::chrono::milliseconds timeout) noexcept
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_targetTimeout = timeout;
}

void NmeaSlavingBridge::setPredictiveCoasting(bool enabled) noexcept
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_predictiveCoasting = enabled;
}

MarineSlavingStatus NmeaSlavingBridge::status() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return statusLocked();
}

MarineSlavingStatus NmeaSlavingBridge::statusLocked() const
{
    MarineSlavingStatus st {};
    st.active = (m_targetType != MarineTargetType::None);
    st.targetType = m_targetType;
    st.targetId = m_targetId;
    st.targetName = m_targetName;
    st.lastContact = m_lastContactTime;

    if (st.active) {
        st.targetPosition = m_lastKnownTargetPos;
        if (m_hasPlatformNav) {
            const auto lookAngles = GeoreferenceUtils::computeLookAnglesToTarget(
                m_platformPos, m_platformHeadingDeg, m_lastKnownTargetPos);
            st.slantRangeMeters = lookAngles.slantRangeMeters;
            st.trueBearingDeg = lookAngles.trueBearingDeg;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = now - m_lastContactTime;
        st.isCoasting = (elapsed > std::chrono::seconds(1));
    }

    return st;
}

bool NmeaSlavingBridge::isSlaving() const noexcept
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return (m_targetType != MarineTargetType::None);
}

void NmeaSlavingBridge::update()
{
    MarineSlavingStatus currentStatus {};
    bool notifyStatus { false };
    bool targetLost { false };
    MarineTargetType lostType { MarineTargetType::None };
    std::uint32_t lostId { 0U };

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_targetType == MarineTargetType::None) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = now - m_lastContactTime;

        if (elapsed > m_targetTimeout) {
            targetLost = true;
            lostType = m_targetType;
            lostId = m_targetId;

            if (m_lossPolicy == TargetLossPolicy::Disengage) {
                m_targetType = MarineTargetType::None;
                if (m_geoLockController) {
                    m_geoLockController->disengage();
                }
            }
        } else if (m_predictiveCoasting && m_targetSogKnots > 0.1 && elapsed > std::chrono::milliseconds(500)) {
            // Dead-reckon target position forward along SOG/COG
            const double dtSec = std::chrono::duration<double>(elapsed).count();
            const double distMeters = m_targetSogKnots * kKnotsToMps * dtSec;
            const auto deadReckonedPos = projectTargetFromRadar(m_lastKnownTargetPos, distMeters, m_targetCogDegrees);

            if (m_geoLockController) {
                (void)m_geoLockController->engage(deadReckonedPos);
            }
        }

        currentStatus = statusLocked();
        notifyStatus = true;
    }

    if (targetLost) {
        std::shared_ptr<const std::vector<std::pair<std::size_t, TargetLostCallback>>> lostCbs;
        {
            std::lock_guard<std::mutex> cbLock(m_callbackMutex);
            lostCbs = m_lostCallbacks;
        }
        for (const auto& item : *lostCbs) {
            if (item.second) {
                item.second(lostType, lostId);
            }
        }
    }

    if (notifyStatus) {
        std::shared_ptr<const std::vector<std::pair<std::size_t, StatusCallback>>> statusCbs;
        {
            std::lock_guard<std::mutex> cbLock(m_callbackMutex);
            statusCbs = m_statusCallbacks;
        }
        for (const auto& item : *statusCbs) {
            if (item.second) {
                item.second(currentStatus);
            }
        }
    }
}

std::size_t NmeaSlavingBridge::addStatusCallback(StatusCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    const std::size_t id = m_nextCallbackId++;
    auto newEntries = std::make_shared<std::vector<std::pair<std::size_t, StatusCallback>>>(*m_statusCallbacks);
    newEntries->emplace_back(id, std::move(cb));
    m_statusCallbacks = newEntries;
    return id;
}

void NmeaSlavingBridge::removeStatusCallback(std::size_t id)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    auto newEntries = std::make_shared<std::vector<std::pair<std::size_t, StatusCallback>>>();
    newEntries->reserve(m_statusCallbacks->size());
    for (const auto& item : *m_statusCallbacks) {
        if (item.first != id) {
            newEntries->push_back(item);
        }
    }
    m_statusCallbacks = newEntries;
}

std::size_t NmeaSlavingBridge::addTargetLostCallback(TargetLostCallback cb)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    const std::size_t id = m_nextCallbackId++;
    auto newEntries = std::make_shared<std::vector<std::pair<std::size_t, TargetLostCallback>>>(*m_lostCallbacks);
    newEntries->emplace_back(id, std::move(cb));
    m_lostCallbacks = newEntries;
    return id;
}

void NmeaSlavingBridge::removeTargetLostCallback(std::size_t id)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    auto newEntries = std::make_shared<std::vector<std::pair<std::size_t, TargetLostCallback>>>();
    newEntries->reserve(m_lostCallbacks->size());
    for (const auto& item : *m_lostCallbacks) {
        if (item.first != id) {
            newEntries->push_back(item);
        }
    }
    m_lostCallbacks = newEntries;
}

Klv::GeoPoint3D NmeaSlavingBridge::projectTargetFromRadar(
    const Klv::GeoPoint3D& platformPos, double rangeMeters, double trueBearingDeg) noexcept
{
    const double lat1 = platformPos.latitudeDeg * kDegToRad;
    const double lon1 = platformPos.longitudeDeg * kDegToRad;
    const double brng = trueBearingDeg * kDegToRad;
    const double dist = rangeMeters / kEarthRadiusMeters;

    const double lat2 = std::asin(std::sin(lat1) * std::cos(dist) + std::cos(lat1) * std::sin(dist) * std::cos(brng));
    const double lon2 = lon1
        + std::atan2(
            std::sin(brng) * std::sin(dist) * std::cos(lat1), std::cos(dist) - std::sin(lat1) * std::sin(lat2));

    Klv::GeoPoint3D target {};
    target.latitudeDeg = lat2 * kRadToDeg;
    target.longitudeDeg = lon2 * kRadToDeg;
    target.altitudeM = 0.0;
    return target;
}

void NmeaSlavingBridge::handleNavUpdate(const Nmea::NmeaNavSnapshot& snap)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (snap.hasPosition) {
        m_platformPos.latitudeDeg = snap.position.latitudeDeg;
        m_platformPos.longitudeDeg = snap.position.longitudeDeg;
        m_platformPos.altitudeM = snap.altitudeMeters;
    }
    if (snap.hasHeading) {
        m_platformHeadingDeg = snap.trueHeadingDegrees;
    }
    m_hasPlatformNav = snap.hasPosition && snap.hasHeading;

    if (m_hasPlatformNav && m_geoLockController) {
        (void)m_geoLockController->updatePlatform(m_platformPos, m_platformHeadingDeg);
    }
}

void NmeaSlavingBridge::handleRadarUpdate(const Nmea::TtmData& ttm)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_targetType != MarineTargetType::RadarArpa || m_targetId != ttm.targetNumber) {
        return;
    }

    m_lastContactTime = std::chrono::steady_clock::now();
    m_targetName = ttm.targetName;
    m_targetSogKnots = ttm.targetSpeedKnots;
    m_targetCogDegrees = ttm.targetCourseDegrees;

    if (m_hasPlatformNav && m_geoLockController) {
        double trueBearing = ttm.bearingDegrees;
        if (ttm.bearingReference == Nmea::TtmReference::Relative) {
            trueBearing = std::fmod(m_platformHeadingDeg + ttm.bearingDegrees, 360.0);
            if (trueBearing < 0.0) {
                trueBearing += 360.0;
            }
        }
        const double rangeMeters = ttm.targetDistanceNmi * 1852.0;
        m_lastKnownTargetPos = projectTargetFromRadar(m_platformPos, rangeMeters, trueBearing);
        (void)m_geoLockController->engage(m_lastKnownTargetPos);
    }
}

void NmeaSlavingBridge::handleAisUpdate(const Nmea::AisVesselTarget& target)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_targetType != MarineTargetType::AisVessel || m_targetId != target.mmsi) {
        return;
    }

    m_lastContactTime = std::chrono::steady_clock::now();
    m_targetName = target.vesselName.empty() ? target.callSign : target.vesselName;
    m_targetSogKnots = target.speedOverGroundKnots;
    m_targetCogDegrees = target.courseOverGroundDegrees;

    if (target.positionValid && m_geoLockController) {
        m_lastKnownTargetPos.latitudeDeg = target.coordinates.latitudeDeg;
        m_lastKnownTargetPos.longitudeDeg = target.coordinates.longitudeDeg;
        m_lastKnownTargetPos.altitudeM = 0.0;
        (void)m_geoLockController->engage(m_lastKnownTargetPos);
    }
}

void NmeaSlavingBridge::workerLoop()
{
    while (m_running.load()) {
        update();

        std::unique_lock<std::mutex> lk(m_workerMutex);
        m_cv.wait_for(lk, std::chrono::milliseconds(50), [this]() { return !m_running.load(); });
    }
}

} // namespace PayloadHal
