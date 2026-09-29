#include "TargetThreatEvaluator.h"
#include "NmeaSlavingBridge.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace PayloadHal {

namespace {
    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
    constexpr double kKnotsToMps = 0.5144444444444444;
    constexpr double kNmiToMeters = 1852.0;

    [[nodiscard]] double normalizeAngle180(double deg) noexcept
    {
        while (deg > 180.0) {
            deg -= 360.0;
        }
        while (deg <= -180.0) {
            deg += 360.0;
        }
        return deg;
    }

    [[nodiscard]] double normalizeAngle360(double deg) noexcept
    {
        double normalized = std::fmod(deg, 360.0);
        if (normalized < 0.0) {
            normalized += 360.0;
        }
        return normalized;
    }

    [[nodiscard]] double angleDiffDeg(double a, double b) noexcept
    {
        return std::abs(normalizeAngle180(a - b));
    }
} // namespace

TargetThreatEvaluator::TargetThreatEvaluator(const ThreatAssessmentConfig& config)
    : m_config(config)
{
}

void TargetThreatEvaluator::setConfig(const ThreatAssessmentConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

ThreatAssessmentConfig TargetThreatEvaluator::config() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void TargetThreatEvaluator::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_evaluatedTargets.clear();
}

void TargetThreatEvaluator::setThreatAlertCallback(ThreatAlertCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_alertCallback = std::move(cb);
}

void TargetThreatEvaluator::calculateCpa(const Klv::GeoPoint3D& ownPos, double ownSogKnots, double ownCogDeg,
    const Klv::GeoPoint3D& targetPos, double targetSogKnots, double targetCogDeg, double& outCpaMeters,
    double& outTcpaSeconds) noexcept
{
    const Klv::GeoPoint2D pOwn { ownPos.latitudeDeg, ownPos.longitudeDeg };
    const Klv::GeoPoint2D pTarget { targetPos.latitudeDeg, targetPos.longitudeDeg };

    const double dist = Klv::KlvGeodesy::distanceMeters(pOwn, pTarget);
    const double bearing = Klv::KlvGeodesy::bearingDeg(pOwn, pTarget);

    const double deltaE = dist * std::sin(bearing * kDegToRad);
    const double deltaN = dist * std::cos(bearing * kDegToRad);

    const double v0_E = ownSogKnots * kKnotsToMps * std::sin(ownCogDeg * kDegToRad);
    const double v0_N = ownSogKnots * kKnotsToMps * std::cos(ownCogDeg * kDegToRad);

    const double vt_E = targetSogKnots * kKnotsToMps * std::sin(targetCogDeg * kDegToRad);
    const double vt_N = targetSogKnots * kKnotsToMps * std::cos(targetCogDeg * kDegToRad);

    const double relVel_E = vt_E - v0_E;
    const double relVel_N = vt_N - v0_N;
    const double relSpeedSq = relVel_E * relVel_E + relVel_N * relVel_N;

    if (relSpeedSq < 1e-4) {
        // Stationary relative to each other; distance remains constant
        outTcpaSeconds = 0.0;
        outCpaMeters = dist;
        return;
    }

    outTcpaSeconds = -(deltaE * relVel_E + deltaN * relVel_N) / relSpeedSq;

    const double cpa_E = deltaE + relVel_E * outTcpaSeconds;
    const double cpa_N = deltaN + relVel_N * outTcpaSeconds;
    outCpaMeters = std::sqrt(cpa_E * cpa_E + cpa_N * cpa_N);
}

double TargetThreatEvaluator::computeThreatScore(double rangeMeters, double cpaMeters, double tcpaSeconds,
    bool isClosing, bool isDarkVessel, bool isEmergency, double sogKnots) const noexcept
{
    if (isEmergency) {
        return m_config.weightEmergency;
    }

    double score { 0.0 };

    // 1. Proximity score component (up to weightProximity)
    if (rangeMeters <= m_config.securityZoneRadiusMeters) {
        score += m_config.weightProximity;
    } else if (rangeMeters <= m_config.warningZoneRadiusMeters) {
        const double factor = 1.0
            - (rangeMeters - m_config.securityZoneRadiusMeters)
                / (m_config.warningZoneRadiusMeters - m_config.securityZoneRadiusMeters);
        score += factor * m_config.weightProximity;
    } else {
        // Soft decay beyond warning zone up to 10 NM
        constexpr double maxRange = 10.0 * kNmiToMeters;
        if (rangeMeters < maxRange) {
            const double factor = 1.0
                - (rangeMeters - m_config.warningZoneRadiusMeters) / (maxRange - m_config.warningZoneRadiusMeters);
            score += factor * (m_config.weightProximity * 0.25);
        }
    }

    // 2. Collision / CPA & TCPA components (only if closing)
    if (isClosing && tcpaSeconds > 0.0) {
        // CPA proximity
        if (cpaMeters <= m_config.criticalCpaMeters) {
            score += m_config.weightCpa;
        } else if (cpaMeters <= m_config.warningCpaMeters) {
            const double cpaFactor = 1.0
                - (cpaMeters - m_config.criticalCpaMeters) / (m_config.warningCpaMeters - m_config.criticalCpaMeters);
            score += cpaFactor * m_config.weightCpa;
        }

        // TCPA imminence
        if (tcpaSeconds <= m_config.criticalTcpaSeconds) {
            score += m_config.weightTcpa;
        } else if (tcpaSeconds <= m_config.warningTcpaSeconds) {
            const double tcpaFactor = 1.0
                - (tcpaSeconds - m_config.criticalTcpaSeconds)
                    / (m_config.warningTcpaSeconds - m_config.criticalTcpaSeconds);
            score += tcpaFactor * m_config.weightTcpa;
        }
    }

    // 3. Dark vessel bonus (unidentified radar contact without AIS)
    if (isDarkVessel) {
        score += m_config.weightDarkVessel;
    }

    // 4. High-Speed Craft bonus
    if (sogKnots >= m_config.highSpeedThresholdKnots) {
        const double speedRatio = std::min((sogKnots - m_config.highSpeedThresholdKnots) / 20.0, 1.0);
        score += (0.5 + 0.5 * speedRatio) * m_config.weightHighSpeed;
    }

    return score;
}

ThreatLevel TargetThreatEvaluator::determineThreatLevel(double threatScore, double rangeMeters, double cpaMeters,
    double tcpaSeconds, bool isClosing, bool isEmergency) const noexcept
{
    if (isEmergency) {
        return ThreatLevel::Emergency;
    }

    if (rangeMeters <= m_config.securityZoneRadiusMeters) {
        return ThreatLevel::Critical;
    }

    if (isClosing && tcpaSeconds > 0.0 && tcpaSeconds <= m_config.criticalTcpaSeconds
        && cpaMeters <= m_config.criticalCpaMeters) {
        return ThreatLevel::Critical;
    }

    if (threatScore >= 65.0) {
        return ThreatLevel::Critical;
    }

    if (rangeMeters <= m_config.warningZoneRadiusMeters || threatScore >= 35.0) {
        return ThreatLevel::Warning;
    }

    if (threatScore >= 12.0) {
        return ThreatLevel::Informational;
    }

    return ThreatLevel::None;
}

std::size_t TargetThreatEvaluator::evaluate(const Nmea::NmeaNavSnapshot& ownNav,
    const std::vector<Nmea::TtmData>& radarTargets, const std::vector<Nmea::AisVesselTarget>& aisTargets)
{
    if (!ownNav.hasPosition) {
        return 0U;
    }

    const Klv::GeoPoint3D ownPos { ownNav.position.latitudeDeg, ownNav.position.longitudeDeg, ownNav.altitudeMeters };
    const double ownHeading = ownNav.hasHeading ? ownNav.trueHeadingDegrees : ownNav.cogDegrees;
    const double ownSog = ownNav.sogKnots;
    const double ownCog = ownNav.cogDegrees;
    const auto now = std::chrono::steady_clock::now();

    std::vector<EvaluatedTarget> evaluatedList;
    std::unordered_set<std::uint32_t> matchedAisMmsis;
    ThreatAlertCallback alertCbCopy {};

    // 1. Process and project Radar targets
    for (const auto& ttm : radarTargets) {
        if (ttm.status == Nmea::TtmTargetStatus::Lost) {
            continue;
        }

        double trueBearing = ttm.bearingDegrees;
        if (ttm.bearingReference == Nmea::TtmReference::Relative) {
            trueBearing = normalizeAngle360(ownHeading + ttm.bearingDegrees);
        }

        const double rangeMeters = ttm.targetDistanceNmi * kNmiToMeters;
        const Klv::GeoPoint3D targetPos = NmeaSlavingBridge::projectTargetFromRadar(ownPos, rangeMeters, trueBearing);

        EvaluatedTarget ev;
        ev.targetId = ttm.targetNumber;
        ev.associatedRadarId = ttm.targetNumber;
        ev.source = TargetTrackSource::RadarArpa;
        ev.targetName = ttm.targetName.empty() ? ("RADAR_" + std::to_string(ttm.targetNumber)) : ttm.targetName;
        ev.position = targetPos;
        ev.rangeMeters = rangeMeters;
        ev.trueBearingDeg = trueBearing;
        ev.relativeBearingDeg = normalizeAngle180(trueBearing - ownHeading);
        ev.sogKnots = ttm.targetSpeedKnots;
        ev.cogDegrees = ttm.targetCourseDegrees;
        ev.lastUpdate = now;

        // Perform CPA calculation
        calculateCpa(ownPos, ownSog, ownCog, targetPos, ev.sogKnots, ev.cogDegrees, ev.cpaMeters, ev.tcpaSeconds);
        ev.isClosing = (ev.tcpaSeconds > 0.0);

        // Check for correlation against active AIS targets
        for (const auto& ais : aisTargets) {
            const Klv::GeoPoint2D pAis { ais.coordinates.latitudeDeg, ais.coordinates.longitudeDeg };
            const Klv::GeoPoint2D pRadar { targetPos.latitudeDeg, targetPos.longitudeDeg };
            const double distMeters = Klv::KlvGeodesy::distanceMeters(pRadar, pAis);
            const double speedDiff = std::abs(ttm.targetSpeedKnots - ais.speedOverGroundKnots);

            if (distMeters <= m_config.associationMaxDistMeters && speedDiff <= m_config.associationMaxSpeedKnots) {
                // If moving at appreciable speed, also test course difference
                if (ttm.targetSpeedKnots > 1.0 && ais.speedOverGroundKnots > 1.0) {
                    if (angleDiffDeg(ttm.targetCourseDegrees, ais.courseOverGroundDegrees)
                        > m_config.associationMaxCourseDeg) {
                        continue;
                    }
                }

                // Successful fusion match
                ev.source = TargetTrackSource::FusedRadarAis;
                ev.associatedAisMmsi = ais.mmsi;
                if (!ais.vesselName.empty()) {
                    ev.targetName = ais.vesselName;
                }
                matchedAisMmsis.insert(ais.mmsi);
                break;
            }
        }

        // Dark vessel condition: radar contact with no corresponding AIS
        ev.isDarkVessel = (ev.source == TargetTrackSource::RadarArpa);

        ev.threatScore = computeThreatScore(
            ev.rangeMeters, ev.cpaMeters, ev.tcpaSeconds, ev.isClosing, ev.isDarkVessel, false, ev.sogKnots);

        ev.threatLevel
            = determineThreatLevel(ev.threatScore, ev.rangeMeters, ev.cpaMeters, ev.tcpaSeconds, ev.isClosing, false);

        evaluatedList.push_back(std::move(ev));
    }

    // 2. Process AIS targets that were not already fused with radar
    for (const auto& ais : aisTargets) {
        if (matchedAisMmsis.find(ais.mmsi) != matchedAisMmsis.end()) {
            continue;
        }

        const Klv::GeoPoint3D targetPos { ais.coordinates.latitudeDeg, ais.coordinates.longitudeDeg, 0.0 };
        const Klv::GeoPoint2D pOwn { ownPos.latitudeDeg, ownPos.longitudeDeg };
        const Klv::GeoPoint2D pAis { ais.coordinates.latitudeDeg, ais.coordinates.longitudeDeg };

        const double rangeMeters = Klv::KlvGeodesy::distanceMeters(pOwn, pAis);
        const double trueBearing = Klv::KlvGeodesy::bearingDeg(pOwn, pAis);

        EvaluatedTarget ev;
        ev.targetId = ais.mmsi;
        ev.associatedAisMmsi = ais.mmsi;
        ev.source = TargetTrackSource::Ais;
        ev.targetName = ais.vesselName.empty() ? ("MMSI_" + std::to_string(ais.mmsi)) : ais.vesselName;
        ev.position = targetPos;
        ev.rangeMeters = rangeMeters;
        ev.trueBearingDeg = trueBearing;
        ev.relativeBearingDeg = normalizeAngle180(trueBearing - ownHeading);
        ev.sogKnots = ais.speedOverGroundKnots;
        ev.cogDegrees = ais.courseOverGroundDegrees;
        ev.isEmergency = ais.isEmergencyBeacon
            || (ais.beaconType != Nmea::AisBeaconType::StandardVessel && ais.beaconType != Nmea::AisBeaconType::None);
        ev.emergencyType = ais.beaconType;
        ev.isDarkVessel = false;
        ev.lastUpdate = now;

        calculateCpa(ownPos, ownSog, ownCog, targetPos, ev.sogKnots, ev.cogDegrees, ev.cpaMeters, ev.tcpaSeconds);
        ev.isClosing = (ev.tcpaSeconds > 0.0);

        ev.threatScore = computeThreatScore(
            ev.rangeMeters, ev.cpaMeters, ev.tcpaSeconds, ev.isClosing, false, ev.isEmergency, ev.sogKnots);

        ev.threatLevel = determineThreatLevel(
            ev.threatScore, ev.rangeMeters, ev.cpaMeters, ev.tcpaSeconds, ev.isClosing, ev.isEmergency);

        evaluatedList.push_back(std::move(ev));
    }

    // 3. Sort prioritized candidates descending by threat score
    std::sort(evaluatedList.begin(), evaluatedList.end(),
        [](const EvaluatedTarget& a, const EvaluatedTarget& b) { return a.threatScore > b.threatScore; });

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_evaluatedTargets = evaluatedList;
        alertCbCopy = m_alertCallback;
    }

    // 4. Trigger alert callback for top critical/emergency target if registered
    if (alertCbCopy && !evaluatedList.empty()) {
        const auto& topTarget = evaluatedList.front();
        if (topTarget.threatLevel == ThreatLevel::Critical || topTarget.threatLevel == ThreatLevel::Emergency) {
            alertCbCopy(topTarget);
        }
    }

    return evaluatedList.size();
}

std::vector<EvaluatedTarget> TargetThreatEvaluator::getPrioritizedTargets() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_evaluatedTargets;
}

std::optional<EvaluatedTarget> TargetThreatEvaluator::getHighestThreatTarget() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_evaluatedTargets.empty()) {
        return std::nullopt;
    }
    return m_evaluatedTargets.front();
}

std::optional<EvaluatedTarget> TargetThreatEvaluator::getTarget(std::uint32_t targetId, TargetTrackSource source) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& t : m_evaluatedTargets) {
        if (t.targetId == targetId && (source == TargetTrackSource::None || t.source == source)) {
            return t;
        }
    }
    return std::nullopt;
}

void TargetThreatEvaluator::markTargetInspected(std::uint32_t targetId, TargetTrackSource source,
    std::chrono::milliseconds cooldown)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto now = std::chrono::steady_clock::now();
    for (auto& rec : m_inspectedTargets) {
        if (rec.targetId == targetId && (source == TargetTrackSource::None || rec.source == TargetTrackSource::None || rec.source == source)) {
            rec.inspectedAt = now;
            rec.cooldownDuration = cooldown;
            return;
        }
    }
    m_inspectedTargets.push_back({ targetId, source, now, cooldown });
}

bool TargetThreatEvaluator::isTargetInCooldown(std::uint32_t targetId, TargetTrackSource source,
    std::chrono::steady_clock::time_point now) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& rec : m_inspectedTargets) {
        if (rec.targetId == targetId && (source == TargetTrackSource::None || rec.source == TargetTrackSource::None || rec.source == source)) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - rec.inspectedAt);
            if (elapsed < rec.cooldownDuration) {
                return true;
            }
        }
    }
    return false;
}

std::optional<EvaluatedTarget> TargetThreatEvaluator::getNextUninspectedCandidate(double minScore,
    std::chrono::steady_clock::time_point now) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& target : m_evaluatedTargets) {
        if (target.threatScore < minScore) {
            continue;
        }
        bool inCooldown = false;
        for (const auto& rec : m_inspectedTargets) {
            bool matches = false;
            if (rec.targetId == target.targetId && (rec.source == TargetTrackSource::None || target.source == TargetTrackSource::None || rec.source == target.source)) {
                matches = true;
            } else if (target.associatedRadarId && rec.targetId == *target.associatedRadarId && (rec.source == TargetTrackSource::RadarArpa || rec.source == TargetTrackSource::None)) {
                matches = true;
            } else if (target.associatedAisMmsi && rec.targetId == *target.associatedAisMmsi && (rec.source == TargetTrackSource::Ais || rec.source == TargetTrackSource::None)) {
                matches = true;
            }

            if (matches) {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - rec.inspectedAt);
                if (elapsed < rec.cooldownDuration) {
                    inCooldown = true;
                    break;
                }
            }
        }
        if (!inCooldown) {
            return target;
        }
    }
    return std::nullopt;
}

void TargetThreatEvaluator::clearCooldownHistory()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inspectedTargets.clear();
}

void TargetThreatEvaluator::cleanupExpiredCooldowns(std::chrono::steady_clock::time_point now)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inspectedTargets.erase(
        std::remove_if(m_inspectedTargets.begin(), m_inspectedTargets.end(),
            [&now](const InspectedTargetRecord& rec) {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - rec.inspectedAt);
                return elapsed >= rec.cooldownDuration;
            }),
        m_inspectedTargets.end());
}

} // namespace PayloadHal

