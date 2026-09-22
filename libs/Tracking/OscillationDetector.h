#pragma once

/// @file OscillationDetector.h
/// @brief Frequency-domain observer for detecting PID limit-cycle hunting and resonance oscillations.

#include "Fft.h"
#include "PidController.h"

#include <cstddef>
#include <vector>

namespace Tracking {

/// @struct OscillationConfig
/// @brief Configuration thresholds and window settings for oscillation detection.
struct OscillationConfig {
    std::size_t windowSize { 128U }; ///< Number of samples in sliding buffer (power of 2: 64, 128, 256).
    double sampleRateHz { 50.0 }; ///< Sample collection frequency in Hertz.
    double minHuntingFreqHz { 0.3 }; ///< Lower frequency boundary for hunting detection in Hertz.
    double maxHuntingFreqHz { 5.0 }; ///< Upper frequency boundary for hunting detection in Hertz.
    double powerRatioThreshold {
        0.35
    }; ///< Ratio of peak power to total spectral power to trigger detection [0.0, 1.0].
    int consecutiveThreshold { 2 }; ///< Minimum consecutive window detections to confirm sustained hunting.
    Math::WindowType windowType { Math::WindowType::Hann }; ///< Spectral window to suppress leakage.
};

/// @class OscillationDetector
/// @brief Sliding-window frequency-domain observer that identifies persistent control oscillations.
/// @details Ingests tracking errors or actuator commands, computes windowed Power Spectral Density (PSD),
///          and detects whether energy is concentrated in a narrowband limit cycle (hunting).
class OscillationDetector {
public:
    /// @brief Constructs an oscillation detector with given configuration.
    /// @param[in] config Detection parameters and buffer sizing.
    explicit OscillationDetector(OscillationConfig config = {});

    /// @brief Ingests a new signal sample into the circular buffer and triggers analysis if buffer is full.
    /// @param[in] sample Real-valued measurement (e.g. tracking error or control output).
    void addSample(double sample);

    /// @brief Forces an immediate spectral evaluation over the current buffer content.
    void update();

    /// @brief Checks if sustained hunting/oscillation is currently detected.
    /// @return True if persistent hunting oscillation is active.
    [[nodiscard]] bool isHunting() const noexcept;

    /// @brief Returns the most recent dominant oscillation frequency in Hertz.
    /// @return Center frequency of the dominant peak.
    [[nodiscard]] double getDominantFrequency() const noexcept;

    /// @brief Returns the spectral power ratio of the dominant peak [0.0, 1.0].
    /// @return Fractional power of the peak relative to total AC signal energy.
    [[nodiscard]] double getOscillationRatio() const noexcept;

    /// @brief Returns the latest list of identified spectral peaks.
    [[nodiscard]] std::vector<Math::SpectralPeak> getLatestPeaks() const;

    /// @brief Automatically reduces PID proportional and derivative gains if hunting is detected.
    /// @param[in,out] pid Target PID controller to adjust.
    /// @param[in] reductionFactor Multiplier applied to Kp and Kd (e.g. 0.85 = 15% reduction).
    /// @return True if gains were attenuated, false if no hunting was active.
    bool autoAttenuate(PidController& pid, double reductionFactor = 0.85);

    /// @brief Clears the buffer and resets all detection states.
    void reset() noexcept;

    /// @brief Reconfigures the detector.
    /// @param[in] config New configuration settings.
    void setConfig(const OscillationConfig& config);

    /// @brief Returns the current detector configuration.
    [[nodiscard]] const OscillationConfig& getConfig() const noexcept
    {
        return m_config;
    }

private:
    void analyzeSpectrum();

    OscillationConfig m_config {};
    std::vector<double> m_buffer {};
    std::size_t m_writeIndex { 0U };
    std::size_t m_sampleCount { 0U };

    bool m_isHunting { false };
    double m_dominantFreqHz { 0.0 };
    double m_dominantPowerRatio { 0.0 };
    int m_consecutiveDetections { 0 };
    std::vector<Math::SpectralPeak> m_latestPeaks {};
};

} // namespace Tracking

namespace PelcoD {
namespace Tracking = ::Tracking;
using OscillationConfig = ::Tracking::OscillationConfig;
using OscillationDetector = ::Tracking::OscillationDetector;
} // namespace PelcoD
