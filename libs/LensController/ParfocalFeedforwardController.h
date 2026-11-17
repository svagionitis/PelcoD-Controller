#pragma once

/// @file ParfocalFeedforwardController.h
/// @brief Dynamic derivative feedforward controller eliminating zoom focus lag.

#include <cstdint>

#include "LensTypes.h"
#include "MotionControlTypes.h"

namespace LensControl {

class ZoomTrackFocusEngine;

/// @class ParfocalFeedforwardController
/// @brief Computes dynamic focus velocity from zoom rate of change and PCHIP spline tangents.
class ParfocalFeedforwardController final {
public:
    /// @brief Constructs feedforward controller with default or custom configuration.
    /// @param config Feedforward parameters and gains.
    explicit ParfocalFeedforwardController(const FeedforwardConfig& config = FeedforwardConfig { }) noexcept;

    /// @brief Default destructor.
    ~ParfocalFeedforwardController() noexcept = default;

    /// @brief Resets internal differentiator filter and states.
    void reset() noexcept;

    /// @brief Configures controller gains and filter constants.
    /// @param config Parameter bundle.
    void setConfig(const FeedforwardConfig& config) noexcept;

    /// @brief Returns active configuration bundle.
    /// @return Reference to configuration.
    [[nodiscard]] const FeedforwardConfig& config() const noexcept;

    /// @brief Computes focus velocity command for the current control cycle.
    /// @param currentZoom Actual measured zoom motor position.
    /// @param currentFocus Actual measured focus motor position.
    /// @param trackEngine Reference to parfocal curve engine for spline evaluation.
    /// @param dt Time elapsed since previous update.
    /// @return Clamped focus velocity effort [-Vmax, +Vmax].
    [[nodiscard]] float update(MotorPosition currentZoom, MotorPosition currentFocus,
        const ZoomTrackFocusEngine& trackEngine, DeltaTimeSec dt) noexcept;

    /// @brief Returns the estimated filtered zoom velocity.
    /// @return Zoom velocity in steps/second.
    [[nodiscard]] VelocityTicksPerSec zoomVelocity() const noexcept;

    /// @brief Returns the feedforward portion of the last velocity calculation.
    /// @return Feedforward term magnitude.
    [[nodiscard]] float lastFeedforwardTerm() const noexcept;

    /// @brief Returns the feedback portion of the last velocity calculation.
    /// @return Feedback term magnitude.
    [[nodiscard]] float lastFeedbackTerm() const noexcept;

private:
    FeedforwardConfig m_config { };
    float m_prevZoom { 0.0F };
    float m_filteredZoomVel { 0.0F };
    float m_lastFfTerm { 0.0F };
    float m_lastFbTerm { 0.0F };
    bool m_isInitialized { false };
};

} // namespace LensControl
