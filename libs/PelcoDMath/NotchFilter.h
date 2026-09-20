#pragma once

/// @file NotchFilter.h
/// @brief Digital 2nd-order IIR biquad notch filter for mechanical resonance and mast vibration rejection.

namespace PelcoD {

/// @class NotchFilter
/// @brief 2nd-order digital IIR biquad notch filter.
/// @details Attenuates a narrow frequency band around center frequency f0 while passing other frequencies unaltered.
///          Operates with zero latency allocation, ideal for high-rate control loops.
class NotchFilter {
public:
    /// @brief Constructs a notch filter with specified parameters.
    /// @param[in] centerFreqHz Center notch frequency in Hertz to reject.
    /// @param[in] sampleRateHz Signal sampling rate in Hertz.
    /// @param[in] qFactor Quality factor Q (higher Q produces a narrower notch band).
    explicit NotchFilter(double centerFreqHz = 10.0, double sampleRateHz = 50.0, double qFactor = 5.0);

    /// @brief Reconfigures the filter coefficients.
    /// @param[in] centerFreqHz Center notch frequency in Hertz.
    /// @param[in] sampleRateHz Signal sampling rate in Hertz.
    /// @param[in] qFactor Quality factor Q (must be positive).
    void setParameters(double centerFreqHz, double sampleRateHz, double qFactor = 5.0) noexcept;

    /// @brief Processes a single input sample through the notch filter.
    /// @param[in] sample Raw time-domain input sample.
    /// @return Filtered output sample.
    [[nodiscard]] double process(double sample) noexcept;

    /// @brief Resets filter state history (delay line).
    void reset() noexcept;

    /// @brief Returns the configured notch center frequency in Hertz.
    [[nodiscard]] double getCenterFrequency() const noexcept
    {
        return m_centerFreqHz;
    }

    /// @brief Returns the configured sampling rate in Hertz.
    [[nodiscard]] double getSampleRate() const noexcept
    {
        return m_sampleRateHz;
    }

    /// @brief Returns the configured Q factor.
    [[nodiscard]] double getQFactor() const noexcept
    {
        return m_qFactor;
    }

private:
    void calculateCoefficients() noexcept;

    double m_centerFreqHz { 10.0 };
    double m_sampleRateHz { 50.0 };
    double m_qFactor { 5.0 };

    double m_b0 { 1.0 };
    double m_b1 { 0.0 };
    double m_b2 { 0.0 };
    double m_a1 { 0.0 };
    double m_a2 { 0.0 };

    double m_x1 { 0.0 };
    double m_x2 { 0.0 };
    double m_y1 { 0.0 };
    double m_y2 { 0.0 };
};

} // namespace PelcoD
