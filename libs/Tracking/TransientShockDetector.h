#pragma once

/// @file TransientShockDetector.h
/// @brief Header declaring real-time Transient Shock & Vibration Impulse Detector using wavelets.

#include "Dwt.h"

#include <cstddef>
#include <vector>

namespace Tracking {

/// @struct ShockDetectorConfig
/// @brief Configuration settings for TransientShockDetector.
struct ShockDetectorConfig {
    std::size_t windowSize { 64U }; ///< Sliding analysis window size in samples (minimum 16).
    std::size_t decompositionLevels { 3U }; ///< Number of wavelet decomposition levels.
    Math::WaveletType wavelet { Math::WaveletType::Db4 }; ///< Wavelet basis function.
    double energyThresholdFactor { 4.0 }; ///< Energy ratio over baseline to flag transient shock.
    double minShockEnergy { 5.0 }; ///< Absolute detail energy floor to avoid false positives in quiet environments.
};

/// @struct ShockEvent
/// @brief Telemetry result of transient shock detection.
struct ShockEvent {
    bool isShockDetected { false }; ///< True if transient impulse detected in latest sample window.
    double shockMagnitude { 0.0 }; ///< Peak detail shock amplitude.
    double detailEnergy { 0.0 }; ///< High-frequency wavelet detail energy.
    double energyRatio { 1.0 }; ///< Detail energy relative to baseline background noise.
};

/// @class TransientShockDetector
/// @brief Detects sudden mechanical shocks, mast impacts, wind gusts, and transient impulses.
/// @details Analyzes incoming sensor telemetry (motor velocity, optical flow error, IMU acceleration)
///          via multi-resolution wavelet details, isolating high-frequency spikes from smooth motion.
class TransientShockDetector {
public:
    explicit TransientShockDetector(ShockDetectorConfig config = {});

    /// @brief Ingests a new sensor or tracking telemetry sample.
    /// @param[in] sample Timestamped scalar telemetry sample.
    /// @return ShockEvent indicating whether a shock occurred.
    ShockEvent addSample(double sample);

    /// @brief Resets detector history and baseline energy state.
    void reset();

    /// @brief Updates detector configuration.
    void setConfig(const ShockDetectorConfig& config);

    /// @brief Gets active configuration.
    [[nodiscard]] const ShockDetectorConfig& getConfig() const noexcept;

    /// @brief Gets current estimated background baseline energy.
    [[nodiscard]] double getBaselineEnergy() const noexcept;

private:
    ShockDetectorConfig m_config;
    std::vector<double> m_buffer;
    double m_baselineEnergy { 1.0 };
    std::size_t m_sampleCount { 0U };
};

} // namespace Tracking
