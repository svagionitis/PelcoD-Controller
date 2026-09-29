#pragma once

/// @file NmeaRouteManager.h
/// @brief High-performance ECDIS Search & Rescue (SAR) and Patrol Route Tracking Manager.

#include "NmeaTypes.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Nmea {

/// @struct NmeaRouteWaypoint
/// @brief Represents a single waypoint in an assembled route with resolved coordinates.
struct NmeaRouteWaypoint {
    std::string id {};
    std::optional<NmeaCoordinates> coordinates {};
    std::size_t sequenceIndex { 0U };
};

/// @struct NmeaRoute
/// @brief Reassembled maritime navigation route consisting of sequentially ordered waypoints.
struct NmeaRoute {
    std::string routeName {};
    char routeType { 'c' }; ///< 'c'=Complete, 'w'=Working
    std::vector<NmeaRouteWaypoint> waypoints {};
    bool isComplete { false };
    std::chrono::steady_clock::time_point lastUpdated {};
};

/// @class NmeaRouteManager
/// @brief Coordinates waypoint dictionary resolution, multi-packet RTE reassembly, and active leg SAR tracking.
class NmeaRouteManager {
public:
    using RouteCallback = std::function<void(const NmeaRoute&)>;
    using WaypointCallback = std::function<void(const WplData&)>;
    using RmbCallback = std::function<void(const RmbData&)>;

    NmeaRouteManager() = default;
    ~NmeaRouteManager() = default;

    // Non-copyable, movable
    NmeaRouteManager(const NmeaRouteManager&) = delete;
    NmeaRouteManager& operator=(const NmeaRouteManager&) = delete;
    NmeaRouteManager(NmeaRouteManager&&) noexcept = default;
    NmeaRouteManager& operator=(NmeaRouteManager&&) noexcept = default;

    /// @brief Ingests a Waypoint Location sentence ($--WPL) into the waypoint dictionary.
    /// @param[in] wpl Deserialized WplData.
    void ingestWpl(const WplData& wpl);

    /// @brief Ingests an RTE sentence slice, reassembling multi-sentence routes.
    /// @param[in] rte Deserialized RteData.
    /// @return True if this slice completed a full route reassembly.
    bool ingestRte(const RteData& rte);

    /// @brief Ingests Recommended Minimum Navigation Information ($--RMB).
    /// @param[in] rmb Deserialized RmbData.
    void ingestRmb(const RmbData& rmb);

    /// @brief Manually stores or overrides a waypoint in the dictionary.
    /// @param[in] waypointId Identifier name.
    /// @param[in] coordinates Latitude and Longitude coordinates.
    void setWaypoint(std::string waypointId, NmeaCoordinates coordinates);

    /// @brief Queries a waypoint by its identifier.
    /// @param[in] waypointId Identifier name.
    /// @return Waypoint coordinates if known, std::nullopt otherwise.
    [[nodiscard]] std::optional<NmeaCoordinates> waypoint(std::string_view waypointId) const;

    /// @brief Queries an assembled route by name.
    /// @param[in] routeName Route identifier.
    /// @return Copy of NmeaRoute if known, std::nullopt otherwise.
    [[nodiscard]] std::optional<NmeaRoute> route(std::string_view routeName) const;

    /// @brief Retrieves the active working route (route marked 'w' or actively selected).
    /// @return Copy of active NmeaRoute if available.
    [[nodiscard]] std::optional<NmeaRoute> activeRoute() const;

    /// @brief Retrieves the latest active RMB navigation state.
    /// @return Latest RmbData if received and valid.
    [[nodiscard]] std::optional<RmbData> activeRmb() const;

    /// @brief Checks whether an active navigation leg is currently valid.
    [[nodiscard]] bool hasActiveLeg() const noexcept;

    /// @brief Calculates forward look-ahead target coordinates along the active leg.
    /// @param[in] lookAheadMeters Distance in meters ahead from origin along the leg vector.
    /// @return Projected geodetic coordinates, or std::nullopt if active leg coordinates are unavailable.
    [[nodiscard]] std::optional<NmeaCoordinates> computeLookAhead(double lookAheadMeters) const;

    /// @brief Calculates an oscillating SAR sweep point across the active track leg.
    /// @param[in] ownShipPos Own ship current geodetic position.
    /// @param[in] crossTrackSweepMeters Lateral offset distance perpendicular to track (+: Right, -: Left).
    /// @param[in] forwardSweepMeters Forward projection distance along track.
    /// @return Target coordinates for gimbal line-of-sight sweep scanning.
    [[nodiscard]] std::optional<NmeaCoordinates> computeSarSweepPoint(
        const NmeaCoordinates& ownShipPos, double crossTrackSweepMeters, double forwardSweepMeters) const;

    /// @brief Retrieves coordinates of the next waypoint after the active destination in the active route.
    /// @return Next waypoint coordinates for pre-turn cueing, or std::nullopt if final waypoint or unknown.
    [[nodiscard]] std::optional<NmeaCoordinates> nextTurnWaypoint() const;

    /// @brief Clears all cached routes and pending assembly fragments.
    void clearRoutes();

    /// @brief Clears all waypoints in the dictionary.
    void clearWaypoints();

    /// @brief Subscribes to route completion and update events.
    void setRouteCallback(RouteCallback cb);

    /// @brief Subscribes to waypoint addition/update events.
    void setWaypointCallback(WaypointCallback cb);

    /// @brief Subscribes to RMB telemetry updates.
    void setRmbCallback(RmbCallback cb);

private:
    struct RteAssemblyState {
        std::uint32_t totalSentences { 1U };
        char routeType { 'c' };
        std::unordered_map<std::uint32_t, std::vector<std::string>> slices {};
        std::chrono::steady_clock::time_point firstSliceTime {};
    };

    void reassembleRouteLocked(const std::string& routeName, RteAssemblyState& state);
    void resolveRouteWaypointsLocked(NmeaRoute& route) const;

    mutable std::mutex m_mutex {};
    std::unordered_map<std::string, NmeaCoordinates> m_waypointDict {};
    std::unordered_map<std::string, NmeaRoute> m_routes {};
    std::unordered_map<std::string, RteAssemblyState> m_pendingRoutes {};
    std::optional<std::string> m_activeRouteName {};
    std::optional<RmbData> m_activeRmb {};

    RouteCallback m_routeCallback {};
    WaypointCallback m_waypointCallback {};
    RmbCallback m_rmbCallback {};
};

} // namespace Nmea
