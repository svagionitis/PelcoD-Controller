#pragma once

/// @file StadiametricRanger.h
/// @brief Passive Stadiametric & Kinematic Triangulation Range Estimator subsystem.
///        Provides passive optical subtended angle ranging and moving-platform
///        multi-observation triangulation without emitting detectable laser radiation.

#include "PlatformLeverArmCompensator.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace PayloadHal {

class IPayload;

/// @enum TargetClassProfile
/// @brief Standardized tactical target profiles with standardized physical dimensions.
enum class TargetClassProfile : std::uint8_t {
    MainBattleTank,         ///< Heavy armored fighting vehicle (e.g., T-72/M1A2: ~3.5m W, 2.4m H, 7.0m L)
    ArmoredPersonnelCarrier,///< APC / IFV (e.g., BMP/BTR: ~2.8m W, 2.2m H, 6.0m L)
    TacticalVehicle,        ///< Light utility vehicle / pickup (~2.0m W, 1.8m H, 4.8m L)
    HumanPersonnel,         ///< Standing human (~0.5m W, 1.8m H, 0.3m L)
    PatrolVessel,           ///< Fast attack naval craft / patrol vessel (~6.0m W, 4.5m H, 25.0m L)
    Helicopter,             ///< Tactical rotorcraft (~3.5m W, 3.8m H, 15.0m L)
    FixedWingUav,           ///< Tactical unmanned aerial vehicle (~2.5m W, 0.8m H, 2.0m L)
    Custom                  ///< Operator-defined physical dimensions
};

/// @enum StadiametricDimensionMode
/// @brief Geometric dimension used as primary stadiametric reference.
enum class StadiametricDimensionMode : std::uint8_t {
    Height,       ///< Vertical dimension (least sensitive to target aspect angle)
    Width,        ///< Horizontal projected beam width
    Length,       ///< Longitude / length
    AutomaticBest ///< Selects dimension with highest pixel resolution and lowest aspect sensitivity
};

/// @enum TriangulationQuality
/// @brief Geometric Dilution of Precision (GDOP) and baseline quality rating.
enum class TriangulationQuality : std::uint8_t {
    InsufficientBaseline, ///< Baseline movement too short or collinear (< minimum convergence angle)
    PoorGdop,             ///< Shallow convergence angle (2° - 5°), wide range uncertainty
    Acceptable,           ///< Moderate baseline convergence (5° - 15°)
    Optimal               ///< High baseline convergence (> 15°), high range precision
};

/// @struct TargetPhysicalDimensions
/// @brief Physical target metric envelope in meters.
struct TargetPhysicalDimensions {
    double widthMeters { 2.0 };
    double heightMeters { 1.8 };
    double lengthMeters { 4.8 };
};

/// @struct OpticalFrameCalibration
/// @brief Optical sensor camera parameters for subtended angle calculations.
struct OpticalFrameCalibration {
    double horizontalFovDeg { 30.0 };
    double verticalFovDeg { 20.0 };
    int frameWidthPx { 1920 };
    int frameHeightPx { 1080 };
};

/// @struct BoundingBoxDetection
/// @brief Optical tracker or operator-drawn target bounding box in normalized [0, 1] screen coordinates.
struct BoundingBoxDetection {
    double normX { 0.5 };           ///< Normalized center X [0.0, 1.0]
    double normY { 0.5 };           ///< Normalized center Y [0.0, 1.0]
    double normWidth { 0.05 };      ///< Normalized width [0.0, 1.0]
    double normHeight { 0.05 };     ///< Normalized height [0.0, 1.0]
    double pixelJitter1Sigma { 1.5 }; ///< Estimated detection pixel noise standard deviation in pixels
};

/// @struct StadiametricEstimate
/// @brief Outcome of optical stadiametric subtended angle ranging.
struct StadiametricEstimate {
    double slantRangeMeters { 0.0 };
    double rangeUncertaintyMeters { 0.0 }; ///< 1-sigma uncertainty standard deviation
    double subtendedAngleMrad { 0.0 };     ///< Subtended optical angle in milliradians
    bool valid { false };
    StadiametricDimensionMode usedDimension { StadiametricDimensionMode::Height };
};

/// @struct BearingObservation
/// @brief Instantaneous platform position and measured line-of-sight unit vector in NED frame.
struct BearingObservation {
    Vector3D platformPositionNedMeters { 0.0, 0.0, 0.0 };
    Vector3D losUnitVectorNed { 1.0, 0.0, 0.0 };
    double azimuthDeg { 0.0 };
    double elevationDeg { 0.0 };
    std::chrono::steady_clock::time_point timestamp { std::chrono::steady_clock::now() };
};

/// @struct TriangulationEstimate
/// @brief Outcome of kinematic multi-observation triangulation.
struct TriangulationEstimate {
    Vector3D targetPositionNedMeters { 0.0, 0.0, 0.0 };
    double slantRangeFromLatestMeters { 0.0 };
    double rangeUncertaintyMeters { 0.0 };
    double baselineTraversedMeters { 0.0 };
    double convergenceAngleDeg { 0.0 };
    double rayMissDistanceMeters { 0.0 };
    TriangulationQuality quality { TriangulationQuality::InsufficientBaseline };
    std::size_t observationsUsed { 0 };
    bool valid { false };
};

/// @struct PassiveRangeSolution
/// @brief Unified fused passive range estimation output combining all available methods.
struct PassiveRangeSolution {
    double estimatedRangeMeters { 0.0 };
    double rangeUncertaintyMeters { 0.0 };
    double confidence01 { 0.0 };
    bool valid { false };
    bool stadiametricAvailable { false };
    bool triangulationAvailable { false };
    StadiametricEstimate stadiametric {};
    TriangulationEstimate triangulation {};
    std::string diagnosticText {};
};

/// @class StadiametricRanger
/// @brief Passive Stadiametric & Kinematic Triangulation Range Estimator.
///        Enables stealthy, covert line-of-sight distance measurement without laser emission.
class StadiametricRanger {
public:
    /// @brief Constructs ranger with designated observation history depth.
    /// @param[in] maxHistory Maximum number of sequential bearing observations to retain.
    explicit StadiametricRanger(std::size_t maxHistory = 30);
    ~StadiametricRanger() = default;

    // --- Configuration & Target Profiles ---

    /// @brief Sets active target classification profile.
    /// @param[in] profile NATO target profile.
    void setTargetProfile(TargetClassProfile profile);

    /// @brief Retrieves the active target classification profile.
    [[nodiscard]] TargetClassProfile targetProfile() const noexcept;

    /// @brief Configures custom target physical dimensions.
    /// @param[in] dims Target dimensions in meters.
    void setCustomTargetDimensions(const TargetPhysicalDimensions& dims);

    /// @brief Retrieves currently effective physical dimensions for active profile.
    [[nodiscard]] TargetPhysicalDimensions targetDimensions() const;

    /// @brief Configures camera optical calibration.
    /// @param[in] calib Optical FOV and frame resolution.
    void setOpticalCalibration(const OpticalFrameCalibration& calib);

    /// @brief Retrieves active optical calibration.
    [[nodiscard]] OpticalFrameCalibration opticalCalibration() const noexcept;

    /// @brief Sets minimum convergence parallax angle threshold in degrees.
    /// @param[in] deg Minimum angle threshold (typically 1.5° - 3.0°).
    void setMinimumConvergenceAngleDeg(double deg) noexcept;

    /// @brief Retrieves minimum convergence parallax angle threshold.
    [[nodiscard]] double minimumConvergenceAngleDeg() const noexcept;

    // --- Stadiametric Passive Ranging ---

    /// @brief Computes instantaneous optical stadiametric range from a bounding box.
    /// @param[in] bbox Normalized bounding box detection on image plane.
    /// @param[in] mode Dimension mode to use (Height, Width, Length, AutomaticBest).
    /// @param[in] aspectAngleDeg Target heading angle relative to Line of Sight (LOS) in degrees [0, 360).
    /// @return StadiametricEstimate containing slant range and 1-sigma uncertainty.
    [[nodiscard]] StadiametricEstimate estimateStadiametricRange(
        const BoundingBoxDetection& bbox,
        StadiametricDimensionMode mode = StadiametricDimensionMode::Height,
        double aspectAngleDeg = 0.0) const;

    // --- Kinematic Multi-Observation Triangulation ---

    /// @brief Ingests a structured bearing observation.
    /// @param[in] obs Observation containing platform NED coordinates and line-of-sight vector.
    void addBearingObservation(const BearingObservation& obs);

    /// @brief Overload taking angles and platform coordinates directly.
    /// @param[in] azimuthDeg LOS azimuth in degrees [0, 360).
    /// @param[in] elevationDeg LOS elevation in degrees [-90, +90] (positive up).
    /// @param[in] platformPositionNed Platform position in local NED frame in meters.
    /// @param[in] timestamp Capture timestamp.
    void addBearingObservation(
        double azimuthDeg, double elevationDeg,
        const Vector3D& platformPositionNed,
        std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now());

    /// @brief Computes 2-observation Closest Point of Approach (CPA) triangulation
    ///        between oldest and newest observations in the history buffer.
    /// @return TriangulationEstimate with 3D target coordinates and miss distance.
    [[nodiscard]] TriangulationEstimate computeTwoPointTriangulation() const;

    /// @brief Computes optimal N-observation least-squares triangulation across all observations in history.
    /// @return TriangulationEstimate with global 3D least-squares target position and covariance bounds.
    [[nodiscard]] TriangulationEstimate computeBatchTriangulation() const;

    /// @brief Clears accumulated triangulation observation history.
    void resetTriangulation();

    /// @brief Queries current number of stored observations.
    [[nodiscard]] std::size_t observationCount() const noexcept;

    // --- Fused Multi-Source Solution ---

    /// @brief Computes a unified fused passive range solution combining stadiametric and triangulation.
    /// @param[in] bbox Optional bounding box for instantaneous stadiametry.
    /// @param[in] aspectAngleDeg Target aspect angle in degrees.
    /// @return PassiveRangeSolution containing fused estimate and diagnostics.
    [[nodiscard]] PassiveRangeSolution computeFusedRange(
        const std::optional<BoundingBoxDetection>& bbox = std::nullopt,
        double aspectAngleDeg = 0.0) const;

    // --- Automatic Ingestion from IPayload ---

    /// @brief Ingests live telemetry from an IPayload instance.
    /// @param[in] payload Reference to active payload.
    /// @param[in] platformPositionNed Platform position in local NED frame.
    /// @param[in] bbox Optional active tracker bounding box.
    void updateFromPayload(
        const IPayload& payload,
        const Vector3D& platformPositionNed = { 0.0, 0.0, 0.0 },
        const std::optional<BoundingBoxDetection>& bbox = std::nullopt);

private:
    TargetClassProfile m_profile { TargetClassProfile::MainBattleTank };
    TargetPhysicalDimensions m_customDims {};
    OpticalFrameCalibration m_calib {};
    double m_minConvergenceAngleDeg { 2.0 };
    std::size_t m_maxHistory { 30 };

    std::deque<BearingObservation> m_history {};
    mutable std::mutex m_mutex {};

    [[nodiscard]] static TargetPhysicalDimensions defaultDimensionsForProfile(TargetClassProfile profile) noexcept;
    [[nodiscard]] static Vector3D anglesToUnitVectorNed(double azDeg, double elDeg) noexcept;
};

} // namespace PayloadHal
