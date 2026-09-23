/**
 * @file StreamHealthOsdFilter.h
 * @brief Tactical OSD overlay filter rendering live status designators, stream health telemetry, and exclusion zones.
 */

#pragma once

#include "DecoderTypes.h"
#include "StreamHealthMonitor.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#if defined(PELCOD_HAS_FILTERS)

#if defined(_WIN32)
#if defined(VideoFilters_EXPORTS)
#define VIDEOFILTERS_API __declspec(dllexport)
#else
#define VIDEOFILTERS_API __declspec(dllimport)
#endif
#else
#define VIDEOFILTERS_API
#endif

namespace Video::Filters {

/**
 * @class StreamHealthOsdFilter
 * @brief Tactical operational overlay filter stamping a real-time '● LIVE' designator badge onto video frames.
 *
 * Implements an animated visual heartbeat, color-coded health indicators (Healthy, Degraded, Frozen,
 * SignalLoss, Blackout, Whiteout), measured FPS display, and optional exclusion zone alignment outlines.
 */
class VIDEOFILTERS_API StreamHealthOsdFilter : public IFrameProcessor {
public:
    /// @enum Position
    /// @brief Corner anchor positions for the OSD designator badge.
    enum class Position : std::uint8_t { TopLeft, TopRight, BottomLeft, BottomRight };

    /// @enum Style
    /// @brief Visual presentation density of the designator badge.
    enum class Style : std::uint8_t {
        TacticalPill, ///< Rounded rectangular badge with pulsing dot, state text, and FPS.
        MinimalBeacon, ///< Compact animated dot and 'LIVE' text without backing scrim.
        FullTelemetry ///< Extended badge with state, measured FPS, and failure/freeze duration.
    };

    /// @brief Constructs a StreamHealthOsdFilter with designated corner position and style.
    /// @param[in] position Corner anchor position (default: TopRight).
    /// @param[in] style Visual styling mode (default: TacticalPill).
    explicit StreamHealthOsdFilter(Position position = Position::TopRight, Style style = Style::TacticalPill);

    ~StreamHealthOsdFilter() override = default;

    /// @brief Processes an incoming image buffer in-place to render the OSD badge.
    /// @param[in,out] data Pointer to the interleaved RGB/BGR pixel buffer.
    /// @param[in] width Frame width in pixels.
    /// @param[in] height Frame height in pixels.
    /// @param[in] format Pixel layout (RGB24 or BGR24).
    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    /// @brief Binds a StreamHealthMonitor instance to automatically poll health and metrics each frame.
    /// @param[in] monitor Shared pointer to the monitor watchdog.
    void bindMonitor(std::shared_ptr<const StreamHealthMonitor> monitor);

    /// @brief Unbinds the monitor instance.
    void unbindMonitor();

    /// @brief Directly pushes a health state and metric snapshot (alternative to monitor binding).
    /// @param[in] state Current stream health state.
    /// @param[in] metrics Current stream metrics snapshot.
    void updateHealth(StreamHealthState state, const StreamHealthMetrics& metrics);

    /// @brief Sets corner anchor position.
    /// @param[in] position Screen corner anchor.
    void setPosition(Position position);

    /// @brief Retrieves the active corner anchor position.
    [[nodiscard]] Position getPosition() const;

    /// @brief Sets visual presentation style.
    /// @param[in] style Badge visual style.
    void setStyle(Style style);

    /// @brief Retrieves active visual presentation style.
    [[nodiscard]] Style getStyle() const;

    /// @brief Sets custom camera or stream label prefix (e.g. "CAM-01" or empty for default "LIVE").
    /// @param[in] label Custom stream label.
    void setCustomLabel(const std::string& label);

    /// @brief Retrieves custom camera/stream label.
    [[nodiscard]] std::string getCustomLabel() const;

    /// @brief Toggles measured FPS text within the badge.
    /// @param[in] show True to show FPS.
    void setShowFps(bool show);

    /// @brief Queries whether measured FPS is displayed.
    [[nodiscard]] bool getShowFps() const;

    /// @brief Toggles elapsed freeze duration timer when stream stalls.
    /// @param[in] show True to show freeze duration timer.
    void setShowFreezeTimer(bool show);

    /// @brief Queries whether freeze timer is displayed.
    [[nodiscard]] bool getShowFreezeTimer() const;

    /// @brief Toggles wireframe outline rendering for active exclusion zones.
    /// @param[in] show True to draw exclusion zone wireframes.
    void setShowExclusionZones(bool show);

    /// @brief Queries whether exclusion zone wireframes are drawn.
    [[nodiscard]] bool getShowExclusionZones() const;

    /// @brief Toggles pulsating beacon animation.
    /// @param[in] enable True to enable pulsing.
    void setPulseAnimation(bool enable);

    /// @brief Queries whether pulsing beacon animation is enabled.
    [[nodiscard]] bool isPulseAnimationEnabled() const;

    /// @brief Sets backing scrim opacity [0.0 = fully transparent, 1.0 = fully opaque].
    /// @param[in] opacity Scrim background alpha.
    void setScrimOpacity(double opacity);

    /// @brief Retrieves active scrim opacity.
    [[nodiscard]] double getScrimOpacity() const;

    /// @brief Retrieves current health state rendered by the filter.
    [[nodiscard]] StreamHealthState getHealthState() const;

    /// @brief Retrieves current metrics snapshot rendered by the filter.
    [[nodiscard]] StreamHealthMetrics getHealthMetrics() const;

private:
    mutable std::mutex m_mutex;
    Position m_position { Position::TopRight };
    Style m_style { Style::TacticalPill };
    std::string m_customLabel {};
    bool m_showFps { true };
    bool m_showFreezeTimer { true };
    bool m_showExclusionZones { false };
    bool m_pulseAnimation { true };
    double m_scrimOpacity { 0.65 };

    std::weak_ptr<const StreamHealthMonitor> m_boundMonitor {};
    StreamHealthState m_state { StreamHealthState::Healthy };
    StreamHealthMetrics m_metrics {};
};

} // namespace Video::Filters

#endif // PELCOD_HAS_FILTERS
