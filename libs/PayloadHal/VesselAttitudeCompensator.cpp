#include "VesselAttitudeCompensator.h"

#include "Nmea/NmeaTypes.h"
#include "Nmea/n2k/N2kTypes.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace PayloadHal {

namespace {

    constexpr double Pi = 3.14159265358979323846;
    constexpr double RadToDeg = 180.0 / Pi;

    [[nodiscard]] double normalizeHeadingDeg(double angleDeg) noexcept
    {
        double normalized = std::fmod(angleDeg, 360.0);
        if (normalized < 0.0) {
            normalized += 360.0;
        }
        return normalized;
    }

    [[nodiscard]] double normalizeDeltaDeg(double deltaDeg) noexcept
    {
        while (deltaDeg > 180.0) {
            deltaDeg -= 360.0;
        }
        while (deltaDeg < -180.0) {
            deltaDeg += 360.0;
        }
        return deltaDeg;
    }

    [[nodiscard]] std::string toUpperString(std::string_view sv)
    {
        std::string res;
        res.reserve(sv.size());
        for (const char c : sv) {
            res.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
        return res;
    }

} // namespace

void VesselAttitudeCompensator::updateAttitude(
    double headingDeg,
    double pitchDeg,
    double rollDeg,
    double heaveMeters,
    std::chrono::steady_clock::time_point now) noexcept
{
    applyUpdate(headingDeg, pitchDeg, rollDeg, heaveMeters, true, now);
}

void VesselAttitudeCompensator::updateFromN2k(
    const Nmea::N2k::Attitude& att,
    std::chrono::steady_clock::time_point now) noexcept
{
    const double headingDeg = normalizeHeadingDeg(att.yawDegrees);
    const double pitchDeg = att.pitchDegrees;
    const double rollDeg = att.rollDegrees;
    applyUpdate(headingDeg, pitchDeg, rollDeg, 0.0, att.hasYaw, now);
}

void VesselAttitudeCompensator::updateFromPashr(
    const Nmea::PashrData& pashr,
    std::chrono::steady_clock::time_point now) noexcept
{
    if (!pashr.valid) {
        return;
    }
    applyUpdate(
        normalizeHeadingDeg(pashr.headingDegrees),
        pashr.pitchDegrees,
        pashr.rollDegrees,
        pashr.heaveMeters,
        pashr.isTrueHeading,
        now);
}

void VesselAttitudeCompensator::updateFromPfec(
    const Nmea::PfecAttitudeData& pfec,
    std::chrono::steady_clock::time_point now) noexcept
{
    if (!pfec.valid) {
        return;
    }
    applyUpdate(
        normalizeHeadingDeg(pfec.yawDegrees),
        pfec.pitchDegrees,
        pfec.rollDegrees,
        0.0,
        true,
        now);
}

void VesselAttitudeCompensator::updateFromXdr(
    const Nmea::XdrData& xdr,
    std::chrono::steady_clock::time_point now) noexcept
{
    if (!xdr.valid) {
        return;
    }

    double pitch { 0.0 };
    double roll { 0.0 };
    double heading { 0.0 };
    double heave { 0.0 };
    bool hasHdg { false };
    bool hasPitch { false };
    bool hasRoll { false };

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        pitch = m_state.isValid ? m_state.pitchDeg : 0.0;
        roll = m_state.isValid ? m_state.rollDeg : 0.0;
        heading = m_state.hasHeading ? m_state.headingDeg : 0.0;
        heave = m_state.heaveMeters;
        hasHdg = m_state.hasHeading;
    }

    for (const auto& tr : xdr.transducers) {
        const auto idUpper = toUpperString(tr.id);
        if (idUpper == "PITCH" || idUpper == "PTCH") {
            pitch = tr.measurement;
            hasPitch = true;
        } else if (idUpper == "ROLL" || idUpper == "RL") {
            roll = tr.measurement;
            hasRoll = true;
        }
    }

    if (hasPitch || hasRoll) {
        applyUpdate(heading, pitch, roll, heave, hasHdg, now);
    }
}

void VesselAttitudeCompensator::updateFromAttitude(
    const Nmea::AttitudeData& att,
    std::chrono::steady_clock::time_point now) noexcept
{
    if (!att.valid) {
        return;
    }
    applyUpdate(
        normalizeHeadingDeg(att.headingDegrees),
        att.pitchDegrees,
        att.rollDegrees,
        att.heaveMeters,
        att.hasHeading,
        now);
}

VesselAttitudeState VesselAttitudeCompensator::attitudeState() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

bool VesselAttitudeCompensator::hasValidAttitude(
    std::chrono::steady_clock::time_point now) const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_state.isValid) {
        return false;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_state.timestamp);
    return elapsed >= std::chrono::milliseconds(0) && elapsed <= m_timeout;
}

void VesselAttitudeCompensator::setAttitudeTimeout(std::chrono::milliseconds timeout) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_timeout = timeout;
}

std::chrono::milliseconds VesselAttitudeCompensator::attitudeTimeout() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_timeout;
}

void VesselAttitudeCompensator::setSmoothing(bool enabled, double alpha) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_smoothingEnabled = enabled;
    m_alpha = std::clamp(alpha, 0.01, 1.0);
}

bool VesselAttitudeCompensator::isSmoothingEnabled() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_smoothingEnabled;
}

GimbalLookAngles VesselAttitudeCompensator::compensateLookAngles(
    const Klv::GeoPoint3D& platformPos,
    const Klv::GeoPoint3D& targetPos,
    const PlatformLeverArmConfig& config,
    std::chrono::steady_clock::time_point now) const noexcept
{
    PlatformPose pose {};
    pose.gpsPosition = platformPos;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const bool valid = m_state.isValid
            && (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_state.timestamp) <= m_timeout);

        if (valid) {
            pose.headingDeg = m_state.headingDeg;
            pose.pitchDeg = m_state.pitchDeg;
            pose.rollDeg = m_state.rollDeg;
            // Adjust GPS altitude by heave if present
            pose.gpsPosition.altitudeM += m_state.heaveMeters;
        } else {
            // Graceful 2D degradation: zero pitch/roll, preserve last heading if known
            pose.headingDeg = m_state.hasHeading ? m_state.headingDeg : 0.0;
            pose.pitchDeg = 0.0;
            pose.rollDeg = 0.0;
        }
    }

    PlatformLeverArmCompensator compensator(config);
    return compensator.computeLookAnglesToTarget(pose, targetPos);
}

double VesselAttitudeCompensator::computeHorizonRoll(
    double gimbalPanDeg,
    double gimbalTiltDeg,
    std::chrono::steady_clock::time_point now) const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const bool valid = m_state.isValid
        && (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_state.timestamp) <= m_timeout);

    if (!valid) {
        return 0.0;
    }

    return GeoreferenceUtils::computeLevelingRoll(
        m_state.rollDeg, m_state.pitchDeg, gimbalPanDeg, gimbalTiltDeg);
}

void VesselAttitudeCompensator::reset() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = VesselAttitudeState {};
}

void VesselAttitudeCompensator::applyUpdate(
    double headingDeg,
    double pitchDeg,
    double rollDeg,
    double heaveMeters,
    bool hasHeading,
    std::chrono::steady_clock::time_point now) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);

    double finalHeading = headingDeg;
    double finalPitch = pitchDeg;
    double finalRoll = rollDeg;
    double finalHeave = heaveMeters;

    double rot = 0.0;
    double pitchRate = 0.0;
    double rollRate = 0.0;

    if (m_state.isValid) {
        const double dtSec = std::chrono::duration<double>(now - m_state.timestamp).count();
        if (dtSec > 1e-4) {
            const double dHeading = normalizeDeltaDeg(headingDeg - m_state.headingDeg);
            const double dPitch = pitchDeg - m_state.pitchDeg;
            const double dRoll = rollDeg - m_state.rollDeg;

            rot = dHeading / dtSec;
            pitchRate = dPitch / dtSec;
            rollRate = dRoll / dtSec;

            if (m_smoothingEnabled) {
                finalHeading = normalizeHeadingDeg(m_state.headingDeg + m_alpha * dHeading);
                finalPitch = m_state.pitchDeg + m_alpha * dPitch;
                finalRoll = m_state.rollDeg + m_alpha * dRoll;
                finalHeave = m_state.heaveMeters + m_alpha * (heaveMeters - m_state.heaveMeters);
            }
        }
    }

    m_state.headingDeg = finalHeading;
    m_state.pitchDeg = finalPitch;
    m_state.rollDeg = finalRoll;
    m_state.heaveMeters = finalHeave;
    m_state.rateOfTurnDegPerSec = rot;
    m_state.pitchRateDegPerSec = pitchRate;
    m_state.rollRateDegPerSec = rollRate;
    m_state.hasHeading = hasHeading;
    m_state.timestamp = now;
    m_state.isValid = true;
}

} // namespace PayloadHal
