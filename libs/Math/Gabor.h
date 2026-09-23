#pragma once

/// @file Gabor.h
/// @brief 1D and 2D Gabor Transform, complex quadrature filter kernels, multi-scale filterbanks,
///        directional sharpness evaluation, and texture appearance modeling.

#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Math {

/// @struct Gabor1DConfig
/// @brief Configuration settings for 1D continuous/analytic Gabor wavelet generation.
struct Gabor1DConfig {
    double centerFrequencyHz { 10.0 }; ///< Center frequency f0 of harmonic carrier in Hertz.
    double sampleRateHz { 100.0 }; ///< Sampling frequency in Hertz.
    double sigmaSeconds { 0.05 }; ///< Standard deviation sigma of the Gaussian envelope in seconds.
    std::size_t tapCount { 63U }; ///< Number of filter taps (must be odd, e.g. 31, 63, 127).
};

/// @struct GaborKernel1D
/// @brief 1D complex analytic Gabor impulse response decomposed into even and odd components.
struct GaborKernel1D {
    std::vector<double>
        realTaps {}; ///< Even/symmetric cosine impulse response: exp(-t^2/(2*sigma^2)) * cos(2*pi*f0*t).
    std::vector<double>
        imagTaps {}; ///< Odd/antisymmetric sine impulse response: exp(-t^2/(2*sigma^2)) * sin(2*pi*f0*t).
    double centerFrequencyHz { 0.0 }; ///< Center carrier frequency in Hertz.
    double sampleRateHz { 0.0 }; ///< Sampling rate in Hertz.
};

/// @struct Gabor2DConfig
/// @brief Configuration parameters for 2D spatial Gabor kernel generation.
struct Gabor2DConfig {
    double wavelength { 8.0 }; ///< Wavelength lambda in pixels (spatial frequency f = 1 / lambda).
    double orientationRad { 0.0 }; ///< Orientation angle theta in radians normal to parallel stripes.
    double sigma { 4.0 }; ///< Gaussian envelope standard deviation in pixels (spatial support scale).
    double spatialAspectRatio { 0.5 }; ///< Ellipticity gamma of the Gaussian envelope (default 0.5).
    double phaseOffsetRad { 0.0 }; ///< Phase offset psi in radians (0.0 for even, pi/2 for odd).
    int kernelSize { 21 }; ///< Square kernel dimension in pixels (must be positive and odd).
    bool removeDc { true }; ///< If true, subtract mean from real kernel to guarantee zero response to flat fields.
};

/// @struct GaborKernel2D
/// @brief Precomputed 2D spatial complex Gabor kernel pair (even and odd quadrature filters).
struct GaborKernel2D {
    std::vector<double> realPart {}; ///< Even/symmetric cosine kernel (row-major, size = kernelSize * kernelSize).
    std::vector<double> imagPart {}; ///< Odd/antisymmetric sine kernel (row-major, size = kernelSize * kernelSize).
    int kernelSize { 0 }; ///< Dimension of the square kernel in pixels.
    double wavelength { 0.0 }; ///< Spatial wavelength lambda in pixels.
    double orientationRad { 0.0 }; ///< Orientation angle theta in radians.
};

/// @struct GaborFilterBankConfig
/// @brief Configuration parameters for multi-scale, multi-orientation 2D Gabor filterbank.
struct GaborFilterBankConfig {
    std::size_t numScales { 3U }; ///< Number of frequency scales S (e.g. 3 or 4 octaves).
    std::size_t numOrientations { 4U }; ///< Number of orientation angles O (e.g. 4 for 0, 45, 90, 135 deg).
    double baseWavelength { 4.0 }; ///< Base (finest) wavelength lambda0 in pixels.
    double scaleFactor { 2.0 }; ///< Scale multiplication factor between successive octaves (default 2.0).
    double spatialAspectRatio { 0.5 }; ///< Spatial aspect ratio gamma (ellipticity).
    double sigmaScaleRatio { 0.56 }; ///< Ratio sigma / lambda determining spatial bandwidth (typically 0.56).
    int kernelSize { 21 }; ///< Square kernel dimension in pixels (must be odd).
    bool removeDc { true }; ///< Remove DC bias from even kernels.
};

/// @struct GaborChannelResult
/// @brief Output response of a single 2D Gabor filter (scale s, orientation o).
struct GaborChannelResult {
    std::size_t scaleIndex { 0U }; ///< Scale index (0 = finest scale).
    std::size_t orientationIndex { 0U }; ///< Orientation index (0 .. numOrientations - 1).
    double orientationRad { 0.0 }; ///< Filter orientation in radians.
    double wavelength { 0.0 }; ///< Filter wavelength in pixels.
    std::vector<double> energy {}; ///< Spatial energy map: sqrt(realResponse^2 + imagResponse^2).
    double meanEnergy { 0.0 }; ///< Average energy across evaluated spatial domain.
    double energyVariance { 0.0 }; ///< Variance of energy across spatial domain.
    double maxEnergy { 0.0 }; ///< Peak energy observed in this channel.
};

/// @struct GaborFilterBankResult
/// @brief Comprehensive evaluation result across all scales and orientations in a filterbank.
struct GaborFilterBankResult {
    std::vector<GaborChannelResult> channels {}; ///< Individual channel responses (size = scales * orientations).
    std::vector<double> maxEnergyProjection {}; ///< Pointwise maximum energy envelope across all channels.
    std::vector<double> directionalSharpness {}; ///< Directional sharpness score per orientation angle.
    std::vector<double> orientationAnglesRad {}; ///< Orientation angles corresponding to directionalSharpness.
    double dominantOrientationRad { 0.0 }; ///< Orientation angle with highest cumulative high-frequency energy.
    double anisotropyIndex { 0.0 }; ///< Directional sharpness anisotropy: (max - min) / (max + min) in [0.0, 1.0].
    std::vector<double> featureVector {}; ///< Texture descriptor vector: [mean_0, var_0, mean_1, var_1, ...].
    int imageWidth { 0 }; ///< Width of evaluated image in pixels.
    int imageHeight { 0 }; ///< Height of evaluated image in pixels.
};

/// @brief Generates a 1D analytic complex Gabor wavelet kernel.
/// @param[in] config 1D Gabor configuration parameters.
/// @return GaborKernel1D containing real (cosine) and imaginary (sine) tap vectors.
/// @note Thread-safe.
[[nodiscard]] GaborKernel1D createGaborKernel1D(const Gabor1DConfig& config);

/// @brief Generates a 2D spatial complex Gabor kernel pair (even and odd).
/// @param[in] config 2D Gabor configuration parameters.
/// @return GaborKernel2D containing normalized real and imaginary kernel arrays.
/// @note Thread-safe.
[[nodiscard]] GaborKernel2D createGaborKernel2D(const Gabor2DConfig& config);

/// @brief Performs 2D spatial convolution of an 8-bit grayscale image with a 2D kernel.
/// @param[in] pixels Pointer to contiguous 8-bit grayscale pixel data.
/// @param[in] width Frame width in pixels.
/// @param[in] height Frame height in pixels.
/// @param[in] stride Row stride in bytes (if <= 0, assumes stride == width).
/// @param[in] kernel Row-major kernel array of dimension (kernelSize * kernelSize).
/// @param[in] kernelSize Square dimension of kernel (must be odd).
/// @return Output floating-point convolution result of size (width * height).
/// @note Thread-safe; uses clamped boundary replication.
[[nodiscard]] std::vector<double> convolve2D(
    const std::uint8_t* pixels, int width, int height, int stride, const std::vector<double>& kernel, int kernelSize);

/// @brief Performs 2D spatial convolution of a floating-point image buffer with a 2D kernel.
/// @param[in] image Row-major floating-point image of size (width * height).
/// @param[in] width Frame width in pixels.
/// @param[in] height Frame height in pixels.
/// @param[in] kernel Row-major kernel array of dimension (kernelSize * kernelSize).
/// @param[in] kernelSize Square dimension of kernel (must be odd).
/// @return Output floating-point convolution result of size (width * height).
/// @note Thread-safe; uses clamped boundary replication.
[[nodiscard]] std::vector<double> convolve2D(
    const std::vector<double>& image, int width, int height, const std::vector<double>& kernel, int kernelSize);

/// @class GaborFilterBank
/// @brief Multi-scale, multi-orientation 2D Gabor filterbank for optical sharpness, texture analysis,
///        and directional motion blur estimation.
/// @details Precomputes a bank of complex Gabor quadrature filters spanning S scales and O orientations.
///          Evaluates image frames to produce orientation energy maps, directional sharpness metrics,
///          dominant orientation angles, and invariant texture feature vectors.
class GaborFilterBank {
public:
    /// @brief Constructs a Gabor filterbank with the specified configuration.
    /// @param[in] config Filterbank scales, orientations, wavelengths, and kernel sizing.
    explicit GaborFilterBank(GaborFilterBankConfig config = {});

    /// @brief Reconfigures the filterbank and precomputes all kernel pairs.
    /// @param[in] config New configuration parameters.
    void setConfig(const GaborFilterBankConfig& config);

    /// @brief Retrieves the active configuration.
    [[nodiscard]] GaborFilterBankConfig getConfig() const noexcept;

    /// @brief Evaluates an 8-bit grayscale frame across all filterbank channels.
    /// @param[in] grayPixels Pointer to 8-bit grayscale pixel buffer.
    /// @param[in] width Frame width in pixels.
    /// @param[in] height Frame height in pixels.
    /// @param[in] stride Row pitch in bytes (if <= 0, stride == width).
    /// @param[in] computeSpatialEnergy If true, populates full per-pixel energy maps in each channel.
    /// @return GaborFilterBankResult containing channel statistics, directional sharpness, and feature vectors.
    [[nodiscard]] GaborFilterBankResult evaluate(
        const std::uint8_t* grayPixels, int width, int height, int stride = 0, bool computeSpatialEnergy = true) const;

    /// @brief Evaluates a floating-point image buffer across all filterbank channels.
    /// @param[in] image Row-major floating-point image of size (width * height).
    /// @param[in] width Frame width in pixels.
    /// @param[in] height Frame height in pixels.
    /// @param[in] computeSpatialEnergy If true, populates full per-pixel energy maps.
    /// @return GaborFilterBankResult containing analysis metrics.
    [[nodiscard]] GaborFilterBankResult evaluate(
        const std::vector<double>& image, int width, int height, bool computeSpatialEnergy = true) const;

    /// @brief Computes directional sharpness scores across orientations for a specific region of interest.
    /// @param[in] grayPixels Pointer to 8-bit grayscale pixel buffer.
    /// @param[in] width Frame width in pixels.
    /// @param[in] height Frame height in pixels.
    /// @param[in] roiX Top-left X coordinate of ROI.
    /// @param[in] roiY Top-left Y coordinate of ROI.
    /// @param[in] roiWidth Width of ROI in pixels.
    /// @param[in] roiHeight Height of ROI in pixels.
    /// @param[in] stride Row pitch in bytes.
    /// @return Vector of directional sharpness values corresponding to each filterbank orientation.
    [[nodiscard]] std::vector<double> evaluateRoiDirectionalSharpness(const std::uint8_t* grayPixels, int width,
        int height, int roiX, int roiY, int roiWidth, int roiHeight, int stride = 0) const;

    /// @brief Returns the total number of filter channels (numScales * numOrientations).
    [[nodiscard]] std::size_t getChannelCount() const noexcept;

    /// @brief Accesses the precomputed 2D Gabor kernel for a specific scale and orientation.
    /// @param[in] scaleIndex Scale index (0 .. numScales - 1).
    /// @param[in] orientationIndex Orientation index (0 .. numOrientations - 1).
    /// @return Reference to precomputed GaborKernel2D.
    [[nodiscard]] const GaborKernel2D& getKernel(std::size_t scaleIndex, std::size_t orientationIndex) const;

private:
    void buildKernels();

    GaborFilterBankConfig m_config {};
    std::vector<GaborKernel2D> m_kernels {}; ///< Flattened kernels: index = s * numOrientations + o.
    std::vector<double> m_orientationAnglesRad {}; ///< Precomputed orientation angles.
};

} // namespace Math
