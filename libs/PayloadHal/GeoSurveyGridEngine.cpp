#include "GeoSurveyGridEngine.h"
#include "GeoreferenceUtils.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace PayloadHal {

namespace {

inline constexpr double PI = 3.14159265358979323846;
inline constexpr double DEG_TO_RAD = PI / 180.0;
inline constexpr double RAD_TO_DEG = 180.0 / PI;
inline constexpr double WGS84_A = 6378137.0; // Semi-major axis in meters

[[nodiscard]] double normalizeHeadingDeg(double deg) noexcept
{
    double h = std::fmod(deg, 360.0);
    if (h < 0.0) {
        h += 360.0;
    }
    return h;
}

void latLonToLocalMeters(double refLat, double refLon, double lat, double lon, double& x, double& y) noexcept
{
    const double latRad = refLat * DEG_TO_RAD;
    x = WGS84_A * (lon - refLon) * DEG_TO_RAD * std::cos(latRad);
    y = WGS84_A * (lat - refLat) * DEG_TO_RAD;
}

void localMetersToLatLon(double refLat, double refLon, double x, double y, double& lat, double& lon) noexcept
{
    const double latRad = refLat * DEG_TO_RAD;
    const double cosLat = std::cos(latRad);
    lat = refLat + (y / WGS84_A) * RAD_TO_DEG;
    lon = refLon + (x / (WGS84_A * (std::abs(cosLat) > 1e-6 ? cosLat : 1e-6))) * RAD_TO_DEG;
}

} // namespace

// =============================================================================
// Construction & Destruction
// =============================================================================

GeoSurveyGridEngine::GeoSurveyGridEngine(
    std::shared_ptr<IPanTiltUnit> ptu,
    std::shared_ptr<ICameraPayload> camera,
    std::shared_ptr<IDemProvider> dem,
    SurveyPlanConfig config) noexcept
    : m_ptu(std::move(ptu))
    , m_camera(std::move(camera))
    , m_dem(std::move(dem))
    , m_config(std::move(config))
{
}

GeoSurveyGridEngine::~GeoSurveyGridEngine()
{
    abortSurvey();
}

// =============================================================================
// Configuration & Trajectory Planning
// =============================================================================

void GeoSurveyGridEngine::setPlanConfig(const SurveyPlanConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    m_state = SurveyState::Idle;
    m_transects.clear();
    m_totalLengthMeters = 0.0;
}

SurveyPlanConfig GeoSurveyGridEngine::planConfig() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

bool GeoSurveyGridEngine::generatePlan()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_config.boundaryPolygon.size() < 3U) {
        transitionState(SurveyState::Idle, "Boundary polygon must contain at least 3 vertices");
        return false;
    }

    transitionState(SurveyState::Planning, "Generating survey transects and sampling terrain");

    // 1. Calculate centroid reference
    double sumLat = 0.0;
    double sumLon = 0.0;
    for (const auto& v : m_config.boundaryPolygon) {
        sumLat += v.latitudeDeg;
        sumLon += v.longitudeDeg;
    }
    const double refLat = sumLat / static_cast<double>(m_config.boundaryPolygon.size());
    const double refLon = sumLon / static_cast<double>(m_config.boundaryPolygon.size());

    // 2. Determine sweep heading
    double sweepAngleDeg = 0.0;
    if (m_config.sweepAngleDeg.has_value()) {
        sweepAngleDeg = normalizeHeadingDeg(*m_config.sweepAngleDeg);
    } else {
        sweepAngleDeg = calculateOptimalSweepHeading(m_config.boundaryPolygon);
    }
    m_optimalHeadingDeg = sweepAngleDeg;

    // 3. Compute optical footprint at nominal survey altitude
    double hfovDeg = m_config.cameraHfovDeg;
    double vfovDeg = m_config.cameraVfovDeg;
    if (m_camera) {
        const auto camTelem = m_camera->currentTelemetry();
        if (camTelem.horizontalFovDeg > 0.0) {
            hfovDeg = camTelem.horizontalFovDeg;
        }
        if (camTelem.verticalFovDeg > 0.0) {
            vfovDeg = camTelem.verticalFovDeg;
        }
    }
    hfovDeg = std::clamp(hfovDeg, 2.0, 120.0);
    vfovDeg = std::clamp(vfovDeg, 1.0, 90.0);

    const double altAgl = std::max(10.0, m_config.surveyAltitudeAglMeters);
    const double footprintWidthMeters = 2.0 * altAgl * std::tan((hfovDeg * 0.5) * DEG_TO_RAD);
    const double footprintLengthMeters = 2.0 * altAgl * std::tan((vfovDeg * 0.5) * DEG_TO_RAD);

    const double sideOverlap = std::clamp(m_config.sideOverlapRatio, 0.05, 0.90);
    const double forwardOverlap = std::clamp(m_config.forwardOverlapRatio, 0.05, 0.95);

    const double trackSpacingMeters = std::max(2.0, footprintWidthMeters * (1.0 - sideOverlap));
    const double stationSpacingMeters = std::max(2.0, footprintLengthMeters * (1.0 - forwardOverlap));

    // 4. Transform boundary polygon to local rotated (u, v) coordinates
    // theta is the angle from North (Y-axis)
    const double thetaRad = sweepAngleDeg * DEG_TO_RAD;
    const double sinTheta = std::sin(thetaRad);
    const double cosTheta = std::cos(thetaRad);

    struct LocalPoint { double u; double v; };
    std::vector<LocalPoint> polyUv;
    polyUv.reserve(m_config.boundaryPolygon.size());

    double minV = std::numeric_limits<double>::max();
    double maxV = std::numeric_limits<double>::lowest();

    for (const auto& pt : m_config.boundaryPolygon) {
        double x = 0.0;
        double y = 0.0;
        latLonToLocalMeters(refLat, refLon, pt.latitudeDeg, pt.longitudeDeg, x, y);
        // Along-track: u = x*sin(theta) + y*cos(theta)
        // Cross-track: v = x*cos(theta) - y*sin(theta)
        const double u = (x * sinTheta) + (y * cosTheta);
        const double v = (x * cosTheta) - (y * sinTheta);
        polyUv.push_back({ u, v });
        minV = std::min(minV, v);
        maxV = std::max(maxV, v);
    }

    const double totalCrossTrackWidth = maxV - minV;
    if (totalCrossTrackWidth <= 1e-3) {
        transitionState(SurveyState::Idle, "Survey polygon cross-track span is too small");
        return false;
    }

    // 5. Generate sliced transects across polygon
    m_transects.clear();
    m_totalLengthMeters = 0.0;

    const std::size_t numTracks = std::max<std::size_t>(1U, static_cast<std::size_t>(std::ceil(totalCrossTrackWidth / trackSpacingMeters)));
    const double actualSpacing = totalCrossTrackWidth / static_cast<double>(numTracks);

    std::uint32_t transectGlobalIdx = 0U;

    for (std::size_t trackIdx = 0; trackIdx < numTracks; ++trackIdx) {
        const double vLine = minV + (static_cast<double>(trackIdx) + 0.5) * actualSpacing;

        // Intersect horizontal line v = vLine with all polygon edges
        std::vector<double> uIntersects;
        const std::size_t nVerts = polyUv.size();
        for (std::size_t i = 0; i < nVerts; ++i) {
            const std::size_t j = (i + 1) % nVerts;
            const double v1 = polyUv[i].v;
            const double v2 = polyUv[j].v;
            const double u1 = polyUv[i].u;
            const double u2 = polyUv[j].u;

            if ((v1 <= vLine && vLine <= v2) || (v2 <= vLine && vLine <= v1)) {
                const double dv = v2 - v1;
                if (std::abs(dv) > 1e-9) {
                    const double t = (vLine - v1) / dv;
                    const double uInt = u1 + t * (u2 - u1);
                    uIntersects.push_back(uInt);
                }
            }
        }

        if (uIntersects.size() < 2U) {
            continue;
        }

        std::sort(uIntersects.begin(), uIntersects.end());
        auto last = std::unique(uIntersects.begin(), uIntersects.end(), [](double a, double b) {
            return std::abs(a - b) < 0.1;
        });
        uIntersects.erase(last, uIntersects.end());

        // Convert endpoints to geodetic
        const auto uvToGeo = [&](double u, double v) {
            const double x = (u * sinTheta) + (v * cosTheta);
            const double y = (u * cosTheta) - (v * sinTheta);
            double lat = 0.0;
            double lon = 0.0;
            localMetersToLatLon(refLat, refLon, x, y, lat, lon);
            return Klv::GeoPoint2D { lat, lon };
        };

        // Pair consecutive intersections (handles both convex and concave polygons)
        for (std::size_t p = 0; p + 1 < uIntersects.size(); ++p) {
            double uStart = uIntersects[p];
            double uEnd = uIntersects[p + 1];
            if (uEnd - uStart < 1.0) {
                continue; // Ignore degenerate micro-segments
            }

            // Verify interval midpoint is inside the polygon (skips exterior voids of concave shapes)
            const double uMid = (uStart + uEnd) * 0.5;
            const Klv::GeoPoint2D ptMid = uvToGeo(uMid, vLine);
            if (!isPointInsidePolygon(ptMid, m_config.boundaryPolygon)) {
                continue;
            }

            SurveyTransect transect {};
            transect.index = transectGlobalIdx++;
            transect.headingDeg = sweepAngleDeg;
            transect.lengthMeters = uEnd - uStart;
            m_totalLengthMeters += transect.lengthMeters;

            // Boustrophedon direction reversal on odd tracks
            const bool reverseDirection = (m_config.patternType == SweepPatternType::Boustrophedon) && (trackIdx % 2U == 1U);
            if (reverseDirection) {
                std::swap(uStart, uEnd);
                transect.headingDeg = normalizeHeadingDeg(sweepAngleDeg + 180.0);
            }

            transect.entryPoint = uvToGeo(uStart, vLine);
            transect.exitPoint = uvToGeo(uEnd, vLine);

            // Generate discrete observation waypoints along transect
            const double legDistance = std::abs(uEnd - uStart);
            const std::size_t numWaypoints = std::max<std::size_t>(2U, static_cast<std::size_t>(std::ceil(legDistance / stationSpacingMeters)) + 1U);

            for (std::size_t wpIdx = 0; wpIdx < numWaypoints; ++wpIdx) {
                const double fraction = static_cast<double>(wpIdx) / static_cast<double>(numWaypoints - 1);
                const double uPos = uStart + fraction * (uEnd - uStart);
                const Klv::GeoPoint2D ptGeo = uvToGeo(uPos, vLine);

                SurveyWaypoint wp {};
                wp.transectIndex = transect.index;
                wp.waypointIndex = static_cast<std::uint32_t>(wpIdx);
                wp.dwellTimeSec = m_config.waypointDwellSec;

                // Sample terrain height from DEM if available
                double elevationM = 0.0;
                if (m_dem) {
                    elevationM = m_dem->getElevationM(ptGeo.latitudeDeg, ptGeo.longitudeDeg).value_or(0.0);
                }
                wp.targetGroundPos = Klv::GeoPoint3D { ptGeo.latitudeDeg, ptGeo.longitudeDeg, elevationM };

                // Adaptive GSD calculation
                const double localAgl = std::max(5.0, (m_config.surveyAltitudeAglMeters + refLat) - elevationM);
                const double localWidth = 2.0 * localAgl * std::tan((hfovDeg * 0.5) * DEG_TO_RAD);
                wp.expectedGsdMeters = (m_config.sensorWidthPixels > 0U)
                    ? (localWidth / static_cast<double>(m_config.sensorWidthPixels))
                    : 0.05;

                transect.waypoints.push_back(wp);
            }

            m_transects.push_back(transect);
        }
    }

    if (m_transects.empty()) {
        transitionState(SurveyState::Idle, "No viable transects could be generated for boundary polygon");
        return false;
    }

    // 6. Initialize discrete occupancy grid
    initializeOccupancyGrid();

    // 7. Update initial metrics
    m_currentTransectIdx = 0U;
    m_currentWaypointIdx = 0U;
    m_waypointDwellElapsedSec = 0.0;
    m_elapsedTimeSec = 0.0;
    updateMetricsLocked();

    transitionState(SurveyState::Idle, "Survey plan generated successfully");
    return true;
}

std::vector<SurveyTransect> GeoSurveyGridEngine::plannedTransects() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_transects;
}

double GeoSurveyGridEngine::totalPlanLengthMeters() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalLengthMeters;
}

double GeoSurveyGridEngine::optimalSweepHeadingDeg() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_optimalHeadingDeg;
}

// =============================================================================
// Survey Execution & Control
// =============================================================================

bool GeoSurveyGridEngine::startSurvey()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_transects.empty()) {
        return false;
    }
    if (m_state == SurveyState::Executing) {
        return false;
    }

    m_currentTransectIdx = 0U;
    m_currentWaypointIdx = 0U;
    m_waypointDwellElapsedSec = 0.0;
    m_elapsedTimeSec = 0.0;
    transitionState(SurveyState::Executing, "Survey execution started");
    return true;
}

bool GeoSurveyGridEngine::pauseSurvey()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != SurveyState::Executing) {
        return false;
    }
    transitionState(SurveyState::Paused, "Survey paused by operator");
    if (m_ptu) {
        m_ptu->stopMotion();
    }
    return true;
}

bool GeoSurveyGridEngine::resumeSurvey()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != SurveyState::Paused) {
        return false;
    }
    transitionState(SurveyState::Executing, "Survey resumed");
    return true;
}

void GeoSurveyGridEngine::abortSurvey()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == SurveyState::Executing || m_state == SurveyState::Paused) {
        transitionState(SurveyState::Aborted, "Survey aborted by operator");
        if (m_ptu) {
            m_ptu->stopMotion();
        }
    }
}

bool GeoSurveyGridEngine::skipToNextTransect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != SurveyState::Executing && m_state != SurveyState::Paused) {
        return false;
    }
    if (m_currentTransectIdx + 1U >= m_transects.size()) {
        return false;
    }

    m_currentTransectIdx++;
    m_currentWaypointIdx = 0U;
    m_waypointDwellElapsedSec = 0.0;
    updateMetricsLocked();
    return true;
}

SurveyState GeoSurveyGridEngine::surveyState() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

void GeoSurveyGridEngine::update(double deltaSeconds, const Klv::GeoPoint3D& platformPos, double platformHeadingDeg)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_state != SurveyState::Executing) {
        return;
    }

    const double dt = std::clamp(deltaSeconds, 0.0, 1.0);
    m_elapsedTimeSec += dt;

    if (m_currentTransectIdx >= m_transects.size()) {
        transitionState(SurveyState::Completed, "All survey transects completed");
        m_metrics.coveragePercentage = 100.0;
        return;
    }

    const auto& transect = m_transects[m_currentTransectIdx];
    if (m_currentWaypointIdx >= transect.waypoints.size()) {
        m_currentTransectIdx++;
        m_currentWaypointIdx = 0U;
        m_waypointDwellElapsedSec = 0.0;
        if (m_currentTransectIdx >= m_transects.size()) {
            transitionState(SurveyState::Completed, "All survey transects completed");
            m_metrics.coveragePercentage = 100.0;
            return;
        }
    }

    const auto& currentWp = m_transects[m_currentTransectIdx].waypoints[m_currentWaypointIdx];

    // Compute look-angles to active target waypoint
    const auto lookAngles = GeoreferenceUtils::computeLookAnglesToTarget(
        platformPos, platformHeadingDeg, currentWp.targetGroundPos);

    if (m_ptu) {
        m_ptu->setAbsoluteAngles(lookAngles.panAngleDeg, lookAngles.tiltAngleDeg);
    }

    // Advance dwell duration
    m_waypointDwellElapsedSec += dt;
    if (m_waypointDwellElapsedSec + 1e-4 >= currentWp.dwellTimeSec) {
        m_waypointDwellElapsedSec = 0.0;
        m_currentWaypointIdx++;
        if (m_currentWaypointIdx >= transect.waypoints.size()) {
            m_currentTransectIdx++;
            m_currentWaypointIdx = 0U;
            if (m_currentTransectIdx >= m_transects.size()) {
                transitionState(SurveyState::Completed, "All survey transects completed");
                m_metrics.coveragePercentage = 100.0;
                return;
            }
        }
    }

    updateMetricsLocked();

    if (m_progressCb) {
        auto cb = m_progressCb;
        const auto metrics = m_metrics;
        const auto tIdx = static_cast<std::uint32_t>(m_currentTransectIdx);
        const auto wIdx = static_cast<std::uint32_t>(m_currentWaypointIdx);
        lock.unlock();
        cb(metrics, tIdx, wIdx);
    }
}

// =============================================================================
// Real-Time Frustum & Coverage Mapping
// =============================================================================

void GeoSurveyGridEngine::stampFootprint(const Klv::FrustumCorners& frustum, const Klv::GeoPoint3D& platformPos)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_grid.isValid()) {
        return;
    }

    const std::vector<Klv::GeoPoint2D> frustumPoly {
        frustum.topLeft,
        frustum.topRight,
        frustum.bottomRight,
        frustum.bottomLeft
    };

    // Find bounding box of frustum in grid space
    double fMinLat = std::min({ frustumPoly[0].latitudeDeg, frustumPoly[1].latitudeDeg, frustumPoly[2].latitudeDeg, frustumPoly[3].latitudeDeg });
    double fMaxLat = std::max({ frustumPoly[0].latitudeDeg, frustumPoly[1].latitudeDeg, frustumPoly[2].latitudeDeg, frustumPoly[3].latitudeDeg });
    double fMinLon = std::min({ frustumPoly[0].longitudeDeg, frustumPoly[1].longitudeDeg, frustumPoly[2].longitudeDeg, frustumPoly[3].longitudeDeg });
    double fMaxLon = std::max({ frustumPoly[0].longitudeDeg, frustumPoly[1].longitudeDeg, frustumPoly[2].longitudeDeg, frustumPoly[3].longitudeDeg });

    std::size_t rMin = 0U;
    std::size_t rMax = m_grid.rows - 1U;
    std::size_t cMin = 0U;
    std::size_t cMax = m_grid.cols - 1U;

    std::size_t tempR = 0U;
    std::size_t tempC = 0U;
    if (m_grid.geoToGrid(fMinLat, fMinLon, tempR, tempC)) {
        rMin = tempR;
        cMin = tempC;
    }
    if (m_grid.geoToGrid(fMaxLat, fMaxLon, tempR, tempC)) {
        rMax = tempR;
        cMax = tempC;
    }
    if (rMin > rMax) std::swap(rMin, rMax);
    if (cMin > cMax) std::swap(cMin, cMax);

    // Rasterize frustum quad onto occupancy grid cells
    for (std::size_t r = rMin; r <= rMax; ++r) {
        for (std::size_t c = cMin; c <= cMax; ++c) {
            const auto currentCell = m_grid.cell(r, c);
            if (currentCell == GridCellState::OutsidePolygon || currentCell == GridCellState::Surveyed) {
                continue;
            }

            double cellLat = 0.0;
            double cellLon = 0.0;
            if (!m_grid.gridToGeo(r, c, cellLat, cellLon)) {
                continue;
            }

            if (isPointInsidePolygon({ cellLat, cellLon }, frustumPoly)) {
                // Check terrain occlusion if DEM is present
                bool isOccluded = false;
                if (m_dem) {
                    const double cellElev = m_dem->getElevationM(cellLat, cellLon).value_or(0.0);
                    // Sample halfway point for terrain blockage
                    const double midLat = (platformPos.latitudeDeg + cellLat) * 0.5;
                    const double midLon = (platformPos.longitudeDeg + cellLon) * 0.5;
                    const double midExpectedAlt = (platformPos.altitudeM + cellElev) * 0.5;
                    const double midTerrainElev = m_dem->getElevationM(midLat, midLon).value_or(0.0);

                    if (midTerrainElev > midExpectedAlt + 2.0) {
                        isOccluded = true;
                    }
                }

                if (isOccluded) {
                    m_grid.setCell(r, c, GridCellState::OccludedTerrainShadow);
                } else {
                    m_grid.setCell(r, c, GridCellState::Surveyed);
                }
            }
        }
    }

    updateMetricsLocked();
}

CoverageMetrics GeoSurveyGridEngine::coverageMetrics() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metrics;
}

SurveyOccupancyGrid GeoSurveyGridEngine::occupancyGrid() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_grid;
}

void GeoSurveyGridEngine::setProgressCallback(ProgressCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progressCb = std::move(cb);
}

void GeoSurveyGridEngine::setStateCallback(StateCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCb = std::move(cb);
}

// =============================================================================
// Static Analytical Utilities
// =============================================================================

bool GeoSurveyGridEngine::isPointInsidePolygon(
    const Klv::GeoPoint2D& point, const std::vector<Klv::GeoPoint2D>& polygon) noexcept
{
    const std::size_t n = polygon.size();
    if (n < 3U) {
        return false;
    }

    // 1. Check if point lies directly on any boundary edge (with small geometric tolerance)
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const double lat_i = polygon[i].latitudeDeg;
        const double lon_i = polygon[i].longitudeDeg;
        const double lat_j = polygon[j].latitudeDeg;
        const double lon_j = polygon[j].longitudeDeg;

        const double minLat = std::min(lat_i, lat_j) - 1e-7;
        const double maxLat = std::max(lat_i, lat_j) + 1e-7;
        const double minLon = std::min(lon_i, lon_j) - 1e-7;
        const double maxLon = std::max(lon_i, lon_j) + 1e-7;

        if (point.latitudeDeg >= minLat && point.latitudeDeg <= maxLat &&
            point.longitudeDeg >= minLon && point.longitudeDeg <= maxLon) {
            const double dLat = lat_j - lat_i;
            const double dLon = lon_j - lon_i;
            const double cross = (point.latitudeDeg - lat_i) * dLon - (point.longitudeDeg - lon_i) * dLat;
            const double segLenSq = (dLat * dLat) + (dLon * dLon);
            if (segLenSq > 1e-18) {
                const double distSq = (cross * cross) / segLenSq;
                if (distSq < 1e-12) { // Within ~0.1m of boundary segment
                    return true;
                }
            } else {
                const double dPt = std::hypot(point.latitudeDeg - lat_i, point.longitudeDeg - lon_i);
                if (dPt < 1e-6) {
                    return true;
                }
            }
        }
    }

    // 2. Standard ray casting for interior test
    bool inside = false;
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const double lat_i = polygon[i].latitudeDeg;
        const double lon_i = polygon[i].longitudeDeg;
        const double lat_j = polygon[j].latitudeDeg;
        const double lon_j = polygon[j].longitudeDeg;

        if (((lat_i > point.latitudeDeg) != (lat_j > point.latitudeDeg))) {
            const double dLat = lat_j - lat_i;
            if (std::abs(dLat) > 1e-12) {
                const double xIntersect = lon_i + (point.latitudeDeg - lat_i) * (lon_j - lon_i) / dLat;
                if (point.longitudeDeg < xIntersect) {
                    inside = !inside;
                }
            }
        }
    }
    return inside;
}

double GeoSurveyGridEngine::calculatePolygonAreaM2(const std::vector<Klv::GeoPoint2D>& polygon) noexcept
{
    const std::size_t n = polygon.size();
    if (n < 3U) {
        return 0.0;
    }

    double refLat = 0.0;
    double refLon = 0.0;
    for (const auto& pt : polygon) {
        refLat += pt.latitudeDeg;
        refLon += pt.longitudeDeg;
    }
    refLat /= static_cast<double>(n);
    refLon /= static_cast<double>(n);

    // Shoelace formula in local Cartesian projection
    double areaAcc = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        double x1 = 0.0; double y1 = 0.0;
        double x2 = 0.0; double y2 = 0.0;
        latLonToLocalMeters(refLat, refLon, polygon[i].latitudeDeg, polygon[i].longitudeDeg, x1, y1);
        latLonToLocalMeters(refLat, refLon, polygon[j].latitudeDeg, polygon[j].longitudeDeg, x2, y2);
        areaAcc += (x1 * y2) - (x2 * y1);
    }
    return std::abs(areaAcc) * 0.5;
}

double GeoSurveyGridEngine::calculateOptimalSweepHeading(const std::vector<Klv::GeoPoint2D>& polygon) noexcept
{
    const std::size_t n = polygon.size();
    if (n < 2U) {
        return 0.0;
    }

    double refLat = 0.0;
    double refLon = 0.0;
    for (const auto& pt : polygon) {
        refLat += pt.latitudeDeg;
        refLon += pt.longitudeDeg;
    }
    refLat /= static_cast<double>(n);
    refLon /= static_cast<double>(n);

    struct Point2D { double x; double y; };
    std::vector<Point2D> pts;
    pts.reserve(n);
    for (const auto& pt : polygon) {
        double x = 0.0; double y = 0.0;
        latLonToLocalMeters(refLat, refLon, pt.latitudeDeg, pt.longitudeDeg, x, y);
        pts.push_back({ x, y });
    }

    double bestHeadingDeg = 0.0;
    double minCrossTrackSpan = std::numeric_limits<double>::max();

    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        const double dx = pts[j].x - pts[i].x;
        const double dy = pts[j].y - pts[i].y;
        const double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1.0) {
            continue;
        }

        // Heading along edge: angle from Y-axis (North)
        const double thetaRad = std::atan2(dx, dy);
        const double sinTheta = std::sin(thetaRad);
        const double cosTheta = std::cos(thetaRad);

        // Project all points to cross-track axis: v = x*cos(theta) - y*sin(theta)
        double minV = std::numeric_limits<double>::max();
        double maxV = std::numeric_limits<double>::lowest();
        for (const auto& p : pts) {
            const double v = (p.x * cosTheta) - (p.y * sinTheta);
            minV = std::min(minV, v);
            maxV = std::max(maxV, v);
        }

        const double span = maxV - minV;
        if (span < minCrossTrackSpan) {
            minCrossTrackSpan = span;
            bestHeadingDeg = normalizeHeadingDeg(thetaRad * RAD_TO_DEG);
        }
    }

    return bestHeadingDeg;
}

// =============================================================================
// Private Helpers
// =============================================================================

void GeoSurveyGridEngine::transitionState(SurveyState newState, const std::string& reason)
{
    m_state = newState;
    if (m_stateCb) {
        m_stateCb(newState, reason);
    }
}

void GeoSurveyGridEngine::initializeOccupancyGrid()
{
    if (m_config.boundaryPolygon.size() < 3U) {
        m_grid = SurveyOccupancyGrid {};
        return;
    }

    double minLat = std::numeric_limits<double>::max();
    double maxLat = std::numeric_limits<double>::lowest();
    double minLon = std::numeric_limits<double>::max();
    double maxLon = std::numeric_limits<double>::lowest();

    for (const auto& pt : m_config.boundaryPolygon) {
        minLat = std::min(minLat, pt.latitudeDeg);
        maxLat = std::max(maxLat, pt.latitudeDeg);
        minLon = std::min(minLon, pt.longitudeDeg);
        maxLon = std::max(maxLon, pt.longitudeDeg);
    }

    // Add 5% buffer margin
    const double latMargin = std::max(0.0001, (maxLat - minLat) * 0.05);
    const double lonMargin = std::max(0.0001, (maxLon - minLon) * 0.05);
    minLat -= latMargin;
    maxLat += latMargin;
    minLon -= lonMargin;
    maxLon += lonMargin;

    const double refLat = (minLat + maxLat) * 0.5;
    const double latSpanMeters = (maxLat - minLat) * (PI / 180.0) * WGS84_A;
    const double lonSpanMeters = (maxLon - minLon) * (PI / 180.0) * WGS84_A * std::cos(refLat * DEG_TO_RAD);

    const double resMeters = std::clamp(m_config.gridResolutionMeters, 1.0, 500.0);
    const std::size_t rows = std::clamp<std::size_t>(static_cast<std::size_t>(std::ceil(latSpanMeters / resMeters)), 10U, 500U);
    const std::size_t cols = std::clamp<std::size_t>(static_cast<std::size_t>(std::ceil(lonSpanMeters / resMeters)), 10U, 500U);

    m_grid.minLat = minLat;
    m_grid.maxLat = maxLat;
    m_grid.minLon = minLon;
    m_grid.maxLon = maxLon;
    m_grid.resolutionMeters = resMeters;
    m_grid.rows = rows;
    m_grid.cols = cols;
    m_grid.cells.assign(rows * cols, GridCellState::OutsidePolygon);

    // Classify interior vs exterior cells
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            double cellLat = 0.0;
            double cellLon = 0.0;
            if (!m_grid.gridToGeo(r, c, cellLat, cellLon)) {
                continue;
            }

            if (isPointInsidePolygon({ cellLat, cellLon }, m_config.boundaryPolygon)) {
                m_grid.setCell(r, c, GridCellState::Unsurveyed);
            }
        }
    }
}

void GeoSurveyGridEngine::updateMetricsLocked()
{
    m_metrics.totalPolygonAreaM2 = calculatePolygonAreaM2(m_config.boundaryPolygon);
    m_metrics.totalTransects = static_cast<std::uint32_t>(m_transects.size());
    m_metrics.completedTransects = static_cast<std::uint32_t>(std::min(m_currentTransectIdx, m_transects.size()));

    std::uint32_t totalWp = 0U;
    std::uint32_t completedWp = 0U;
    for (std::size_t t = 0; t < m_transects.size(); ++t) {
        totalWp += static_cast<std::uint32_t>(m_transects[t].waypoints.size());
        if (t < m_currentTransectIdx) {
            completedWp += static_cast<std::uint32_t>(m_transects[t].waypoints.size());
        } else if (t == m_currentTransectIdx) {
            completedWp += static_cast<std::uint32_t>(std::min(m_currentWaypointIdx, m_transects[t].waypoints.size()));
        }
    }
    m_metrics.totalWaypoints = totalWp;
    m_metrics.completedWaypoints = completedWp;
    m_metrics.elapsedTimeSec = m_elapsedTimeSec;

    // Count grid cells for area calculation
    std::size_t unsurveyedCount = 0U;
    std::size_t surveyedCount = 0U;
    std::size_t shadowCount = 0U;

    for (const auto state : m_grid.cells) {
        if (state == GridCellState::Unsurveyed) {
            unsurveyedCount++;
        } else if (state == GridCellState::Surveyed) {
            surveyedCount++;
        } else if (state == GridCellState::OccludedTerrainShadow) {
            shadowCount++;
        }
    }

    const double cellAreaM2 = m_grid.resolutionMeters * m_grid.resolutionMeters;
    m_metrics.surveyedAreaM2 = static_cast<double>(surveyedCount) * cellAreaM2;
    m_metrics.occludedShadowAreaM2 = static_cast<double>(shadowCount) * cellAreaM2;

    const std::size_t totalValidCells = unsurveyedCount + surveyedCount + shadowCount;
    if (totalValidCells > 0U) {
        m_metrics.coveragePercentage = std::clamp(
            (static_cast<double>(surveyedCount) / static_cast<double>(totalValidCells)) * 100.0, 0.0, 100.0);
    } else {
        m_metrics.coveragePercentage = 0.0;
    }

    // Estimate ETR based on remaining waypoints
    const std::uint32_t remainingWp = (totalWp > completedWp) ? (totalWp - completedWp) : 0U;
    m_metrics.estimatedTimeRemainingSec = static_cast<double>(remainingWp) * (m_config.waypointDwellSec + 0.5);
}

} // namespace PayloadHal
