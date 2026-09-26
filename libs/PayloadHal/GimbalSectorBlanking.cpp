#include "GimbalSectorBlanking.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

namespace {

constexpr double PI = 3.14159265358979323846;

double angularDifferenceDeg(double aDeg, double bDeg)
{
    double diff = std::fmod(std::abs(aDeg - bDeg), 360.0);
    return diff > 180.0 ? 360.0 - diff : diff;
}

} // namespace

GimbalSectorBlanking::GimbalSectorBlanking(std::vector<BlankingZone> initialZones)
    : m_zones(std::move(initialZones))
{
}

double GimbalSectorBlanking::normalizeAzimuth(double azDeg) noexcept
{
    double az = std::fmod(azDeg, 360.0);
    if (az < 0.0) {
        az += 360.0;
    }
    return az;
}

bool GimbalSectorBlanking::addZone(const BlankingZone& zone)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (zone.id.empty()) {
        return false;
    }
    for (const auto& existing : m_zones) {
        if (existing.id == zone.id) {
            return false;
        }
    }
    m_zones.push_back(zone);
    return true;
}

bool GimbalSectorBlanking::updateZone(const BlankingZone& zone)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& existing : m_zones) {
        if (existing.id == zone.id) {
            existing = zone;
            return true;
        }
    }
    return false;
}

bool GimbalSectorBlanking::removeZone(const std::string& zoneId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = std::remove_if(m_zones.begin(), m_zones.end(),
        [&zoneId](const BlankingZone& z) { return z.id == zoneId; });
    if (it != m_zones.end()) {
        m_zones.erase(it, m_zones.end());
        return true;
    }
    return false;
}

void GimbalSectorBlanking::clearZones()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_zones.clear();
}

bool GimbalSectorBlanking::setZoneEnabled(const std::string& zoneId, bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& z : m_zones) {
        if (z.id == zoneId) {
            z.enabled = enabled;
            return true;
        }
    }
    return false;
}

std::vector<BlankingZone> GimbalSectorBlanking::zones() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_zones;
}

std::optional<BlankingZone> GimbalSectorBlanking::zone(const std::string& zoneId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& z : m_zones) {
        if (z.id == zoneId) {
            return z;
        }
    }
    return std::nullopt;
}

bool GimbalSectorBlanking::isPointInZone(const BlankingZone& zone, double azDeg, double elDeg)
{
    if (!zone.enabled) {
        return false;
    }

    if (!zone.polygonVertices.empty()) {
        // Spherical / planar ray-crossing algorithm on polygon
        const size_t numVerts = zone.polygonVertices.size();
        if (numVerts < 3) {
            return false;
        }

        const double azNorm = normalizeAzimuth(azDeg);
        bool inside = false;

        for (size_t i = 0, j = numVerts - 1; i < numVerts; j = i++) {
            const double viAz = normalizeAzimuth(zone.polygonVertices[i].azimuthDeg);
            const double viEl = zone.polygonVertices[i].elevationDeg;
            const double vjAz = normalizeAzimuth(zone.polygonVertices[j].azimuthDeg);
            const double vjEl = zone.polygonVertices[j].elevationDeg;

            // Check if elevation is within edge vertical span
            if (((viEl > elDeg) != (vjEl > elDeg))) {
                const double dEl = vjEl - viEl;
                if (std::abs(dEl) > 1e-9) {
                    double dAz = vjAz - viAz;
                    if (dAz > 180.0) dAz -= 360.0;
                    else if (dAz < -180.0) dAz += 360.0;

                    const double intersectAz = viAz + (elDeg - viEl) * dAz / dEl;
                    double diff = azNorm - intersectAz;
                    if (diff < -180.0) diff += 360.0;
                    else if (diff > 180.0) diff -= 360.0;

                    if (diff < 0.0) {
                        inside = !inside;
                    }
                }
            }
        }
        return inside;
    }

    // Rectangular bounding box check
    if (elDeg < zone.elMinDeg || elDeg > zone.elMaxDeg) {
        return false;
    }

    const double azSpan = std::abs(zone.azMaxDeg - zone.azMinDeg);
    if (azSpan >= 360.0) {
        return true;
    }

    const double az = normalizeAzimuth(azDeg);
    const double azMin = normalizeAzimuth(zone.azMinDeg);
    const double azMax = normalizeAzimuth(zone.azMaxDeg);

    if (azMin <= azMax) {
        return (az >= azMin && az <= azMax);
    } else {
        // Wraps around 0/360 boundary
        return (az >= azMin || az <= azMax);
    }
}

double GimbalSectorBlanking::distanceToZone(const BlankingZone& zone, double azDeg, double elDeg)
{
    if (isPointInZone(zone, azDeg, elDeg)) {
        return 0.0;
    }

    double dEl = 0.0;
    if (elDeg < zone.elMinDeg) {
        dEl = zone.elMinDeg - elDeg;
    } else if (elDeg > zone.elMaxDeg) {
        dEl = elDeg - zone.elMaxDeg;
    }

    const double az = normalizeAzimuth(azDeg);
    const double azMin = normalizeAzimuth(zone.azMinDeg);
    const double azMax = normalizeAzimuth(zone.azMaxDeg);

    double dAz = 0.0;
    if (azMin <= azMax) {
        if (az < azMin) {
            dAz = std::min(azMin - az, (az + 360.0) - azMax);
        } else if (az > azMax) {
            dAz = std::min(az - azMax, (azMin + 360.0) - az);
        }
    } else {
        // Wraps around 0
        if (az > azMax && az < azMin) {
            dAz = std::min(az - azMax, azMin - az);
        }
    }

    return std::sqrt(dAz * dAz + dEl * dEl);
}

SectorEvaluationResult GimbalSectorBlanking::evaluate(
    double azDeg, double elDeg, SectorReferenceFrame frame) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    SectorEvaluationResult result {};
    result.mechanicalAllowed = true;
    result.laserAllowed = true;
    result.videoBlanked = false;
    result.inWarningMargin = false;
    result.distanceToNearestZoneDeg = 999.0;

    std::string firstInhibitZoneId {};

    for (const auto& z : m_zones) {
        if (!z.enabled || z.frame != frame) {
            continue;
        }

        const bool inZone = isPointInZone(z, azDeg, elDeg);
        const double dist = distanceToZone(z, azDeg, elDeg);
        result.distanceToNearestZoneDeg = std::min(result.distanceToNearestZoneDeg, dist);

        if (!inZone && dist <= z.safetyMarginDeg) {
            result.inWarningMargin = true;
        }

        if (inZone) {
            result.activeZoneIds.push_back(z.id);

            if (z.type == SectorZoneType::MechanicalKeepOut || z.type == SectorZoneType::TotalExclusion) {
                result.mechanicalAllowed = false;
            }
            if (z.type == SectorZoneType::LaserInhibit || z.type == SectorZoneType::TotalExclusion) {
                result.laserAllowed = false;
                if (firstInhibitZoneId.empty()) {
                    firstInhibitZoneId = z.id;
                }
            }
            if (z.type == SectorZoneType::VideoBlanking || z.type == SectorZoneType::TotalExclusion) {
                result.videoBlanked = true;
            }
        }
    }

    // Trigger interlock callback on transition
    if (m_interlockCb && (result.laserAllowed != m_lastLaserAllowed)) {
        m_lastLaserAllowed = result.laserAllowed;
        m_interlockCb(result.laserAllowed, firstInhibitZoneId);
    }

    return result;
}

bool GimbalSectorBlanking::isLaserAllowed(double azDeg, double elDeg, SectorReferenceFrame frame) const
{
    const auto eval = evaluate(azDeg, elDeg, frame);
    return eval.laserAllowed;
}

bool GimbalSectorBlanking::isMotionAllowed(double azDeg, double elDeg, SectorReferenceFrame frame) const
{
    const auto eval = evaluate(azDeg, elDeg, frame);
    return eval.mechanicalAllowed;
}

bool GimbalSectorBlanking::isVideoBlanked(double azDeg, double elDeg, SectorReferenceFrame frame) const
{
    const auto eval = evaluate(azDeg, elDeg, frame);
    return eval.videoBlanked;
}

PathValidationResult GimbalSectorBlanking::validatePath(
    double fromAzDeg, double fromElDeg,
    double toAzDeg, double toElDeg,
    SectorZoneType checkType,
    SectorReferenceFrame frame) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    PathValidationResult result {};
    result.pathClear = true;
    result.clamped = false;
    result.safeTargetAzDeg = toAzDeg;
    result.safeTargetElDeg = toElDeg;

    // Discretize path into sample segments (at least 1 step per degree)
    double deltaAz = toAzDeg - fromAzDeg;
    if (deltaAz > 180.0) deltaAz -= 360.0;
    else if (deltaAz < -180.0) deltaAz += 360.0;

    const double deltaEl = toElDeg - fromElDeg;
    const double angularDistance = std::sqrt(deltaAz * deltaAz + deltaEl * deltaEl);
    const int numSteps = std::max(10, static_cast<int>(std::ceil(angularDistance * 2.0)));

    for (int i = 0; i <= numSteps; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(numSteps);
        const double curAz = fromAzDeg + t * deltaAz;
        const double curEl = fromElDeg + t * deltaEl;

        for (const auto& z : m_zones) {
            if (!z.enabled || z.frame != frame) {
                continue;
            }

            const bool matchesType = (z.type == checkType) ||
                                     (checkType == SectorZoneType::MechanicalKeepOut && z.type == SectorZoneType::TotalExclusion) ||
                                     (checkType == SectorZoneType::LaserInhibit && z.type == SectorZoneType::TotalExclusion);

            if (matchesType && isPointInZone(z, curAz, curEl)) {
                result.pathClear = false;
                result.clamped = true;
                result.blockingZoneId = z.id;

                // Clamp to safe boundary right before entry point minus safety margin
                const double safeT = std::max(0.0, t - (z.safetyMarginDeg / std::max(1.0, angularDistance)));
                result.safeTargetAzDeg = fromAzDeg + safeT * deltaAz;
                result.safeTargetElDeg = fromElDeg + safeT * deltaEl;
                return result;
            }
        }
    }

    return result;
}

void GimbalSectorBlanking::registerInterlockCallback(InterlockCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_interlockCb = std::move(cb);
}

} // namespace PayloadHal
