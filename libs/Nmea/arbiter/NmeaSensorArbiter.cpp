#include "NmeaSensorArbiter.h"

#include <algorithm>
#include <cmath>

namespace Nmea::Arbiter {

namespace {
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kDegToRad = kPi / 180.0;
    constexpr double kEarthRadiusMeters = 6371000.0;
} // namespace

NmeaSensorArbiter::NmeaSensorArbiter(const ArbiterConfig& config)
    : m_config { config }
{
    m_primaryGps.source = GpsSourceId::Primary;
    m_secondaryGps.source = GpsSourceId::Secondary;
    m_primaryHeading.source = HeadingSourceId::Primary;
    m_secondaryHeading.source = HeadingSourceId::Secondary;
}

void NmeaSensorArbiter::setConfig(const ArbiterConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

ArbiterConfig NmeaSensorArbiter::config() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void NmeaSensorArbiter::setGpsFailoverCallback(GpsFailoverCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_gpsFailoverCb = std::move(cb);
}

void NmeaSensorArbiter::setHeadingFailoverCallback(HeadingFailoverCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_headingFailoverCb = std::move(cb);
}

void NmeaSensorArbiter::setDivergenceCallback(DivergenceCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_divergenceCb = std::move(cb);
}

void NmeaSensorArbiter::updateGps(GpsSourceId source, const GgaData& gga)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    GpsChannelStatus& st = (source == GpsSourceId::Primary) ? m_primaryGps : m_secondaryGps;
    const auto now = std::chrono::steady_clock::now();

    if (gga.valid && gga.fixQuality != NmeaFixQuality::Invalid) {
        if (st.positionValid) {
            const double dt = std::chrono::duration<double>(now - st.lastUpdate).count();
            if (dt > 0.05) {
                const double dist = calculateDistanceMeters(
                    st.latitudeDeg, st.longitudeDeg, gga.coordinates.latitudeDeg, gga.coordinates.longitudeDeg);
                const double speed = dist / dt;
                st.kinematicJumpDetected = (speed > m_config.maxSanitySpeedMps && dist > 50.0);
            }
        }
        st.latitudeDeg = gga.coordinates.latitudeDeg;
        st.longitudeDeg = gga.coordinates.longitudeDeg;
        st.altitudeMeters = gga.altitudeMeters;
        st.fixQuality = gga.fixQuality;
        st.satellites = gga.numSatellites;
        st.hdop = gga.hdop;
        st.positionValid = true;
    } else {
        st.fixQuality = gga.fixQuality;
        st.positionValid = false;
    }

    st.lastUpdate = now;
    computeGpsHealth(st, now);
    arbitrateGps(now);
    checkDivergence();
}

void NmeaSensorArbiter::updateGps(GpsSourceId source, const RmcData& rmc)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    GpsChannelStatus& st = (source == GpsSourceId::Primary) ? m_primaryGps : m_secondaryGps;
    const auto now = std::chrono::steady_clock::now();

    if (rmc.valid && rmc.statusActive) {
        if (st.positionValid) {
            const double dt = std::chrono::duration<double>(now - st.lastUpdate).count();
            if (dt > 0.05) {
                const double dist = calculateDistanceMeters(
                    st.latitudeDeg, st.longitudeDeg, rmc.coordinates.latitudeDeg, rmc.coordinates.longitudeDeg);
                const double speed = dist / dt;
                st.kinematicJumpDetected = (speed > m_config.maxSanitySpeedMps && dist > 50.0);
            }
        }
        st.latitudeDeg = rmc.coordinates.latitudeDeg;
        st.longitudeDeg = rmc.coordinates.longitudeDeg;
        st.sogKnots = rmc.speedOverGroundKnots;
        st.cogDegrees = rmc.courseOverGroundDegrees;
        st.positionValid = true;
        if (st.fixQuality == NmeaFixQuality::Invalid) {
            st.fixQuality = NmeaFixQuality::GpsFix;
        }
    } else {
        st.positionValid = false;
    }

    st.lastUpdate = now;
    computeGpsHealth(st, now);
    arbitrateGps(now);
    checkDivergence();
}

void NmeaSensorArbiter::updateGpsCoordinates(GpsSourceId source, double lat, double lon, double altMeters,
    NmeaFixQuality fix, std::uint8_t sats, double hdop, double sogKnots, double cogDeg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    GpsChannelStatus& st = (source == GpsSourceId::Primary) ? m_primaryGps : m_secondaryGps;
    const auto now = std::chrono::steady_clock::now();

    if (fix != NmeaFixQuality::Invalid) {
        if (st.positionValid) {
            const double dt = std::chrono::duration<double>(now - st.lastUpdate).count();
            if (dt > 0.05) {
                const double dist = calculateDistanceMeters(st.latitudeDeg, st.longitudeDeg, lat, lon);
                const double speed = dist / dt;
                st.kinematicJumpDetected = (speed > m_config.maxSanitySpeedMps && dist > 50.0);
            }
        }
        st.latitudeDeg = lat;
        st.longitudeDeg = lon;
        st.altitudeMeters = altMeters;
        st.fixQuality = fix;
        st.satellites = sats;
        st.hdop = hdop;
        st.sogKnots = sogKnots;
        st.cogDegrees = cogDeg;
        st.positionValid = true;
    } else {
        st.fixQuality = fix;
        st.positionValid = false;
    }

    st.lastUpdate = now;
    computeGpsHealth(st, now);
    arbitrateGps(now);
    checkDivergence();
}

void NmeaSensorArbiter::updateHeading(HeadingSourceId source, double headingDeg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    HeadingChannelStatus& st = (source == HeadingSourceId::Primary) ? m_primaryHeading : m_secondaryHeading;
    const auto now = std::chrono::steady_clock::now();

    if (st.headingValid) {
        const double dt = std::chrono::duration<double>(now - st.lastUpdate).count();
        if (dt > 0.05) {
            const double delta = calculateHeadingDeltaDeg(st.headingDegrees, headingDeg);
            const double rate = delta / dt;
            st.angularJumpDetected = (rate > m_config.maxSanityTurnRateDegPerSec);
        }
    }

    st.headingDegrees = headingDeg;
    st.headingValid = true;
    st.lastUpdate = now;

    computeHeadingHealth(st, now);
    arbitrateHeading(now);
    checkDivergence();
}

void NmeaSensorArbiter::updateAttitude(HeadingSourceId source, double pitchDeg, double rollDeg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    HeadingChannelStatus& st = (source == HeadingSourceId::Primary) ? m_primaryHeading : m_secondaryHeading;
    st.pitchDegrees = pitchDeg;
    st.rollDegrees = rollDeg;
    st.hasAttitude = true;
    st.lastUpdate = std::chrono::steady_clock::now();
}

void NmeaSensorArbiter::evaluate(std::chrono::steady_clock::time_point now)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    computeGpsHealth(m_primaryGps, now);
    computeGpsHealth(m_secondaryGps, now);
    computeHeadingHealth(m_primaryHeading, now);
    computeHeadingHealth(m_secondaryHeading, now);

    arbitrateGps(now);
    arbitrateHeading(now);
    checkDivergence();
}

void NmeaSensorArbiter::computeGpsHealth(GpsChannelStatus& st, std::chrono::steady_clock::time_point now)
{
    if (st.lastUpdate.time_since_epoch().count() == 0 || (now - st.lastUpdate) > m_config.gpsTimeout) {
        st.online = false;
        st.healthScore = 0.0;
        return;
    }

    st.online = true;

    double score = 0.0;
    switch (st.fixQuality) {
    case NmeaFixQuality::RtkFixed:
        score = 100.0;
        break;
    case NmeaFixQuality::RtkFloat:
        score = 90.0;
        break;
    case NmeaFixQuality::DgpsFix:
        score = 85.0;
        break;
    case NmeaFixQuality::PpsFix:
        score = 80.0;
        break;
    case NmeaFixQuality::GpsFix:
        score = 70.0;
        break;
    case NmeaFixQuality::Estimated:
        score = 35.0;
        break;
    case NmeaFixQuality::Manual:
    case NmeaFixQuality::Simulation:
        score = 15.0;
        break;
    case NmeaFixQuality::Invalid:
    default:
        score = 0.0;
        break;
    }

    // HDOP penalties
    if (st.hdop > 1.0) {
        const double penalty = std::min(30.0, (st.hdop - 1.0) * 10.0);
        score = std::max(0.0, score - penalty);
    }
    if (st.hdop > 5.0) {
        score = std::max(0.0, score - 30.0);
    }

    // Satellite count penalties
    if (st.satellites > 0U && st.satellites < 4U) {
        score = std::max(0.0, score - 30.0);
    }

    // Anti-spoofing kinematic jump penalty
    if (st.kinematicJumpDetected) {
        score = std::max(0.0, score - 50.0);
    }

    st.healthScore = std::clamp(score, 0.0, 100.0);
}

void NmeaSensorArbiter::computeHeadingHealth(HeadingChannelStatus& st, std::chrono::steady_clock::time_point now)
{
    if (st.lastUpdate.time_since_epoch().count() == 0 || (now - st.lastUpdate) > m_config.headingTimeout) {
        st.online = false;
        st.healthScore = 0.0;
        return;
    }

    st.online = true;

    if (!st.headingValid) {
        st.healthScore = 0.0;
        return;
    }

    double score = 100.0;
    if (st.angularJumpDetected) {
        score = 40.0;
    }
    st.healthScore = score;
}

void NmeaSensorArbiter::arbitrateGps(std::chrono::steady_clock::time_point /*now*/)
{
    GpsSourceId target = m_activeGps;
    std::string reason {};

    if (m_config.policy == FailoverPolicy::ManualOverride) {
        target = m_config.manualGpsSelection;
        reason = "Manual override";
    } else if (m_config.policy == FailoverPolicy::PrimarySecondaryAutoRevert) {
        if (m_primaryGps.online && m_primaryGps.healthScore >= m_config.minimumHealthyScore) {
            target = GpsSourceId::Primary;
            reason
                = "Primary GNSS healthy (score: " + std::to_string(static_cast<int>(m_primaryGps.healthScore)) + "%)";
        } else if (m_secondaryGps.online && m_secondaryGps.healthScore >= m_config.minimumHealthyScore) {
            target = GpsSourceId::Secondary;
            reason = !m_primaryGps.online ? "Primary GNSS timed out" : "Primary GNSS degraded";
        } else {
            // Both degraded; pick highest
            target = (m_primaryGps.healthScore >= m_secondaryGps.healthScore) ? GpsSourceId::Primary
                                                                              : GpsSourceId::Secondary;
            reason = "Both GNSS degraded; fallback to highest health";
        }
    } else { // HighestQualityFirst
        if (m_secondaryGps.healthScore > (m_primaryGps.healthScore + 5.0)) {
            target = GpsSourceId::Secondary;
            reason = "Secondary GNSS score superior (" + std::to_string(static_cast<int>(m_secondaryGps.healthScore))
                + "% vs " + std::to_string(static_cast<int>(m_primaryGps.healthScore)) + "%)";
        } else if (m_primaryGps.healthScore >= m_config.minimumHealthyScore) {
            target = GpsSourceId::Primary;
            reason = "Primary GNSS equal or superior (" + std::to_string(static_cast<int>(m_primaryGps.healthScore))
                + "%)";
        } else if (m_secondaryGps.healthScore >= m_config.minimumHealthyScore) {
            target = GpsSourceId::Secondary;
            reason = "Secondary GNSS meets minimum score";
        }
    }

    if (target != m_activeGps) {
        const GpsSourceId oldSource = m_activeGps;
        m_activeGps = target;
        if (m_gpsFailoverCb) {
            m_gpsFailoverCb(oldSource, target, reason);
        }
    }
}

void NmeaSensorArbiter::arbitrateHeading(std::chrono::steady_clock::time_point /*now*/)
{
    HeadingSourceId target = m_activeHeading;
    std::string reason {};

    if (m_config.policy == FailoverPolicy::ManualOverride) {
        target = m_config.manualHeadingSelection;
        reason = "Manual override";
    } else if (m_config.policy == FailoverPolicy::PrimarySecondaryAutoRevert) {
        if (m_primaryHeading.online && m_primaryHeading.healthScore >= m_config.minimumHealthyScore) {
            target = HeadingSourceId::Primary;
            reason = "Primary Gyro healthy";
        } else if (m_secondaryHeading.online && m_secondaryHeading.healthScore >= m_config.minimumHealthyScore) {
            target = HeadingSourceId::Secondary;
            reason = !m_primaryHeading.online ? "Primary Gyro timed out" : "Primary Gyro degraded";
        } else {
            target = (m_primaryHeading.healthScore >= m_secondaryHeading.healthScore) ? HeadingSourceId::Primary
                                                                                      : HeadingSourceId::Secondary;
            reason = "Fallback to highest gyro health";
        }
    } else { // HighestQualityFirst
        if (m_secondaryHeading.healthScore > (m_primaryHeading.healthScore + 5.0)) {
            target = HeadingSourceId::Secondary;
            reason = "Secondary Gyro score superior";
        } else if (m_primaryHeading.healthScore >= m_config.minimumHealthyScore) {
            target = HeadingSourceId::Primary;
            reason = "Primary Gyro equal or superior";
        } else if (m_secondaryHeading.healthScore >= m_config.minimumHealthyScore) {
            target = HeadingSourceId::Secondary;
            reason = "Secondary Gyro meets minimum score";
        }
    }

    if (target != m_activeHeading) {
        const HeadingSourceId oldSource = m_activeHeading;
        m_activeHeading = target;
        if (m_headingFailoverCb) {
            m_headingFailoverCb(oldSource, target, reason);
        }
    }
}

void NmeaSensorArbiter::checkDivergence()
{
    bool notify = false;

    // GPS divergence
    if (m_primaryGps.online && m_secondaryGps.online && m_primaryGps.positionValid && m_secondaryGps.positionValid) {
        const double deltaDist = calculateDistanceMeters(m_primaryGps.latitudeDeg, m_primaryGps.longitudeDeg,
            m_secondaryGps.latitudeDeg, m_secondaryGps.longitudeDeg);
        m_divergence.positionDeltaMeters = deltaDist;
        const bool div = (deltaDist > m_config.maxPositionDivergenceMeters);
        if (div != m_divergence.positionDiverged) {
            m_divergence.positionDiverged = div;
            notify = true;
        }
    } else {
        m_divergence.positionDiverged = false;
        m_divergence.positionDeltaMeters = 0.0;
    }

    // Heading divergence
    if (m_primaryHeading.online && m_secondaryHeading.online && m_primaryHeading.headingValid
        && m_secondaryHeading.headingValid) {
        const double deltaHead
            = calculateHeadingDeltaDeg(m_primaryHeading.headingDegrees, m_secondaryHeading.headingDegrees);
        m_divergence.headingDeltaDeg = deltaHead;
        const bool div = (deltaHead > m_config.maxHeadingDivergenceDeg);
        if (div != m_divergence.headingDiverged) {
            m_divergence.headingDiverged = div;
            notify = true;
        }
    } else {
        m_divergence.headingDiverged = false;
        m_divergence.headingDeltaDeg = 0.0;
    }

    if (notify && m_divergenceCb) {
        m_divergenceCb(m_divergence);
    }
}

GpsSourceId NmeaSensorArbiter::activeGpsSource() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeGps;
}

HeadingSourceId NmeaSensorArbiter::activeHeadingSource() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeHeading;
}

NmeaNavSnapshot NmeaSensorArbiter::activeNavSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    NmeaNavSnapshot snapshot {};

    const GpsChannelStatus& gps = (m_activeGps == GpsSourceId::Primary) ? m_primaryGps : m_secondaryGps;
    if (gps.online && gps.positionValid) {
        snapshot.position.latitudeDeg = gps.latitudeDeg;
        snapshot.position.longitudeDeg = gps.longitudeDeg;
        snapshot.altitudeMeters = gps.altitudeMeters;
        snapshot.sogKnots = gps.sogKnots;
        snapshot.cogDegrees = gps.cogDegrees;
        snapshot.fixQuality = gps.fixQuality;
        snapshot.hasPosition = true;
    }

    const HeadingChannelStatus& hdg
        = (m_activeHeading == HeadingSourceId::Primary) ? m_primaryHeading : m_secondaryHeading;
    if (hdg.online && hdg.headingValid) {
        snapshot.trueHeadingDegrees = hdg.headingDegrees;
        snapshot.hasHeading = true;
    }
    if (hdg.online && hdg.hasAttitude) {
        snapshot.pitchDegrees = hdg.pitchDegrees;
        snapshot.rollDegrees = hdg.rollDegrees;
        snapshot.hasAttitude = true;
    }

    snapshot.timestamp = std::chrono::steady_clock::now();
    return snapshot;
}

GpsChannelStatus NmeaSensorArbiter::gpsStatus(GpsSourceId source) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return (source == GpsSourceId::Primary) ? m_primaryGps : m_secondaryGps;
}

HeadingChannelStatus NmeaSensorArbiter::headingStatus(HeadingSourceId source) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return (source == HeadingSourceId::Primary) ? m_primaryHeading : m_secondaryHeading;
}

DivergenceStatus NmeaSensorArbiter::divergenceStatus() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_divergence;
}

double NmeaSensorArbiter::calculateDistanceMeters(double lat1, double lon1, double lat2, double lon2) noexcept
{
    const double dLat = (lat2 - lat1) * kDegToRad;
    const double dLon = (lon2 - lon1) * kDegToRad;
    const double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0)
        + std::cos(lat1 * kDegToRad) * std::cos(lat2 * kDegToRad) * std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(std::max(0.0, 1.0 - a)));
    return kEarthRadiusMeters * c;
}

double NmeaSensorArbiter::calculateHeadingDeltaDeg(double h1, double h2) noexcept
{
    double diff = std::abs(h1 - h2);
    if (diff > 180.0) {
        diff = 360.0 - diff;
    }
    return diff;
}

void NmeaSensorArbiter::updateSpeedLog(const VbwData& vbw)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_speedLog = vbw;
}

void NmeaSensorArbiter::updateWaterDepth(const DptData& dpt)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_waterDepth = dpt;
}

std::optional<double> NmeaSensorArbiter::arbitratedWaterSpeed() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_speedLog.has_value() && m_speedLog->waterSpeedStatus == 'A') {
        return m_speedLog->longitudinalWaterSpeedKnots;
    }
    return std::nullopt;
}

std::optional<double> NmeaSensorArbiter::arbitratedGroundSpeed() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const GpsChannelStatus& gps = (m_activeGps == GpsSourceId::Primary) ? m_primaryGps : m_secondaryGps;
    if (gps.online && gps.positionValid) {
        return gps.sogKnots;
    }
    if (m_speedLog.has_value() && m_speedLog->groundSpeedStatus == 'A') {
        return m_speedLog->longitudinalGroundSpeedKnots;
    }
    return std::nullopt;
}

std::optional<double> NmeaSensorArbiter::arbitratedDepthMeters() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_waterDepth.has_value() && m_waterDepth->valid) {
        return m_waterDepth->waterDepthMeters;
    }
    return std::nullopt;
}

} // namespace Nmea::Arbiter
