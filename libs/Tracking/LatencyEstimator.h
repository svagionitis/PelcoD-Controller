#pragma once

/// @file LatencyEstimator.h
/// @brief Empirical command-to-video latency estimator using normalized cross-correlation and parabolic interpolation.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

namespace Tracking {

/// @enum PeakPolarity
/// @brief Polarity mode for cross-correlation peak detection.
enum class PeakPolarity : std::uint8_t {
    Absolute, ///< Evaluates peak magnitude |r| (ideal for general physical plants).
    Negative, ///< Detects negative correlation peaks (commanded motion vs camera-induced visual scene flow).
    Positive ///< Detects positive correlation peaks (commanded motion vs direct tracking response).
};

/// @struct LatencyEstimatorConfig
/// @brief Configuration settings for cross-correlation latency estimation.
struct LatencyEstimatorConfig {
    std::size_t bufferCapacity { 256U }; ///< Total sample capacity for observation buffers.
    double sampleRateHz { 50.0 }; ///< Signal sampling frequency in Hertz.
    double minLagMs { 0.0 }; ///< Minimum search lag in milliseconds (>= 0).
    double maxLagMs { 400.0 }; ///< Maximum search lag in milliseconds.
    double confidenceThreshold { 0.5 }; ///< Minimum peak correlation coefficient [0.0, 1.0] to consider estimate valid.
    double smoothingAlpha { 0.2 }; ///< Exponential moving average smoothing weight for latency updates [0.0, 1.0].
    PeakPolarity polarity { PeakPolarity::Absolute }; ///< Peak detection polarity mode.
    double minSignalVariance { 1e-6 }; ///< Minimum required reference and response signal variance.
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

    /// @brief Ingests an asynchronous timestamped reference command sample.
    /// @param[in] timestampSec Monotonic time in seconds.
    /// @param[in] reference Commanded excitation value.
    void addTimestampedReference(double timestampSec, double reference);

    /// @brief Ingests an asynchronous timestamped response observation sample.
    /// @param[in] timestampSec Monotonic time in seconds.
    /// @param[in] response Observed visual feedback value.
    void addTimestampedResponse(double timestampSec, double response);

    /// @brief Ingests a pair of timestamped reference and response samples.
    /// @param[in] timestampSec Monotonic time in seconds.
    /// @param[in] reference Commanded excitation signal.
    /// @param[in] response Observed visual feedback signal.
    void addTimestampedSample(double timestampSec, double reference, double response);

    /// @brief Evaluates cross-correlation across the current buffer contents and updates estimates.
    void update();

    /// @brief Returns the estimated latency in milliseconds.
    [[nodiscard]] double getEstimatedLatencyMs() const noexcept;

    /// @brief Returns the estimated latency in seconds.
    [[nodiscard]] double getEstimatedLatencySeconds() const noexcept;

    /// @brief Returns the signed peak Pearson cross-correlation coefficient [-1.0, 1.0].
    [[nodiscard]] double getPeakCorrelation() const noexcept;

    /// @brief Returns the peak correlation magnitude [0.0, 1.0].
    [[nodiscard]] double getPeakCorrelationMagnitude() const noexcept;

    /// @brief Returns true if the peak correlation meets or exceeds the confidence threshold.
    [[nodiscard]] bool isConfident() const noexcept;

    /// @brief Returns the discrete cross-correlation curve across all evaluated search lags.
    [[nodiscard]] std::vector<double> getCorrelationCurve() const;

    /// @brief Returns the reference signal variance computed during the latest update.
    [[nodiscard]] double getReferenceVariance() const noexcept;

    /// @brief Returns the response signal variance computed during the latest update.
    [[nodiscard]] double getResponseVariance() const noexcept;

    /// @brief Clears sample buffers and resets estimation state.
    void reset() noexcept;

    /// @brief Reconfigures the estimator.
    /// @param[in] config New configuration settings.
    void setConfig(const LatencyEstimatorConfig& config);

    /// @brief Returns the active estimator configuration.
    [[nodiscard]] const LatencyEstimatorConfig& getConfig() const noexcept;

private:
    void processTimestampedQueuesLocked();

    mutable std::mutex m_mutex;
    LatencyEstimatorConfig m_config {};

    std::vector<double> m_refBuffer {};
    std::vector<double> m_respBuffer {};
    std::size_t m_count { 0U };

    // Asynchronous timestamped resampling queues
    std::deque<std::pair<double, double>> m_refQueue {};
    std::deque<std::pair<double, double>> m_respQueue {};
    double m_nextResampleTime { -1.0 };

    double m_estimatedLatencyMs { 0.0 };
    double m_peakCorrelation { 0.0 };
    double m_lastVarRef { 0.0 };
    double m_lastVarResp { 0.0 };
    bool m_isConfident { false };
    std::vector<double> m_latestCorrelationCurve {};
};

} // namespace Tracking

namespace PelcoD {
namespace Tracking = ::Tracking;
using PeakPolarity = ::Tracking::PeakPolarity;
using LatencyEstimatorConfig = ::Tracking::LatencyEstimatorConfig;
using LatencyEstimator = ::Tracking::LatencyEstimator;
} // namespace PelcoD
