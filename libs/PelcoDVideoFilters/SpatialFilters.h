/**
 * @file SpatialFilters.h
 * @brief Video filters for spatial convolutions, blur, sharpening, denoise, dehazing, and lens optical corrections.
 */

#pragma once

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

#include "DecoderTypes.h"
#include <cstdint>
#include <vector>

#ifndef VIDEOFILTERS_API
#define VIDEOFILTERS_API
#endif

#if defined(PELCOD_HAS_FILTERS)

namespace PelcoD::Video::Filters {

/**
 * @class GaussianBlurFilter
 * @brief Applies Gaussian blur to a video frame in-place.
 */
class VIDEOFILTERS_API GaussianBlurFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param kernelSize Size of Gaussian blur kernel (must be positive and odd).
     */
    GaussianBlurFilter(int kernelSize = 5);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setKernelSize(int kernelSize)
    {
        m_kernelSize = kernelSize;
    }
    int getKernelSize() const
    {
        return m_kernelSize;
    }

private:
    int m_kernelSize;
};

/**
 * @class EdgeDetectionFilter
 * @brief Performs Canny edge detection overlay on a video frame.
 */
class VIDEOFILTERS_API EdgeDetectionFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param threshold1 First threshold for the hysteresis procedure.
     * @param threshold2 Second threshold for the hysteresis procedure.
     */
    EdgeDetectionFilter(double threshold1 = 50.0, double threshold2 = 150.0);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setThresholds(double t1, double t2)
    {
        m_threshold1 = t1;
        m_threshold2 = t2;
    }
    double getThreshold1() const
    {
        return m_threshold1;
    }
    double getThreshold2() const
    {
        return m_threshold2;
    }

private:
    double m_threshold1;
    double m_threshold2;
};

/**
 * @class SharpenFilter
 * @brief Applies an unsharp masking or Laplacian kernel with configurable radius and strength.
 */
class VIDEOFILTERS_API SharpenFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param strength Sharpen intensity multiplier (0.0 = no effect, 1.0 = standard, up to 5.0).
     * @param radius Feature size radius (1 = 3x3, 2 = 5x5, 3 = 7x7).
     */
    SharpenFilter(double strength = 1.0, int radius = 1);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setStrength(double strength)
    {
        m_strength = strength;
    }
    double getStrength() const
    {
        return m_strength;
    }
    void setRadius(int radius)
    {
        m_radius = radius;
    }
    int getRadius() const
    {
        return m_radius;
    }

private:
    double m_strength;
    int m_radius;
};

/**
 * @class BilateralFilter
 * @brief Smooths the frame while preserving edges using a bilateral filter.
 */
class VIDEOFILTERS_API BilateralFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param d Diameter of each pixel neighborhood.
     * @param sigmaColor Filter sigma in the color space.
     * @param sigmaSpace Filter sigma in the coordinate space.
     */
    BilateralFilter(int d = 9, double sigmaColor = 75.0, double sigmaSpace = 75.0);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

private:
    int m_d;
    double m_sigmaColor;
    double m_sigmaSpace;
};

/**
 * @class CustomConvolutionFilter
 * @brief Applies user-defined NxN spatial matrix convolution with normalization and bias offset.
 */
class VIDEOFILTERS_API CustomConvolutionFilter : public IFrameProcessor {
public:
    CustomConvolutionFilter();
    CustomConvolutionFilter(
        const std::vector<float>& kernel, int kernelRows, int kernelCols, bool normalize = false, double bias = 0.0);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setKernel(const std::vector<float>& kernel, int rows, int cols, bool normalize = false, double bias = 0.0);
    const std::vector<float>& getKernel() const
    {
        return m_kernelData;
    }
    int getRows() const
    {
        return m_rows;
    }
    int getCols() const
    {
        return m_cols;
    }
    bool isNormalized() const
    {
        return m_normalize;
    }
    double getBias() const
    {
        return m_bias;
    }

private:
    std::vector<float> m_kernelData;
    int m_rows;
    int m_cols;
    bool m_normalize;
    double m_bias;
};

/**
 * @class TemporalDenoiseFilter
 * @brief Temporal running average filter with motion masking to mitigate heat shimmer / scintillation.
 */
class VIDEOFILTERS_API TemporalDenoiseFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor for temporal noise / scintillation dampening filter.
     * @param blendRate Historical frame weight (0.0 = off, 0.5 = 50% history, up to 0.95).
     * @param motionThreshold Pixel difference threshold (0-255) above which motion mask excludes pixels from averaging.
     */
    TemporalDenoiseFilter(double blendRate = 0.5, double motionThreshold = 30.0);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setBlendRate(double rate)
    {
        m_blendRate = rate;
    }
    double getBlendRate() const
    {
        return m_blendRate;
    }
    void setMotionThreshold(double threshold)
    {
        m_motionThreshold = threshold;
    }
    double getMotionThreshold() const
    {
        return m_motionThreshold;
    }
    void reset();

private:
    double m_blendRate;
    double m_motionThreshold;
    std::vector<std::uint8_t> m_historyBuffer;
    int m_lastWidth = 0;
    int m_lastHeight = 0;
};

/**
 * @class LensDistortionFilter
 * @brief Radial barrel and pincushion optical lens distortion correction.
 */
class VIDEOFILTERS_API LensDistortionFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param k1 Radial distortion coefficient 1 (>0 for barrel, <0 for pincushion).
     * @param k2 Radial distortion coefficient 2.
     * @param centerOffsetX Normalized center X offset (-0.5 to +0.5).
     * @param centerOffsetY Normalized center Y offset (-0.5 to +0.5).
     */
    LensDistortionFilter(double k1 = 0.05, double k2 = 0.0, double centerOffsetX = 0.0, double centerOffsetY = 0.0);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setParameters(double k1, double k2, double centerOffsetX = 0.0, double centerOffsetY = 0.0);
    double getK1() const
    {
        return m_k1;
    }
    double getK2() const
    {
        return m_k2;
    }
    double getCenterOffsetX() const
    {
        return m_centerOffsetX;
    }
    double getCenterOffsetY() const
    {
        return m_centerOffsetY;
    }

private:
    double m_k1;
    double m_k2;
    double m_centerOffsetX;
    double m_centerOffsetY;
};

/**
 * @class DarkChannelDehazeFilter
 * @brief Atmospheric scattering inversion based on Dark Channel Prior (DCP) for fog and haze removal.
 */
class VIDEOFILTERS_API DarkChannelDehazeFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param omega Dehazing intensity factor (0.0 = no dehaze, 0.85 = typical, up to 0.95).
     * @param patchSize Patch neighborhood kernel radius for local dark channel calculation.
     * @param t0 Minimum transmission threshold floor to avoid noise amplification.
     */
    DarkChannelDehazeFilter(double omega = 0.85, int patchSize = 9, double t0 = 0.1);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setOmega(double omega)
    {
        m_omega = omega;
    }
    double getOmega() const
    {
        return m_omega;
    }
    void setPatchSize(int patchSize)
    {
        m_patchSize = patchSize;
    }
    int getPatchSize() const
    {
        return m_patchSize;
    }
    void setT0(double t0)
    {
        m_t0 = t0;
    }
    double getT0() const
    {
        return m_t0;
    }

private:
    double m_omega;
    int m_patchSize;
    double m_t0;
};

/**
 * @class ChromaticAberrationFilter
 * @brief Corrects lateral chromatic aberration (radial color fringing) typical of telephoto optics.
 */
class VIDEOFILTERS_API ChromaticAberrationFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param redCoeff Radial distortion correction factor for the Red channel.
     * @param blueCoeff Radial distortion correction factor for the Blue channel.
     * @param centerOffsetX Normalized optical axis X center offset (-0.5 to +0.5).
     * @param centerOffsetY Normalized optical axis Y center offset (-0.5 to +0.5).
     */
    ChromaticAberrationFilter(
        double redCoeff = 0.005, double blueCoeff = -0.005, double centerOffsetX = 0.0, double centerOffsetY = 0.0);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setParameters(double redCoeff, double blueCoeff, double centerOffsetX = 0.0, double centerOffsetY = 0.0);
    double getRedCoeff() const
    {
        return m_redCoeff;
    }
    double getBlueCoeff() const
    {
        return m_blueCoeff;
    }
    double getCenterOffsetX() const
    {
        return m_centerOffsetX;
    }
    double getCenterOffsetY() const
    {
        return m_centerOffsetY;
    }

private:
    double m_redCoeff;
    double m_blueCoeff;
    double m_centerOffsetX;
    double m_centerOffsetY;
};

} // namespace PelcoD::Video::Filters

namespace PelcoD::Video {
using Filters::BilateralFilter;
using Filters::ChromaticAberrationFilter;
using Filters::CustomConvolutionFilter;
using Filters::DarkChannelDehazeFilter;
using Filters::EdgeDetectionFilter;
using Filters::GaussianBlurFilter;
using Filters::LensDistortionFilter;
using Filters::SharpenFilter;
using Filters::TemporalDenoiseFilter;
} // namespace PelcoD::Video

#endif // PELCOD_HAS_FILTERS

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
