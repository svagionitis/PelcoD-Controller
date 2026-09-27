#pragma once

/// @file GeoSurveyGridEngine.h
/// @brief Terrain-Aware Polygonal Geo-Survey & Search Grid Engine.
/// @details Generates optimal boustrophedon sweep trajectories over arbitrary convex and concave WGS-84
///          polygons, dynamically adapts Ground Sampling Distance (GSD) using Digital Elevation Models (DEM),
///          maintains a real-time discrete occupancy coverage grid, and drives closed-loop gimbal tracking.

#include "GeoSurveyGridTypes.h"
#include "ICameraPayload.h"
#include "IDemProvider.h"
#include "IPanTiltUnit.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace PayloadHal {

/// @class GeoSurveyGridEngine
/// @brief Orchestrates polygonal geo-surveys, terrain-aware sweep trajectories, and real-time coverage mapping.
class GeoSurveyGridEngine {
public:
    using ProgressCallback = std::function<void(
        const CoverageMetrics& metrics, std::uint32_t currentTransect, std::uint32_t currentWaypoint)>;
    using StateCallback = std::function<void(SurveyState newState, const std::string& reason)>;

    /// @brief Constructs the engine with optional PTU, camera, and DEM terrain provider.
    /// @param[in] ptu Optional gimbal interface for line-of-sight pointing.
    /// @param[in] camera Optional camera payload for optical FOV and GSD queries.
    /// @param[in] dem Optional digital elevation model provider for terrain height and shadow analysis.
    /// @param[in] config Initial survey configuration parameters.
    explicit GeoSurveyGridEngine(
        std::shared_ptr<IPanTiltUnit> ptu = nullptr,
        std::shared_ptr<ICameraPayload> camera = nullptr,
        std::shared_ptr<IDemProvider> dem = nullptr,
        SurveyPlanConfig config = {}) noexcept;

    virtual ~GeoSurveyGridEngine();

    // Disable copy semantics; allow move semantics
    GeoSurveyGridEngine(const GeoSurveyGridEngine&) = delete;
    GeoSurveyGridEngine& operator=(const GeoSurveyGridEngine&) = delete;
    GeoSurveyGridEngine(GeoSurveyGridEngine&&) noexcept = default;
    GeoSurveyGridEngine& operator=(GeoSurveyGridEngine&&) noexcept = default;

    // --- Configuration & Trajectory Planning ---

    /// @brief Sets the survey plan configuration parameters.
    /// @param[in] config Survey plan configuration.
    void setPlanConfig(const SurveyPlanConfig& config);

    /// @brief Retrieves the active survey plan configuration.
    [[nodiscard]] SurveyPlanConfig planConfig() const;

    /// @brief Generates the survey trajectory, transect lines, waypoints, and initializes the occupancy grid.
    /// @return True if a valid survey plan was successfully generated, false if boundary polygon is invalid.
    bool generatePlan();

    /// @brief Retrieves the generated survey transect lines.
    [[nodiscard]] std::vector<SurveyTransect> plannedTransects() const;

    /// @brief Computes cumulative linear survey track length in meters.
    [[nodiscard]] double totalPlanLengthMeters() const noexcept;

    /// @brief Computes the optimal sweep heading in degrees aligning with the polygon's principal axis.
    [[nodiscard]] double optimalSweepHeadingDeg() const noexcept;

    // --- Survey Execution & Control ---

    /// @brief Starts execution of the generated survey plan.
    /// @return True if execution started, false if no valid plan exists or already executing.
    bool startSurvey();

    /// @brief Suspends active survey execution and holds current gimbal orientation.
    /// @return True if paused.
    bool pauseSurvey();

    /// @brief Resumes a paused survey execution.
    /// @return True if resumed.
    bool resumeSurvey();

    /// @brief Aborts active survey execution and stops all gimbal motion.
    void abortSurvey();

    /// @brief Skips remaining waypoints in current transect and commands gimbal to next transect track line.
    /// @return True if next transect was selected, false if on final transect.
    bool skipToNextTransect();

    /// @brief Queries the current operational survey state.
    [[nodiscard]] SurveyState surveyState() const noexcept;

    /// @brief Advances survey execution, drives gimbal tracking, stamps footprints, and updates metrics.
    /// @param[in] deltaSeconds Time elapsed since last update in seconds.
    /// @param[in] platformPos Host vehicle 3D coordinate (latitude, longitude, altitude MSL).
    /// @param[in] platformHeadingDeg Host vehicle true compass heading in degrees [0, 360).
    void update(double deltaSeconds, const Klv::GeoPoint3D& platformPos, double platformHeadingDeg);

    // --- Real-Time Frustum & Coverage Mapping ---

    /// @brief Stamps a camera 4-corner ground projection footprint onto the occupancy grid.
    /// @param[in] frustum 4-corner ground projection frustum coordinates.
    /// @param[in] platformPos Host platform 3D coordinate for terrain occlusion ray casting.
    void stampFootprint(const Klv::FrustumCorners& frustum, const Klv::GeoPoint3D& platformPos);

    /// @brief Retrieves instantaneous coverage statistics and completion progress.
    [[nodiscard]] CoverageMetrics coverageMetrics() const noexcept;

    /// @brief Retrieves the current discrete survey occupancy grid.
    [[nodiscard]] SurveyOccupancyGrid occupancyGrid() const;

    // --- Callbacks ---

    void setProgressCallback(ProgressCallback cb);
    void setStateCallback(StateCallback cb);

    // --- Static Geometric & Analytical Utilities ---

    /// @brief Determines whether a geodetic point lies inside an arbitrary closed polygon.
    /// @param[in] point Geodetic 2D coordinate.
    /// @param[in] polygon Ordered list of polygon boundary vertices.
    /// @return True if point is inside or on the boundary.
    [[nodiscard]] static bool isPointInsidePolygon(
        const Klv::GeoPoint2D& point, const std::vector<Klv::GeoPoint2D>& polygon) noexcept;

    /// @brief Computes the surface area of a WGS-84 boundary polygon in square meters.
    /// @param[in] polygon Ordered list of polygon boundary vertices.
    /// @return Surface area in square meters.
    [[nodiscard]] static double calculatePolygonAreaM2(
        const std::vector<Klv::GeoPoint2D>& polygon) noexcept;

    /// @brief Computes the optimal sweep angle aligning with the polygon's longest axis.
    /// @param[in] polygon Ordered list of polygon boundary vertices.
    /// @return True compass heading in degrees [0, 360).
    [[nodiscard]] static double calculateOptimalSweepHeading(
        const std::vector<Klv::GeoPoint2D>& polygon) noexcept;

private:
    void transitionState(SurveyState newState, const std::string& reason);
    void initializeOccupancyGrid();
    void updateMetricsLocked();

    std::shared_ptr<IPanTiltUnit> m_ptu {};
    std::shared_ptr<ICameraPayload> m_camera {};
    std::shared_ptr<IDemProvider> m_dem {};

    mutable std::mutex m_mutex;
    SurveyPlanConfig m_config {};
    SurveyState m_state { SurveyState::Idle };

    std::vector<SurveyTransect> m_transects {};
    double m_totalLengthMeters { 0.0 };
    double m_optimalHeadingDeg { 0.0 };

    // Execution state
    std::size_t m_currentTransectIdx { 0U };
    std::size_t m_currentWaypointIdx { 0U };
    double m_waypointDwellElapsedSec { 0.0 };
    double m_elapsedTimeSec { 0.0 };

    // Real-time grid & metrics
    SurveyOccupancyGrid m_grid {};
    CoverageMetrics m_metrics {};

    ProgressCallback m_progressCb {};
    StateCallback m_stateCb {};
};

} // namespace PayloadHal
