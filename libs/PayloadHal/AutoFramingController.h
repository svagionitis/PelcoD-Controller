#pragma once

/// @file AutoFramingController.h
/// @brief Range-adaptive optical field-of-view and zoom controller for automated target framing and zoom scheduling.

#include "ICameraPayload.h"
#include "Nmea/AisTypes.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

namespace PayloadHal {

/// @enum LensZoomCurveType
/// @brief Optical curve mapping normalized zoom position [0.0 .. 1.0] to focal length and HFOV.
enum class LensZoomCurveType : std::uint8_t {
    LinearHfov = 0,     ///< Direct linear interpolation of HFOV across zoom [0..1]
    LogarithmicFocal    ///< Exponential focal length growth (typical for motorized optical zoom)
};

/// @struct TargetPhysicalEnvelope
/// @brief 3D metric bounding dimensions of a target.
struct TargetPhysicalEnvelope {
    double lengthMeters { 20.0 };  ///< Length along longitudinal axis in meters
    double beamMeters { 6.0 };     ///< Beam / width across transverse axis in meters
    double heightMeters { 4.0 };   ///< Height above waterline in meters
};

/// @struct TargetFramingMetrics
/// @brief Geometric framing evaluation results for a target.
struct TargetFramingMetrics {
    double apparentWidthMeters { 0.0 };      ///< Projected apparent width perpendicular to LOS
    double apparentHeightMeters { 0.0 };     ///< Apparent height in meters
    double relativeAspectAngleDeg { 0.0 };   ///< Target heading relative to LOS in degrees [0, 180]
    double desiredHfovDeg { 0.0 };           ///< Desired horizontal FOV in degrees
    double targetNormalizedZoom { 0.0 };     ///< Calculated ideal normalized zoom [0.0 .. 1.0]
};

/// @struct AutoFramingConfig
/// @brief Configuration parameters for range-adaptive target optical framing and zoom scheduling.
struct AutoFramingConfig {
    double targetFrameOccupancyRatio { 0.30 };    ///< Target occupies 30% of horizontal FOV
    double targetVerticalOccupancyRatio { 0.50 };  ///< Target occupies up to 50% of vertical FOV
    double frameAspectRatio { 16.0 / 9.0 };        ///< Frame aspect ratio (Width / Height)
    double minTargetDimensionMeters { 2.0 };       ///< Minimum clamp for target size (e.g. small skiff/buoy)
    double maxTargetDimensionMeters { 400.0 };     ///< Maximum clamp for target size (e.g. supertanker)
    double defaultTargetLengthMeters { 20.0 };     ///< Default target length when dimensions unavailable
    double defaultTargetBeamMeters { 6.0 };        ///< Default target beam
    double defaultTargetHeightMeters { 4.0 };      ///< Default target height
    double wideHfovDeg { 60.0 };                   ///< Horizontal FOV at 1.0x (full wide) in degrees
    double teleHfovDeg { 2.0 };                    ///< Horizontal FOV at maximum optical tele in degrees
    double focalLengthMinMm { 4.3 };               ///< Lens focal length at wide limit in mm
    double focalLengthMaxMm { 129.0 };             ///< Lens focal length at tele limit in mm
    LensZoomCurveType zoomCurve { LensZoomCurveType::LogarithmicFocal }; ///< Optical zoom curve model
    double minSlantRangeMeters { 10.0 };           ///< Minimum range clamp to prevent division by zero
    double maxSlantRangeMeters { 50000.0 };        ///< Maximum range clamp (50 km)
    double rangeHysteresisRatio { 0.06 };          ///< 6% range deadband to suppress zoom motor hunting
    double zoomDeadband01 { 0.025 };               ///< 2.5% normalized zoom deadband
    double maxZoomVelocityPerSec { 0.35 };         ///< Maximum zoom slew rate in normalized units / sec
    double zoomConvergenceTolerance01 { 0.02 };   ///< Zoom considered converged within 2%
    std::chrono::milliseconds maxFramingDuration { 4000 }; ///< Timeout for framing convergence
    bool triggerAutofocusOnConvergence { true };   ///< Trigger one-push AF once zoom settles
};

/// @class AutoFramingController
/// @brief Computes optimal camera optical field-of-view, schedules zoom motion, and manages focus handoff.
/// @details Ensures that electro-optical sensors automatically zoom to display targets at consistent,
///          identifiable resolution regardless of whether the target is 200m or 5km away, taking into account
///          target 3D aspect angle, non-linear lens geometry, motor slew rate limits, and deadbands.
class AutoFramingController {
public:
    /// @brief Constructs an AutoFramingController with optional configuration.
    /// @param[in] config Framing ratios, optical boundaries, and kinematics limits.
    explicit AutoFramingController(const AutoFramingConfig& config = {});

    virtual ~AutoFramingController() = default;

    // Non-copyable, movable
    AutoFramingController(const AutoFramingController&) = delete;
    AutoFramingController& operator=(const AutoFramingController&) = delete;
    AutoFramingController(AutoFramingController&&) noexcept = default;
    AutoFramingController& operator=(AutoFramingController&&) noexcept = default;

    /// @brief Updates framing configuration parameters.
    /// @param[in] config New configuration.
    void setConfig(const AutoFramingConfig& config);

    /// @brief Retrieves the active configuration snapshot.
    /// @return Current AutoFramingConfig.
    [[nodiscard]] AutoFramingConfig config() const;

    /// @brief Calculates apparent target width perpendicular to the line-of-sight vector.
    /// @param[in] lengthMeters Physical length of target in meters.
    /// @param[in] beamMeters Physical beam/width of target in meters.
    /// @param[in] aspectAngleDeg Target heading relative to LOS in degrees.
    /// @return Apparent projected width in meters.
    [[nodiscard]] static double calculateApparentWidth(
        double lengthMeters, double beamMeters, double aspectAngleDeg) noexcept;

    /// @brief Calculates the ideal Horizontal Field of View (HFOV) for a target.
    /// @param[in] slantRangeMeters Distance to target in meters.
    /// @param[in] targetDimensionMeters Physical target dimension (length or beam) in meters.
    /// @param[in] occupancyRatio Desired screen width ratio [0.05 .. 0.95].
    /// @return Desired HFOV in degrees.
    [[nodiscard]] static double calculateDesiredHfov(
        double slantRangeMeters, double targetDimensionMeters, double occupancyRatio = 0.30) noexcept;

    /// @brief Calculates framing HFOV constrained by both horizontal width and vertical height occupancy.
    /// @param[in] slantRangeMeters Distance to target in meters.
    /// @param[in] apparentWidthMeters Projected target width in meters.
    /// @param[in] apparentHeightMeters Target height in meters.
    /// @param[in] widthRatio Desired width occupancy ratio.
    /// @param[in] heightRatio Desired height occupancy ratio.
    /// @param[in] aspectRatio Frame aspect ratio (Width / Height).
    /// @return Desired HFOV in degrees without clipping either dimension.
    [[nodiscard]] static double calculateFramingHfov(
        double slantRangeMeters, double apparentWidthMeters, double apparentHeightMeters,
        double widthRatio, double heightRatio, double aspectRatio) noexcept;

    /// @brief Calculates normalized optical zoom position [0.0 .. 1.0] from target range and size (legacy 5-param).
    /// @param[in] slantRangeMeters Distance to target in meters.
    /// @param[in] targetDimensionMeters Physical target dimension in meters.
    /// @param[in] occupancyRatio Desired screen occupancy ratio.
    /// @param[in] wideHfovDeg Lens horizontal FOV at full wide.
    /// @param[in] teleHfovDeg Lens horizontal FOV at full tele.
    /// @return Normalized zoom position [0.0 (wide) .. 1.0 (tele)].
    [[nodiscard]] static double calculateNormalizedZoom(double slantRangeMeters, double targetDimensionMeters,
        double occupancyRatio, double wideHfovDeg, double teleHfovDeg) noexcept;

    /// @brief Calculates normalized zoom position from desired HFOV taking lens curve into account.
    /// @param[in] desiredHfovDeg Desired horizontal FOV in degrees.
    /// @param[in] wideHfovDeg Lens horizontal FOV at 0.0 (wide).
    /// @param[in] teleHfovDeg Lens horizontal FOV at 1.0 (tele).
    /// @param[in] curveType Lens optical mapping curve.
    /// @return Normalized zoom position [0.0 .. 1.0].
    [[nodiscard]] static double calculateNormalizedZoom(
        double desiredHfovDeg, double wideHfovDeg, double teleHfovDeg,
        LensZoomCurveType curveType) noexcept;

    /// @brief Estimates target reference dimension in meters from AIS dimensions or fallback.
    /// @param[in] dimensions AIS physical dimensions struct.
    /// @return Estimated dimension in meters.
    [[nodiscard]] double estimateTargetDimension(const Nmea::AisDimensions& dimensions) const noexcept;

    /// @brief Estimates 3D target physical envelope from AIS dimensions and optional ship type.
    /// @param[in] dimensions AIS physical dimensions struct.
    /// @param[in] shipType Optional AIS ship type code (e.g. 70=Cargo, 80=Tanker, 30=Fishing).
    /// @return TargetPhysicalEnvelope in meters.
    [[nodiscard]] TargetPhysicalEnvelope estimateTargetEnvelope(
        const Nmea::AisDimensions& dimensions,
        std::optional<std::uint8_t> shipType = std::nullopt) const noexcept;

    /// @brief Evaluates complete framing metrics for a target at range and aspect angle.
    /// @param[in] slantRangeMeters Distance to target in meters.
    /// @param[in] envelope Target 3D dimensions.
    /// @param[in] targetTrueHeadingDeg Target heading or COG in degrees [0, 360).
    /// @param[in] cameraAzimuthDeg Camera LOS geographic azimuth in degrees [0, 360).
    /// @return TargetFramingMetrics record.
    [[nodiscard]] TargetFramingMetrics evaluateFraming(
        double slantRangeMeters, const TargetPhysicalEnvelope& envelope,
        double targetTrueHeadingDeg, double cameraAzimuthDeg) const noexcept;

    /// @brief Initiates or updates range-adaptive framing on designated camera with scheduling.
    /// @param[in,out] camera Active camera payload.
    /// @param[in] slantRangeMeters Distance to target in meters.
    /// @param[in] envelope Target 3D metric envelope.
    /// @param[in] targetTrueHeadingDeg Target true heading or COG.
    /// @param[in] cameraAzimuthDeg Camera azimuth in degrees.
    /// @return True if a new trajectory was scheduled or active target maintained.
    bool scheduleFraming(
        ICameraPayload& camera, double slantRangeMeters,
        const TargetPhysicalEnvelope& envelope,
        double targetTrueHeadingDeg, double cameraAzimuthDeg);

    /// @brief Periodic update driving scheduled zoom trajectory, deadbands, and autofocus triggers.
    /// @param[in,out] camera Active camera payload.
    /// @param[in] dt Elapsed time since previous update cycle.
    void update(ICameraPayload& camera, std::chrono::milliseconds dt);

    /// @brief Queries whether current optical zoom has converged to scheduled target.
    /// @return True if zoom error is within convergence tolerance.
    [[nodiscard]] bool isZoomConverged() const noexcept;

    /// @brief Returns the active scheduled target normalized zoom.
    /// @return Target normalized zoom [0.0 .. 1.0].
    [[nodiscard]] double targetZoom() const noexcept;

    /// @brief Returns the current estimated / commanded zoom position.
    /// @return Current normalized zoom [0.0 .. 1.0].
    [[nodiscard]] double currentZoom() const noexcept;

    /// @brief Returns estimated time remaining until zoom converges.
    /// @return Milliseconds remaining.
    [[nodiscard]] std::chrono::milliseconds estimatedConvergenceTime() const noexcept;

    /// @brief Resets scheduling state and aborts active zoom transitions.
    void reset();

    /// @brief Direct single-step target framing command (backward-compatible).
    /// @param[in,out] camera Reference to ICameraPayload instance.
    /// @param[in] slantRangeMeters Distance to target in meters.
    /// @param[in] targetDimensionMeters Physical target dimension in meters.
    /// @return True if zoom command was successfully accepted by camera.
    bool frameTarget(ICameraPayload& camera, double slantRangeMeters, double targetDimensionMeters);

private:
    mutable std::mutex m_mutex {};
    AutoFramingConfig m_config {};

    double m_currentZoom01 { 0.0 };
    double m_targetZoom01 { 0.0 };
    double m_lastFramedRangeMeters { 0.0 };
    bool m_isConverged { true };
    bool m_autofocusTriggered { false };
    std::chrono::steady_clock::time_point m_transitionStartTime {};
};

} // namespace PayloadHal
