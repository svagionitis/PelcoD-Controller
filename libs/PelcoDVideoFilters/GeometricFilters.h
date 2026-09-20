/**
 * @file GeometricFilters.h
 * @brief Video filters for geometric spatial transforms, mirroring, mosaic pixelation, ROI processing, electronic image stabilization, and picture-in-picture.
 */

#pragma once

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

#include "DecoderTypes.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#ifndef VIDEOFILTERS_API
#define VIDEOFILTERS_API
#endif

#if defined(PELCOD_HAS_FILTERS)

namespace PelcoD::Video::Filters {

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

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

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

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

private:
    int m_blockSize;
};

/**
 * @class LocalAreaProcessingFilter
 * @brief Sightline-inspired Local Area Processing (LAP) dynamic range contrast enhancement.
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

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

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

    ImageStabilizationFilter(const ImageStabilizationFilter&) = delete;
    ImageStabilizationFilter& operator=(const ImageStabilizationFilter&) = delete;
    ImageStabilizationFilter(ImageStabilizationFilter&&) noexcept;
    ImageStabilizationFilter& operator=(ImageStabilizationFilter&&) noexcept;

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

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

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

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

    void setSecondaryFrame(const std::uint8_t* data, int width, int height, PixelFormat format);
    void clearSecondaryFrame();

    void setBorder(bool showBorder, std::uint8_t r = 0, std::uint8_t g = 255, std::uint8_t b = 64, int thickness = 2);
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
    std::uint8_t m_borderR { 0 };
    std::uint8_t m_borderG { 255 };
    std::uint8_t m_borderB { 64 };
    int m_borderThickness { 2 };
    bool m_showBadge { true };

    std::vector<std::uint8_t> m_secondaryBuffer;
    int m_secondaryWidth { 0 };
    int m_secondaryHeight { 0 };
    PixelFormat m_secondaryFormat { PixelFormat::RGB24 };
};

} // namespace PelcoD::Video::Filters

namespace PelcoD::Video {
using Filters::ImageStabilizationFilter;
using Filters::LocalAreaProcessingFilter;
using Filters::MirrorFilter;
using Filters::MosaicFilter;
using Filters::PictureInPictureFilter;
} // namespace PelcoD::Video

#endif // PELCOD_HAS_FILTERS

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
