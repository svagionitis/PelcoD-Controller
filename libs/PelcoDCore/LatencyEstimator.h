#pragma once

/// @file LatencyEstimator.h
/// @brief Empirical command-to-video latency estimator using normalized cross-correlation and parabolic interpolation.

#include <cstddef>
#include <vector>

namespace PelcoD {

/// @struct LatencyEstimatorConfig
/// @brief Configuration settings for cross-correlation latency estimation.
struct LatencyEstimatorConfig {
    std::size_t bufferCapacity { 256U }; ///< Total sample capacity for observation buffers.
    double sampleRateHz { 50.0 }; ///< Signal sampling frequency in Hertz.
    double minLagMs { 0.0 }; ///< Minimum search lag in milliseconds (>= 0).
    double maxLagMs { 400.0 }; ///< Maximum search lag in milliseconds.
    double confidenceThreshold {
        0.5
    }; ///< Minimum peak correlation coefficient [-1.0, 1.0] to consider estimate valid.
    double smoothingAlpha { 0.2 }; ///< Exponential moving average smoothing weight for latency updates [0.0, 1.0].
};

/// @class LatencyEstimator
/// @brief Measures the physical time delay between commanded actions and observed responses.
/// @details Ingests reference input signals (e.g. commanded PTZ motor speed) and observed feedback
///          (e.g. optical flow visual velocity), computing normalized cross-correlation across a search lag
///          window with 3-point parabolic interpolation for sub-sample fractional millisecond resolution.
class LatencyEstimator {
public:
    /// @brief Constructs a latency estimator with given configuration.
    /// @param[in] config Estimation parameters and search ranges.
    explicit LatencyEstimator(LatencyEstimatorConfig config = {});

    /// @brief Ingests synchronized reference and observed response samples.
    /// @param[in] reference Commanded excitation signal (e.g. motor speed).
    /// @param[in] response Observed visual or telemetry feedback signal (e.g. optical flow velocity).
    void addSample(double reference, double response);

    /// @brief Evaluates cross-correlation across the current buffer contents and updates estimates.
    void update();

    /// @brief Returns the estimated latency in milliseconds.
    [[nodiscard]] double getEstimatedLatencyMs() const noexcept;

    /// @brief Returns the peak Pearson cross-correlation coefficient [-1.0, 1.0].
    [[nodiscard]] double getPeakCorrelation() const noexcept;

    /// @brief Returns true if the peak correlation meets or exceeds the confidence threshold.
    [[nodiscard]] bool isConfident() const noexcept;

    /// @brief Returns the discrete cross-correlation curve across all evaluated search lags.
    [[nodiscard]] std::vector<double> getCorrelationCurve() const;

    /// @brief Clears sample buffers and resets estimation state.
    void reset() noexcept;

    /// @brief Reconfigures the estimator.
    /// @param[in] config New configuration settings.
    void setConfig(const LatencyEstimatorConfig& config);

    /// @brief Returns the active estimator configuration.
    [[nodiscard]] const LatencyEstimatorConfig& getConfig() const noexcept;

private:
    LatencyEstimatorConfig m_config {};

    std::vector<double> m_refBuffer {};
    std::vector<double> m_respBuffer {};
    std::size_t m_count { 0U };

    double m_estimatedLatencyMs { 0.0 };
    double m_peakCorrelation { 0.0 };
    bool m_isConfident { false };
    std::vector<double> m_latestCorrelationCurve {};
};

} // namespace PelcoD
