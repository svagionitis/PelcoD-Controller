#pragma once

/// @file Fft.h
/// @brief Header declaring Fast Fourier Transform (FFT), windowing functions, and spectral analysis tools.

#include <complex>
#include <cstddef>
#include <vector>

namespace Math {

using Complex = std::complex<double>;

/// @enum WindowType
/// @brief Windowing function types for spectral leakage suppression in FFT analysis.
enum class WindowType {
    Rectangular, ///< Uniform rectangular window (no attenuation).
    Hann, ///< Hann (Hanning) cosine window: 0.5 * (1 - cos(2*pi*n/(N-1))).
    Hamming, ///< Hamming window: 0.54 - 0.46 * cos(2*pi*n/(N-1)).
    Blackman ///< Blackman window: 0.42 - 0.5*cos + 0.08*cos(4*pi*n/(N-1)).
};

/// @struct SpectralPeak
/// @brief Represents a detected frequency component from Power Spectral Density analysis.
struct SpectralPeak {
    double frequencyHz { 0.0 }; ///< Center frequency in Hertz.
    double magnitude { 0.0 }; ///< Absolute spectral magnitude (|X(f)|).
    double powerRatio { 0.0 }; ///< Fraction of total spectral energy in this peak [0.0, 1.0].
};

/// @brief In-place 1D Radix-2 Cooley-Tukey Fast Fourier Transform.
/// @details Computes discrete Fourier transform of a complex vector whose length is a power of 2.
///          When inverse is true, computes the IFFT normalized by 1/N.
/// @param[in,out] data In-place vector of complex values to transform. Size must be a power of 2.
/// @param[in] inverse Set to true for inverse FFT (IFFT), false for forward FFT.
/// @note Thread-safe; operates strictly on local argument data.
void fft(std::vector<Complex>& data, bool inverse = false);

/// @brief Computes the forward FFT of a real-valued input sequence.
/// @details Zero-pads input to the next power of 2 if necessary.
/// @param[in] realSignal Sequence of real-valued time-domain samples.
/// @return Complex spectrum vector of length N (power of 2).
/// @note Thread-safe.
[[nodiscard]] std::vector<Complex> rfft(const std::vector<double>& realSignal);

/// @brief Computes the inverse FFT of a complex spectrum returning the real-valued signal.
/// @param[in] spectrum Complex spectrum sequence. Size must be a power of 2.
/// @return Real components of the inverse transformed sequence.
/// @note Thread-safe.
[[nodiscard]] std::vector<double> irfft(const std::vector<Complex>& spectrum);

/// @brief Applies a windowing function to a real-valued signal in-place.
/// @param[in,out] signal Real signal samples to window.
/// @param[in] window Windowing function type.
/// @note Thread-safe.
void applyWindow(std::vector<double>& signal, WindowType window);

/// @brief Computes Power Spectral Density (PSD) and identifies dominant peaks.
/// @details Windows the signal, runs forward FFT, computes single-sided power spectrum up to Nyquist,
///          and extracts spectral peaks sorted by descending magnitude.
/// @param[in] signal Time-domain samples.
/// @param[in] sampleRateHz Sampling rate in Hertz.
/// @param[in] window Windowing function to reduce spectral leakage.
/// @return Vector of SpectralPeak sorted in descending order of magnitude.
/// @note Thread-safe.
[[nodiscard]] std::vector<SpectralPeak> computePsd(
    const std::vector<double>& signal, double sampleRateHz, WindowType window = WindowType::Hann);

/// @brief Returns the next power of two greater than or equal to n.
/// @param[in] n Non-negative integer.
/// @return Next power of two (minimum 1).
[[nodiscard]] std::size_t nextPowerOfTwo(std::size_t n) noexcept;

/// @brief In-place 2D Radix-2 Fast Fourier Transform of an M x N complex matrix.
/// @details Computes separable 2D discrete Fourier transform (row-wise then column-wise).
///          rows and cols must be powers of 2.
///          When inverse is true, computes the 2D IFFT normalized by 1/(rows * cols).
/// @param[in,out] matrix Row-major complex matrix of size rows * cols.
/// @param[in] rows Number of rows (must be power of 2).
/// @param[in] cols Number of columns (must be power of 2).
/// @param[in] inverse Set to true for inverse 2D FFT, false for forward 2D FFT.
/// @note Thread-safe.
void fft2D(std::vector<Complex>& matrix, std::size_t rows, std::size_t cols, bool inverse = false);

/// @brief Applies a separable 2D window function to an M x N real image buffer in-place.
/// @param[in,out] image Row-major real matrix of size rows * cols.
/// @param[in] rows Number of rows.
/// @param[in] cols Number of columns.
/// @param[in] window Windowing function type.
/// @note Thread-safe.
void applyWindow2D(std::vector<double>& image, std::size_t rows, std::size_t cols, WindowType window);

/// @brief Checks if a given integer is an exact power of two.
/// @param[in] n Integer to check.
/// @return True if n > 0 and n is a power of 2, false otherwise.
[[nodiscard]] bool isPowerOfTwo(std::size_t n) noexcept;

} // namespace Math
