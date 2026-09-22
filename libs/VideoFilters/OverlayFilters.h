/**
 * @file OverlayFilters.h
 * @brief Video filters for tactical HUD reticles, text overlays, privacy masking, timestamp watermarks, and telemetry OSD.
 */

#pragma once

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

#include "DecoderTypes.h"
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#ifndef VIDEOFILTERS_API
#define VIDEOFILTERS_API
#endif

#if defined(PELCOD_HAS_FILTERS)

namespace Video::Filters {

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

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

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

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

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

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

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

    void setMaskColor(std::uint8_t r, std::uint8_t g, std::uint8_t b);
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
    std::uint8_t m_maskR { 0 };
    std::uint8_t m_maskG { 0 };
    std::uint8_t m_maskB { 0 };
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

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

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

    std::uint64_t getFrameCounter() const;
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
    std::uint64_t m_frameCounter { 0 };
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

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

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

} // namespace Video::Filters

namespace Video {
using Filters::PrivacyMaskFilter;
using Filters::TacticalReticleOverlayFilter;
using Filters::TelemetryOsdFilter;
using Filters::TextOverlayFilter;
using Filters::TimestampWatermarkFilter;
} // namespace Video

#endif // PELCOD_HAS_FILTERS

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
