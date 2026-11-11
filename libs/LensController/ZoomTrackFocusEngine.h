#pragma once

/// @file ZoomTrackFocusEngine.h
/// @brief Parfocal tracking engine with PCHIP spline evaluation and LUT parser.

#include <cstddef>
#include <cstdint>

#include <string>
#include <string_view>

#include "LensTypes.h"
#include "ZoomTrackTypes.h"

namespace LensControl {

/// @class ZoomTrackFocusEngine
/// @brief Evaluates, parses, serializes, and calibrates parfocal zoom-to-focus curves.
class ZoomTrackFocusEngine final {
public:
    static constexpr std::size_t kMaxKnots = TrackCurve::kMaxKnots;
    static constexpr std::size_t kMaxCurves = TrackCurveSet::kMaxCurves;

    /// @brief Constructs a zoom-track focus engine in default state.
    ZoomTrackFocusEngine() noexcept;

    /// @brief Default destructor.
    ~ZoomTrackFocusEngine() noexcept = default;

    // File I/O and Parsing
    /// @brief Parses a curve set from string content.
    /// @param text Text containing one or more curve definitions.
    /// @return Status code indicating parsing success or specific error.
    [[nodiscard]] TrackCurveError parseString(std::string_view text) noexcept;

    /// @brief Reads and parses a curve set from a file path.
    /// @param filePath Path to calibration file.
    /// @return Status code indicating parsing success or specific error.
    [[nodiscard]] TrackCurveError parseFile(std::string_view filePath) noexcept;

    /// @brief Serializes the active curve set to a file.
    /// @param filePath Target output path.
    /// @return True on successful write.
    [[nodiscard]] bool serializeFile(std::string_view filePath) const noexcept;

    /// @brief Serializes the active curve set into a string.
    /// @return Formatted multi-curve calibration text.
    [[nodiscard]] std::string serializeString() const;

    // Configuration & Curve Selection
    /// @brief Selects the mathematical interpolation mode (PCHIP vs Linear).
    /// @param mode Interpolation algorithm.
    void setMode(InterpolationMode mode) noexcept;

    /// @brief Returns the active mathematical interpolation mode.
    /// @return Current interpolation algorithm.
    [[nodiscard]] InterpolationMode mode() const noexcept;

    /// @brief Selects the active curve index used for single-curve evaluation.
    /// @param index Curve index within [0..curveCount - 1].
    void setActiveCurve(std::size_t index) noexcept;

    /// @brief Returns the current active curve index.
    /// @return Active curve index.
    [[nodiscard]] std::size_t activeCurve() const noexcept;

    /// @brief Returns the total number of loaded curves in the engine.
    /// @return Curve count [0..kMaxCurves].
    [[nodiscard]] std::size_t curveCount() const noexcept;

    /// @brief Clears all loaded curves and resets active index to 0.
    void clearCurves() noexcept;

    // Direct Curve Management
    /// @brief Adds a pre-populated curve to the set and computes its PCHIP slopes.
    /// @param curve Valid curve with at least 2 knots.
    /// @return True if curve was valid and added successfully.
    [[nodiscard]] bool addCurve(const TrackCurve& curve) noexcept;

    /// @brief Retrieves a pointer to a loaded curve.
    /// @param idx Curve index.
    /// @return Pointer to curve or nullptr if out of range.
    [[nodiscard]] const TrackCurve* getCurve(std::size_t idx) const noexcept;

    // Zero-Heap Real-Time Evaluation (O(log N))
    /// @brief Evaluates required focus motor position for a given zoom position.
    /// @param zoom Current zoom motor position.
    /// @return Interpolated focus motor position.
    [[nodiscard]] MotorPosition evaluateFocus(MotorPosition zoom) const noexcept;

    /// @brief Evaluates focus motor position blending across multi-distance curves.
    /// @param zoom Current zoom motor position.
    /// @param distanceMeters Target distance in meters (0.0F or negative = infinity).
    /// @return Interpolated focus motor position.
    [[nodiscard]] MotorPosition evaluateDistance(MotorPosition zoom, float distanceMeters) const noexcept;

    // PCHIP Tangent Computation
    /// @brief Computes shape-preserving PCHIP derivatives for all knots in the curve.
    /// @param curve Curve whose knot slopes will be populated in-place.
    static void computePchip(TrackCurve& curve) noexcept;

    // Calibration Helpers
    /// @brief Initiates an automated calibration sweep.
    /// @param knotCount Number of sample points across zoom travel [2..kMaxKnots].
    /// @param zMin Minimum zoom coordinate for calibration.
    /// @param zMax Maximum zoom coordinate for calibration.
    void startCalib(std::size_t knotCount, MotorPosition zMin, MotorPosition zMax) noexcept;

    /// @brief Tests whether calibration sweep is currently in progress.
    /// @return True if calibrating.
    [[nodiscard]] bool isCalibrating() const noexcept;

    /// @brief Queries the target zoom position for the current calibration step.
    /// @return Zoom coordinate to position the actuator before AF.
    [[nodiscard]] MotorPosition nextCalibZoom() const noexcept;

    /// @brief Records the converged focus position for the current calibration step.
    /// @param focusPos Converged focus coordinate.
    void recordCalibKnot(MotorPosition focusPos) noexcept;

    /// @brief Aborts an in-progress calibration sweep.
    void cancelCalib() noexcept;

private:
    [[nodiscard]] MotorPosition evalPchip(const TrackCurve& curve, MotorPosition zoom) const noexcept;
    [[nodiscard]] MotorPosition evalLinear(const TrackCurve& curve, MotorPosition zoom) const noexcept;

    TrackCurveSet m_curveSet { };
    InterpolationMode m_mode { InterpolationMode::PchipSpline };

    // Calibration state
    bool m_isCalibrating { false };
    std::size_t m_calibTotalKnots { 0U };
    std::size_t m_calibCurrentKnot { 0U };
    MotorPosition m_calibZoomMin { 0U };
    MotorPosition m_calibZoomMax { 65535U };
    TrackCurve m_calibStagingCurve { };
};

} // namespace LensControl
