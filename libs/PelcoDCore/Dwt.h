#pragma once

/// @file Dwt.h
/// @brief Header declaring Discrete Wavelet Transform (DWT), inverse DWT, and wavelet shrinkage denoising.

#include <cstddef>
#include <vector>

namespace PelcoD::Math {

/// @enum WaveletType
/// @brief Wavelet family types supported by the transform engine.
enum class WaveletType {
    Haar, ///< 2-tap Haar orthogonal wavelet (db1).
    Db4 ///< 4-tap Daubechies orthogonal wavelet (db4) with 2 vanishing moments.
};

/// @struct WaveletDecomposition1D
/// @brief Result of multi-level 1D Discrete Wavelet Transform.
struct WaveletDecomposition1D {
    std::vector<double> cA; ///< Coarse approximation coefficients at level L.
    std::vector<std::vector<double>> cD; ///< Detail coefficients for levels 1..L (cD[0] = level 1).
    WaveletType wavelet { WaveletType::Haar }; ///< Wavelet basis used.
    std::size_t originalLength { 0U }; ///< Original signal length before padding.
};

/// @struct WaveletCoefficients2D
/// @brief Result of single-level 2D Discrete Wavelet Transform.
struct WaveletCoefficients2D {
    std::vector<double> LL; ///< Low-Low sub-band (Approximation).
    std::vector<double> LH; ///< Low-High sub-band (Horizontal detail).
    std::vector<double> HL; ///< High-Low sub-band (Vertical detail).
    std::vector<double> HH; ///< High-High sub-band (Diagonal detail).
    std::size_t rows { 0U }; ///< Rows in each sub-band.
    std::size_t cols { 0U }; ///< Columns in each sub-band.
};

/// @brief Single-level 1D forward Discrete Wavelet Transform.
/// @param[in] signal Input 1D signal.
/// @param[out] cA Output approximation coefficients (length ceil(signal.size() / 2)).
/// @param[out] cD Output detail coefficients (length ceil(signal.size() / 2)).
/// @param[in] wavelet Wavelet family type (Haar or Db4).
void dwt1D(const std::vector<double>& signal, std::vector<double>& cA, std::vector<double>& cD,
    WaveletType wavelet = WaveletType::Haar);

/// @brief Single-level 1D inverse Discrete Wavelet Transform.
/// @param[in] cA Approximation coefficients.
/// @param[in] cD Detail coefficients.
/// @param[in] wavelet Wavelet family type (Haar or Db4).
/// @param[in] targetLength Desired reconstructed signal length (if 0, defaults to 2 * cA.size()).
/// @return Reconstructed 1D signal.
[[nodiscard]] std::vector<double> idwt1D(const std::vector<double>& cA, const std::vector<double>& cD,
    WaveletType wavelet = WaveletType::Haar, std::size_t targetLength = 0U);

/// @brief Multi-level 1D forward wavelet decomposition.
/// @param[in] signal Input signal.
/// @param[in] levels Number of decomposition levels (minimum 1).
/// @param[in] wavelet Wavelet family type.
/// @return WaveletDecomposition1D with approximation and hierarchical detail bands.
[[nodiscard]] WaveletDecomposition1D wavedec(
    const std::vector<double>& signal, std::size_t levels, WaveletType wavelet = WaveletType::Db4);

/// @brief Multi-level 1D inverse wavelet reconstruction.
/// @param[in] decomp Hierarchical wavelet decomposition structure.
/// @return Perfectly reconstructed original signal.
[[nodiscard]] std::vector<double> waverec(const WaveletDecomposition1D& decomp);

/// @brief Denoises a 1D signal using universal VisuShrink soft-thresholding.
/// @param[in] signal Input noisy signal.
/// @param[in] levels Number of wavelet decomposition levels.
/// @param[in] wavelet Wavelet family type.
/// @return Denoised signal with noise suppressed and edges preserved.
[[nodiscard]] std::vector<double> waveletDenoise(
    const std::vector<double>& signal, std::size_t levels = 3U, WaveletType wavelet = WaveletType::Db4);

/// @brief Single-level 2D forward Discrete Wavelet Transform of an M x N matrix.
/// @param[in] image Row-major image data of size rows * cols.
/// @param[in] rows Number of rows in image (must be even).
/// @param[in] cols Number of columns in image (must be even).
/// @param[in] wavelet Wavelet family type.
/// @return WaveletCoefficients2D containing LL, LH, HL, HH sub-bands of size (rows/2) x (cols/2).
[[nodiscard]] WaveletCoefficients2D dwt2D(
    const std::vector<double>& image, std::size_t rows, std::size_t cols, WaveletType wavelet = WaveletType::Haar);

/// @brief Single-level 2D inverse Discrete Wavelet Transform reconstructing M x N matrix.
/// @param[in] coeffs WaveletCoefficients2D structure containing LL, LH, HL, HH sub-bands.
/// @param[in] wavelet Wavelet family type.
/// @return Reconstructed row-major image of size (2*rows) * (2*cols).
[[nodiscard]] std::vector<double> idwt2D(const WaveletCoefficients2D& coeffs, WaveletType wavelet = WaveletType::Haar);

} // namespace PelcoD::Math
