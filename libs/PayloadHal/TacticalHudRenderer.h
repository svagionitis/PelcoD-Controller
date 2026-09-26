#pragma once

/// @file TacticalHudRenderer.h
/// @brief Tactical Heads-Up Display (HUD) & Symbology Renderer.

#include "PayloadTypes.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace PayloadHal {

class IPayload;

/// @enum ReticleType
/// @brief Electronic crosshair / reticle geometry.
enum class ReticleType : std::uint8_t {
    Crosshair,     ///< Standard 4-quadrant crosshair with center gap.
    MilDotLadder,  ///< Stadiametric ladder with 1-mil increment hash marks.
    CircleDot,     ///< Center pip enclosed by outer lead circle.
    BoxReticle,    ///< 4-corner targeting box with center pip.
    BoresightPlus, ///< Precision miniature boresight plus marker.
    None           ///< Reticle hidden.
};

/// @enum HudDeclutterLevel
/// @brief Information density filter for high-stress tactical operations.
enum class HudDeclutterLevel : std::uint8_t {
    Full,        ///< All telemetry, tapes, status boxes, and reticles visible.
    Standard,    ///< Heading tape, pitch ladder, reticle, and target data.
    Minimal,     ///< Reticle, tracking box, and range readout only.
    DeCluttered  ///< Pure reticle crosshair only.
};

/// @enum HudColorPalette
/// @brief Optical color schemes tailored for daylight and night vision devices (NVG).
enum class HudColorPalette : std::uint8_t {
    TacticalGreen,     ///< Phosphor NVG-compatible green (#00FF41).
    AviationWhite,     ///< Crisp high-contrast white (#FFFFFF).
    HighContrastAmber, ///< Anti-glare amber (#FFB000).
    ThermalRed,        ///< Low-signature dark red (#FF3333).
    Cyan               ///< Maritime high-visibility cyan (#00F0FF).
};

/// @enum CoordinateDisplayFormat
/// @brief Text formatting for geodetic target coordinates.
enum class CoordinateDisplayFormat : std::uint8_t {
    DecimalDegrees,        ///< 37.978361° N, 23.721000° E
    DegreesMinutesSeconds, ///< 37°58'42.1" N, 23°43'15.6" E
    Mgrs                   ///< Military Grid Reference System (e.g. 34SFH 12345 67890)
};

/// @struct HudColor
/// @brief RGBA color representation with floating-point channels [0.0 to 1.0].
struct HudColor {
    float r { 0.0f };
    float g { 1.0f };
    float b { 0.25f };
    float a { 1.0f };
};

/// @struct HudPoint2D
/// @brief Normalized screen coordinates [0.0 to 1.0]. (0,0) is top-left, (1,1) is bottom-right.
struct HudPoint2D {
    float x { 0.5f };
    float y { 0.5f };
};

/// @struct HudLine
/// @brief 2D line segment primitive in normalized screen space.
struct HudLine {
    HudPoint2D start {};
    HudPoint2D end {};
    HudColor color {};
    float strokeWidth { 1.5f };
};

/// @struct HudRect
/// @brief 2D bounding rectangle primitive in normalized screen space.
struct HudRect {
    HudPoint2D topLeft {};
    float width { 0.0f };
    float height { 0.0f };
    HudColor strokeColor {};
    float strokeWidth { 1.5f };
    bool filled { false };
    HudColor fillColor {};
};

/// @struct HudCircle
/// @brief 2D circle / arc primitive in normalized screen space.
struct HudCircle {
    HudPoint2D center {};
    float radius { 0.05f };
    HudColor color {};
    float strokeWidth { 1.5f };
};

/// @struct HudText
/// @brief Text label primitive in normalized screen space.
struct HudText {
    std::string text {};
    HudPoint2D position {};
    HudColor color {};
    float fontSize { 14.0f };
    bool bold { false };
    bool centerAligned { false };
};

/// @struct HudDrawList
/// @brief Batch of 2D vector drawing primitives generated for one frame.
struct HudDrawList {
    std::vector<HudLine> lines {};
    std::vector<HudRect> rectangles {};
    std::vector<HudCircle> circles {};
    std::vector<HudText> textLabels {};
};

/// @struct TacticalHudConfig
/// @brief Customization parameters for the HUD renderer.
struct TacticalHudConfig {
    ReticleType reticle { ReticleType::MilDotLadder };
    HudDeclutterLevel declutterLevel { HudDeclutterLevel::Full };
    HudColorPalette colorPalette { HudColorPalette::TacticalGreen };
    CoordinateDisplayFormat coordFormat { CoordinateDisplayFormat::DegreesMinutesSeconds };

    bool showHeadingTape { true };
    bool showPitchLadder { true };
    bool showHorizonLine { true };
    bool showTrackingGate { true };
    bool showVelocityLeadPip { true };
    bool showLrfTelemetry { true };
    bool showTargetGeoCoords { true };
    bool showSensorOpticsInfo { true };
    bool showSafetyWarnings { true };

    float defaultStrokeWidth { 1.5f };
    float customReticleScale { 1.0f };
};

/// @struct HudTelemetrySnapshot
/// @brief Combined sensor and mission telemetry ingested per frame.
struct HudTelemetrySnapshot {
    // Gimbal Orientation & Platform Attitude
    double panAngleDeg { 0.0 };
    double tiltAngleDeg { 0.0 };
    double rollAngleDeg { 0.0 };
    double platformHeadingDeg { 0.0 };
    double platformPitchDeg { 0.0 };
    double platformRollDeg { 0.0 };
    double platformAltMslMeters { 0.0 };

    // Camera Optics
    std::string opticalChannelName { "VISIBLE" };
    double horizontalFovDeg { 30.0 };
    double opticalZoomFactor { 1.0 };
    double digitalCropFactor { 1.0 };
    std::string paletteName { "DAYLIGHT" };

    // LRF & Laser
    bool lrfValid { false };
    double slantRangeMeters { 0.0 };
    bool laserArmed { false };
    bool laserFiring { false };
    bool laserInterlockActive { false };

    // Auto-Tracker / Lead-Angle
    bool targetTrackActive { false };
    std::string trackerStatus { "OFF" }; ///< TRACKING, COASTING, ACQUIRING
    HudPoint2D targetCenterNormalized { 0.5f, 0.5f };
    float targetWidthNormalized { 0.06f };
    float targetHeightNormalized { 0.06f };
    HudPoint2D leadPipNormalized { 0.5f, 0.5f };
    bool hasLeadVector { false };

    // Target Geolocation
    bool hasTargetGeo { false };
    double targetLatDeg { 0.0 };
    double targetLonDeg { 0.0 };
    double targetAltMslM { 0.0 };
    std::string targetMgrs {};

    // System Status
    std::string systemHealth { "READY" };
    std::string warningBanner {};
};

/// @class TacticalHudRenderer
/// @brief Renders MIL-STD-2525 / STANAG compliant tactical HUD overlays, reticles, and tapes.
class TacticalHudRenderer {
public:
    TacticalHudRenderer();
    ~TacticalHudRenderer();

    // Non-copyable, movable
    TacticalHudRenderer(const TacticalHudRenderer&) = delete;
    TacticalHudRenderer& operator=(const TacticalHudRenderer&) = delete;
    TacticalHudRenderer(TacticalHudRenderer&&) noexcept = default;
    TacticalHudRenderer& operator=(TacticalHudRenderer&&) noexcept = default;

    // --- Configuration ---

    /// @brief Configures HUD display parameters, declutter mode, and reticle.
    /// @param[in] config Desired configuration structure.
    void setConfig(const TacticalHudConfig& config) noexcept;

    /// @brief Queries current HUD configuration.
    [[nodiscard]] TacticalHudConfig config() const noexcept;

    /// @brief Sets active electronic reticle geometry.
    /// @param[in] type Reticle type enum.
    void setReticleType(ReticleType type) noexcept;

    /// @brief Queries active reticle geometry.
    [[nodiscard]] ReticleType reticleType() const noexcept;

    /// @brief Sets information declutter filter level.
    /// @param[in] level Declutter mode enum.
    void setDeclutterLevel(HudDeclutterLevel level) noexcept;

    /// @brief Queries active declutter level.
    [[nodiscard]] HudDeclutterLevel declutterLevel() const noexcept;

    /// @brief Sets standard military HUD color palette.
    /// @param[in] palette Color scheme enum.
    void setColorPalette(HudColorPalette palette) noexcept;

    /// @brief Queries active color scheme enum.
    [[nodiscard]] HudColorPalette colorPalette() const noexcept;

    /// @brief Sets a custom RGBA color overriding preset palettes.
    /// @param[in] color Custom RGBA color structure.
    void setCustomColor(const HudColor& color) noexcept;

    /// @brief Queries current primary drawing color.
    [[nodiscard]] HudColor currentColor() const noexcept;

    // --- Telemetry Ingestion ---

    /// @brief Ingests an explicit telemetry snapshot.
    /// @param[in] telemetry Sensor and tracking telemetry.
    void updateTelemetry(const HudTelemetrySnapshot& telemetry);

    /// @brief Queries last reported telemetry snapshot.
    [[nodiscard]] HudTelemetrySnapshot telemetry() const noexcept;

    /// @brief Synchronizes telemetry directly from an IPayload instance.
    /// @param[in] payload Composite payload station reference.
    void updateFromPayload(const IPayload& payload);

    // --- Vector Primitive Generation ---

    /// @brief Generates a decoupled 2D vector draw list in normalized coordinates.
    /// @return Batch of lines, rects, circles, and text primitives.
    [[nodiscard]] HudDrawList generateDrawList() const;

    // --- Framebuffer Rasterization ---

    /// @brief Renders the HUD symbology onto a 32-bit RGBA pixel buffer.
    /// @param[in,out] rgbaBuffer Raw pointer to RGBA pixel buffer (at least width * height * 4 bytes).
    /// @param[in] width Framebuffer width in pixels.
    /// @param[in] height Framebuffer height in pixels.
    /// @param[in] strideBytes Row stride in bytes (default 0 = width * 4).
    /// @return True if rasterized successfully.
    bool renderRgba(std::uint8_t* rgbaBuffer, int width, int height, int strideBytes = 0) const;

private:
    mutable std::mutex m_mutex;
    TacticalHudConfig m_config {};
    HudTelemetrySnapshot m_telemetry {};
    HudColor m_currentColor { 0.0f, 1.0f, 0.25f, 1.0f }; // TacticalGreen default

    void updatePaletteColorLocked() noexcept;

    // Internal symbology builders
    void buildReticle(HudDrawList& drawList) const;
    void buildHeadingTape(HudDrawList& drawList) const;
    void buildPitchLadder(HudDrawList& drawList) const;
    void buildHorizonLine(HudDrawList& drawList) const;
    void buildTrackingGate(HudDrawList& drawList) const;
    void buildTargetTelemetry(HudDrawList& drawList) const;
    void buildSensorTelemetry(HudDrawList& drawList) const;
    void buildLrfTelemetry(HudDrawList& drawList) const;
    void buildWarningBanners(HudDrawList& drawList) const;
};

} // namespace PayloadHal
