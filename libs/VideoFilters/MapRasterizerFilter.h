#pragma once

/// @file MapRasterizerFilter.h
/// @brief OpenCV-based tactical map rasterizer rendering an interactive or telemetry-driven
///        mini-map radar picture-in-picture (PIP) inset directly onto video frames.

#include "CompositeTileProvider.h"
#include "DecoderTypes.h"
#include "DiskTileCache.h"
#include "GeoTypes.h"
#include "KlvTypes.h"
#include "MapViewport.h"
#include "TacticalOverlay.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#if defined(PELCOD_HAS_FILTERS)

#ifndef VIDEOFILTERS_API
#define VIDEOFILTERS_API
#endif

namespace Video::Filters {

/// @class MapRasterizerFilter
/// @brief High-performance OpenCV video filter that renders a tactical mini-map inset overlay
///        onto live or recorded video streams, displaying platform location, heading,
///        optical sensor footprint frustum, and target line-of-sight.
class VIDEOFILTERS_API MapRasterizerFilter : public IFrameProcessor {
public:
    /// @enum InsetCorner
    /// @brief Corner placement of the mini-map inset within the video frame.
    enum class InsetCorner : std::uint8_t {
        BottomRight,
        BottomLeft,
        TopRight,
        TopLeft,
        Custom
    };

    /// @enum ColorTheme
    /// @brief Symbology and border chromatic theme.
    enum class ColorTheme : std::uint8_t {
        TacticalCyan,  ///< Modern digital glass cockpit cyan
        TacticalGreen, ///< NVG high-visibility monochrome green
        Amber,         ///< FLIR thermal amber
        White          ///< Monochromatic white
    };

    explicit MapRasterizerFilter(int insetWidth = 260, int insetHeight = 200);
    ~MapRasterizerFilter() override = default;

    // Filter activation
    void setEnabled(bool enabled) noexcept;
    [[nodiscard]] bool isEnabled() const noexcept;

    // Inset layout
    void setCorner(InsetCorner corner) noexcept;
    [[nodiscard]] InsetCorner corner() const noexcept;

    void setCustomPosition(int x, int y) noexcept;
    void setInsetSize(int width, int height) noexcept;
    [[nodiscard]] int insetWidth() const noexcept;
    [[nodiscard]] int insetHeight() const noexcept;

    void setOpacity(double alpha) noexcept;
    [[nodiscard]] double opacity() const noexcept;

    void setTheme(ColorTheme theme) noexcept;
    [[nodiscard]] ColorTheme theme() const noexcept;

    // Zoom and offline configuration
    void setZoom(double zoom) noexcept;
    [[nodiscard]] double zoom() const noexcept;

    void setOfflineDirectory(const std::string& directoryPath);
    void setOfflineOnly(bool offlineOnly) noexcept;

    // Telemetry feeds
    void setPlatformTelemetry(const Klv::GeoPoint2D& pos, double headingDeg) noexcept;
    void setTargetPosition(const Klv::GeoPoint2D& targetPos) noexcept;
    void clearTarget() noexcept;

    void setFrustum(const Klv::FrustumCorners& frustum) noexcept;
    void clearFrustum() noexcept;

    // IFrameProcessor implementation
    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

private:
    mutable std::mutex m_mutex;

    bool m_enabled { true };
    InsetCorner m_corner { InsetCorner::BottomRight };
    int m_customX { 20 };
    int m_customY { 20 };
    int m_insetWidth { 260 };
    int m_insetHeight { 200 };
    int m_margin { 20 };
    double m_opacity { 0.85 };
    ColorTheme m_theme { ColorTheme::TacticalCyan };

    Mapping::MapViewport m_viewport;
    std::shared_ptr<Mapping::DiskTileCache> m_diskCache;
    Mapping::CompositeTileProvider m_tileProvider;

    Klv::GeoPoint2D m_platformPos { 0.0, 0.0 };
    double m_platformHeading { 0.0 };
    bool m_hasPlatform { false };

    std::optional<Klv::GeoPoint2D> m_targetPos;

    Klv::FrustumCorners m_frustum {};
    bool m_hasFrustum { false };
};

} // namespace Video::Filters

#endif // PELCOD_HAS_FILTERS
