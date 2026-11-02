#pragma once

/// @file Misb1909Types.h
/// @brief Strongly-typed domain models and graphical primitives for MISB ST 1909.1 HUD layout.
/// @details Defines normalized display coordinate structures, color palettes, flight instrument
///          geometries, and composite layout containers for non-destructive tactical overlays.

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "GeoTypes.h"
#include "KlvTypes.h"

namespace Klv {

/// @class NormalizedCoord
/// @brief Normalized coordinate value clamped strictly to [0.0F, 1.0F].
class NormalizedCoord {
public:
    /// @brief Default constructor initializing to 0.0F.
    constexpr NormalizedCoord() noexcept = default;

    /// @brief Explicit constructor clamping value to [0.0F, 1.0F].
    /// @param[in] val Raw floating-point coordinate value.
    explicit constexpr NormalizedCoord(float val) noexcept
        : m_val(std::clamp(val, 0.0F, 1.0F))
    {
    }

    /// @brief Retrieves the normalized coordinate value.
    /// @return Clamped float in range [0.0F, 1.0F].
    [[nodiscard]] constexpr float value() const noexcept
    {
        return m_val;
    }

private:
    float m_val { 0.0F };
};

/// @struct NormalizedPoint
/// @brief 2D point in normalized display coordinate space [0.0, 1.0].
struct NormalizedPoint {
    NormalizedCoord x { NormalizedCoord(0.5F) }; ///< Horizontal axis [0.0=left, 1.0=right]
    NormalizedCoord y { NormalizedCoord(0.5F) }; ///< Vertical axis [0.0=top, 1.0=bottom]
};

/// @struct NormalizedRect
/// @brief 2D bounding rectangle in normalized display coordinate space.
struct NormalizedRect {
    NormalizedPoint topLeft {}; ///< Top-left coordinate
    NormalizedCoord width { NormalizedCoord(0.0F) }; ///< Rectangle width
    NormalizedCoord height { NormalizedCoord(0.0F) }; ///< Rectangle height
};

/// @enum TextAlignment
/// @brief Horizontal text alignment relative to anchor.
enum class TextAlignment : std::uint8_t {
    Left = 0U, ///< Left-aligned at anchor point
    Center = 1U, ///< Horizontally centered at anchor point
    Right = 2U ///< Right-aligned at anchor point
};

/// @enum StackDirection
/// @brief Multi-line text block expansion direction.
enum class StackDirection : std::uint8_t {
    Downwards = 0U, ///< Subsequent lines expand towards bottom of viewport
    Upwards = 1U ///< Subsequent lines expand towards top of viewport
};

/// @enum SymbologyDensity
/// @brief HUD declutter presentation density levels.
enum class SymbologyDensity : std::uint8_t {
    Minimal = 0U, ///< Reticle and critical slant range only
    Standard = 1U, ///< Standard flight instruments and target card
    FullTactical = 2U, ///< All 8 groups, footprint corners, and MGRS
    Misb1909Standard = 3U ///< Strict MISB ST 1909.1 standard 8 layout groups
};

/// @enum TacticalColorPalette
/// @brief Tactical chromatic themes optimized for operational display environments.
enum class TacticalColorPalette : std::uint8_t {
    TacticalGreen = 0U, ///< NVG Night Vision high-visibility green (#00FF41)
    Amber = 1U, ///< FLIR Thermal sensor anti-glare amber (#FFB000)
    ElectricCyan = 2U, ///< Modern digital glass cockpit cyan (#00F0FF)
    CombatRed = 3U, ///< Threat / target engagement alert red (#FF3333)
    AviationWhite = 4U ///< Crisp high-contrast monochromatic white (#FFFFFF)
};

/// @enum CoordinateDisplayMode
/// @brief Display format for geographic coordinates.
enum class CoordinateDisplayMode : std::uint8_t {
    DecimalDegrees = 0U, ///< e.g. 34.2500* LAT, -116.1200* LON
    DegreesMinutesSeconds = 1U, ///< e.g. 34*15'00.0"N 116*07'12.0"W
    Mgrs = 2U ///< Military Grid Reference System (e.g. 11SMT 82060 91440)
};

/// @struct HudColorRgba
/// @brief RGBA color representation with floating-point channels in [0.0F, 1.0F].
struct HudColorRgba {
    float r { 0.0F }; ///< Red channel [0.0, 1.0]
    float g { 1.0F }; ///< Green channel [0.0, 1.0]
    float b { 0.25F }; ///< Blue channel [0.0, 1.0]
    float a { 1.0F }; ///< Alpha channel [0.0, 1.0]
};

/// @struct HudTextItem
/// @brief Renderable text element with normalized anchor.
struct HudTextItem {
    std::string text {}; ///< Text string to render
    NormalizedPoint anchor {}; ///< Normalized anchor position
    TextAlignment alignment { TextAlignment::Left }; ///< Horizontal text alignment
    float fontSizeScale { 1.0F }; ///< Relative font scale multiplier
    HudColorRgba color {}; ///< Color override
};

/// @struct HudLineItem
/// @brief Renderable line segment primitive in normalized space.
struct HudLineItem {
    NormalizedPoint start {}; ///< Segment start point
    NormalizedPoint end {}; ///< Segment end point
    float strokeWidth { 1.5F }; ///< Line stroke width in pixels
    bool isDashed { false }; ///< True for stippled / dashed lines
    HudColorRgba color {}; ///< Line color
};

/// @struct HudArcItem
/// @brief Renderable circle or arc primitive in normalized space.
struct HudArcItem {
    NormalizedPoint center {}; ///< Circle center point
    float radiusNorm { 0.05F }; ///< Radius normalized to viewport height
    float startAngleDeg { 0.0F }; ///< Arc start angle in degrees
    float sweepAngleDeg { 360.0F }; ///< Arc sweep angle in degrees
    float strokeWidth { 1.5F }; ///< Arc stroke width in pixels
    HudColorRgba color {}; ///< Arc color
};

/// @struct PitchLadderRung
/// @brief Single rung of the artificial horizon pitch ladder.
struct PitchLadderRung {
    float pitchDeg { 0.0F }; ///< Pitch rung angle in degrees (e.g. +10, -10)
    NormalizedPoint leftStart {}; ///< Left segment outer point
    NormalizedPoint leftEnd {}; ///< Left segment inner point (near center gap)
    NormalizedPoint rightStart {}; ///< Right segment inner point (near center gap)
    NormalizedPoint rightEnd {}; ///< Right segment outer point
    NormalizedPoint leftTabEnd {}; ///< Left vertical tab pointing towards horizon
    NormalizedPoint rightTabEnd {}; ///< Right vertical tab pointing towards horizon
    bool isDashed { false }; ///< True for diving rungs (< 0 deg)
};

/// @struct PitchLadderLayout
/// @brief Complete artificial horizon and pitch ladder layout.
struct PitchLadderLayout {
    NormalizedPoint horizonLeft {}; ///< Horizon bar left segment start
    NormalizedPoint horizonRight {}; ///< Horizon bar right segment end
    float rollAngleDeg { 0.0F }; ///< Platform roll derotation angle in degrees
    std::vector<PitchLadderRung> rungs {}; ///< Set of pitch ladder rungs
    bool visible { true }; ///< Visibility flag
};

/// @struct HeadingTapeTick
/// @brief Individual degree tick mark on the compass heading ribbon.
struct HeadingTapeTick {
    float headingDeg { 0.0F }; ///< Compass heading in degrees [0, 360)
    NormalizedPoint top {}; ///< Top point of tick
    NormalizedPoint bottom {}; ///< Bottom point of tick
    bool isMajor { false }; ///< True for major 10-degree increments
    std::string label {}; ///< Text label (e.g. "N", "27", "28")
};

/// @struct HeadingTapeLayout
/// @brief Top compass heading ribbon layout.
struct HeadingTapeLayout {
    NormalizedPoint tapeStart {}; ///< Tape left boundary
    NormalizedPoint tapeEnd {}; ///< Tape right boundary
    NormalizedPoint caretTip {}; ///< Pointer triangle caret tip
    float currentHeadingDeg { 0.0F }; ///< Active platform / sensor heading
    std::string digitalReadout {}; ///< Digital heading string (e.g. "270*")
    std::vector<HeadingTapeTick> ticks {}; ///< Visible ticks
    bool visible { true }; ///< Visibility flag
};

/// @struct ReticleLayout
/// @brief Center targeting boresight reticle.
struct ReticleLayout {
    NormalizedPoint center { NormalizedPoint { NormalizedCoord(0.5F), NormalizedCoord(0.5F) } };
    float armLengthNorm { 0.025F }; ///< Crosshair arm length
    float centerGapNorm { 0.008F }; ///< Central open gap
    std::vector<HudLineItem> crosshairLines {}; ///< 4 crosshair segments
    bool visible { true }; ///< Visibility flag
};

/// @struct Misb1909GroupData
/// @brief Formatted text block corresponding to one of the 8 ST 1909 standard groups.
struct Misb1909GroupData {
    std::uint8_t groupId { 1U }; ///< Group identifier 1..8
    std::string groupTitle {}; ///< Human-readable group name
    NormalizedPoint anchor {}; ///< Group anchor position
    TextAlignment alignment { TextAlignment::Left }; ///< Text horizontal alignment
    StackDirection stackDirection { StackDirection::Downwards }; ///< Multi-line flow
    std::vector<std::string> lines {}; ///< Formatted text lines
    bool visible { true }; ///< Visibility flag
};

/// @struct TacticalOverlayLayout
/// @brief Complete, self-contained tactical HUD presentation model.
struct TacticalOverlayLayout {
    Misb1909GroupData mainSensorGroup {}; ///< Group 1 (Top-Left)
    Misb1909GroupData securityGroup {}; ///< Group 2 (Top-Center)
    Misb1909GroupData platformGroup {}; ///< Group 3 (Top-Right)
    Misb1909GroupData northArrowGroup {}; ///< Group 4 (Mid-Left)
    Misb1909GroupData laserSensorGroup {}; ///< Group 5 (Bottom-Left)
    Misb1909GroupData dateTimeGroup {}; ///< Group 6 (Bottom-Center)
    Misb1909GroupData targetGroup {}; ///< Group 7 (Bottom-Right)
    ReticleLayout reticle {}; ///< Group 8 (Center Reticle)
    PitchLadderLayout pitchLadder {}; ///< Flight instrument
    HeadingTapeLayout headingTape {}; ///< Flight instrument
    std::vector<HudLineItem> vectorLines {}; ///< Auxiliary vector lines
    std::vector<HudTextItem> auxiliaryTexts {}; ///< Auxiliary text elements
    HudColorRgba activeThemeColor {}; ///< Theme RGBA color
};

/// @struct Misb1909Config
/// @brief User and operational configuration parameters for HUD layout generation.
struct Misb1909Config {
    SymbologyDensity density { SymbologyDensity::Misb1909Standard };
    TacticalColorPalette palette { TacticalColorPalette::TacticalGreen };
    CoordinateDisplayMode coordinateMode { CoordinateDisplayMode::DecimalDegrees };
    bool showHorizon { true };
    bool showHeadingTape { true };
    bool showSecurityBanners { true };
    bool showNorthArrow { true };
    bool showLaserGroup { true };
    float tapeSpanDegrees { 60.0F };
    float ladderStepDegrees { 5.0F };
    float maxLadderPitchDegrees { 20.0F };
};

} // namespace Klv
