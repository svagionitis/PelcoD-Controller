/**
 * @file ColorFilters.h
 * @brief Video filters for color balance, brightness, contrast, tone curves, and histogram transformations.
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

namespace Video::Filters {

/**
 * @class BrightnessContrastFilter
 * @brief Adjusts brightness and contrast of a video frame in-place.
 */
class VIDEOFILTERS_API BrightnessContrastFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param alpha Contrast multiplier (1.0 is normal).
     * @param beta Brightness offset (0 is normal).
     */
    BrightnessContrastFilter(double alpha = 1.0, int beta = 0);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setAlpha(double alpha)
    {
        m_alpha = alpha;
    }
    double getAlpha() const
    {
        return m_alpha;
    }
    void setBeta(int beta)
    {
        m_beta = beta;
    }
    int getBeta() const
    {
        return m_beta;
    }

private:
    double m_alpha;
    int m_beta;
};

/**
 * @class InvertColorsFilter
 * @brief Inverts the color channels of a video frame in-place (negative effect).
 */
class VIDEOFILTERS_API InvertColorsFilter : public IFrameProcessor {
public:
    InvertColorsFilter() = default;

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;
};

/**
 * @class GrayscaleFilter
 * @brief Converts a color video frame to grayscale in-place, preserving its 3-channel structure.
 */
class VIDEOFILTERS_API GrayscaleFilter : public IFrameProcessor {
public:
    GrayscaleFilter() = default;

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;
};

/**
 * @class SepiaFilter
 * @brief Applies a vintage sepia color transformation to a video frame in-place.
 */
class VIDEOFILTERS_API SepiaFilter : public IFrameProcessor {
public:
    SepiaFilter() = default;

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;
};

/**
 * @class ColorTintFilter
 * @brief Scales independent RGB color channels to apply an arbitrary color tint or cast.
 */
class VIDEOFILTERS_API ColorTintFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param rScale Red channel scaling factor.
     * @param gScale Green channel scaling factor.
     * @param bScale Blue channel scaling factor.
     */
    ColorTintFilter(double rScale = 1.0, double gScale = 1.0, double bScale = 1.0);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setScales(double r, double g, double b)
    {
        m_rScale = r;
        m_gScale = g;
        m_bScale = b;
    }

private:
    double m_rScale;
    double m_gScale;
    double m_bScale;
};

/**
 * @class ClaheFilter
 * @brief Contrast Limited Adaptive Histogram Equalization (CLAHE) with blend control.
 */
class VIDEOFILTERS_API ClaheFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param clipLimit Threshold for contrast limiting.
     * @param tileGridSize Size of grid for histogram equalization.
     * @param blend Alpha blend between original and CLAHE output (0.0 to 1.0).
     */
    ClaheFilter(double clipLimit = 2.0, int tileGridSize = 8, double blend = 1.0);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setClipLimit(double clipLimit)
    {
        m_clipLimit = clipLimit;
    }
    double getClipLimit() const
    {
        return m_clipLimit;
    }
    void setTileGridSize(int tileGridSize)
    {
        m_tileGridSize = tileGridSize;
    }
    int getTileGridSize() const
    {
        return m_tileGridSize;
    }
    void setBlend(double blend)
    {
        m_blend = blend;
    }
    double getBlend() const
    {
        return m_blend;
    }

private:
    double m_clipLimit;
    int m_tileGridSize;
    double m_blend;
};

/**
 * @class GammaCorrectionFilter
 * @brief Applies non-linear gamma power-law correction to map image dynamic range.
 */
class VIDEOFILTERS_API GammaCorrectionFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param gamma Gamma value (1.0 is normal, < 1.0 is brighter, > 1.0 is darker).
     */
    GammaCorrectionFilter(double gamma = 1.0);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setGamma(double gamma)
    {
        m_gamma = gamma;
    }
    double getGamma() const
    {
        return m_gamma;
    }

private:
    double m_gamma;
};

/**
 * @class VignetteFilter
 * @brief Applies a dark vignette effect towards the edges of the frame in-place.
 */
class VIDEOFILTERS_API VignetteFilter : public IFrameProcessor {
public:
    VignetteFilter() = default;

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;
};

/**
 * @class ThresholdFilter
 * @brief Converts the frame to a binary black and white representation in-place.
 */
class VIDEOFILTERS_API ThresholdFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param thresholdValue Intensity threshold value (0-255).
     */
    ThresholdFilter(double thresholdValue = 127.0);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setThresholdValue(double val)
    {
        m_thresholdValue = val;
    }
    double getThresholdValue() const
    {
        return m_thresholdValue;
    }

private:
    double m_thresholdValue;
};

/**
 * @class HistogramEqualizationFilter
 * @brief Sightline-inspired adaptive histogram equalization with edge-weighted and anti-saturation modes.
 */
class VIDEOFILTERS_API HistogramEqualizationFilter : public IFrameProcessor {
public:
    /**
     * @enum Mode
     * @brief Histogram Equalization algorithm variants.
     */
    enum class Mode {
        Standard, ///< Standard global histogram equalization
        SquareRoot, ///< Square-root weighted equalization to prevent dominant flat backgrounds from saturating
        FeatureBased ///< Feature-based histogram equalization weighted by Sobel edge energy
    };

    /**
     * @brief Constructor.
     * @param mode Equalization mode.
     * @param blend Alpha blend with original image (0.0 to 1.0).
     * @param brightnessOffset Brightness shift offset (-128 to +128).
     * @param histAveRate Temporal smoothing factor (0.0 = instant update, 0.5 = 50% history, up to 0.95).
     * @param maxPercentBin Maximum percentage of pixels allowed in a single bin (0.01 to 1.0).
     * @param gamma Integrated power law gamma curve (1.0 = linear).
     */
    HistogramEqualizationFilter(Mode mode = Mode::Standard, double blend = 1.0, double brightnessOffset = 0.0,
        double histAveRate = 0.0, double maxPercentBin = 1.0, double gamma = 1.0);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setMode(Mode mode)
    {
        m_mode = mode;
    }
    Mode getMode() const
    {
        return m_mode;
    }
    void setBlend(double blend)
    {
        m_blend = blend;
    }
    double getBlend() const
    {
        return m_blend;
    }
    void setBrightnessOffset(double offset)
    {
        m_brightnessOffset = offset;
    }
    double getBrightnessOffset() const
    {
        return m_brightnessOffset;
    }
    void setHistAveRate(double rate)
    {
        m_histAveRate = rate;
    }
    double getHistAveRate() const
    {
        return m_histAveRate;
    }
    void setMaxPercentBin(double maxPct)
    {
        m_maxPercentBin = maxPct;
    }
    double getMaxPercentBin() const
    {
        return m_maxPercentBin;
    }
    void setGamma(double gamma)
    {
        m_gamma = gamma;
    }
    double getGamma() const
    {
        return m_gamma;
    }
    void resetTemporalMap();

private:
    Mode m_mode;
    double m_blend;
    double m_brightnessOffset;
    double m_histAveRate;
    double m_maxPercentBin;
    double m_gamma;
    std::vector<float> m_prevLut;
};

/**
 * @class ColorEnhanceFilter
 * @brief Boosts color saturation and vibrancy in HSV color space.
 */
class VIDEOFILTERS_API ColorEnhanceFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param factor Saturation scaling factor (0.0 = monochrome, 1.0 = unmodified, 1.5 = vivid, up to 3.0).
     */
    ColorEnhanceFilter(double factor = 1.5);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setFactor(double factor)
    {
        m_factor = factor;
    }
    double getFactor() const
    {
        return m_factor;
    }

private:
    double m_factor;
};

/**
 * @class WhiteBalanceFilter
 * @brief Automatic White Balance (AWB) using Gray-World or White-Patch algorithms.
 */
class VIDEOFILTERS_API WhiteBalanceFilter : public IFrameProcessor {
public:
    /**
     * @enum Mode
     * @brief AWB algorithm variant.
     */
    enum class Mode {
        GrayWorld, ///< Equalizes channel mean intensities to global scene mean
        WhitePatch ///< Scales channels based on peak specular highlights
    };

    /**
     * @brief Constructor.
     * @param mode White balance algorithm mode.
     * @param strength Alpha blend between original and white-balanced image (0.0 to 1.0).
     */
    WhiteBalanceFilter(Mode mode = Mode::GrayWorld, double strength = 1.0);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setMode(Mode mode)
    {
        m_mode = mode;
    }
    Mode getMode() const
    {
        return m_mode;
    }
    void setStrength(double strength)
    {
        m_strength = strength;
    }
    double getStrength() const
    {
        return m_strength;
    }

private:
    Mode m_mode;
    double m_strength;
};

} // namespace Video::Filters

namespace Video {
using Filters::BrightnessContrastFilter;
using Filters::ClaheFilter;
using Filters::ColorEnhanceFilter;
using Filters::ColorTintFilter;
using Filters::GammaCorrectionFilter;
using Filters::GrayscaleFilter;
using Filters::HistogramEqualizationFilter;
using Filters::InvertColorsFilter;
using Filters::SepiaFilter;
using Filters::ThresholdFilter;
using Filters::VignetteFilter;
using Filters::WhiteBalanceFilter;
} // namespace Video

#endif // PELCOD_HAS_FILTERS

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
