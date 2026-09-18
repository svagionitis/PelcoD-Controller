#pragma once

/// @file Stft.h
/// @brief Short-Time Fourier Transform (STFT) and real-time streaming spectrogram engine.

#include "Fft.h"

#include <cstddef>
#include <deque>
#include <mutex>
#include <vector>

namespace PelcoD {

/// @struct StftConfig
/// @brief Configuration settings for Short-Time Fourier Transform analysis and history buffering.
struct StftConfig {
    std::size_t windowSize { 128U }; ///< FFT window size (must be power of two: 32, 64, 128, 256, 512).
    std::size_t hopSize { 64U }; ///< Advance step between successive FFT windows (e.g. windowSize / 2 for 50% overlap).
    double sampleRateHz { 50.0 }; ///< Sampling rate of the incoming signal in Hertz.
    Math::WindowType windowType { Math::WindowType::Hann }; ///< Window function to suppress spectral leakage.
    std::size_t maxHistoryFrames { 100U }; ///< Maximum number of historical time slices to retain in memory.
    double minDb { -60.0 }; ///< Lower decibel floor for log-magnitude dynamic range clamping.
    double maxDb { 0.0 }; ///< Upper decibel ceiling for log-magnitude scaling.
    bool detrend { true }; ///< Subtract mean from each analysis window to suppress 0 Hz DC bias.
};

/// @struct SpectrogramFrame
/// @brief Single time-slice frequency distribution snapshot produced by STFT.
struct SpectrogramFrame {
    double timestamp { 0.0 }; ///< Elapsed time in seconds corresponding to this time slice.
    std::vector<double> powerSpectrum {}; ///< Linear Power Spectral Density bins [0 Hz to Nyquist].
    std::vector<double> dbSpectrum {}; ///< Logarithmic power spectrum in decibels clamped to [minDb, maxDb].
    double peakFrequencyHz { 0.0 }; ///< Frequency bin containing highest spectral power.
    double peakMagnitude { 0.0 }; ///< Linear magnitude of the dominant frequency peak.
    double powerRatio { 0.0 }; ///< Ratio of dominant peak power to total spectral power [0.0, 1.0].
    double spectralCentroidHz { 0.0 }; ///< Spectral center of mass / energy-weighted average frequency.
    double spectralFlatness { 0.0 }; ///< Wiener entropy [0.0 = pure harmonic tone, 1.0 = white noise].
    double totalEnergy { 0.0 }; ///< Sum of spectral power across all frequency bins.
};

/// @class Stft
/// @brief Streaming Short-Time Fourier Transform engine maintaining a rolling 2D time-frequency spectrogram.
/// @details Ingests incoming continuous signal samples into a circular buffer, evaluates sliding window FFTs
///          at configurable hop intervals, and manages a rolling history buffer for real-time waterfall display.
///          Thread-safe for concurrent sample ingestion and GUI/TUI rendering.
class Stft {
public:
    /// @brief Constructs an STFT processor with the specified configuration.
    /// @param[in] config Transform parameters, windowing, and history sizing.
    explicit Stft(StftConfig config = {});

    /// @brief Reconfigures the STFT processor and flushes internal buffers.
    /// @param[in] config New configuration parameters.
    void setConfig(const StftConfig& config);

    /// @brief Retrieves the active configuration.
    [[nodiscard]] StftConfig getConfig() const noexcept;

    /// @brief Ingests a single real-valued sample into the streaming circular buffer.
    /// @param[in] sample New signal value (e.g. tracking error, motor velocity, or vibration).
    /// @param[in] timestamp Optional timestamp in seconds (-1.0 to auto-increment by 1 / sampleRateHz).
    void addSample(double sample, double timestamp = -1.0);

    /// @brief Ingests a contiguous block of samples.
    /// @param[in] samples Array of time-domain values.
    /// @param[in] startTime Timestamp of the first sample in seconds.
    void addSamples(const std::vector<double>& samples, double startTime = 0.0);

    /// @brief Retrieves a snapshot of the rolling spectrogram frame history.
    /// @return Copy of the chronological deque of SpectrogramFrames (oldest to newest).
    [[nodiscard]] std::vector<SpectrogramFrame> getHistory() const;

    /// @brief Checks if at least one spectrogram frame has been computed.
    [[nodiscard]] bool hasFrames() const noexcept;

    /// @brief Retrieves the most recently computed spectrogram frame.
    /// @return Latest SpectrogramFrame.
    [[nodiscard]] SpectrogramFrame getLatestFrame() const;

    /// @brief Generates the physical frequency values (in Hertz) for each bin from 0 Hz to Nyquist.
    /// @return Vector of frequency centers of size (windowSize / 2 + 1).
    [[nodiscard]] std::vector<double> getFrequencyBinsHz() const;

    /// @brief Clears all sample and frame history buffers.
    void reset() noexcept;

private:
    void processWindow(double windowEndTime);

    mutable std::mutex m_mutex;
    StftConfig m_config {};

    std::vector<double> m_sampleBuffer {};
    std::size_t m_samplesSinceLastHop { 0U };
    double m_currentTimestamp { 0.0 };

    std::deque<SpectrogramFrame> m_history {};
};

} // namespace PelcoD
