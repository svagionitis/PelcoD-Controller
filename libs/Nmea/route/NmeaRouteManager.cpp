#include "NmeaRouteManager.h"

#include <algorithm>
#include <cmath>

namespace Nmea {

namespace {

    constexpr double kEarthRadiusMeters = 6371000.0;
    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;

    [[nodiscard]] double normalizeDegrees(double deg) noexcept
    {
        while (deg < 0.0) {
            deg += 360.0;
        }
        while (deg >= 360.0) {
            deg -= 360.0;
        }
        return deg;
    }

    [[nodiscard]] double computeBearing(const NmeaCoordinates& from, const NmeaCoordinates& to) noexcept
    {
        const double phi1 = from.latitudeDeg * kDegToRad;
        const double phi2 = to.latitudeDeg * kDegToRad;
        const double deltaLambda = (to.longitudeDeg - from.longitudeDeg) * kDegToRad;

        const double y = std::sin(deltaLambda) * std::cos(phi2);
        const double x = std::cos(phi1) * std::sin(phi2) - std::sin(phi1) * std::cos(phi2) * std::cos(deltaLambda);
        const double bearingRad = std::atan2(y, x);
        return normalizeDegrees(bearingRad * kRadToDeg);
    }

    [[nodiscard]] NmeaCoordinates projectPoint(
        const NmeaCoordinates& origin, double distanceMeters, double bearingDeg) noexcept
    {
        const double delta = distanceMeters / kEarthRadiusMeters;
        const double theta = bearingDeg * kDegToRad;
        const double phi1 = origin.latitudeDeg * kDegToRad;
        const double lambda1 = origin.longitudeDeg * kDegToRad;

        const double sinPhi2 = std::sin(phi1) * std::cos(delta) + std::cos(phi1) * std::sin(delta) * std::cos(theta);
        const double phi2 = std::asin(std::clamp(sinPhi2, -1.0, 1.0));

        const double y = std::sin(theta) * std::sin(delta) * std::cos(phi1);
        const double x = std::cos(delta) - std::sin(phi1) * std::sin(phi2);
        const double lambda2 = lambda1 + std::atan2(y, x);

        NmeaCoordinates target {};
        target.latitudeDeg = phi2 * kRadToDeg;
        target.longitudeDeg = lambda2 * kRadToDeg;

        // Wrap longitude to [-180, 180]
        while (target.longitudeDeg > 180.0) {
            target.longitudeDeg -= 360.0;
        }
        while (target.longitudeDeg < -180.0) {
            target.longitudeDeg += 360.0;
        }

        return target;
    }

} // namespace

void NmeaRouteManager::ingestWpl(const WplData& wpl)
{
    if (!wpl.valid || wpl.waypointId.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_waypointDict[wpl.waypointId] = wpl.coordinates;

    // Resolve any existing routes that might be missing this waypoint's coordinates
    for (auto& [name, route] : m_routes) {
        for (auto& wpt : route.waypoints) {
            if (wpt.id == wpl.waypointId && !wpt.coordinates.has_value()) {
                wpt.coordinates = wpl.coordinates;
            }
        }
    }

    if (m_waypointCallback) {
        m_waypointCallback(wpl);
    }
}

bool NmeaRouteManager::ingestRte(const RteData& rte)
{
    if (!rte.valid || rte.routeName.empty() || rte.totalSentences == 0U || rte.sentenceNumber == 0U) {
        return false;
    }

    RouteCallback callbackToInvoke {};
    NmeaRoute completedRoute {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& state = m_pendingRoutes[rte.routeName];
        if (state.slices.empty()) {
            state.firstSliceTime = std::chrono::steady_clock::now();
        }
        state.totalSentences = rte.totalSentences;
        state.routeType = rte.routeType;
        state.slices[rte.sentenceNumber] = rte.waypointIds;

        // Check if all slices have been received
        if (state.slices.size() == state.totalSentences) {
            reassembleRouteLocked(rte.routeName, state);
            completedRoute = m_routes[rte.routeName];
            m_pendingRoutes.erase(rte.routeName);

            if (rte.routeType == 'w' || !m_activeRouteName.has_value()) {
                m_activeRouteName = rte.routeName;
            }
            callbackToInvoke = m_routeCallback;
        }
    }

    if (callbackToInvoke) {
        callbackToInvoke(completedRoute);
    }

    return completedRoute.isComplete;
}

void NmeaRouteManager::reassembleRouteLocked(const std::string& routeName, RteAssemblyState& state)
{
    NmeaRoute route {};
    route.routeName = routeName;
    route.routeType = state.routeType;
    route.lastUpdated = std::chrono::steady_clock::now();

    std::size_t seqIndex { 0U };
    for (std::uint32_t i = 1U; i <= state.totalSentences; ++i) {
        const auto it = state.slices.find(i);
        if (it != state.slices.end()) {
            for (const auto& wptId : it->second) {
                NmeaRouteWaypoint wpt {};
                wpt.id = wptId;
                wpt.sequenceIndex = seqIndex++;
                route.waypoints.push_back(std::move(wpt));
            }
        }
    }

    resolveRouteWaypointsLocked(route);
    route.isComplete = true;
    m_routes[routeName] = route;
}

void NmeaRouteManager::resolveRouteWaypointsLocked(NmeaRoute& route) const
{
    for (auto& wpt : route.waypoints) {
        const auto it = m_waypointDict.find(wpt.id);
        if (it != m_waypointDict.end()) {
            wpt.coordinates = it->second;
        }
    }
}

void NmeaRouteManager::ingestRmb(const RmbData& rmb)
{
    if (!rmb.valid) {
        return;
    }

    RmbCallback cbToInvoke {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeRmb = rmb;

        // If destination waypoint has coordinates, cache in dictionary
        if (!rmb.destWaypointId.empty()) {
            m_waypointDict[rmb.destWaypointId] = rmb.destCoordinates;
        }

        cbToInvoke = m_rmbCallback;
    }

    if (cbToInvoke) {
        cbToInvoke(rmb);
    }
}

void NmeaRouteManager::setWaypoint(std::string waypointId, NmeaCoordinates coordinates)
{
    WplData wpl {};
    wpl.waypointId = std::move(waypointId);
    wpl.coordinates = coordinates;
    wpl.valid = true;
    ingestWpl(wpl);
}

std::optional<NmeaCoordinates> NmeaRouteManager::waypoint(std::string_view waypointId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_waypointDict.find(std::string(waypointId));
    if (it != m_waypointDict.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<NmeaRoute> NmeaRouteManager::route(std::string_view routeName) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_routes.find(std::string(routeName));
    if (it != m_routes.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<NmeaRoute> NmeaRouteManager::activeRoute() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_activeRouteName.has_value()) {
        const auto it = m_routes.find(*m_activeRouteName);
        if (it != m_routes.end()) {
            return it->second;
        }
    }
    return std::nullopt;
}

std::optional<RmbData> NmeaRouteManager::activeRmb() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeRmb;
}

bool NmeaRouteManager::hasActiveLeg() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeRmb.has_value() && m_activeRmb->statusActive;
}

std::optional<NmeaCoordinates> NmeaRouteManager::computeLookAhead(double lookAheadMeters) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_activeRmb.has_value() || !m_activeRmb->statusActive) {
        return std::nullopt;
    }

    const auto& rmb = *m_activeRmb;
    std::optional<NmeaCoordinates> originCoords {};
    if (!rmb.originWaypointId.empty()) {
        const auto it = m_waypointDict.find(rmb.originWaypointId);
        if (it != m_waypointDict.end()) {
            originCoords = it->second;
        }
    }

    if (originCoords.has_value()) {
        const double trackBearing = computeBearing(*originCoords, rmb.destCoordinates);
        return projectPoint(*originCoords, lookAheadMeters, trackBearing);
    }

    // Fallback: project back from destination
    const double rangeMeters = rmb.rangeToDestNmi * 1852.0;
    const double forwardDist = (lookAheadMeters < rangeMeters) ? lookAheadMeters : rangeMeters;
    return projectPoint(
        rmb.destCoordinates, rangeMeters - forwardDist, normalizeDegrees(rmb.bearingToDestTrueDeg + 180.0));
}

std::optional<NmeaCoordinates> NmeaRouteManager::computeSarSweepPoint(
    const NmeaCoordinates& ownShipPos, double crossTrackSweepMeters, double forwardSweepMeters) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_activeRmb.has_value()) {
        return std::nullopt;
    }

    const auto& rmb = *m_activeRmb;
    const double trackBearing = rmb.bearingToDestTrueDeg;

    // 1. Advance forward along track
    const auto forwardPoint = projectPoint(ownShipPos, forwardSweepMeters, trackBearing);

    // 2. Displace perpendicular to track (+: 90 deg right, -: 90 deg left)
    const double lateralBearing = normalizeDegrees(trackBearing + (crossTrackSweepMeters >= 0.0 ? 90.0 : -90.0));
    return projectPoint(forwardPoint, std::abs(crossTrackSweepMeters), lateralBearing);
}

std::optional<NmeaCoordinates> NmeaRouteManager::nextTurnWaypoint() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_activeRmb.has_value() || !m_activeRouteName.has_value()) {
        return std::nullopt;
    }

    const auto routeIt = m_routes.find(*m_activeRouteName);
    if (routeIt == m_routes.end()) {
        return std::nullopt;
    }

    const auto& waypoints = routeIt->second.waypoints;
    const std::string& currentDestId = m_activeRmb->destWaypointId;

    for (std::size_t i = 0U; i < waypoints.size(); ++i) {
        if (waypoints[i].id == currentDestId && (i + 1U < waypoints.size())) {
            const auto& nextWpt = waypoints[i + 1U];
            if (nextWpt.coordinates.has_value()) {
                return nextWpt.coordinates;
            }
            const auto dictIt = m_waypointDict.find(nextWpt.id);
            if (dictIt != m_waypointDict.end()) {
                return dictIt->second;
            }
            break;
        }
    }

    return std::nullopt;
}

void NmeaRouteManager::clearRoutes()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_routes.clear();
    m_pendingRoutes.clear();
    m_activeRouteName.reset();
}

void NmeaRouteManager::clearWaypoints()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_waypointDict.clear();
}

void NmeaRouteManager::setRouteCallback(RouteCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_routeCallback = std::move(cb);
}

void NmeaRouteManager::setWaypointCallback(WaypointCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_waypointCallback = std::move(cb);
}

void NmeaRouteManager::setRmbCallback(RmbCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rmbCallback = std::move(cb);
}

} // namespace Nmea
