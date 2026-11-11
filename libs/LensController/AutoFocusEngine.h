#pragma once

/// @file AutoFocusEngine.h
/// @brief Deterministic, zero-heap closed-loop autofocus engine supporting multi-algorithm AF.

#include <cstddef>
#include <cstdint>

#include <array>

#include "AutoFocusTypes.h"
#include "FocusEvaluator.h"
#include "ILensDriver.h"
#include "LensTypes.h"
#include "PeakFitter.h"

namespace LensControl {

/// @class AutoFocusEngine
/// @brief Closed-loop autofocus coordinator driving search algorithms, dynamic ROI, and peak convergence.
class AutoFocusEngine final {
public:
    static constexpr std::size_t kMaxSamples = 256U;
    static constexpr std::size_t kDelayRingSize = 16U;

    /// @brief Constructs an autofocus engine referencing lens driver and focus evaluator.
    /// @param driver Hardware or mock lens actuator driver.
    /// @param evaluator Zero-heap focus sharpness evaluator.
    explicit AutoFocusEngine(ILensDriver& driver, FocusEvaluator& evaluator) noexcept;

    ~AutoFocusEngine() noexcept = default;
    AutoFocusEngine(const AutoFocusEngine&) = delete;
    AutoFocusEngine& operator=(const AutoFocusEngine&) = delete;
    AutoFocusEngine(AutoFocusEngine&&) = delete;
    AutoFocusEngine& operator=(AutoFocusEngine&&) = delete;

    /// @brief Triggers a one-shot autofocus search cycle.
    /// @param method Algorithm variant to execute.
    /// @param config Calibrated parameter overrides.
    void trigger(
        AutoFocusMethod method = AutoFocusMethod::ScanThreePhase, const AutoFocusConfig& config = { }) noexcept;

    /// @brief Aborts active autofocus operation and halts motor movement.
    void abort() noexcept;

    /// @brief Ingests an incoming video frame for sharpness evaluation.
    /// @param luma Pointer to 8-bit grayscale pixel data.
    /// @param width Frame width in pixels.
    /// @param height Frame height in pixels.
    /// @param stride Row stride in bytes (if 0, assumes width).
    /// @param timestampNs Acquisition timestamp in nanoseconds.
    /// @param frameId Monotonically increasing frame sequence identifier.
    void processFrame(const std::uint8_t* luma, std::size_t width, std::size_t height, std::size_t stride = 0U,
        std::uint64_t timestampNs = 0U, std::uint32_t frameId = 0U) noexcept;

    /// @brief Updates state machine and issues actuator commands on control loop tick.
    /// @param currentFocusPos Feedback position from motor encoder.
    /// @param nowNs Current monotonic clock timestamp in nanoseconds.
    void step(MotorPosition currentFocusPos, std::uint64_t nowNs = 0U) noexcept;

    /// @brief Dynamically binds an active tracker bounding box to auto-center optical ROI.
    /// @param normX Target center X in normalized coordinates [0.0, 1.0].
    /// @param normY Target center Y in normalized coordinates [0.0, 1.0].
    /// @param normW Target width in normalized coordinates [0.0, 1.0].
    /// @param normH Target height in normalized coordinates [0.0, 1.0].
    /// @param marginFrac Fractional padding margin around target bounding box.
    void setTrackerRoi(float normX, float normY, float normW, float normH, float marginFrac = 0.2F) noexcept;

    /// @brief Clears active tracker ROI and reverts to default center ROI.
    void clearTrackerRoi() noexcept;

    /// @brief Enables or disables continuous autofocus (CAF) defocus monitoring.
    /// @param enable True to monitor scene sharpness and trigger refocus upon defocus.
    void setCafEnabled(bool enable) noexcept;

    /// @brief Queries whether continuous autofocus (CAF) monitoring is active.
    /// @return True if CAF is enabled.
    [[nodiscard]] bool isCafEnabled() const noexcept;

    /// @brief Checks whether autofocus operation is currently in progress.
    /// @return True if search state machine is active.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Returns the current state of the autofocus state machine.
    /// @return AutoFocusState enum value.
    [[nodiscard]] AutoFocusState state() const noexcept;

    /// @brief Returns the last converged focus position.
    /// @return MotorPosition of converged focus summit.
    [[nodiscard]] MotorPosition convergedPosition() const noexcept;

    /// @brief Returns the last computed peak fit result.
    /// @return PeakFitResult struct.
    [[nodiscard]] PeakFitResult lastPeakFit() const noexcept;

    /// @brief Returns the number of recorded sample points in the active/last run.
    /// @return Count of valid samples in internal buffer.
    [[nodiscard]] std::size_t sampleCount() const noexcept;

    /// @brief Retrieves a recorded sample point by index.
    /// @param index Sample index [0..sampleCount-1].
    /// @return SamplePoint structure.
    [[nodiscard]] SamplePoint getSample(std::size_t index) const noexcept;

    /// @brief Returns active configuration.
    /// @return Reference to AutoFocusConfig.
    [[nodiscard]] const AutoFocusConfig& config() const noexcept;

private:
    void stepScan(MotorPosition pos) noexcept;
    void stepSeek(MotorPosition pos) noexcept;
    void stepGss(MotorPosition pos) noexcept;
    void stepMomentum(MotorPosition pos) noexcept;
    void recordSample(MotorPosition pos, FocusMetricScore score, std::uint64_t timeNs) noexcept;
    void slewToTarget(MotorPosition target, bool applyBacklash) noexcept;
    void finishConvergence(MotorPosition bestPos) noexcept;

    ILensDriver& m_driver;
    FocusEvaluator& m_evaluator;
    AutoFocusConfig m_config { };
    AutoFocusMethod m_method { AutoFocusMethod::ScanThreePhase };
    AutoFocusState m_state { AutoFocusState::Idle };

    std::array<SamplePoint, kMaxSamples> m_samples { };
    std::size_t m_sampleCount { 0U };

    std::array<MotorPosition, kDelayRingSize> m_delayRing { };
    std::size_t m_delayHead { 0U };

    MotorPosition m_targetPos { 0U };
    MotorPosition m_convergedPos { 0U };
    MotorPosition m_lastFeedbackPos { 0U };
    PeakFitResult m_lastPeak { };

    FocusMetricScore m_latestScore { };
    bool m_newScoreAvailable { false };

    std::uint32_t m_maxScoreSeen { 0U };
    std::uint32_t m_prevScore { 0U };
    std::uint8_t m_declines { 0U };
    bool m_sweepDirectionForward { true };
    bool m_seekReversed { false };
    bool m_backlashApproached { false };

    // Golden Section Search (GSS) State
    std::uint16_t m_gssA { 0U };
    std::uint16_t m_gssB { 65535U };
    std::uint16_t m_gssX1 { 0U };
    std::uint16_t m_gssX2 { 0U };
    std::uint32_t m_gssScore1 { 0U };
    std::uint32_t m_gssScore2 { 0U };
    bool m_gssEvalX1 { true };
    bool m_gssInitialized { false };

    // Adaptive Momentum State
    float m_momentumVelocity { 0.0F };
    std::int32_t m_momentumLastPos { 0 };

    // Continuous Autofocus (CAF)
    std::uint32_t m_cafPeakScore { 0U };
    bool m_cafEnabled { false };

    // Timing & Timeout
    std::uint64_t m_startTimeNs { 0U };
};

} // namespace LensControl
