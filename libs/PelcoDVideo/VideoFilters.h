/**
 * @file VideoFilters.h
 * @brief Header declaring OpenCV-based post-processing video filters and Sightline-inspired enhancement algorithms.
 */

#pragma once

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

#include "DecoderTypes.h"
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Visibility macros for shared library export/import
#if defined(_MSC_VER)
#ifdef VIDEOFILTERS_EXPORTS
#define VIDEOFILTERS_API __declspec(dllexport)
#else
#define VIDEOFILTERS_API __declspec(dllimport)
#endif
#else
#ifdef VIDEOFILTERS_EXPORTS
#define VIDEOFILTERS_API __attribute__((visibility("default")))
#else
#define VIDEOFILTERS_API
#endif
#endif

#if defined(PELCOD_HAS_FILTERS)

namespace PelcoD::Video {

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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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
 * @class TextOverlayFilter
 * @brief Overlays a customizable string overlay onto the corner of video frames.
 */
class VIDEOFILTERS_API TextOverlayFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param text Text message to display.
     * @param x X coordinate offset from top-left.
     * @param y Y coordinate offset from top-left.
     * @param scale Text font scale factor.
     */
    TextOverlayFilter(const std::string& text, int x = 10, int y = 30, double scale = 1.0);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setText(const std::string& text)
    {
        m_text = text;
    }
    const std::string& getText() const
    {
        return m_text;
    }

private:
    std::string m_text;
    int m_x;
    int m_y;
    double m_scale;
};

/**
 * @class MirrorFilter
 * @brief Flips the video frame horizontally or vertically in-place.
 */
class VIDEOFILTERS_API MirrorFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param horizontal If true, flips horizontally. If false, flips vertically.
     */
    MirrorFilter(bool horizontal = true);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setHorizontal(bool horizontal)
    {
        m_horizontal = horizontal;
    }
    bool isHorizontal() const
    {
        return m_horizontal;
    }

private:
    bool m_horizontal;
};

/**
 * @class InvertColorsFilter
 * @brief Inverts the color channels of a video frame in-place (negative effect).
 */
class VIDEOFILTERS_API InvertColorsFilter : public IFrameProcessor {
public:
    InvertColorsFilter() = default;

    void process(uint8_t* data, int width, int height, PixelFormat format) override;
};

/**
 * @class GrayscaleFilter
 * @brief Converts a color video frame to grayscale in-place, preserving its 3-channel structure.
 */
class VIDEOFILTERS_API GrayscaleFilter : public IFrameProcessor {
public:
    GrayscaleFilter() = default;

    void process(uint8_t* data, int width, int height, PixelFormat format) override;
};

/**
 * @class SepiaFilter
 * @brief Applies a vintage sepia color transformation to a video frame in-place.
 */
class VIDEOFILTERS_API SepiaFilter : public IFrameProcessor {
public:
    SepiaFilter() = default;

    void process(uint8_t* data, int width, int height, PixelFormat format) override;
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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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
 * @class ColorTintFilter
 * @brief Multiplies color channels by specified scale factors in-place to achieve color tinting.
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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

private:
    int m_d;
    double m_sigmaColor;
    double m_sigmaSpace;
};

/**
 * @class GammaCorrectionFilter
 * @brief Adjusts the gamma of the video frame in-place.
 */
class VIDEOFILTERS_API GammaCorrectionFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param gamma Gamma value (1.0 is normal, < 1.0 is brighter, > 1.0 is darker).
     */
    GammaCorrectionFilter(double gamma = 1.0);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;
};

/**
 * @class MosaicFilter
 * @brief Applies a mosaic (pixelation) effect to the frame in-place.
 */
class VIDEOFILTERS_API MosaicFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param blockSize Width and height of pixel blocks.
     */
    MosaicFilter(int blockSize = 8);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

private:
    int m_blockSize;
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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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

// =========================================================================
// Sightline Enhancement Suite: Thermal, Tactical, and Vision Filters
// =========================================================================

/**
 * @enum FalseColorPalette
 * @brief Predefined thermal and false-color palettes inspired by Sightline IDD / EAN-Enhancement.
 */
enum class FalseColorPalette {
    WhiteHot, ///< Grayscale (white is hot, black is cold)
    BlackHot, ///< Inverted grayscale (black is hot, white is cold)
    Iron256, ///< Thermal Iron progression (black -> purple -> red -> yellow -> white)
    Jet, ///< Classic rainbow / jet spectrum
    Rainbow, ///< Multi-color rainbow spectrum
    HotCold, ///< Blue (cold) to Red (hot)
    IceFire, ///< Deep blue to vibrant red/orange saturation detector
    HotIron, ///< Hot iron color curve
    Turbo, ///< Google Turbo smooth colormap
    Bone, ///< Bone colormap with subtle blue/gray undertones
    UserPalette ///< Custom 256-entry user defined LUT
};

/**
 * @class FalseColorFilter
 * @brief Maps grayscale / thermal intensity to false-color palettes or custom 256-entry LUTs.
 */
class VIDEOFILTERS_API FalseColorFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param palette Desired false-color palette.
     */
    FalseColorFilter(FalseColorPalette palette = FalseColorPalette::Iron256);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setPalette(FalseColorPalette palette)
    {
        m_palette = palette;
    }
    FalseColorPalette getPalette() const
    {
        return m_palette;
    }

    /**
     * @brief Sets custom 256-entry RGB LUT (must contain 256 * 3 = 768 bytes).
     */
    void setUserPalette(const std::vector<uint8_t>& lut256x3);
    const std::vector<uint8_t>& getUserPalette() const
    {
        return m_userPalette;
    }

    /**
     * @brief Loads custom user palette from binary file (256x3 bytes RGB or YUV format).
     */
    bool loadUserPaletteFromFile(const std::string& filepath, bool isYuv = false);

    /**
     * @brief Saves current custom user palette to binary file.
     */
    bool saveUserPaletteToFile(const std::string& filepath, bool asYuv = false) const;

    /**
     * @brief Generates smoothly interpolated palette between specified control points.
     * @param controlPoints Map of intensity (0-255) to RGB triplet {R, G, B}.
     * @param smooth If true, applies smoothing across color bands.
     */
    void generateInterpolatedPalette(const std::map<uint8_t, std::vector<uint8_t>>& controlPoints, bool smooth = true);

private:
    FalseColorPalette m_palette;
    std::vector<uint8_t> m_userPalette; // 768 bytes (256 * RGB)
    void initDefaultUserPalette();
};

/**
 * @class LocalAreaProcessingFilter
 * @brief Local Area Processing (LAP) emphasizing local variations to reveal features in shadows/highlights.
 */
class VIDEOFILTERS_API LocalAreaProcessingFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor for Local Area Processing filter.
     * @param strength Size factor of local neighborhood kernel (1 to 18, radius = 2*strength+1).
     * @param blend Alpha blend between original and LAP image (0.0 to 1.0).
     * @param lapMinDiff Minimum local difference threshold to suppress flat area contour artifacts.
     */
    LocalAreaProcessingFilter(int strength = 5, double blend = 0.5, double lapMinDiff = 5.0);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setStrength(int strength)
    {
        m_strength = strength;
    }
    int getStrength() const
    {
        return m_strength;
    }
    void setBlend(double blend)
    {
        m_blend = blend;
    }
    double getBlend() const
    {
        return m_blend;
    }
    void setLapMinDiff(double minDiff)
    {
        m_lapMinDiff = minDiff;
    }
    double getLapMinDiff() const
    {
        return m_lapMinDiff;
    }

private:
    int m_strength;
    double m_blend;
    double m_lapMinDiff;
};

/**
 * @class HistogramEqualizationFilter
 * @brief Global, Square-Root, and Feature-Based Histogram Equalization with temporal anti-flicker smoothing.
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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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
 * @class CustomConvolutionFilter
 * @brief Applies user-defined NxN spatial matrix convolution with normalization and bias offset.
 */
class VIDEOFILTERS_API CustomConvolutionFilter : public IFrameProcessor {
public:
    CustomConvolutionFilter();
    CustomConvolutionFilter(
        const std::vector<float>& kernel, int kernelRows, int kernelCols, bool normalize = false, double bias = 0.0);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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
    std::vector<uint8_t> m_historyBuffer;
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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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

} // namespace PelcoD::Video

#endif // PELCOD_HAS_FILTERS

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
