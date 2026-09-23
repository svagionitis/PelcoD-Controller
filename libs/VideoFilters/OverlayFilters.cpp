/**
 * @file OverlayFilters.cpp
 * @brief Implementations of text overlays, tactical reticles, privacy masks, watermarks, and telemetry OSD.
 */

#include "OverlayFilters.h"

#if defined(PELCOD_HAS_FILTERS)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <opencv2/opencv.hpp>
#include <sstream>

namespace Video::Filters {

// --- TextOverlayFilter ---
TextOverlayFilter::TextOverlayFilter(const std::string& text, int x, int y, double scale)
    : m_text(text)
    , m_x(x)
    , m_y(y)
    , m_scale(scale)
{
}

void TextOverlayFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }
    cv::Mat mat(height, width, CV_8UC3, data);
    cv::Scalar color = (format == PixelFormat::BGR24) ? cv::Scalar(0, 255, 255) : cv::Scalar(255, 255, 0); // Yellow

    cv::putText(mat, m_text, cv::Point(m_x, m_y), cv::FONT_HERSHEY_SIMPLEX, m_scale, color, 2);
}

// -----------------------------------------------------------------------------
// TacticalReticleOverlayFilter Implementation
// -----------------------------------------------------------------------------
TacticalReticleOverlayFilter::TacticalReticleOverlayFilter(Style style, Color color, int lineThickness, int deadbandGap)
    : m_style(style)
    , m_color(color)
    , m_lineThickness(std::max(1, lineThickness))
    , m_deadbandGap(std::max(2, deadbandGap))
{
}

void TacticalReticleOverlayFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0 || (format != PixelFormat::RGB24 && format != PixelFormat::BGR24)) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);

    cv::Scalar cvColor;
    switch (m_color) {
    case Color::Red:
        cvColor = (format == PixelFormat::RGB24) ? cv::Scalar(255, 48, 48) : cv::Scalar(48, 48, 255);
        break;
    case Color::Amber:
        cvColor = (format == PixelFormat::RGB24) ? cv::Scalar(255, 191, 0) : cv::Scalar(0, 191, 255);
        break;
    case Color::White:
        cvColor = cv::Scalar(255, 255, 255);
        break;
    case Color::Cyan:
        cvColor = (format == PixelFormat::RGB24) ? cv::Scalar(0, 255, 255) : cv::Scalar(255, 255, 0);
        break;
    case Color::TacticalGreen:
    default:
        cvColor = (format == PixelFormat::RGB24) ? cv::Scalar(0, 255, 64) : cv::Scalar(64, 255, 0);
        break;
    }

    const int cx = width / 2;
    const int cy = height / 2;
    const int gap = m_deadbandGap;
    const int thick = m_lineThickness;

    if (m_style == Style::Crosshair) {
        const int armLen = std::min(width, height) / 4;
        cv::line(mat, cv::Point(cx - armLen, cy), cv::Point(cx - gap, cy), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx + gap, cy), cv::Point(cx + armLen, cy), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx, cy - armLen), cv::Point(cx, cy - gap), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx, cy + gap), cv::Point(cx, cy + armLen), cvColor, thick, cv::LINE_AA);
        cv::circle(mat, cv::Point(cx, cy), gap / 2, cvColor, thick, cv::LINE_AA);
    } else if (m_style == Style::MilDot) {
        const int armLen = std::min(width, height) / 3;
        cv::line(mat, cv::Point(cx - armLen, cy), cv::Point(cx - gap, cy), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx + gap, cy), cv::Point(cx + armLen, cy), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx, cy - armLen), cv::Point(cx, cy - gap), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx, cy + gap), cv::Point(cx, cy + armLen), cvColor, thick, cv::LINE_AA);

        const int milSpacing = 20;
        for (int d = gap + milSpacing; d <= armLen; d += milSpacing) {
            cv::circle(mat, cv::Point(cx + d, cy), 2, cvColor, -1, cv::LINE_AA);
            cv::circle(mat, cv::Point(cx - d, cy), 2, cvColor, -1, cv::LINE_AA);
            cv::circle(mat, cv::Point(cx, cy + d), 2, cvColor, -1, cv::LINE_AA);
            cv::circle(mat, cv::Point(cx, cy - d), 2, cvColor, -1, cv::LINE_AA);
        }
        cv::circle(mat, cv::Point(cx, cy), 2, cvColor, -1, cv::LINE_AA);
    } else if (m_style == Style::Stadiametric) {
        const int armLen = std::min(width, height) / 3;
        cv::line(mat, cv::Point(cx - armLen, cy), cv::Point(cx + armLen, cy), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx, cy - armLen), cv::Point(cx, cy + armLen), cvColor, thick, cv::LINE_AA);

        const int tickSpacing = 16;
        for (int i = 1; i <= 6; ++i) {
            const int y = cy + i * tickSpacing;
            const int tickW = 6 + i * 4;
            cv::line(mat, cv::Point(cx - tickW, y), cv::Point(cx + tickW, y), cvColor, thick, cv::LINE_AA);
        }
    } else if (m_style == Style::CornerBrackets) {
        const int margin = 20;
        const int bracketLen = 30;
        cv::line(mat, cv::Point(margin, margin), cv::Point(margin + bracketLen, margin), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(margin, margin), cv::Point(margin, margin + bracketLen), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(width - margin, margin), cv::Point(width - margin - bracketLen, margin), cvColor, thick,
            cv::LINE_AA);
        cv::line(mat, cv::Point(width - margin, margin), cv::Point(width - margin, margin + bracketLen), cvColor, thick,
            cv::LINE_AA);
        cv::line(mat, cv::Point(margin, height - margin), cv::Point(margin + bracketLen, height - margin), cvColor,
            thick, cv::LINE_AA);
        cv::line(mat, cv::Point(margin, height - margin), cv::Point(margin, height - margin - bracketLen), cvColor,
            thick, cv::LINE_AA);
        cv::line(mat, cv::Point(width - margin, height - margin),
            cv::Point(width - margin - bracketLen, height - margin), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(width - margin, height - margin),
            cv::Point(width - margin, height - margin - bracketLen), cvColor, thick, cv::LINE_AA);

        cv::line(mat, cv::Point(cx - 8, cy), cv::Point(cx + 8, cy), cvColor, thick, cv::LINE_AA);
        cv::line(mat, cv::Point(cx, cy - 8), cv::Point(cx, cy + 8), cvColor, thick, cv::LINE_AA);
    }
}

// --- PrivacyMaskFilter ---
PrivacyMaskFilter::PrivacyMaskFilter(ConcealmentMode defaultMode)
    : m_defaultMode(defaultMode)
{
}

void PrivacyMaskFilter::setMaskColor(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    std::scoped_lock lock(m_mutex);
    m_maskR = r;
    m_maskG = g;
    m_maskB = b;
}

int PrivacyMaskFilter::addZone(
    double xNorm, double yNorm, double widthNorm, double heightNorm, ConcealmentMode mode, const std::string& label)
{
    std::scoped_lock lock(m_mutex);
    PrivacyZone zone;
    zone.id = m_nextZoneId++;
    zone.xNorm = std::max(0.0, std::min(1.0, xNorm));
    zone.yNorm = std::max(0.0, std::min(1.0, yNorm));
    zone.widthNorm = std::max(0.0, std::min(1.0 - zone.xNorm, widthNorm));
    zone.heightNorm = std::max(0.0, std::min(1.0 - zone.yNorm, heightNorm));
    zone.mode = mode;
    zone.enabled = true;
    zone.label = label;
    m_zones.push_back(zone);
    return zone.id;
}

int PrivacyMaskFilter::addZone(const PrivacyZone& zone)
{
    std::scoped_lock lock(m_mutex);
    PrivacyZone z = zone;
    if (z.id <= 0) {
        z.id = m_nextZoneId++;
    } else {
        m_nextZoneId = std::max(m_nextZoneId, z.id + 1);
    }
    z.xNorm = std::max(0.0, std::min(1.0, z.xNorm));
    z.yNorm = std::max(0.0, std::min(1.0, z.yNorm));
    z.widthNorm = std::max(0.0, std::min(1.0 - z.xNorm, z.widthNorm));
    z.heightNorm = std::max(0.0, std::min(1.0 - z.yNorm, z.heightNorm));
    m_zones.push_back(z);
    return z.id;
}

bool PrivacyMaskFilter::removeZone(int id)
{
    std::scoped_lock lock(m_mutex);
    auto it = std::remove_if(m_zones.begin(), m_zones.end(), [id](const PrivacyZone& z) { return z.id == id; });
    if (it != m_zones.end()) {
        m_zones.erase(it, m_zones.end());
        return true;
    }
    return false;
}

void PrivacyMaskFilter::clearZones()
{
    std::scoped_lock lock(m_mutex);
    m_zones.clear();
}

void PrivacyMaskFilter::setZoneEnabled(int id, bool enabled)
{
    std::scoped_lock lock(m_mutex);
    for (auto& z : m_zones) {
        if (z.id == id) {
            z.enabled = enabled;
            break;
        }
    }
}

std::vector<PrivacyMaskFilter::PrivacyZone> PrivacyMaskFilter::getZones() const
{
    std::scoped_lock lock(m_mutex);
    return m_zones;
}

void PrivacyMaskFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }

    std::vector<PrivacyZone> activeZones;
    std::uint8_t mr = 0;
    std::uint8_t mg = 0;
    std::uint8_t mb = 0;
    int blurK = 25;
    int mosaicBlock = 16;
    {
        std::scoped_lock lock(m_mutex);
        if (m_zones.empty()) {
            return;
        }
        activeZones = m_zones;
        mr = m_maskR;
        mg = m_maskG;
        mb = m_maskB;
        blurK = m_blurKernelSize;
        mosaicBlock = m_mosaicBlockSize;
    }

    cv::Mat mat(height, width, CV_8UC3, data);

    for (const auto& zone : activeZones) {
        if (!zone.enabled) {
            continue;
        }

        int rx = std::clamp(static_cast<int>(std::round(zone.xNorm * static_cast<double>(width))), 0, width - 1);
        int ry = std::clamp(static_cast<int>(std::round(zone.yNorm * static_cast<double>(height))), 0, height - 1);
        int rw = std::clamp(static_cast<int>(std::round(zone.widthNorm * static_cast<double>(width))), 0, width - rx);
        int rh
            = std::clamp(static_cast<int>(std::round(zone.heightNorm * static_cast<double>(height))), 0, height - ry);

        if (rw <= 0 || rh <= 0) {
            continue;
        }

        cv::Rect roi(rx, ry, rw, rh);
        cv::Mat roiMat = mat(roi);

        switch (zone.mode) {
        case ConcealmentMode::Blackout: {
            cv::Scalar color = (format == PixelFormat::BGR24) ? cv::Scalar(mb, mg, mr) : cv::Scalar(mr, mg, mb);
            roiMat.setTo(color);
            break;
        }
        case ConcealmentMode::Blur: {
            int k = std::max(3, blurK | 1); // Ensure odd
            cv::GaussianBlur(roiMat, roiMat, cv::Size(k, k), 0);
            break;
        }
        case ConcealmentMode::Mosaic: {
            int bs = std::max(2, mosaicBlock);
            int smallW = std::max(1, rw / bs);
            int smallH = std::max(1, rh / bs);
            cv::Mat smallMat;
            cv::resize(roiMat, smallMat, cv::Size(smallW, smallH), 0, 0, cv::INTER_NEAREST);
            cv::resize(smallMat, roiMat, roiMat.size(), 0, 0, cv::INTER_NEAREST);
            break;
        }
        }
    }
}

// --- TimestampWatermarkFilter ---
TimestampWatermarkFilter::TimestampWatermarkFilter(
    Position position, const std::string& cameraName, bool showTimestamp, bool showFrameCounter)
    : m_position(position)
    , m_cameraName(cameraName)
    , m_showTimestamp(showTimestamp)
    , m_showFrameCounter(showFrameCounter)
{
}

void TimestampWatermarkFilter::setCameraName(const std::string& name)
{
    std::scoped_lock lock(m_mutex);
    m_cameraName = name;
}

std::string TimestampWatermarkFilter::getCameraName() const
{
    std::scoped_lock lock(m_mutex);
    return m_cameraName;
}

void TimestampWatermarkFilter::setGpsCoordinates(double latitude, double longitude, double altitudeMeters, bool enabled)
{
    std::scoped_lock lock(m_mutex);
    m_latitude = latitude;
    m_longitude = longitude;
    m_altitudeMeters = altitudeMeters;
    m_showGps = enabled;
}

void TimestampWatermarkFilter::clearGpsCoordinates()
{
    std::scoped_lock lock(m_mutex);
    m_showGps = false;
}

void TimestampWatermarkFilter::setCustomTimestamp(const std::string& isoString)
{
    std::scoped_lock lock(m_mutex);
    m_customTimestamp = isoString;
}

void TimestampWatermarkFilter::setUseSystemClock(bool useSystem)
{
    std::scoped_lock lock(m_mutex);
    m_useSystemClock = useSystem;
}

bool TimestampWatermarkFilter::isUsingSystemClock() const
{
    std::scoped_lock lock(m_mutex);
    return m_useSystemClock;
}

std::uint64_t TimestampWatermarkFilter::getFrameCounter() const
{
    std::scoped_lock lock(m_mutex);
    return m_frameCounter;
}

void TimestampWatermarkFilter::resetFrameCounter()
{
    std::scoped_lock lock(m_mutex);
    m_frameCounter = 0;
}

void TimestampWatermarkFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }

    Position pos = Position::TopLeft;
    std::string camName;
    bool showTime = false;
    bool showSeq = false;
    bool showGps = false;
    double lat = 0.0;
    double lon = 0.0;
    double alt = 0.0;
    bool useSys = true;
    std::string customTime;
    double scrimAlpha = 0.65;
    Color colorMode = Color::White;
    std::uint64_t frameNum = 0;

    {
        std::scoped_lock lock(m_mutex);
        m_frameCounter++;
        frameNum = m_frameCounter;
        pos = m_position;
        camName = m_cameraName;
        showTime = m_showTimestamp;
        showSeq = m_showFrameCounter;
        showGps = m_showGps;
        lat = m_latitude;
        lon = m_longitude;
        alt = m_altitudeMeters;
        useSys = m_useSystemClock;
        customTime = m_customTimestamp;
        scrimAlpha = m_scrimOpacity;
        colorMode = m_color;
    }

    // Build line 1
    std::string line1;
    if (!camName.empty()) {
        line1 += "[" + camName + "] ";
    }
    if (showTime) {
        if (!useSys && !customTime.empty()) {
            line1 += customTime + " ";
        } else {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm tmBuf {};
#if defined(_WIN32)
            gmtime_s(&tmBuf, &t);
#else
            gmtime_r(&t, &tmBuf);
#endif
            char timeBuf[64];
            std::snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d.%03d UTC ", tmBuf.tm_year + 1900,
                tmBuf.tm_mon + 1, tmBuf.tm_mday, tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec,
                static_cast<int>(ms.count()));
            line1 += timeBuf;
        }
    }
    if (showSeq) {
        char seqBuf[32];
        std::snprintf(seqBuf, sizeof(seqBuf), "#%06llu", static_cast<unsigned long long>(frameNum));
        line1 += seqBuf;
    }

    // Build line 2 (GPS)
    std::string line2;
    if (showGps) {
        char gpsBuf[128];
        std::snprintf(gpsBuf, sizeof(gpsBuf), "GPS: %.4f%c %.4f%c ALT: %.1fm", std::abs(lat), (lat >= 0.0 ? 'N' : 'S'),
            std::abs(lon), (lon >= 0.0 ? 'E' : 'W'), alt);
        line2 = gpsBuf;
    }

    if (line1.empty() && line2.empty()) {
        return;
    }

    cv::Mat mat(height, width, CV_8UC3, data);

    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = 0.42;
    int thickness = 1;
    int baseline1 = 0;
    int baseline2 = 0;
    cv::Size sz1 = !line1.empty() ? cv::getTextSize(line1, fontFace, fontScale, thickness, &baseline1) : cv::Size(0, 0);
    cv::Size sz2 = !line2.empty() ? cv::getTextSize(line2, fontFace, fontScale, thickness, &baseline2) : cv::Size(0, 0);

    int contentW = std::max(sz1.width, sz2.width);
    int lineH = std::max(sz1.height, sz2.height);
    int numLines = (line1.empty() ? 0 : 1) + (line2.empty() ? 0 : 1);
    int padX = 8;
    int padY = 5;
    int boxW = contentW + padX * 2;
    int boxH = numLines * lineH + (numLines > 1 ? 6 : 0) + padY * 2;

    int margin = 8;
    int boxX = margin;
    int boxY = margin;

    switch (pos) {
    case Position::TopLeft:
        boxX = margin;
        boxY = margin;
        break;
    case Position::TopRight:
        boxX = width - boxW - margin;
        boxY = margin;
        break;
    case Position::BottomLeft:
        boxX = margin;
        boxY = height - boxH - margin;
        break;
    case Position::BottomRight:
        boxX = width - boxW - margin;
        boxY = height - boxH - margin;
        break;
    }

    boxX = std::clamp(boxX, 0, std::max(0, width - boxW));
    boxY = std::clamp(boxY, 0, std::max(0, height - boxH));
    boxW = std::min(boxW, width - boxX);
    boxH = std::min(boxH, height - boxY);

    if (boxW <= 4 || boxH <= 4) {
        return;
    }

    // Scrim background
    if (scrimAlpha > 0.0) {
        cv::Rect scrimRect(boxX, boxY, boxW, boxH);
        cv::Mat roi = mat(scrimRect);
        cv::Mat scrimColor(roi.size(), roi.type(), cv::Scalar(18, 18, 18));
        cv::addWeighted(scrimColor, scrimAlpha, roi, 1.0 - scrimAlpha, 0.0, roi);
    }

    // Text color
    cv::Scalar textColor;
    switch (colorMode) {
    case Color::Amber:
        textColor = (format == PixelFormat::BGR24) ? cv::Scalar(0, 191, 255) : cv::Scalar(255, 191, 0);
        break;
    case Color::TacticalGreen:
        textColor = (format == PixelFormat::BGR24) ? cv::Scalar(64, 255, 0) : cv::Scalar(0, 255, 64);
        break;
    case Color::Cyan:
        textColor = (format == PixelFormat::BGR24) ? cv::Scalar(255, 255, 0) : cv::Scalar(0, 255, 255);
        break;
    case Color::White:
    default:
        textColor = cv::Scalar(255, 255, 255);
        break;
    }

    int curY = boxY + padY + lineH;
    if (!line1.empty()) {
        cv::putText(mat, line1, cv::Point(boxX + padX, curY), fontFace, fontScale, textColor, thickness, cv::LINE_AA);
        curY += lineH + 6;
    }
    if (!line2.empty()) {
        cv::putText(mat, line2, cv::Point(boxX + padX, curY), fontFace, fontScale, textColor, thickness, cv::LINE_AA);
    }
}

// --- TelemetryOsdFilter ---
TelemetryOsdFilter::TelemetryOsdFilter(Color color, bool showCompass, bool showReticleAngles)
    : m_color(color)
    , m_showCompass(showCompass)
    , m_showReticleAngles(showReticleAngles)
{
}

void TelemetryOsdFilter::setTelemetry(const TelemetryData& data)
{
    std::scoped_lock lock(m_mutex);
    m_telemetry = data;
}

TelemetryOsdFilter::TelemetryData TelemetryOsdFilter::getTelemetry() const
{
    std::scoped_lock lock(m_mutex);
    return m_telemetry;
}

void TelemetryOsdFilter::setPanTiltZoom(double panDegrees, double tiltDegrees, double zoomMagnification)
{
    std::scoped_lock lock(m_mutex);
    m_telemetry.panDegrees = panDegrees;
    m_telemetry.tiltDegrees = tiltDegrees;
    m_telemetry.zoomMagnification = zoomMagnification;
}

std::string TelemetryOsdFilter::formatHeading(double azimuthDegrees)
{
    double az = std::fmod(azimuthDegrees, 360.0);
    if (az < 0.0) {
        az += 360.0;
    }
    static const char* const CARDINALS[16]
        = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
    int idx = static_cast<int>(std::round(az / 22.5)) % 16;
    return CARDINALS[idx];
}

void TelemetryOsdFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }

    TelemetryData telem;
    Color col = Color::TacticalGreen;
    bool showC = false;
    bool showRA = false;

    {
        std::scoped_lock lock(m_mutex);
        telem = m_telemetry;
        col = m_color;
        showC = m_showCompass;
        showRA = m_showReticleAngles;
    }

    cv::Mat mat(height, width, CV_8UC3, data);

    cv::Scalar drawColor;
    switch (col) {
    case Color::Amber:
        drawColor = (format == PixelFormat::BGR24) ? cv::Scalar(0, 191, 255) : cv::Scalar(255, 191, 0);
        break;
    case Color::Cyan:
        drawColor = (format == PixelFormat::BGR24) ? cv::Scalar(255, 255, 0) : cv::Scalar(0, 255, 255);
        break;
    case Color::White:
        drawColor = cv::Scalar(255, 255, 255);
        break;
    case Color::Red:
        drawColor = (format == PixelFormat::BGR24) ? cv::Scalar(0, 0, 255) : cv::Scalar(255, 0, 0);
        break;
    case Color::TacticalGreen:
    default:
        drawColor = (format == PixelFormat::BGR24) ? cv::Scalar(64, 255, 0) : cv::Scalar(0, 255, 64);
        break;
    }

    int fontFace = cv::FONT_HERSHEY_PLAIN;
    double fontScale = 0.9;
    int thickness = 1;

    // 1. Compass banner at top center
    if (showC && width >= 160) {
        std::string card = formatHeading(telem.panDegrees);
        char compassBuf[64];
        std::snprintf(compassBuf, sizeof(compassBuf), "-|  %05.1f DEG [%s]  |-", telem.panDegrees, card.c_str());
        int base = 0;
        cv::Size sz = cv::getTextSize(compassBuf, fontFace, fontScale, thickness, &base);
        int cx = (width - sz.width) / 2;
        int cy = 20;

        // Subtle backing rect
        cv::Rect bannerRect(std::max(0, cx - 6), 6, sz.width + 12, sz.height + 8);
        if (bannerRect.x + bannerRect.width <= width && bannerRect.y + bannerRect.height <= height) {
            cv::Mat bannerRoi = mat(bannerRect);
            cv::Mat bg(bannerRoi.size(), bannerRoi.type(), cv::Scalar(15, 15, 15));
            cv::addWeighted(bg, 0.6, bannerRoi, 0.4, 0.0, bannerRoi);
        }
        cv::putText(mat, compassBuf, cv::Point(cx, cy), fontFace, fontScale, drawColor, thickness, cv::LINE_AA);
    }

    // 2. Reticle / PTZ telemetry angles (bottom-left)
    if (showRA && height >= 80) {
        char azElBuf[64];
        std::snprintf(azElBuf, sizeof(azElBuf), "AZ: %05.1f DEG  EL: %+05.1f DEG", telem.panDegrees, telem.tiltDegrees);
        char zoomBuf[64];
        std::snprintf(zoomBuf, sizeof(zoomBuf), "ZOOM: %.1fx  HFOV: %.1f DEG", telem.zoomMagnification,
            telem.horizontalFovDegrees);

        int leftX = 14;
        int botY = height - 26;
        cv::putText(mat, azElBuf, cv::Point(leftX, botY), fontFace, fontScale, drawColor, thickness, cv::LINE_AA);
        cv::putText(mat, zoomBuf, cv::Point(leftX, botY + 14), fontFace, fontScale, drawColor, thickness, cv::LINE_AA);

        // Payload & Status (bottom-right)
        if (!telem.sensorPayload.empty() || !telem.statusMessage.empty()) {
            std::string rightStr = telem.sensorPayload + " | " + telem.statusMessage;
            int baseR = 0;
            cv::Size rSz = cv::getTextSize(rightStr, fontFace, fontScale, thickness, &baseR);
            int rx = std::max(14, width - rSz.width - 14);
            cv::putText(
                mat, rightStr, cv::Point(rx, botY + 14), fontFace, fontScale, drawColor, thickness, cv::LINE_AA);
        }
    }
}

} // namespace Video::Filters

#endif // PELCOD_HAS_FILTERS
