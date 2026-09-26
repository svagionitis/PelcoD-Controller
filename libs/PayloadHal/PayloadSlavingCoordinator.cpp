/// @file PayloadSlavingCoordinator.cpp
/// @brief Implementation of Multi-Payload Master/Slave Slaving and Predictive Blind-Zone Handover coordinator.

#include "PayloadSlavingCoordinator.h"
#include "GimbalSectorBlanking.h"
#include "ILaserRangeFinder.h"
#include "IPanTiltUnit.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace PayloadHal {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kEpsilon = 1e-9;

double degToRad(double deg) noexcept
{
    return deg * kDegToRad;
}

double radToDeg(double rad) noexcept
{
    return rad * kRadToDeg;
}

double normalizeAngleDeg(double angle) noexcept
{
    while (angle > 180.0) {
        angle -= 360.0;
    }
    while (angle < -180.0) {
        angle += 360.0;
    }
    return angle;
}

struct Mat3x3 {
    double m[3][3] { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };

    Vector3D multiply(const Vector3D& v) const noexcept
    {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        };
    }

    Mat3x3 transpose() const noexcept
    {
        Mat3x3 res {};
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                res.m[r][c] = m[c][r];
            }
        }
        return res;
    }
};

Mat3x3 eulerZyxToMatrix(double yawDeg, double pitchDeg, double rollDeg) noexcept
{
    const double psi = degToRad(yawDeg);
    const double theta = degToRad(pitchDeg);
    const double phi = degToRad(rollDeg);

    const double cPsi = std::cos(psi);
    const double sPsi = std::sin(psi);
    const double cTheta = std::cos(theta);
    const double sTheta = std::sin(theta);
    const double cPhi = std::cos(phi);
    const double sPhi = std::sin(phi);

    Mat3x3 r {};
    r.m[0][0] = cPsi * cTheta;
    r.m[0][1] = cPsi * sTheta * sPhi - sPsi * cPhi;
    r.m[0][2] = cPsi * sTheta * cPhi + sPsi * sPhi;

    r.m[1][0] = sPsi * cTheta;
    r.m[1][1] = sPsi * sTheta * sPhi + cPsi * cPhi;
    r.m[1][2] = sPsi * sTheta * cPhi - cPsi * sPhi;

    r.m[2][0] = -sTheta;
    r.m[2][1] = cTheta * sPhi;
    r.m[2][2] = cTheta * cPhi;

    return r;
}

Vector3D gimbalAnglesToUnitVector(double panDeg, double tiltDeg) noexcept
{
    const double panRad = degToRad(panDeg);
    const double tiltRad = degToRad(tiltDeg);

    const double cTilt = std::cos(tiltRad);
    return {
        cTilt * std::cos(panRad),
        cTilt * std::sin(panRad),
        -std::sin(tiltRad)
    };
}

void unitVectorToGimbalAngles(const Vector3D& v, double& panDeg, double& tiltDeg) noexcept
{
    const double horiz = std::sqrt(v.x * v.x + v.y * v.y);
    if (horiz < kEpsilon && std::abs(v.z) < kEpsilon) {
        panDeg = 0.0;
        tiltDeg = 0.0;
        return;
    }

    panDeg = radToDeg(std::atan2(v.y, v.x));
    tiltDeg = radToDeg(std::atan2(-v.z, horiz));
    panDeg = normalizeAngleDeg(panDeg);
}

} // namespace

PayloadSlavingCoordinator::PayloadSlavingCoordinator() = default;

PayloadSlavingCoordinator::~PayloadSlavingCoordinator() = default;

bool PayloadSlavingCoordinator::registerStation(const PayloadStationRecord& record)
{
    if (record.stationId.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stations.find(record.stationId) != m_stations.end()) {
        return false;
    }

    m_stations[record.stationId] = record;
    if (record.isMaster && record.capability.canBeMaster) {
        // Demote any existing master
        for (auto& [id, st] : m_stations) {
            if (id != record.stationId) {
                st.isMaster = false;
            }
        }
        m_masterId = record.stationId;
    }

    return true;
}

bool PayloadSlavingCoordinator::unregisterStation(const std::string& stationId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stations.find(stationId);
    if (it == m_stations.end()) {
        return false;
    }

    if (m_masterId == stationId) {
        m_masterId.clear();
        m_handoverState = HandoverState::Idle;
    }

    if (m_candidateSlaveId == stationId) {
        m_candidateSlaveId.clear();
        m_handoverState = HandoverState::Idle;
    }

    m_stations.erase(it);
    return true;
}

std::optional<PayloadStationRecord> PayloadSlavingCoordinator::station(const std::string& stationId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stations.find(stationId);
    if (it != m_stations.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<PayloadStationRecord> PayloadSlavingCoordinator::stations() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<PayloadStationRecord> result;
    result.reserve(m_stations.size());
    for (const auto& [_, st] : m_stations) {
        result.push_back(st);
    }
    return result;
}

void PayloadSlavingCoordinator::clearStations()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stations.clear();
    m_masterId.clear();
    m_candidateSlaveId.clear();
    m_handoverState = HandoverState::Idle;
}

void PayloadSlavingCoordinator::setSlavingMode(SlavingMode mode) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mode = mode;
    if (m_mode == SlavingMode::Disabled) {
        m_handoverState = HandoverState::Idle;
        m_candidateSlaveId.clear();
    }
}

SlavingMode PayloadSlavingCoordinator::slavingMode() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_mode;
}

bool PayloadSlavingCoordinator::setMasterStation(const std::string& stationId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stations.find(stationId);
    if (it == m_stations.end() || !it->second.capability.canBeMaster) {
        return false;
    }

    for (auto& [_, st] : m_stations) {
        st.isMaster = false;
    }

    it->second.isMaster = true;
    it->second.isSlaved = false;
    m_masterId = stationId;
    m_handoverState = HandoverState::Tracking;
    return true;
}

std::string PayloadSlavingCoordinator::masterStationId() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_masterId;
}

bool PayloadSlavingCoordinator::setStationSlaved(const std::string& stationId, bool slaved)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stations.find(stationId);
    if (it == m_stations.end()) {
        return false;
    }

    if (slaved && !it->second.capability.canBeSlave) {
        return false;
    }

    it->second.isSlaved = slaved;
    if (slaved && it->second.isMaster) {
        it->second.isMaster = false;
        if (m_masterId == stationId) {
            m_masterId.clear();
        }
    }
    return true;
}

bool PayloadSlavingCoordinator::isStationSlaved(const std::string& stationId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stations.find(stationId);
    if (it != m_stations.end()) {
        return it->second.isSlaved;
    }
    return false;
}

void PayloadSlavingCoordinator::setGeodeticTarget(const Klv::GeoPoint3D& targetGps)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_geodeticTarget = targetGps;
}

std::optional<Klv::GeoPoint3D> PayloadSlavingCoordinator::geodeticTarget() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_geodeticTarget;
}

void PayloadSlavingCoordinator::clearGeodeticTarget()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_geodeticTarget.reset();
}

void PayloadSlavingCoordinator::setDefaultEngagementRange(double rangeMeters) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultEngagementRangeM = std::max(1.0, rangeMeters);
}

double PayloadSlavingCoordinator::defaultEngagementRange() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_defaultEngagementRangeM;
}

void PayloadSlavingCoordinator::setHandoverThresholds(double warningMarginDeg, double timeoutSec) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_warningMarginDeg = std::max(0.1, warningMarginDeg);
    m_handoverTimeoutSec = std::max(0.1, timeoutSec);
}

double PayloadSlavingCoordinator::warningMarginDeg() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_warningMarginDeg;
}

double PayloadSlavingCoordinator::handoverTimeoutSec() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_handoverTimeoutSec;
}

std::optional<SlavedLookAngles> PayloadSlavingCoordinator::computeSlaveLookAngles(
    const std::string& masterId,
    const std::string& slaveId,
    double masterPanDeg,
    double masterTiltDeg,
    double slantRangeMeters) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return computeSlaveLookAnglesLocked(masterId, slaveId, masterPanDeg, masterTiltDeg, slantRangeMeters);
}

std::optional<SlavedLookAngles> PayloadSlavingCoordinator::computeSlaveLookAnglesLocked(
    const std::string& masterId,
    const std::string& slaveId,
    double masterPanDeg,
    double masterTiltDeg,
    double slantRangeMeters) const
{
    auto mIt = m_stations.find(masterId);
    auto sIt = m_stations.find(slaveId);
    if (mIt == m_stations.end() || sIt == m_stations.end()) {
        return std::nullopt;
    }

    const auto& master = mIt->second;
    const auto& slave = sIt->second;

    const Mat3x3 rMountMaster = eulerZyxToMatrix(
        master.mountOrientation.yawDeg,
        master.mountOrientation.pitchDeg,
        master.mountOrientation.rollDeg);

    const Mat3x3 rMountSlave = eulerZyxToMatrix(
        slave.mountOrientation.yawDeg,
        slave.mountOrientation.pitchDeg,
        slave.mountOrientation.rollDeg);

    // Compute unit pointing vector of master in its local gimbal frame
    const Vector3D uGimbalM = gimbalAnglesToUnitVector(masterPanDeg, masterTiltDeg);

    // Transform into host platform body frame
    const Vector3D uBodyM = rMountMaster.multiply(uGimbalM);

    SlavedLookAngles result {};

    // Check for infinite range / collimated LOS or zero range
    const bool isCollimated = (m_mode == SlavingMode::CollimatedLOS) || (slantRangeMeters <= kEpsilon);

    if (isCollimated) {
        // Parallel pointing in platform body space
        const Mat3x3 rSlaveTranspose = rMountSlave.transpose();
        const Vector3D vGimbalS = rSlaveTranspose.multiply(uBodyM);

        double panDeg = 0.0;
        double tiltDeg = 0.0;
        unitVectorToGimbalAngles(vGimbalS, panDeg, tiltDeg);

        result.panDeg = panDeg;
        result.tiltDeg = tiltDeg;
        result.slantRangeMeters = 0.0;
        result.isCollimated = true;
    } else {
        // Target position in platform body frame
        const Vector3D pTargetBody = {
            master.platformOffsetM.x + slantRangeMeters * uBodyM.x,
            master.platformOffsetM.y + slantRangeMeters * uBodyM.y,
            master.platformOffsetM.z + slantRangeMeters * uBodyM.z
        };

        // Vector from slave station to target in platform body frame
        const Vector3D vSlaveBody = {
            pTargetBody.x - slave.platformOffsetM.x,
            pTargetBody.y - slave.platformOffsetM.y,
            pTargetBody.z - slave.platformOffsetM.z
        };

        const double distS = std::sqrt(
            vSlaveBody.x * vSlaveBody.x +
            vSlaveBody.y * vSlaveBody.y +
            vSlaveBody.z * vSlaveBody.z);

        const Mat3x3 rSlaveTranspose = rMountSlave.transpose();
        const Vector3D vGimbalS = rSlaveTranspose.multiply(vSlaveBody);

        double panDeg = 0.0;
        double tiltDeg = 0.0;
        unitVectorToGimbalAngles(vGimbalS, panDeg, tiltDeg);

        result.panDeg = panDeg;
        result.tiltDeg = tiltDeg;
        result.slantRangeMeters = distS;
        result.isCollimated = false;
    }

    return result;
}

bool PayloadSlavingCoordinator::requestHandover(const std::string& targetStationId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stations.find(targetStationId);
    if (it == m_stations.end() || !it->second.capability.canBeMaster || targetStationId == m_masterId) {
        return false;
    }

    m_candidateSlaveId = targetStationId;
    m_handoverState = HandoverState::CueingSlave;
    m_handoverTimerSec = 0.0;
    m_lastHandoverReason = "Operator manual request";

    notifyHandoverStateLocked(m_masterId, m_candidateSlaveId, HandoverState::CueingSlave);
    return true;
}

void PayloadSlavingCoordinator::cancelHandover()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_handoverState != HandoverState::Idle && m_handoverState != HandoverState::Tracking) {
        const std::string prevCand = m_candidateSlaveId;
        m_candidateSlaveId.clear();
        m_handoverState = HandoverState::Tracking;
        m_lastHandoverReason = "Handover cancelled by operator";
        notifyHandoverStateLocked(m_masterId, prevCand, HandoverState::Tracking);
    }
}

HandoverState PayloadSlavingCoordinator::handoverState() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_handoverState;
}

SlavingStatusReport PayloadSlavingCoordinator::statusReport() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    SlavingStatusReport rep {};
    rep.mode = m_mode;
    rep.activeMasterId = m_masterId;
    rep.handoverState = m_handoverState;
    rep.candidateSlaveId = m_candidateSlaveId;
    rep.lastHandoverReason = m_lastHandoverReason;

    for (const auto& [id, st] : m_stations) {
        if (st.isSlaved) {
            rep.activeSlaveIds.push_back(id);
        }
    }

    if (!m_masterId.empty()) {
        auto mIt = m_stations.find(m_masterId);
        if (mIt != m_stations.end() && mIt->second.payload && mIt->second.payload->panTilt()) {
            const auto tel = mIt->second.payload->panTilt()->currentTelemetry();
            rep.masterDistanceToBlindZoneDeg = distanceToNearestBlindZoneLocked(m_masterId, tel.panAngleDeg, tel.tiltAngleDeg);
        }
    }

    return rep;
}

void PayloadSlavingCoordinator::setHandoverCallback(HandoverCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handoverCallback = std::move(callback);
}

void PayloadSlavingCoordinator::notifyHandoverStateLocked(
    const std::string& oldMaster, const std::string& newMaster, HandoverState state)
{
    if (m_handoverCallback) {
        m_handoverCallback(oldMaster, newMaster, state);
    }
}

bool PayloadSlavingCoordinator::isStationClearOfBlindZonesLocked(
    const std::string& stationId, double panDeg, double tiltDeg) const
{
    auto it = m_stations.find(stationId);
    if (it == m_stations.end() || !it->second.payload) {
        return true;
    }

    const auto& st = it->second;
    if (st.payload->sectorBlanking()) {
        const auto res = st.payload->sectorBlanking()->evaluate(panDeg, tiltDeg);
        if (!res.mechanicalAllowed || res.inWarningMargin) {
            return false;
        }
    }

    if (st.payload->panTilt()) {
        double minPan = 0.0, maxPan = 0.0, minTilt = 0.0, maxTilt = 0.0;
        if (st.payload->panTilt()->getLimits(minPan, maxPan, minTilt, maxTilt)) {
            if (maxPan - minPan < 360.0) {
                if (panDeg < minPan || panDeg > maxPan) {
                    return false;
                }
            }
            if (tiltDeg < minTilt || tiltDeg > maxTilt) {
                return false;
            }
        }
    }

    return true;
}

double PayloadSlavingCoordinator::distanceToNearestBlindZoneLocked(
    const std::string& stationId, double panDeg, double tiltDeg) const
{
    auto it = m_stations.find(stationId);
    if (it == m_stations.end() || !it->second.payload) {
        return 999.0;
    }

    double minDist = 999.0;
    const auto& st = it->second;

    if (st.payload->sectorBlanking()) {
        const auto res = st.payload->sectorBlanking()->evaluate(panDeg, tiltDeg);
        minDist = std::min(minDist, res.distanceToNearestZoneDeg);
    }

    if (st.payload->panTilt()) {
        double minPan = 0.0, maxPan = 0.0, minTilt = 0.0, maxTilt = 0.0;
        if (st.payload->panTilt()->getLimits(minPan, maxPan, minTilt, maxTilt)) {
            if (maxPan - minPan < 360.0) {
                const double distPanMin = std::abs(panDeg - minPan);
                const double distPanMax = std::abs(maxPan - panDeg);
                minDist = std::min({minDist, distPanMin, distPanMax});
            }
            const double distTiltMin = std::abs(tiltDeg - minTilt);
            const double distTiltMax = std::abs(maxTilt - tiltDeg);

            minDist = std::min({minDist, distTiltMin, distTiltMax});
        }
    }

    return minDist;
}

std::string PayloadSlavingCoordinator::selectBestCandidateSlaveLocked(
    const std::string& currentMasterId,
    double targetPanDeg,
    double targetTiltDeg,
    double slantRangeMeters) const
{
    std::string bestCandidateId {};
    double minSlewDist = std::numeric_limits<double>::max();

    for (const auto& [id, st] : m_stations) {
        if (id == currentMasterId || !st.isEnabled || !st.capability.canBeMaster) {
            continue;
        }

        // Compute where this candidate would have to look
        const auto lookOpt = computeSlaveLookAnglesLocked(currentMasterId, id, targetPanDeg, targetTiltDeg, slantRangeMeters);
        if (!lookOpt.has_value()) {
            continue;
        }

        // Verify that the required angles are clear of keep-out zones
        if (!isStationClearOfBlindZonesLocked(id, lookOpt->panDeg, lookOpt->tiltDeg)) {
            continue;
        }

        // Compute slew distance from current position
        double currPan = 0.0;
        double currTilt = 0.0;
        if (st.payload && st.payload->panTilt()) {
            const auto tel = st.payload->panTilt()->currentTelemetry();
            currPan = tel.panAngleDeg;
            currTilt = tel.tiltAngleDeg;
        }

        const double dPan = normalizeAngleDeg(lookOpt->panDeg - currPan);
        const double dTilt = lookOpt->tiltDeg - currTilt;
        const double slewDist = std::sqrt(dPan * dPan + dTilt * dTilt);

        if (slewDist < minSlewDist) {
            minSlewDist = slewDist;
            bestCandidateId = id;
        }
    }

    return bestCandidateId;
}

void PayloadSlavingCoordinator::update(double dtSeconds)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_mode == SlavingMode::Disabled || m_masterId.empty()) {
        return;
    }

    auto mIt = m_stations.find(m_masterId);
    if (mIt == m_stations.end() || !mIt->second.payload) {
        return;
    }

    const auto& master = mIt->second;

    // Retrieve master's current orientation
    double masterPan = 0.0;
    double masterTilt = 0.0;
    if (master.payload->panTilt()) {
        const auto tel = master.payload->panTilt()->currentTelemetry();
        masterPan = tel.panAngleDeg;
        masterTilt = tel.tiltAngleDeg;
    }

    // Retrieve master's slant range
    double rangeM = m_defaultEngagementRangeM;
    if (master.payload->lrf()) {
        const auto lastM = master.payload->lrf()->lastMeasurement();
        if (lastM.has_value() && lastM->slantRangeMeters > 0.0) {
            rangeM = lastM->slantRangeMeters;
        }
    }

    // --- Autonomous Blind-Zone Handover State Machine ---
    if (m_mode == SlavingMode::AutonomousHandoff) {
        const double distToBlind = distanceToNearestBlindZoneLocked(m_masterId, masterPan, masterTilt);

        switch (m_handoverState) {
        case HandoverState::Idle:
        case HandoverState::Tracking: {
            if (distToBlind <= m_warningMarginDeg) {
                m_handoverState = HandoverState::ApproachingBlindZone;
                m_lastHandoverReason = "Master approaching blind zone or travel limit";
                notifyHandoverStateLocked(m_masterId, "", HandoverState::ApproachingBlindZone);

                const std::string bestSlave = selectBestCandidateSlaveLocked(m_masterId, masterPan, masterTilt, rangeM);
                if (!bestSlave.empty()) {
                    m_candidateSlaveId = bestSlave;
                    m_handoverState = HandoverState::CueingSlave;
                    m_handoverTimerSec = 0.0;
                    notifyHandoverStateLocked(m_masterId, m_candidateSlaveId, HandoverState::CueingSlave);
                } else {
                    m_handoverState = HandoverState::HandoffFailed;
                    m_lastHandoverReason = "No unobstructed candidate slave available";
                    notifyHandoverStateLocked(m_masterId, "", HandoverState::HandoffFailed);
                }
            } else {
                m_handoverState = HandoverState::Tracking;
            }
            break;
        }

        case HandoverState::CueingSlave:
        case HandoverState::SlaveConverging: {
            m_handoverState = HandoverState::SlaveConverging;
            m_handoverTimerSec += dtSeconds;

            auto cIt = m_stations.find(m_candidateSlaveId);
            if (cIt == m_stations.end() || !cIt->second.payload || !cIt->second.payload->panTilt()) {
                m_handoverState = HandoverState::HandoffFailed;
                m_lastHandoverReason = "Candidate slave lost or invalid";
                notifyHandoverStateLocked(m_masterId, m_candidateSlaveId, HandoverState::HandoffFailed);
                break;
            }

            const auto lookOpt = computeSlaveLookAnglesLocked(m_masterId, m_candidateSlaveId, masterPan, masterTilt, rangeM);
            if (!lookOpt.has_value()) {
                m_handoverState = HandoverState::HandoffFailed;
                break;
            }

            // Command candidate slave to slew
            cIt->second.payload->panTilt()->setAbsoluteAngles(lookOpt->panDeg, lookOpt->tiltDeg);

            // Check alignment error
            const auto candTel = cIt->second.payload->panTilt()->currentTelemetry();
            const double errPan = normalizeAngleDeg(candTel.panAngleDeg - lookOpt->panDeg);
            const double errTilt = candTel.tiltAngleDeg - lookOpt->tiltDeg;
            const double totalErr = std::sqrt(errPan * errPan + errTilt * errTilt);

            if (totalErr <= cIt->second.capability.lockToleranceDeg) {
                // Convergence achieved! Transfer control atomically
                m_handoverState = HandoverState::TransferringControl;
                notifyHandoverStateLocked(m_masterId, m_candidateSlaveId, HandoverState::TransferringControl);

                const std::string oldMasterId = m_masterId;
                const std::string newMasterId = m_candidateSlaveId;

                // Promote candidate to master
                auto oldIt = m_stations.find(oldMasterId);
                if (oldIt != m_stations.end()) {
                    oldIt->second.isMaster = false;
                    oldIt->second.isSlaved = true; // Former master becomes slaved
                }

                cIt->second.isMaster = true;
                cIt->second.isSlaved = false;
                m_masterId = newMasterId;
                m_candidateSlaveId.clear();
                m_handoverState = HandoverState::HandoverComplete;
                m_lastHandoverReason = "Successful blind-zone handover";

                notifyHandoverStateLocked(oldMasterId, newMasterId, HandoverState::HandoverComplete);
            } else if (m_handoverTimerSec > m_handoverTimeoutSec) {
                m_handoverState = HandoverState::HandoffFailed;
                m_lastHandoverReason = "Candidate slave convergence timed out";
                notifyHandoverStateLocked(m_masterId, m_candidateSlaveId, HandoverState::HandoffFailed);
            }
            break;
        }

        case HandoverState::HandoverComplete: {
            m_handoverState = HandoverState::Tracking;
            break;
        }

        case HandoverState::HandoffFailed: {
            if (distToBlind > m_warningMarginDeg) {
                m_handoverState = HandoverState::Tracking;
            }
            break;
        }

        default:
            break;
        }
    }

    // --- Command Slaves to Track Target ---
    for (auto& [id, st] : m_stations) {
        if (!st.isSlaved || id == m_masterId || !st.payload || !st.payload->panTilt()) {
            continue;
        }

        const auto lookOpt = computeSlaveLookAnglesLocked(m_masterId, id, masterPan, masterTilt, rangeM);
        if (lookOpt.has_value()) {
            st.payload->panTilt()->setAbsoluteAngles(lookOpt->panDeg, lookOpt->tiltDeg);
        }
    }
}

} // namespace PayloadHal
