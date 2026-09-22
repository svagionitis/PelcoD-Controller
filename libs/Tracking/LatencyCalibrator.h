#pragma once

/// @file LatencyCalibrator.h
/// @brief Active doublet pulse excitation controller for rapid end-to-end PTZ latency calibration.

#include "LatencyEstimator.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace Tracking {

/// @struct CalibrationResult
/// @brief Outcome of an active doublet latency calibration run.
struct CalibrationResult {
    bool success { false }; ///< True if calibration completed with high statistical confidence.
    double latencyMs { 0.0 }; ///< Calibrated end-to-end physical latency in milliseconds.
    double latencySeconds { 0.0 }; ///< Calibrated latency in seconds (for direct Kalman lookahead tuning).
    double correlation { 0.0 }; ///< Peak cross-correlation coefficient achieved.
    std::string message {}; ///< Diagnostic status message.
};

/// @enum CalibrationState
/// @brief Operating state of the active calibration state machine.
enum class CalibrationState : std::uint8_t {
    Idle, ///< Inactive; ready for calibration trigger.
    PreSettle, ///< Settling baseline before excitation pulse.
    PositivePulse, ///< Forward excitation pulse active.
    InterDwell, ///< Zero-speed pause between doublet pulses.
    NegativePulse, ///< Reverse excitation pulse active.
    PostCollect, ///< Observation window collecting delayed video responses.
    Completed, ///< Calibration finished; results ready.
    Failed ///< Calibration failed (e.g. low SNR, no motion detected).
};

/// @class LatencyCalibrator
/// @brief Coordinates active doublet pulse excitation to measure physical command-to-video delay.
/// @details Generates subtle, non-disruptive doublet velocity commands, ingests visual motion feedback
///          from optical flow, and computes empirical latency via normalized cross-correlation.
class LatencyCalibrator {
public:
    /// @brief Motor command dispatch callback signature: (panDir, panSpeed, tiltDir, tiltSpeed).
    using CommandCallback = std::function<void(int panDir, int panSpeed, int tiltDir, int tiltSpeed)>;

    /// @brief Constructs a LatencyCalibrator with an optional command dispatch callback.
    /// @param[in] cmdCb Callback to dispatch PTZ motor commands.
    /// @param[in] config LatencyEstimator configuration for the calibration run.
    explicit LatencyCalibrator(CommandCallback cmdCb = nullptr, LatencyEstimatorConfig config = {});

    /// @brief Destructor ensuring motion is halted if running.
    ~LatencyCalibrator();

    /// @brief Configures the command callback.
    /// @param[in] cmdCb Dispatch callback.
    void setCommandCallback(CommandCallback cmdCb);

    /// @brief Initiates an active doublet calibration sequence.
    /// @param[in] panPulseSpeed Discrete Pelco-D pan speed for the pulse (1 to 63, default 25).
    /// @param[in] tiltPulseSpeed Discrete Pelco-D tilt speed for the pulse (0 to 63, default 0).
    /// @param[in] nowSec Monotonic start time in seconds (or <= 0 to use current steady clock).
    /// @return True if calibration commenced, false if already in progress or invalid.
    bool start(int panPulseSpeed = 25, int tiltPulseSpeed = 0, double nowSec = 0.0);

    /// @brief Cancels an ongoing calibration and immediately commands motors to stop.
    void cancel();

    /// @brief Advances the calibration state machine.
    /// @param[in] nowSec Current monotonic clock time in seconds.
    void update(double nowSec);

    /// @brief Ingests observed visual motion velocity from video optical flow.
    /// @param[in] nowSec Monotonic frame arrival time in seconds.
    /// @param[in] visualVelocityX Horizontal visual motion velocity (e.g. pixels/sec or dx/dt).
    /// @param[in] visualVelocityY Vertical visual motion velocity (e.g. pixels/sec or dy/dt).
    void ingestVisualMotion(double nowSec, double visualVelocityX, double visualVelocityY = 0.0);

    /// @brief Query if calibration is actively in progress.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Query current state of the calibration state machine.
    [[nodiscard]] CalibrationState getState() const noexcept;

    /// @brief Retrieve the latest calibration outcome.
    [[nodiscard]] CalibrationResult getResult() const noexcept;

    /// @brief Access the underlying LatencyEstimator.
    [[nodiscard]] LatencyEstimator& getEstimator() noexcept;

    /// @brief Access the underlying LatencyEstimator (const).
    [[nodiscard]] const LatencyEstimator& getEstimator() const noexcept;

private:
    void dispatchStopLocked();
    void dispatchCommandLocked(int panDir, int panSpeed, int tiltDir, int tiltSpeed);
    static double getMonotonicNowSeconds() noexcept;

    mutable std::mutex m_mutex;
    CommandCallback m_cmdCb { nullptr };
    LatencyEstimator m_estimator;

    CalibrationState m_state { CalibrationState::Idle };
    CalibrationResult m_result {};

    int m_pulsePanSpeed { 25 };
    int m_pulseTiltSpeed { 0 };

    double m_phaseStartTime { 0.0 };
    double m_lastCommandValue { 0.0 };

    // Timing constants for doublet profile (in seconds)
    static constexpr double PRE_SETTLE_DURATION { 0.20 };
    static constexpr double PULSE_DURATION { 0.15 };
    static constexpr double INTER_DWELL_DURATION { 0.10 };
    static constexpr double POST_COLLECT_DURATION { 0.45 };
};

} // namespace Tracking

namespace PelcoD {
namespace Tracking = ::Tracking;
using CalibrationResult = ::Tracking::CalibrationResult;
using CalibrationState = ::Tracking::CalibrationState;
using LatencyCalibrator = ::Tracking::LatencyCalibrator;
} // namespace PelcoD
