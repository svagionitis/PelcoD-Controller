#pragma once

/// @file GeoSurveyGridTypes.h
/// @brief Data types and structures for the Terrain-Aware Polygonal Geo-Survey & Search Grid Engine.
/// @details Defines boundary polygon geometry, boustrophedon sweep trajectory waypoints,
///          discrete 2D occupancy coverage grid, and statistical survey execution metrics.

#include "Klv/KlvTypes.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace PayloadHal {

/// @enum SurveyState
/// @brief Operational state of the polygonal geo-survey engine.
enum class SurveyState : std::uint8_t {
    Idle,       ///< No active survey plan; engine standing by
    Planning,   ///< Generating survey transects and sampling terrain
    Executing,  ///< Actively driving gimbal / tracking survey waypoints
    Paused,     ///< Execution suspended; gimbal holding orientation
    Completed,  ///< All survey transects and waypoints successfully covered
    Aborted     ///< Survey cancelled by operator or safety interlock
};

/// @enum SweepPatternType
/// @brief Aerial sweep trajectory routing pattern.
enum class SweepPatternType : std::uint8_t {
    Boustrophedon,  ///< Serpentine back-and-forth sweep lines (alternating directions)
    Unidirectional  ///< Parallel raster tracks in same direction with repositioning
};

/// @enum GridCellState
/// @brief Status of an individual discrete survey coverage cell.
enum class GridCellState : std::uint8_t {
    OutsidePolygon,         ///< Outside survey boundary perimeter
    Unsurveyed,             ///< Inside perimeter; not yet observed
    Surveyed,               ///< Imaged with clear line of sight
    OccludedTerrainShadow   ///< Within sensor frustum but blocked by foreground terrain
};

/// @struct SurveyWaypoint
/// @brief Discrete geodetic observation station along a survey track.
struct SurveyWaypoint {
    Klv::GeoPoint3D targetGroundPos {}; ///< Ground intersection point on terrain
    double commandedPanDeg { 0.0 };     ///< Required gimbal pan angle
    double commandedTiltDeg { 0.0 };    ///< Required gimbal tilt angle
    double expectedGsdMeters { 0.05 };  ///< Calculated Ground Sampling Distance
    double dwellTimeSec { 0.25 };       ///< Exposure/stabilization dwell duration
    std::uint32_t transectIndex { 0U }; ///< Parent transect line index
    std::uint32_t waypointIndex { 0U }; ///< Index within transect
};

/// @struct SurveyTransect
/// @brief Single continuous linear sweep track across the survey polygon.
struct SurveyTransect {
    std::uint32_t index { 0U };
    Klv::GeoPoint2D entryPoint {};
    Klv::GeoPoint2D exitPoint {};
    double headingDeg { 0.0 };
    double lengthMeters { 0.0 };
    std::vector<SurveyWaypoint> waypoints {};
};

/// @struct SurveyPlanConfig
/// @brief Parameters defining the survey boundary, optical constraints, and overlap.
struct SurveyPlanConfig {
    std::vector<Klv::GeoPoint2D> boundaryPolygon {}; ///< Ordered WGS-84 boundary vertices
    double surveyAltitudeAglMeters { 200.0 };        ///< Target survey altitude AGL in meters
    std::optional<double> sweepAngleDeg {};          ///< Sweep heading (nullopt = auto-align with OMBB)
    double forwardOverlapRatio { 0.70 };             ///< Forward (along-track) overlap [0.10 .. 0.90]
    double sideOverlapRatio { 0.50 };                ///< Side (cross-track) overlap [0.10 .. 0.85]
    double cameraHfovDeg { 45.0 };                   ///< Horizontal FOV in degrees
    double cameraVfovDeg { 25.3 };                   ///< Vertical FOV in degrees
    std::uint32_t sensorWidthPixels { 3840U };       ///< Optical sensor horizontal resolution
    std::uint32_t sensorHeightPixels { 2160U };      ///< Optical sensor vertical resolution
    double gridResolutionMeters { 10.0 };            ///< Discretized coverage matrix cell size in meters
    double waypointDwellSec { 0.25 };                ///< Station stabilization dwell time in seconds
    double slewSpeedDegPerSec { 20.0 };              ///< Gimbal slew velocity between stations
    SweepPatternType patternType { SweepPatternType::Boustrophedon };
};

/// @struct CoverageMetrics
/// @brief Real-time statistical metrics of the survey execution.
struct CoverageMetrics {
    double totalPolygonAreaM2 { 0.0 };               ///< Total area of the surveyed boundary polygon
    double surveyedAreaM2 { 0.0 };                   ///< Area covered and imaged without terrain shadow
    double occludedShadowAreaM2 { 0.0 };             ///< Area within sensor frustum blocked by terrain
    double coveragePercentage { 0.0 };               ///< Ratio of surveyed area to total polygon area [0 .. 100%]
    std::uint32_t totalTransects { 0U };             ///< Total count of sweep lines
    std::uint32_t completedTransects { 0U };         ///< Finished sweep lines
    std::uint32_t totalWaypoints { 0U };             ///< Total waypoint observation stations
    std::uint32_t completedWaypoints { 0U };         ///< Completed observation stations
    double elapsedTimeSec { 0.0 };                   ///< Active survey execution duration
    double estimatedTimeRemainingSec { 0.0 };        ///< Estimated Time Remaining based on remaining stations
};

/// @struct SurveyOccupancyGrid
/// @brief Discretized 2D grid matrix mapping terrestrial survey coverage.
struct SurveyOccupancyGrid {
    double minLat { 0.0 };
    double maxLat { 0.0 };
    double minLon { 0.0 };
    double maxLon { 0.0 };
    double resolutionMeters { 10.0 };
    std::size_t rows { 0U };
    std::size_t cols { 0U };
    std::vector<GridCellState> cells {};

    [[nodiscard]] bool isValid() const noexcept
    {
        return (rows > 0U && cols > 0U && cells.size() == rows * cols);
    }

    [[nodiscard]] bool geoToGrid(double lat, double lon, std::size_t& row, std::size_t& col) const noexcept
    {
        if (lat < minLat || lat > maxLat || lon < minLon || lon > maxLon || rows == 0 || cols == 0) {
            return false;
        }
        const double dLat = maxLat - minLat;
        const double dLon = maxLon - minLon;
        if (dLat <= 1e-12 || dLon <= 1e-12) {
            return false;
        }
        const double normRow = (lat - minLat) / dLat;
        const double normCol = (lon - minLon) / dLon;
        row = std::min(rows - 1, static_cast<std::size_t>(normRow * rows));
        col = std::min(cols - 1, static_cast<std::size_t>(normCol * cols));
        return true;
    }

    [[nodiscard]] bool gridToGeo(std::size_t row, std::size_t col, double& lat, double& lon) const noexcept
    {
        if (row >= rows || col >= cols || rows == 0 || cols == 0) {
            return false;
        }
        const double dLat = maxLat - minLat;
        const double dLon = maxLon - minLon;
        lat = minLat + ((static_cast<double>(row) + 0.5) / static_cast<double>(rows)) * dLat;
        lon = minLon + ((static_cast<double>(col) + 0.5) / static_cast<double>(cols)) * dLon;
        return true;
    }

    [[nodiscard]] GridCellState cell(std::size_t row, std::size_t col) const noexcept
    {
        if (row >= rows || col >= cols) {
            return GridCellState::OutsidePolygon;
        }
        return cells[row * cols + col];
    }

    void setCell(std::size_t row, std::size_t col, GridCellState state) noexcept
    {
        if (row < rows && col < cols) {
            cells[row * cols + col] = state;
        }
    }
};

} // namespace PayloadHal
