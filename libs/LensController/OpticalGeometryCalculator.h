#pragma once

/// @file OpticalGeometryCalculator.h
/// @brief Real-time analytical Depth-of-Field and hyperfocal distance calculator.

#include <cstdint>

#include "AfHeuristicsTypes.h"
#include "LensTypes.h"

namespace LensControl {

/// @struct OpticsConfig
/// @brief Physical sensor and optical format parameters.
struct OpticsConfig {
    FNumber aperture { 1.6F };
    CircleOfConfusionMm coc { 0.0069F };
    FocalLengthMm minFocalLength { 4.3F }; ///< Focal length at full wide (mm)
    FocalLengthMm maxFocalLength { 129.0F }; ///< Focal length at full tele (mm)
    MotorPosition wideLimitZoom { 5000U }; ///< Zoom position threshold for instant wide lock
    DistanceMeters nominalSubjectDist { 25.0F }; ///< Nominal distance for DOF step scaling
};

/// @class OpticalGeometryCalculator
/// @brief Evaluates physical optical limits, hyperfocal fast-paths, and adaptive search step scaling.
class OpticalGeometryCalculator final {
public:
    using OpticsConfig = LensControl::OpticsConfig;

    explicit OpticalGeometryCalculator(const OpticsConfig& config = { }) noexcept;
    ~OpticalGeometryCalculator() noexcept = default;

    OpticalGeometryCalculator(const OpticalGeometryCalculator&) noexcept = default;
    OpticalGeometryCalculator& operator=(const OpticalGeometryCalculator&) noexcept = default;
    OpticalGeometryCalculator(OpticalGeometryCalculator&&) noexcept = default;
    OpticalGeometryCalculator& operator=(OpticalGeometryCalculator&&) noexcept = default;

    /// @brief Calculates the hyperfocal distance for a given focal length.
    /// @param focalLength Optical focal length.
    /// @return Hyperfocal distance in meters.
    [[nodiscard]] DistanceMeters calculateHyperfocal(FocalLengthMm focalLength) const noexcept;

    /// @brief Calculates near, far, and total Depth-of-Field for a given distance and focal length.
    /// @param subjectDist Distance from lens entrance pupil to target subject in meters.
    /// @param focalLength Optical focal length in millimeters.
    /// @return DofLimits struct.
    [[nodiscard]] DofLimits calculateDof(DistanceMeters subjectDist, FocalLengthMm focalLength) const noexcept;

    /// @brief Checks whether the zoom position qualifies for instant wide-angle hyperfocal lock.
    /// @param zoomPos Current zoom motor encoder position.
    [[nodiscard]] bool isWideAngle(MotorPosition zoomPos) const noexcept;

    /// @brief Computes adaptive coarse, mid, and fine sweep step sizes for current zoom.
    /// @param focalLength Current optical focal length.
    /// @param baseCoarse Default coarse steps at full telephoto.
    /// @param baseMid Default mid steps at full telephoto.
    /// @param baseFine Default fine steps at full telephoto.
    /// @return AdaptiveStepConfig with scaled step sizes.
    [[nodiscard]] AdaptiveStepConfig calculateSteps(FocalLengthMm focalLength, std::uint16_t baseCoarse,
        std::uint16_t baseMid, std::uint16_t baseFine) const noexcept;

    /// @brief Reconfigures optical properties.
    void setConfig(const OpticsConfig& config) noexcept;

    /// @brief Returns the active optical configuration.
    [[nodiscard]] OpticsConfig config() const noexcept;

private:
    OpticsConfig m_config { };
};

} // namespace LensControl
