#pragma once

/// @file GoertzelFilter.h
/// @brief 2nd-order IIR Goertzel filter for targeted single-frequency spectral evaluation.

#include <cstddef>

namespace Math {

/// @class GoertzelFilter
/// @brief Real-time single-bin discrete Fourier transform evaluator.
/// @details Evaluates the spectral energy at an exact target frequency using an efficient 2nd-order
///          IIR difference equation with O(N) complexity, zero complex multiplications in the inner loop,
///          and support for continuous non-integer frequency tuning.
class GoertzelFilter {
public:
    /// @brief Constructs a Goertzel filter for a specific frequency.
    /// @details Initializes resonator coefficients for targetFreqHz at sampleRateHz over blockSize samples.
    /// @param[in] targetFreqHz Target frequency to monitor in Hertz.
    /// @param[in] sampleRateHz Signal sampling rate in Hertz.
    /// @param[in] blockSize Number of samples per evaluation block.
    explicit GoertzelFilter(double targetFreqHz = 10.0, double sampleRateHz = 50.0, std::size_t blockSize = 64U);

    /// @brief Reconfigures the filter parameters.
    /// @details Recomputes the 2nd-order IIR coefficient and resets all accumulator state variables.
    /// @param[in] targetFreqHz Target frequency in Hertz.
    /// @param[in] sampleRateHz Signal sampling rate in Hertz.
    /// @param[in] blockSize Number of samples per evaluation block.
    void setParameters(double targetFreqHz, double sampleRateHz, std::size_t blockSize = 64U) noexcept;

    /// @brief Ingests a single real-valued sample in streaming mode.
    /// @details Updates the internal 2nd-order difference equation. When blockSize samples have been ingested,
    ///          computes and latches the magnitude and power, and resets accumulators for the next block.
    /// @param[in] sample Input sample.
    /// @return True if a block boundary was reached and new spectral values were latched.
    [[nodiscard]] bool processSample(double sample) noexcept;

    /// @brief Evaluates a contiguous block of samples in batch mode.
    /// @details Runs the Goertzel recursion over size samples without mutating streaming accumulators.
    /// @param[in] data Pointer to sample array.
    /// @param[in] size Number of samples in the array.
    /// @return Normalized spectral magnitude (|X(f_t)|).
    [[nodiscard]] double computeMagnitude(const double* data, std::size_t size) const noexcept;

    /// @brief Evaluates a contiguous block of samples in batch mode, returning power.
    /// @details Computes the squared normalized magnitude for the provided sample buffer.
    /// @param[in] data Pointer to sample array.
    /// @param[in] size Number of samples in the array.
    /// @return Normalized spectral power (|X(f_t)|^2).
    [[nodiscard]] double computePower(const double* data, std::size_t size) const noexcept;

    /// @brief Returns the latest latched spectral magnitude (|X(f_t)|).
    /// @details Reflects the magnitude computed at the completion of the most recent streaming block.
    /// @return Latched spectral magnitude.
    [[nodiscard]] double getMagnitude() const noexcept;

    /// @brief Returns the latest latched spectral power (|X(f_t)|^2).
    /// @details Reflects the power computed at the completion of the most recent streaming block.
    /// @return Latched spectral power.
    [[nodiscard]] double getPower() const noexcept;

    /// @brief Checks if the latest spectral magnitude exceeds a specified threshold.
    /// @details Compares getMagnitude() against thresholdMagnitude.
    /// @param[in] thresholdMagnitude Minimum magnitude to trigger detection.
    /// @return True if latched magnitude >= thresholdMagnitude.
    [[nodiscard]] bool hasDetected(double thresholdMagnitude) const noexcept;

    /// @brief Resets all accumulator states and latched values to zero.
    /// @details Clears delay states m_s1, m_s2, sample counter, and latched magnitude/power.
    void reset() noexcept;

    /// @brief Returns the target frequency in Hertz.
    /// @return Configured target frequency in Hertz.
    [[nodiscard]] double getTargetFrequency() const noexcept;

    /// @brief Returns the sampling rate in Hertz.
    /// @return Configured sampling rate in Hertz.
    [[nodiscard]] double getSampleRate() const noexcept;

    /// @brief Returns the block size in samples.
    /// @return Configured evaluation block size.
    [[nodiscard]] std::size_t getBlockSize() const noexcept;

private:
    void calculateCoefficients() noexcept;

    double m_targetFreqHz { 10.0 };
    double m_sampleRateHz { 50.0 };
    std::size_t m_blockSize { 64U };

    double m_coeff { 0.0 };
    double m_cosOmega { 1.0 };

    double m_s1 { 0.0 };
    double m_s2 { 0.0 };
    std::size_t m_sampleIndex { 0U };

    double m_latchedMagnitude { 0.0 };
    double m_latchedPower { 0.0 };
};

} // namespace Math
