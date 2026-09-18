#pragma once

/// @file ChirpCalibrator.h
/// @brief Active swept-sine excitation controller for empirical plant identification and Bode estimation.

#include "PlantIdentifier.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace PelcoD {

/// @enum CalibrationAxis
/// @brief Selected motion axis for the swept-sine plant identification routine.
enum class CalibrationAxis : std::uint8_t {
    Pan, ///< Azimuth (Pan Left / Pan Right).
    Tilt ///< Elevation (Tilt Down / Tilt Up).
};

/// @enum ChirpCalibratorState
/// @brief Operational state of the active chirp calibration state machine.
enum class ChirpCalibratorState : std::uint8_t {
    Idle, ///< Inactive; ready for execution.
    PreSettle, ///< Baseline settling prior to excitation.
    Sweeping, ///< Active swept-sine chirp command generation.
    PostSettle, ///< Observation window collecting delayed plant deceleration.
    Analyzing, ///< Computing cross-spectral transfer function and margins.
    Completed, ///< Plant identification finished with results ready.
    Failed ///< Calibration aborted or insufficient signal SNR.
};

/// @class ChirpCalibrator
/// @brief Coordinates physical PTZ motor excitation and visual feedback for frequency response modeling.
/// @details Generates smooth frequency-swept command waveforms, drives the pan/tilt gimbal,
///          ingests optical flow velocity feedback, and computes empirical Bode plots and PID gains.
class ChirpCalibrator {
public:
    /// @brief Motor command dispatch callback: (panDir, panSpeed, tiltDir, tiltSpeed).
    using CommandCallback = std::function<void(int panDir, int panSpeed, int tiltDir, int tiltSpeed)>;

    /// @brief Construct a ChirpCalibrator with an optional command dispatch callback and config.
    /// @param[in] cmdCb Dispatch callback to drive PTZ hardware.
    /// @param[in] config Plant identifier and sweep configuration.
    explicit ChirpCalibrator(CommandCallback cmdCb = nullptr, ChirpConfig config = {});

    /// @brief Destructor ensuring motion is commanded to halt immediately.
    ~ChirpCalibrator();

    /// @brief Set or update the motor command dispatch callback.
    /// @param[in] cmdCb Dispatch callback.
    void setCommandCallback(CommandCallback cmdCb);

    /// @brief Initiates an active plant identification sweep.
    /// @param[in] axis Motion axis to excite (Pan or Tilt).
    /// @param[in] maxSpeed Peak discrete Pelco-D speed limit (1 to 63, default 25).
    /// @param[in] nowSec Monotonic start time in seconds (or <= 0 to use steady clock).
    /// @return True if sweep commenced, false if already active or invalid parameters.
    bool start(CalibrationAxis axis = CalibrationAxis::Pan, int maxSpeed = 25, double nowSec = 0.0);

    /// @brief Cancels an ongoing calibration and immediately dispatches motor stop commands.
    void cancel();

    /// @brief Advances the calibration state machine.
    /// @param[in] nowSec Current monotonic time in seconds.
    void update(double nowSec);

    /// @brief Ingests observed motion velocity from video optical flow or position telemetry.
    /// @param[in] nowSec Monotonic frame arrival timestamp in seconds.
    /// @param[in] visualVelocityX Horizontal visual motion velocity (e.g. pixels/sec).
    /// @param[in] visualVelocityY Vertical visual motion velocity (e.g. pixels/sec).
    void ingestVisualMotion(double nowSec, double visualVelocityX, double visualVelocityY = 0.0);

    /// @brief Query whether a calibration sweep is currently in progress.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Query current state machine status.
    [[nodiscard]] ChirpCalibratorState getState() const noexcept;

    /// @brief Query calibration progress as a normalized ratio [0.0, 1.0].
    [[nodiscard]] double getProgress() const noexcept;

    /// @brief Query active calibration axis.
    [[nodiscard]] CalibrationAxis getAxis() const noexcept;

    /// @brief Retrieve complete identification outcome.
    [[nodiscard]] PlantIdentificationResult getResult() const;

    /// @brief Access the underlying PlantIdentifier engine.
    [[nodiscard]] PlantIdentifier& getIdentifier() noexcept;

    /// @brief Access the underlying PlantIdentifier engine (const).
    [[nodiscard]] const PlantIdentifier& getIdentifier() const noexcept;

private:
    void dispatchStopLocked();
    void dispatchCommandLocked(int panDir, int panSpeed, int tiltDir, int tiltSpeed);
    static double getMonotonicNowSeconds() noexcept;

    mutable std::mutex m_mutex;
    CommandCallback m_cmdCb { nullptr };
    PlantIdentifier m_identifier;

    ChirpCalibratorState m_state { ChirpCalibratorState::Idle };
    CalibrationAxis m_axis { CalibrationAxis::Pan };
    int m_maxSpeed { 25 };

    double m_phaseStartTime { 0.0 };
    double m_sweepStartTime { 0.0 };
    double m_progress { 0.0 };
    double m_lastCommandVal { 0.0 };

    PlantIdentificationResult m_result {};

    // Timing durations in seconds
    static constexpr double PRE_SETTLE_DURATION { 0.40 };
    static constexpr double POST_SETTLE_DURATION { 0.60 };
};

} // namespace PelcoD
