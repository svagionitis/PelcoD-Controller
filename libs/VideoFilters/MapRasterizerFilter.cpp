#include "MapRasterizerFilter.h"

#if defined(PELCOD_HAS_FILTERS)

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace Video::Filters {

MapRasterizerFilter::MapRasterizerFilter(int insetWidth, int insetHeight)
    : m_insetWidth(std::max(100, insetWidth))
    , m_insetHeight(std::max(80, insetHeight))
    , m_viewport({ 37.9838, 23.7275 }, 12.0, static_cast<double>(m_insetWidth), static_cast<double>(m_insetHeight))
    , m_diskCache(nullptr)
    , m_tileProvider(m_diskCache) {}

void MapRasterizerFilter::setEnabled(bool enabled) noexcept {
    m_enabled = enabled;
}

bool MapRasterizerFilter::isEnabled() const noexcept {
    return m_enabled;
}

void MapRasterizerFilter::setCorner(InsetCorner corner) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_corner = corner;
}

MapRasterizerFilter::InsetCorner MapRasterizerFilter::corner() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_corner;
}

void MapRasterizerFilter::setCustomPosition(int x, int y) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_customX = x;
    m_customY = y;
}

void MapRasterizerFilter::setInsetSize(int width, int height) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_insetWidth = std::max(100, width);
    m_insetHeight = std::max(80, height);
    m_viewport.setSize(static_cast<double>(m_insetWidth), static_cast<double>(m_insetHeight));
}

int MapRasterizerFilter::insetWidth() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_insetWidth;
}

int MapRasterizerFilter::insetHeight() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_insetHeight;
}

void MapRasterizerFilter::setOpacity(double alpha) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_opacity = std::clamp(alpha, 0.05, 1.0);
}

double MapRasterizerFilter::opacity() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_opacity;
}

void MapRasterizerFilter::setTheme(ColorTheme theme) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_theme = theme;
}

MapRasterizerFilter::ColorTheme MapRasterizerFilter::theme() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_theme;
}

void MapRasterizerFilter::setZoom(double zoom) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_viewport.setZoom(zoom);
}

double MapRasterizerFilter::zoom() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_viewport.zoom();
}

void MapRasterizerFilter::setOfflineDirectory(const std::string& directoryPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_diskCache = std::make_shared<Mapping::DiskTileCache>(directoryPath);
    m_tileProvider.setDiskCache(m_diskCache);
}

void MapRasterizerFilter::setOfflineOnly(bool offlineOnly) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tileProvider.setOfflineOnly(offlineOnly);
}

void MapRasterizerFilter::setPlatformTelemetry(const Klv::GeoPoint2D& pos, double headingDeg) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_platformPos = pos;
    m_platformHeading = headingDeg;
    m_hasPlatform = true;
    m_viewport.setCenter(pos);
}

void MapRasterizerFilter::setTargetPosition(const Klv::GeoPoint2D& targetPos) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_targetPos = targetPos;
}

void MapRasterizerFilter::clearTarget() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_targetPos.reset();
}

void MapRasterizerFilter::setFrustum(const Klv::FrustumCorners& frustum) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frustum = frustum;
    m_hasFrustum = true;
}

void MapRasterizerFilter::clearFrustum() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hasFrustum = false;
}

void MapRasterizerFilter::process(std::uint8_t* data, int width, int height, PixelFormat format) {
    if (!m_enabled || data == nullptr || width <= 0 || height <= 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (width < (m_insetWidth + m_margin) || height < (m_insetHeight + m_margin)) {
        return;
    }

    // Determine ROI placement
    int rx = 0;
    int ry = 0;

    switch (m_corner) {
    case InsetCorner::BottomRight:
        rx = width - m_insetWidth - m_margin;
        ry = height - m_insetHeight - m_margin;
        break;
    case InsetCorner::BottomLeft:
        rx = m_margin;
        ry = height - m_insetHeight - m_margin;
        break;
    case InsetCorner::TopRight:
        rx = width - m_insetWidth - m_margin;
        ry = m_margin;
        break;
    case InsetCorner::TopLeft:
        rx = m_margin;
        ry = m_margin;
        break;
    case InsetCorner::Custom:
        rx = std::clamp(m_customX, 0, width - m_insetWidth);
        ry = std::clamp(m_customY, 0, height - m_insetHeight);
        break;
    }

    // Colors depending on pixel layout (RGB24 vs BGR24)
    const bool isBgr = (format == PixelFormat::BGR24);
    cv::Scalar themeColor;
    cv::Scalar bgColor;
    cv::Scalar gridColor;

    switch (m_theme) {
    case ColorTheme::TacticalGreen:
        themeColor = isBgr ? cv::Scalar(64, 255, 0) : cv::Scalar(0, 255, 64);
        break;
    case ColorTheme::Amber:
        themeColor = isBgr ? cv::Scalar(0, 191, 255) : cv::Scalar(255, 191, 0);
        break;
    case ColorTheme::White:
        themeColor = cv::Scalar(255, 255, 255);
        break;
    case ColorTheme::TacticalCyan:
    default:
        themeColor = isBgr ? cv::Scalar(255, 229, 0) : cv::Scalar(0, 229, 255);
        break;
    }

    bgColor = isBgr ? cv::Scalar(28, 20, 14) : cv::Scalar(14, 20, 28);
    gridColor = isBgr ? cv::Scalar(56, 42, 30) : cv::Scalar(30, 42, 56);

    // Create offscreen mini-map image
    cv::Mat miniMap(m_insetHeight, m_insetWidth, CV_8UC3, bgColor);

    // Draw coordinate grid lines (every 40 pixels)
    for (int gx = 40; gx < m_insetWidth; gx += 40) {
        cv::line(miniMap, cv::Point(gx, 0), cv::Point(gx, m_insetHeight), gridColor, 1);
    }
    for (int gy = 40; gy < m_insetHeight; gy += 40) {
        cv::line(miniMap, cv::Point(0, gy), cv::Point(m_insetWidth, gy), gridColor, 1);
    }

    // Render Frustum footprint
    if (m_hasFrustum) {
        const auto sf = Mapping::TacticalOverlay::projectFrustum(m_viewport, m_frustum);
        const Mapping::ScreenRect vRect { 0.0, 0.0, static_cast<double>(m_insetWidth), static_cast<double>(m_insetHeight) };

        if (Mapping::TacticalOverlay::isFrustumVisible(sf, vRect)) {
            std::vector<cv::Point> pts {
                cv::Point(static_cast<int>(sf.corners[0].x), static_cast<int>(sf.corners[0].y)),
                cv::Point(static_cast<int>(sf.corners[1].x), static_cast<int>(sf.corners[1].y)),
                cv::Point(static_cast<int>(sf.corners[2].x), static_cast<int>(sf.corners[2].y)),
                cv::Point(static_cast<int>(sf.corners[3].x), static_cast<int>(sf.corners[3].y))
            };

            // Transparent fill
            cv::Mat polyMat = miniMap.clone();
            const std::vector<std::vector<cv::Point>> polyList { pts };
            cv::fillPoly(polyMat, polyList, themeColor);
            cv::addWeighted(polyMat, 0.25, miniMap, 0.75, 0.0, miniMap);

            // Crisp outline
            cv::polylines(miniMap, polyList, true, themeColor, 1, cv::LINE_AA);
        }
    }

    // Render Platform and Heading vector
    if (m_hasPlatform) {
        const auto sp = m_viewport.geoToScreen(m_platformPos);
        const auto sv = Mapping::TacticalOverlay::projectHeadingVector(
            m_viewport, m_platformPos, m_platformHeading, 24.0);

        const cv::Point origin(static_cast<int>(sv.origin.x), static_cast<int>(sv.origin.y));
        const cv::Point tip(static_cast<int>(sv.tip.x), static_cast<int>(sv.tip.y));

        cv::arrowedLine(miniMap, origin, tip, themeColor, 2, cv::LINE_AA, 0, 0.35);
        cv::circle(miniMap, cv::Point(static_cast<int>(sp.x), static_cast<int>(sp.y)), 5, themeColor, 2, cv::LINE_AA);

        // Target line-of-sight if present
        if (m_targetPos.has_value()) {
            const auto los = Mapping::TacticalOverlay::projectLineOfSight(m_viewport, m_platformPos, *m_targetPos);
            const cv::Point losTip(static_cast<int>(los.tip.x), static_cast<int>(los.tip.y));

            cv::line(miniMap, origin, losTip, themeColor, 1, cv::LINE_AA);
            cv::drawMarker(miniMap, losTip, themeColor, cv::MARKER_CROSS, 8, 1, cv::LINE_AA);
        }
    }

    // Draw Tactical Title Banner & Inset Border
    cv::rectangle(miniMap, cv::Rect(0, 0, m_insetWidth, 18), bgColor * 1.4, cv::FILLED);
    cv::putText(miniMap, "TACTICAL MAP", cv::Point(6, 13), cv::FONT_HERSHEY_SIMPLEX, 0.38, themeColor, 1, cv::LINE_AA);

    const std::string zStr = "Z:" + std::to_string(static_cast<int>(m_viewport.zoom()));
    cv::putText(miniMap, zStr, cv::Point(m_insetWidth - 36, 13), cv::FONT_HERSHEY_SIMPLEX, 0.35, themeColor, 1, cv::LINE_AA);

    cv::rectangle(miniMap, cv::Rect(0, 0, m_insetWidth, m_insetHeight), themeColor, 1, cv::LINE_AA);

    // Alpha blend into input frame buffer
    cv::Mat frameMat(height, width, CV_8UC3, data);
    cv::Rect roi(rx, ry, m_insetWidth, m_insetHeight);
    cv::Mat frameRoi = frameMat(roi);

    cv::addWeighted(miniMap, m_opacity, frameRoi, 1.0 - m_opacity, 0.0, frameRoi);
}

} // namespace Video::Filters

#endif // PELCOD_HAS_FILTERS
