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
#include <mutex>
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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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
 * @class ImageStabilizationFilter
 * @brief Electronic Image Stabilization (EIS) compensating for camera mast vibration and telephoto jitter.
 */
class VIDEOFILTERS_API ImageStabilizationFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param smoothingFactor Weight of trajectory history (0.1 = fast, 0.8 = smooth, up to 0.95).
     * @param maxJitterPixels Maximum pixel jitter threshold before resetting trajectory (deliberate PTZ pan/tilt).
     * @param cropMarginPercent Auto-crop margin percentage to mask warping border artifacts (e.g. 0.04 = 4%).
     */
    ImageStabilizationFilter(
        double smoothingFactor = 0.8, double maxJitterPixels = 30.0, double cropMarginPercent = 0.04);
    ~ImageStabilizationFilter() override;

    // Non-copyable due to unique_ptr PIMPL
    ImageStabilizationFilter(const ImageStabilizationFilter&) = delete;
    ImageStabilizationFilter& operator=(const ImageStabilizationFilter&) = delete;
    ImageStabilizationFilter(ImageStabilizationFilter&&) noexcept;
    ImageStabilizationFilter& operator=(ImageStabilizationFilter&&) noexcept;

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setSmoothingFactor(double factor)
    {
        m_smoothingFactor = factor;
    }
    double getSmoothingFactor() const
    {
        return m_smoothingFactor;
    }
    void setMaxJitterPixels(double maxJitter)
    {
        m_maxJitterPixels = maxJitter;
    }
    double getMaxJitterPixels() const
    {
        return m_maxJitterPixels;
    }
    void setCropMarginPercent(double margin)
    {
        m_cropMarginPercent = margin;
    }
    double getCropMarginPercent() const
    {
        return m_cropMarginPercent;
    }

    using MotionCallback = std::function<void(double dx, double dy, double dt)>;

    /// @brief Set a callback to receive real-time frame-to-frame translation for latency estimation.
    void setMotionCallback(MotionCallback callback);

    /// @brief Retrieve the latest frame-to-frame translation in pixels.
    void getLastFrameMotion(double& dx, double& dy) const noexcept;

    void reset();

private:
    double m_smoothingFactor;
    double m_maxJitterPixels;
    double m_cropMarginPercent;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @class WhiteBalanceFilter
 * @brief Automatic White Balance (AWB) adjusting color casts from artificial lights and atmospheric scattering.
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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

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

/**
 * @class IsothermFilter
 * @brief Isolates critical temperature/intensity bands with alert colors while rendering background in monochrome.
 */
class VIDEOFILTERS_API IsothermFilter : public IFrameProcessor {
public:
    enum class Preset {
        Custom, ///< Custom threshold range
        HumanBody, ///< Narrow band for personnel body heat (~140 to 180 in 8-bit luma)
        HighHeat ///< High intensity threshold for fire, engines, and muzzle flashes (> 200)
    };

    enum class HighlightColor {
        Red, ///< Tactical Alert Red (RGB 255, 0, 0)
        Amber, ///< High-Vis Amber (RGB 255, 191, 0)
        Cyan, ///< Electric Cyan (RGB 0, 255, 255)
        Iron256 ///< Thermal colormap slice
    };

    IsothermFilter(int lowThreshold = 140, int highThreshold = 180, HighlightColor color = HighlightColor::Red,
        bool whiteHotBackground = true);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setPreset(Preset preset);
    Preset getPreset() const
    {
        return m_preset;
    }

    void setThresholds(int low, int high);
    int getLowThreshold() const
    {
        return m_lowThreshold;
    }
    int getHighThreshold() const
    {
        return m_highThreshold;
    }

    void setHighlightColor(HighlightColor color)
    {
        m_color = color;
    }
    HighlightColor getHighlightColor() const
    {
        return m_color;
    }

    void setWhiteHotBackground(bool whiteHot)
    {
        m_whiteHotBackground = whiteHot;
    }
    bool isWhiteHotBackground() const
    {
        return m_whiteHotBackground;
    }

private:
    Preset m_preset;
    int m_lowThreshold;
    int m_highThreshold;
    HighlightColor m_color;
    bool m_whiteHotBackground;
};

/**
 * @class HotspotTrackerFilter
 * @brief Automatically locates, tracks, and annotates peak thermal hot and cold spots with optical bore radiometry.
 */
class VIDEOFILTERS_API HotspotTrackerFilter : public IFrameProcessor {
public:
    struct RadiometryStats {
        int hotX { 0 };
        int hotY { 0 };
        uint8_t hotVal { 0 };
        int coldX { 0 };
        int coldY { 0 };
        uint8_t coldVal { 0 };
        uint8_t centerMean { 0 };
    };

    HotspotTrackerFilter(bool showOverlay = true, int centerBoxSize = 32);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setShowOverlay(bool show)
    {
        m_showOverlay = show;
    }
    bool getShowOverlay() const
    {
        return m_showOverlay;
    }

    void setCenterBoxSize(int size)
    {
        m_centerBoxSize = size;
    }
    int getCenterBoxSize() const
    {
        return m_centerBoxSize;
    }

    RadiometryStats getStats() const;

private:
    bool m_showOverlay;
    int m_centerBoxSize;
    mutable std::mutex m_statsMutex;
    RadiometryStats m_stats;
};

/**
 * @class MovingTargetIndicatorFilter
 * @brief Ground Moving Target Indication (GMTI/MTI) detecting moving objects against a static/stabilized background.
 */
class VIDEOFILTERS_API MovingTargetIndicatorFilter : public IFrameProcessor {
public:
    struct TargetBox {
        int x { 0 };
        int y { 0 };
        int width { 0 };
        int height { 0 };
        int id { 0 };
    };

    MovingTargetIndicatorFilter(int minArea = 100, int maxArea = 50000, int maxTargets = 16);
    ~MovingTargetIndicatorFilter() override;

    MovingTargetIndicatorFilter(const MovingTargetIndicatorFilter&) = delete;
    MovingTargetIndicatorFilter& operator=(const MovingTargetIndicatorFilter&) = delete;
    MovingTargetIndicatorFilter(MovingTargetIndicatorFilter&&) noexcept;
    MovingTargetIndicatorFilter& operator=(MovingTargetIndicatorFilter&&) noexcept;

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setMinArea(int minArea)
    {
        m_minArea = minArea;
    }
    int getMinArea() const
    {
        return m_minArea;
    }

    void setMaxArea(int maxArea)
    {
        m_maxArea = maxArea;
    }
    int getMaxArea() const
    {
        return m_maxArea;
    }

    void setMaxTargets(int maxTargets)
    {
        m_maxTargets = maxTargets;
    }
    int getMaxTargets() const
    {
        return m_maxTargets;
    }

    void reset();
    std::size_t getTargetCount() const;
    std::vector<TargetBox> getTargets() const;

private:
    int m_minArea;
    int m_maxArea;
    int m_maxTargets;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @class TacticalReticleOverlayFilter
 * @brief Overlays military boresight reticles, mil-dot scales, and stadiametric rangefinder markings.
 */
class VIDEOFILTERS_API TacticalReticleOverlayFilter : public IFrameProcessor {
public:
    enum class Style {
        Crosshair, ///< Boresight crosshair with open center circle
        MilDot, ///< Calibrated mil-dots on X and Y axes
        Stadiametric, ///< Human/vehicle height reference scale brackets
        CornerBrackets ///< Tactical camera sensor frame border brackets
    };

    enum class Color {
        TacticalGreen, ///< High-vis Night-vision Green (RGB 0, 255, 64)
        Red, ///< Alert Red (RGB 255, 48, 48)
        Amber, ///< FLIR Amber (RGB 255, 191, 0)
        White, ///< White (RGB 255, 255, 255)
        Cyan ///< Electric Cyan (RGB 0, 255, 255)
    };

    TacticalReticleOverlayFilter(Style style = Style::Crosshair, Color color = Color::TacticalGreen,
        int lineThickness = 1, int deadbandGap = 16);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setStyle(Style style)
    {
        m_style = style;
    }
    Style getStyle() const
    {
        return m_style;
    }

    void setColor(Color color)
    {
        m_color = color;
    }
    Color getColor() const
    {
        return m_color;
    }

    void setLineThickness(int thickness)
    {
        m_lineThickness = thickness;
    }
    int getLineThickness() const
    {
        return m_lineThickness;
    }

    void setDeadbandGap(int gap)
    {
        m_deadbandGap = gap;
    }
    int getDeadbandGap() const
    {
        return m_deadbandGap;
    }

private:
    Style m_style;
    Color m_color;
    int m_lineThickness;
    int m_deadbandGap;
};

/**
 * @class OpticalFlowFieldFilter
 * @brief Computes and visualizes spatial motion vectors across the surveillance scene using optical flow.
 */
class VIDEOFILTERS_API OpticalFlowFieldFilter : public IFrameProcessor {
public:
    enum class DisplayMode {
        VectorArrows, ///< Tactical velocity arrows on a spatial grid
        ColorFlow ///< Directional color wheel (Hue = angle, Saturation/Value = speed)
    };

    OpticalFlowFieldFilter(DisplayMode mode = DisplayMode::VectorArrows, int gridStep = 16, double minVelocity = 1.5,
        double arrowScale = 2.0);
    ~OpticalFlowFieldFilter() override;

    OpticalFlowFieldFilter(const OpticalFlowFieldFilter&) = delete;
    OpticalFlowFieldFilter& operator=(const OpticalFlowFieldFilter&) = delete;
    OpticalFlowFieldFilter(OpticalFlowFieldFilter&&) noexcept;
    OpticalFlowFieldFilter& operator=(OpticalFlowFieldFilter&&) noexcept;

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setDisplayMode(DisplayMode mode)
    {
        m_mode = mode;
    }
    DisplayMode getDisplayMode() const
    {
        return m_mode;
    }

    void setGridStep(int step)
    {
        m_gridStep = std::max(4, step);
    }
    int getGridStep() const
    {
        return m_gridStep;
    }

    void setMinVelocity(double minVel)
    {
        m_minVelocity = std::max(0.0, minVel);
    }
    double getMinVelocity() const
    {
        return m_minVelocity;
    }

    void setArrowScale(double scale)
    {
        m_arrowScale = scale;
    }
    double getArrowScale() const
    {
        return m_arrowScale;
    }

    void reset();

private:
    DisplayMode m_mode;
    int m_gridStep;
    double m_minVelocity;
    double m_arrowScale;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @class CentroidTargetTrackerFilter
 * @brief Locks onto and tracks a visual target, computing azimuth/elevation boresight error telemetry for PTZ tracking.
 */
class VIDEOFILTERS_API CentroidTargetTrackerFilter : public IFrameProcessor {
public:
    struct TargetState {
        int x { 0 };
        int y { 0 };
        int width { 0 };
        int height { 0 };
        double errorX { 0.0 }; ///< Normalized X offset from boresight (-1.0 left to +1.0 right)
        double errorY { 0.0 }; ///< Normalized Y offset from boresight (-1.0 up to +1.0 down)
        double vx { 0.0 }; ///< Target velocity X in px/frame
        double vy { 0.0 }; ///< Target velocity Y in px/frame
        double ax { 0.0 }; ///< Target acceleration X in px/frame^2
        double ay { 0.0 }; ///< Target acceleration Y in px/frame^2
        double confidence { 0.0 }; ///< Tracking confidence (0.0 to 1.0)
        bool locked { false };
        bool isCoasting { false }; ///< True if target is temporarily occluded and coasting on prediction
        double predictedErrorX { 0.0 }; ///< Latency-compensated predicted boresight error X
        double predictedErrorY { 0.0 }; ///< Latency-compensated predicted boresight error Y
        double scaleFactor { 1.0 }; ///< Current scale ratio relative to initial acquisition
        double appearanceScore { 1.0 }; ///< Appearance signature correlation score (0.0 to 1.0)
        double normalizedWidth { 0.0 }; ///< Target width normalized by viewport frame width [0.0 to 1.0]
        double normalizedHeight { 0.0 }; ///< Target height normalized by viewport frame height [0.0 to 1.0]
    };

    CentroidTargetTrackerFilter(bool autoAcquire = true, int targetWidth = 40, int targetHeight = 40);
    ~CentroidTargetTrackerFilter() override;

    CentroidTargetTrackerFilter(const CentroidTargetTrackerFilter&) = delete;
    CentroidTargetTrackerFilter& operator=(const CentroidTargetTrackerFilter&) = delete;
    CentroidTargetTrackerFilter(CentroidTargetTrackerFilter&&) noexcept;
    CentroidTargetTrackerFilter& operator=(CentroidTargetTrackerFilter&&) noexcept;

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setAutoAcquire(bool autoAcquire)
    {
        m_autoAcquire = autoAcquire;
    }
    bool isAutoAcquire() const
    {
        return m_autoAcquire;
    }

    void acquireTarget(int x, int y, int width, int height);
    void releaseTarget();

    bool isTargetLocked() const;
    TargetState getTargetState(double lookaheadLatencySeconds = -1.0) const;

    /// @brief Sets the dynamic lookahead latency (in seconds) used by default.
    void setDynamicLookaheadLatency(double seconds) noexcept;

    /// @brief Gets the current dynamic lookahead latency (in seconds).
    double getDynamicLookaheadLatency() const noexcept;

    void setMaxCoastFrames(int frames) noexcept;
    int getMaxCoastFrames() const noexcept;
    void setProcessNoise(double qPos, double qVel, double qAcc = 1e-1) noexcept;
    void setMeasurementNoise(double rPos) noexcept;
    void setAdaptiveProcessNoiseEnabled(bool enabled) noexcept;
    bool isAdaptiveProcessNoiseEnabled() const noexcept;

    void setScaleAdaptation(bool enabled) noexcept;
    bool isScaleAdaptation() const noexcept;
    void setAppearanceFusion(bool enabled) noexcept;
    bool isAppearanceFusion() const noexcept;
    void setAppearanceLearningRate(double rate) noexcept;
    double getAppearanceLearningRate() const noexcept;

    void setTrajectoryTrail(bool enabled, int maxPoints = 30) noexcept;
    bool isTrajectoryTrail() const noexcept;
    int getTrajectoryMaxPoints() const noexcept;

    void setPredictiveVector(bool enabled, double lookaheadSeconds = 1.5) noexcept;
    bool isPredictiveVector() const noexcept;
    double getPredictiveVectorLookahead() const noexcept;

private:
    bool m_autoAcquire;
    int m_defaultWidth;
    int m_defaultHeight;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @class PerimeterTripwireFilter
 * @brief Virtual security tripwire detecting directional line-crossing intrusions with visual alarms.
 */
class VIDEOFILTERS_API PerimeterTripwireFilter : public IFrameProcessor {
public:
    enum class Direction {
        Bidirectional, ///< Crossings in either direction trigger alarm
        A_to_B, ///< Only crossings from A side to B side trigger alarm
        B_to_A ///< Only crossings from B side to A side trigger alarm
    };

    PerimeterTripwireFilter(double x1Norm = 0.1, double y1Norm = 0.5, double x2Norm = 0.9, double y2Norm = 0.5,
        Direction direction = Direction::Bidirectional);
    ~PerimeterTripwireFilter() override;

    PerimeterTripwireFilter(const PerimeterTripwireFilter&) = delete;
    PerimeterTripwireFilter& operator=(const PerimeterTripwireFilter&) = delete;
    PerimeterTripwireFilter(PerimeterTripwireFilter&&) noexcept;
    PerimeterTripwireFilter& operator=(PerimeterTripwireFilter&&) noexcept;

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setTripwire(double x1Norm, double y1Norm, double x2Norm, double y2Norm);
    void getTripwire(double& x1Norm, double& y1Norm, double& x2Norm, double& y2Norm) const;

    void setDirection(Direction dir)
    {
        m_direction = dir;
    }
    Direction getDirection() const
    {
        return m_direction;
    }

    bool hasAlarm() const;
    std::size_t getIntrusionCount() const;
    void resetIntrusionCount();

private:
    double m_x1Norm;
    double m_y1Norm;
    double m_x2Norm;
    double m_y2Norm;
    Direction m_direction;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @class MotionHeatmapFilter
 * @brief Temporal motion accumulation buffer revealing high-traffic paths and unauthorized loitering zones.
 */
class VIDEOFILTERS_API MotionHeatmapFilter : public IFrameProcessor {
public:
    MotionHeatmapFilter(double decayFactor = 0.95, double opacity = 0.40, int threshold = 20);
    ~MotionHeatmapFilter() override;

    MotionHeatmapFilter(const MotionHeatmapFilter&) = delete;
    MotionHeatmapFilter& operator=(const MotionHeatmapFilter&) = delete;
    MotionHeatmapFilter(MotionHeatmapFilter&&) noexcept;
    MotionHeatmapFilter& operator=(MotionHeatmapFilter&&) noexcept;

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setDecayFactor(double decay)
    {
        m_decayFactor = std::max(0.01, std::min(0.999, decay));
    }
    double getDecayFactor() const
    {
        return m_decayFactor;
    }

    void setOpacity(double opacity)
    {
        m_opacity = std::max(0.0, std::min(1.0, opacity));
    }
    double getOpacity() const
    {
        return m_opacity;
    }

    void setThreshold(int thresh)
    {
        m_threshold = std::max(1, std::min(255, thresh));
    }
    int getThreshold() const
    {
        return m_threshold;
    }

    void reset();

private:
    double m_decayFactor;
    double m_opacity;
    int m_threshold;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @class PrivacyMaskFilter
 * @brief Multi-zone static or dynamic geometric privacy masks (blackout, blur, mosaic) for regulatory GDPR compliance.
 */
class VIDEOFILTERS_API PrivacyMaskFilter : public IFrameProcessor {
public:
    enum class ConcealmentMode {
        Blackout, ///< Solid fill obscuration (default black)
        Blur, ///< Heavy Gaussian blur concealing identities/text while preserving ambient light
        Mosaic ///< Pixelated mosaic blocks
    };

    struct PrivacyZone {
        int id { 0 };
        double xNorm { 0.0 }; ///< Normalized left coordinate [0.0, 1.0]
        double yNorm { 0.0 }; ///< Normalized top coordinate [0.0, 1.0]
        double widthNorm { 0.0 }; ///< Normalized width [0.0, 1.0]
        double heightNorm { 0.0 }; ///< Normalized height [0.0, 1.0]
        ConcealmentMode mode { ConcealmentMode::Blackout };
        bool enabled { true };
        std::string label;
    };

    PrivacyMaskFilter(ConcealmentMode defaultMode = ConcealmentMode::Blackout);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    int addZone(double xNorm, double yNorm, double widthNorm, double heightNorm,
        ConcealmentMode mode = ConcealmentMode::Blackout, const std::string& label = "");
    int addZone(const PrivacyZone& zone);
    bool removeZone(int id);
    void clearZones();
    void setZoneEnabled(int id, bool enabled);
    std::vector<PrivacyZone> getZones() const;

    void setDefaultMode(ConcealmentMode mode)
    {
        m_defaultMode = mode;
    }
    ConcealmentMode getDefaultMode() const
    {
        return m_defaultMode;
    }

    void setMaskColor(uint8_t r, uint8_t g, uint8_t b);
    void setBlurKernelSize(int ksize)
    {
        m_blurKernelSize = std::max(3, ksize);
    }
    int getBlurKernelSize() const
    {
        return m_blurKernelSize;
    }
    void setMosaicBlockSize(int blockSize)
    {
        m_mosaicBlockSize = std::max(2, blockSize);
    }
    int getMosaicBlockSize() const
    {
        return m_mosaicBlockSize;
    }

private:
    mutable std::mutex m_mutex;
    std::vector<PrivacyZone> m_zones;
    int m_nextZoneId { 1 };
    ConcealmentMode m_defaultMode { ConcealmentMode::Blackout };
    uint8_t m_maskR { 0 };
    uint8_t m_maskG { 0 };
    uint8_t m_maskB { 0 };
    int m_blurKernelSize { 25 };
    int m_mosaicBlockSize { 16 };
};

/**
 * @class TimestampWatermarkFilter
 * @brief Real-time evidential OSD watermark (ISO 8601 timestamp, camera ID, GPS, frame sequence counter).
 */
class VIDEOFILTERS_API TimestampWatermarkFilter : public IFrameProcessor {
public:
    enum class Position { TopLeft, TopRight, BottomLeft, BottomRight };

    enum class Color { White, Amber, TacticalGreen, Cyan };

    TimestampWatermarkFilter(Position position = Position::TopLeft, const std::string& cameraName = "CAM-01",
        bool showTimestamp = true, bool showFrameCounter = true);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setPosition(Position pos)
    {
        m_position = pos;
    }
    Position getPosition() const
    {
        return m_position;
    }

    void setCameraName(const std::string& name);
    std::string getCameraName() const;

    void setShowTimestamp(bool show)
    {
        m_showTimestamp = show;
    }
    bool getShowTimestamp() const
    {
        return m_showTimestamp;
    }

    void setShowFrameCounter(bool show)
    {
        m_showFrameCounter = show;
    }
    bool getShowFrameCounter() const
    {
        return m_showFrameCounter;
    }

    void setGpsCoordinates(double latitude, double longitude, double altitudeMeters, bool enabled = true);
    void clearGpsCoordinates();

    void setCustomTimestamp(const std::string& isoString);
    void setUseSystemClock(bool useSystem);
    bool isUsingSystemClock() const;

    void setScrimOpacity(double opacity)
    {
        m_scrimOpacity = std::max(0.0, std::min(1.0, opacity));
    }
    double getScrimOpacity() const
    {
        return m_scrimOpacity;
    }

    void setColor(Color color)
    {
        m_color = color;
    }
    Color getColor() const
    {
        return m_color;
    }

    uint64_t getFrameCounter() const;
    void resetFrameCounter();

private:
    mutable std::mutex m_mutex;
    Position m_position;
    std::string m_cameraName;
    bool m_showTimestamp;
    bool m_showFrameCounter;
    bool m_showGps { false };
    double m_latitude { 0.0 };
    double m_longitude { 0.0 };
    double m_altitudeMeters { 0.0 };
    bool m_useSystemClock { true };
    std::string m_customTimestamp;
    double m_scrimOpacity { 0.65 };
    Color m_color { Color::White };
    uint64_t m_frameCounter { 0 };
};

/**
 * @class TelemetryOsdFilter
 * @brief Tactical operational OSD overlay rendering real-time pan/tilt angles, compass heading, FOV, and payload
 * telemetry.
 */
class VIDEOFILTERS_API TelemetryOsdFilter : public IFrameProcessor {
public:
    enum class Color { TacticalGreen, Amber, Cyan, White, Red };

    struct TelemetryData {
        double panDegrees { 0.0 }; ///< 0.0 to 360.0 degrees
        double tiltDegrees { 0.0 }; ///< -90.0 (nadir) to +90.0 (zenith)
        double zoomMagnification { 1.0 }; ///< Optical zoom multiplier (e.g. 1.0x to 40.0x)
        double horizontalFovDegrees { 60.0 }; ///< Horizontal field of view in degrees
        std::string sensorPayload { "OPTICAL HD" };
        std::string statusMessage { "LINK: OK" };
    };

    TelemetryOsdFilter(Color color = Color::TacticalGreen, bool showCompass = true, bool showReticleAngles = true);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setTelemetry(const TelemetryData& data);
    TelemetryData getTelemetry() const;

    void setPanTiltZoom(double panDegrees, double tiltDegrees, double zoomMagnification);

    void setColor(Color color)
    {
        m_color = color;
    }
    Color getColor() const
    {
        return m_color;
    }

    void setShowCompass(bool show)
    {
        m_showCompass = show;
    }
    bool getShowCompass() const
    {
        return m_showCompass;
    }

    void setShowReticleAngles(bool show)
    {
        m_showReticleAngles = show;
    }
    bool getShowReticleAngles() const
    {
        return m_showReticleAngles;
    }

    static std::string formatHeading(double azimuthDegrees);

private:
    mutable std::mutex m_mutex;
    Color m_color;
    bool m_showCompass;
    bool m_showReticleAngles;
    TelemetryData m_telemetry;
};

/**
 * @class PictureInPictureFilter
 * @brief Overlays a secondary video feed, sensor stream, or center-bore electronic zoom inset onto the main viewport.
 */
class VIDEOFILTERS_API PictureInPictureFilter : public IFrameProcessor {
public:
    enum class Mode {
        DigitalZoom, ///< Electronic center-bore crop and magnification
        SecondaryFeed ///< External secondary stream / sensor frame buffer
    };

    enum class Corner { TopRight, TopLeft, BottomRight, BottomLeft };

    PictureInPictureFilter(Mode mode = Mode::DigitalZoom, Corner corner = Corner::TopRight, double scaleRatio = 0.28,
        double digitalZoomFactor = 2.0);

    void process(uint8_t* data, int width, int height, PixelFormat format) override;

    void setMode(Mode mode)
    {
        m_mode = mode;
    }
    Mode getMode() const
    {
        return m_mode;
    }

    void setCorner(Corner corner)
    {
        m_corner = corner;
    }
    Corner getCorner() const
    {
        return m_corner;
    }

    void setScaleRatio(double ratio)
    {
        m_scaleRatio = std::max(0.10, std::min(0.60, ratio));
    }
    double getScaleRatio() const
    {
        return m_scaleRatio;
    }

    void setDigitalZoomFactor(double factor)
    {
        m_digitalZoomFactor = std::max(1.1, std::min(10.0, factor));
    }
    double getDigitalZoomFactor() const
    {
        return m_digitalZoomFactor;
    }

    void setSecondaryFrame(const uint8_t* data, int width, int height, PixelFormat format);
    void clearSecondaryFrame();

    void setBorder(bool showBorder, uint8_t r = 0, uint8_t g = 255, uint8_t b = 64, int thickness = 2);
    void setShowBadge(bool show)
    {
        m_showBadge = show;
    }
    bool getShowBadge() const
    {
        return m_showBadge;
    }

private:
    mutable std::mutex m_mutex;
    Mode m_mode;
    Corner m_corner;
    double m_scaleRatio;
    double m_digitalZoomFactor;
    bool m_showBorder { true };
    uint8_t m_borderR { 0 };
    uint8_t m_borderG { 255 };
    uint8_t m_borderB { 64 };
    int m_borderThickness { 2 };
    bool m_showBadge { true };

    std::vector<uint8_t> m_secondaryBuffer;
    int m_secondaryWidth { 0 };
    int m_secondaryHeight { 0 };
    PixelFormat m_secondaryFormat { PixelFormat::RGB24 };
};

} // namespace PelcoD::Video

#endif // PELCOD_HAS_FILTERS

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
