/**
 * @file StreamHealthOsdFilter.cpp
 * @brief Implementation of tactical live stream status designator and exclusion zone OSD overlay.
 */

#include "StreamHealthOsdFilter.h"

#if defined(PELCOD_HAS_FILTERS)

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <opencv2/opencv.hpp>

namespace Video::Filters {

namespace {

    cv::Scalar makeColor(std::uint8_t r, std::uint8_t g, std::uint8_t b, PixelFormat format) noexcept
    {
        if (format == PixelFormat::BGR24) {
            return cv::Scalar(b, g, r);
        }
        return cv::Scalar(r, g, b);
    }

    std::string formatSeconds(double seconds)
    {
        char buf[32];
        if (seconds < 60.0) {
            std::snprintf(buf, sizeof(buf), "%.1fs", seconds);
        } else {
            const int mins = static_cast<int>(seconds) / 60;
            const int secs = static_cast<int>(seconds) % 60;
            std::snprintf(buf, sizeof(buf), "%dm%02ds", mins, secs);
        }
        return std::string(buf);
    }

} // namespace

StreamHealthOsdFilter::StreamHealthOsdFilter(Position position, Style style)
    : m_position(position)
    , m_style(style)
{
}

void StreamHealthOsdFilter::bindMonitor(std::shared_ptr<const StreamHealthMonitor> monitor)
{
    std::scoped_lock lock(m_mutex);
    m_boundMonitor = monitor;
}

void StreamHealthOsdFilter::unbindMonitor()
{
    std::scoped_lock lock(m_mutex);
    m_boundMonitor.reset();
}

void StreamHealthOsdFilter::updateHealth(StreamHealthState state, const StreamHealthMetrics& metrics)
{
    std::scoped_lock lock(m_mutex);
    m_state = state;
    m_metrics = metrics;
}

void StreamHealthOsdFilter::setPosition(Position position)
{
    std::scoped_lock lock(m_mutex);
    m_position = position;
}

StreamHealthOsdFilter::Position StreamHealthOsdFilter::getPosition() const
{
    std::scoped_lock lock(m_mutex);
    return m_position;
}

void StreamHealthOsdFilter::setStyle(Style style)
{
    std::scoped_lock lock(m_mutex);
    m_style = style;
}

StreamHealthOsdFilter::Style StreamHealthOsdFilter::getStyle() const
{
    std::scoped_lock lock(m_mutex);
    return m_style;
}

void StreamHealthOsdFilter::setCustomLabel(const std::string& label)
{
    std::scoped_lock lock(m_mutex);
    m_customLabel = label;
}

std::string StreamHealthOsdFilter::getCustomLabel() const
{
    std::scoped_lock lock(m_mutex);
    return m_customLabel;
}

void StreamHealthOsdFilter::setShowFps(bool show)
{
    std::scoped_lock lock(m_mutex);
    m_showFps = show;
}

bool StreamHealthOsdFilter::getShowFps() const
{
    std::scoped_lock lock(m_mutex);
    return m_showFps;
}

void StreamHealthOsdFilter::setShowFreezeTimer(bool show)
{
    std::scoped_lock lock(m_mutex);
    m_showFreezeTimer = show;
}

bool StreamHealthOsdFilter::getShowFreezeTimer() const
{
    std::scoped_lock lock(m_mutex);
    return m_showFreezeTimer;
}

void StreamHealthOsdFilter::setShowExclusionZones(bool show)
{
    std::scoped_lock lock(m_mutex);
    m_showExclusionZones = show;
}

bool StreamHealthOsdFilter::getShowExclusionZones() const
{
    std::scoped_lock lock(m_mutex);
    return m_showExclusionZones;
}

void StreamHealthOsdFilter::setPulseAnimation(bool enable)
{
    std::scoped_lock lock(m_mutex);
    m_pulseAnimation = enable;
}

bool StreamHealthOsdFilter::isPulseAnimationEnabled() const
{
    std::scoped_lock lock(m_mutex);
    return m_pulseAnimation;
}

void StreamHealthOsdFilter::setScrimOpacity(double opacity)
{
    std::scoped_lock lock(m_mutex);
    m_scrimOpacity = std::max(0.0, std::min(1.0, opacity));
}

double StreamHealthOsdFilter::getScrimOpacity() const
{
    std::scoped_lock lock(m_mutex);
    return m_scrimOpacity;
}

StreamHealthState StreamHealthOsdFilter::getHealthState() const
{
    std::scoped_lock lock(m_mutex);
    if (auto mon = m_boundMonitor.lock()) {
        return mon->getState();
    }
    return m_state;
}

StreamHealthMetrics StreamHealthOsdFilter::getHealthMetrics() const
{
    std::scoped_lock lock(m_mutex);
    if (auto mon = m_boundMonitor.lock()) {
        return mon->getMetrics();
    }
    return m_metrics;
}

void StreamHealthOsdFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (data == nullptr || width < 48 || height < 32
        || (format != PixelFormat::RGB24 && format != PixelFormat::BGR24)) {
        return;
    }

    StreamHealthState state = StreamHealthState::Healthy;
    StreamHealthMetrics metrics {};
    Position pos = Position::TopRight;
    Style style = Style::TacticalPill;
    std::string label {};
    bool showFps = true;
    bool showFreeze = true;
    bool showZones = false;
    bool pulse = true;
    double opacity = 0.65;
    std::vector<ExclusionZone> zones {};

    {
        std::scoped_lock lock(m_mutex);
        if (auto mon = m_boundMonitor.lock()) {
            state = mon->getState();
            metrics = mon->getMetrics();
            zones = mon->getExclusionZones();
        } else {
            state = m_state;
            metrics = m_metrics;
        }
        pos = m_position;
        style = m_style;
        label = m_customLabel;
        showFps = m_showFps;
        showFreeze = m_showFreezeTimer;
        showZones = m_showExclusionZones;
        pulse = m_pulseAnimation;
        opacity = m_scrimOpacity;
    }

    cv::Mat mat(height, width, CV_8UC3, data);

    // Compute animation pulse factor
    double pulseFactor = 1.0;
    if (pulse) {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

        switch (state) {
        case StreamHealthState::Healthy:
            // Smooth sinusoidal breathing (period ~1.0 s)
            pulseFactor = 0.65 + 0.35 * std::sin(static_cast<double>(ms) * 0.00628);
            break;
        case StreamHealthState::Degraded:
            // Faster pulsing (period ~0.7 s)
            pulseFactor = 0.60 + 0.40 * std::sin(static_cast<double>(ms) * 0.00897);
            break;
        case StreamHealthState::Frozen:
        case StreamHealthState::SignalLoss:
            // Strobe blinking (period 500 ms)
            pulseFactor = ((ms % 500) < 250) ? 1.0 : 0.25;
            break;
        case StreamHealthState::Blackout:
        case StreamHealthState::Whiteout:
            pulseFactor = ((ms % 700) < 350) ? 1.0 : 0.40;
            break;
        }
    }

    // Determine status text and theme colors
    std::string statusText;
    std::uint8_t baseR = 0;
    std::uint8_t baseG = 255;
    std::uint8_t baseB = 64;

    switch (state) {
    case StreamHealthState::Healthy:
        statusText = "LIVE";
        baseR = 0;
        baseG = 255;
        baseB = 64; // Tactical Green
        break;
    case StreamHealthState::Degraded:
        statusText = "DEGRADED";
        baseR = 255;
        baseG = 191;
        baseB = 0; // Tactical Amber
        break;
    case StreamHealthState::Frozen:
        if (showFreeze && metrics.frozenDurationSec > 0.0) {
            statusText = "FROZEN (" + formatSeconds(metrics.frozenDurationSec) + ")";
        } else {
            statusText = "FROZEN";
        }
        baseR = 255;
        baseG = 40;
        baseB = 40; // Alert Red
        break;
    case StreamHealthState::SignalLoss:
        if (showFreeze && metrics.secondsSinceLastFrame > 0.0) {
            statusText = "NO SIGNAL (" + formatSeconds(metrics.secondsSinceLastFrame) + ")";
        } else {
            statusText = "NO SIGNAL";
        }
        baseR = 255;
        baseG = 50;
        baseB = 50; // Alert Red
        break;
    case StreamHealthState::Blackout:
        statusText = "OCCLUDED";
        baseR = 220;
        baseG = 110;
        baseB = 255; // Magenta/Purple
        break;
    case StreamHealthState::Whiteout:
        statusText = "WHITEOUT";
        baseR = 0;
        baseG = 220;
        baseB = 255; // Cyan
        break;
    }

    const auto dotR = static_cast<std::uint8_t>(std::clamp(static_cast<double>(baseR) * pulseFactor, 0.0, 255.0));
    const auto dotG = static_cast<std::uint8_t>(std::clamp(static_cast<double>(baseG) * pulseFactor, 0.0, 255.0));
    const auto dotB = static_cast<std::uint8_t>(std::clamp(static_cast<double>(baseB) * pulseFactor, 0.0, 255.0));

    const cv::Scalar beaconColor = makeColor(dotR, dotG, dotB, format);
    const cv::Scalar textColor = makeColor(240, 240, 240, format);

    // Assemble text
    std::string badgeText;
    if (!label.empty()) {
        badgeText = label + " " + statusText;
    } else {
        badgeText = statusText;
    }

    if (showFps && (style != Style::MinimalBeacon || state == StreamHealthState::Healthy)) {
        char fpsBuf[32];
        std::snprintf(fpsBuf, sizeof(fpsBuf), "  %.1f FPS", metrics.measuredFps);
        badgeText += fpsBuf;
    }

    const int fontFace = cv::FONT_HERSHEY_PLAIN;
    const double fontScale = (style == Style::FullTelemetry) ? 1.0 : 0.85;
    const int fontThickness = 1;
    int baseline = 0;
    const cv::Size textSize = cv::getTextSize(badgeText, fontFace, fontScale, fontThickness, &baseline);

    const int dotRadius = 4;
    const int padX = 8;
    const int padY = 5;
    const int dotGap = 6;

    const int badgeWidth = padX + (dotRadius * 2) + dotGap + textSize.width + padX;
    const int badgeHeight = (padY * 2) + std::max(dotRadius * 2, textSize.height);

    const int margin = 10;
    int badgeX = margin;
    int badgeY = margin;

    switch (pos) {
    case Position::TopLeft:
        badgeX = margin;
        badgeY = margin;
        break;
    case Position::TopRight:
        badgeX = std::max(0, width - badgeWidth - margin);
        badgeY = margin;
        break;
    case Position::BottomLeft:
        badgeX = margin;
        badgeY = std::max(0, height - badgeHeight - margin);
        break;
    case Position::BottomRight:
        badgeX = std::max(0, width - badgeWidth - margin);
        badgeY = std::max(0, height - badgeHeight - margin);
        break;
    }

    // 1. Draw backing scrim if not minimal beacon
    if (style != Style::MinimalBeacon && opacity > 0.0) {
        cv::Rect scrimRect(badgeX, badgeY, badgeWidth, badgeHeight);
        scrimRect &= cv::Rect(0, 0, width, height);
        if (scrimRect.width > 0 && scrimRect.height > 0) {
            cv::Mat roi = mat(scrimRect);
            const cv::Mat bg(roi.size(), roi.type(), cv::Scalar(18, 18, 18));
            cv::addWeighted(bg, opacity, roi, 1.0 - opacity, 0.0, roi);
        }
    }

    // 2. Draw beacon dot
    const cv::Point dotCenter(badgeX + padX + dotRadius, badgeY + badgeHeight / 2);
    cv::circle(mat, dotCenter, dotRadius, beaconColor, -1, cv::LINE_AA);

    // 3. Draw text label
    const cv::Point textOrigin(
        badgeX + padX + dotRadius * 2 + dotGap, badgeY + (badgeHeight + textSize.height) / 2 - 1);
    cv::putText(mat, badgeText, textOrigin, fontFace, fontScale, textColor, fontThickness, cv::LINE_AA);

    // 4. Render exclusion zone wireframes if requested
    if (showZones) {
        const cv::Scalar zoneColor = makeColor(255, 191, 0, format); // Amber
        for (const auto& zone : zones) {
            const int zx = zone.isNormalized ? static_cast<int>(zone.normX * static_cast<double>(width)) : zone.x;
            const int zy = zone.isNormalized ? static_cast<int>(zone.normY * static_cast<double>(height)) : zone.y;
            const int zw
                = zone.isNormalized ? static_cast<int>(zone.normWidth * static_cast<double>(width)) : zone.width;
            const int zh
                = zone.isNormalized ? static_cast<int>(zone.normHeight * static_cast<double>(height)) : zone.height;

            cv::Rect zRect(zx, zy, zw, zh);
            zRect &= cv::Rect(0, 0, width, height);
            if (zRect.width > 0 && zRect.height > 0) {
                cv::rectangle(mat, zRect, zoneColor, 1, cv::LINE_AA);
                cv::putText(mat, "EXCLUSION ROI", cv::Point(zRect.x + 3, zRect.y + 11), fontFace, 0.75, zoneColor, 1,
                    cv::LINE_AA);
            }
        }
    }
}

} // namespace Video::Filters

#endif // PELCOD_HAS_FILTERS
