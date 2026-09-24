/**
 * @file TacticalHudFilter.h
 * @brief Military / STANAG 4609 compliant Head-Up Display (HUD) and tactical OSD overlay filter.
 */

#pragma once

#include "DecoderTypes.h"
#include "KlvTypes.h"

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

/**
 * @class TacticalHudFilter
 * @brief High-performance tactical Head-Up Display (HUD) rendering artificial horizon, pitch ladder,
 *        compass ribbon, boresight reticle, target geodetic card, and MISB ST 0102 security banners.
 *
 * Implements IFrameProcessor for direct insertion into video playback, recording, and streaming pipelines.
 * Thread-safe for real-time telemetry updates from STANAG 4609 / MISB ST 0601 metadata decoders.
 */
class VIDEOFILTERS_API TacticalHudFilter : public IFrameProcessor {
public:
    /// @enum HudMode
    /// @brief Symbology presentation density levels.
    enum class HudMode : std::uint8_t {
        Minimal,     ///< Center reticle, top heading ribbon, and compact slant range.
        Standard,    ///< Reticle, compass ribbon, artificial horizon, target coordinate card, security banner.
        FullTactical ///< Standard + ownship platform navigation card + tactical footprint radar inset.
    };

    /// @enum ColorPalette
    /// @brief Tactical chromatic themes optimized for different operational environments.
    enum class ColorPalette : std::uint8_t {
        TacticalGreen, ///< NVG Night Vision high-visibility monochrome green (0, 255, 64)
        Amber,         ///< FLIR Thermal sensor amber (255, 191, 0)
        ElectricCyan,  ///< Modern digital glass cockpit cyan (0, 255, 255)
        CombatRed,     ///< Threat / target engagement alert red (255, 48, 48)
        White          ///< Monochromatic white (255, 255, 255)
    };

    /// @enum CoordinateFormat
    /// @brief Display format for geographical latitude and longitude.
    enum class CoordinateFormat : std::uint8_t {
        Dms,        ///< Degrees, Minutes, Seconds (e.g. 37°46'29.7"N 122°25'09.9"W)
        DecimalDeg  ///< Decimal Degrees (e.g. 37.774929°N 122.419416°W)
    };

    /// @struct TargetData
    /// @brief Target and optical ground intersection telemetry.
    struct TargetData {
        std::optional<double> latitudeDeg;  ///< Target WGS-84 latitude
        std::optional<double> longitudeDeg; ///< Target WGS-84 longitude
        std::optional<double> elevationM;   ///< Target MSL elevation in meters
        std::optional<double> slantRangeM;  ///< Slant range to target in meters
        std::optional<double> widthM;       ///< Target footprint width in meters
    };

    /// @struct PlatformData
    /// @brief Ownship platform navigation and mission telemetry.
    struct PlatformData {
        double headingDeg { 0.0 };          ///< Platform heading [0, 360) deg
        double pitchDeg { 0.0 };            ///< Platform pitch [-20, +20] deg
        double rollDeg { 0.0 };             ///< Platform roll [-50, +50] deg
        double sensorAzimuthDeg { 0.0 };    ///< Sensor relative azimuth [0, 360) deg
        double sensorElevationDeg { 0.0 };  ///< Sensor relative elevation [-90, +90] deg
        double hfovDeg { 60.0 };            ///< Horizontal optical field of view
        double vfovDeg { 35.0 };            ///< Vertical optical field of view
        double zoomLevel { 1.0 };           ///< Optical zoom multiplier (e.g. 1.0x to 40.0x)
        std::optional<double> latitudeDeg;  ///< Ownship latitude
        std::optional<double> longitudeDeg; ///< Ownship longitude
        std::optional<double> altitudeM;    ///< Ownship MSL altitude in meters
        std::string tailNumber;             ///< Platform callsign / tail ID
        std::string missionId;              ///< Active mission ID
        std::string sensorPayload;          ///< Sensor payload model / camera type
        std::uint64_t timestampUs { 0U };   ///< Precision timestamp in microseconds since epoch
    };

    /// @brief Constructs a TacticalHudFilter with designated HUD mode and color palette.
    /// @param[in] mode Initial display mode (default: Standard).
    /// @param[in] palette Initial tactical color theme (default: TacticalGreen).
    explicit TacticalHudFilter(HudMode mode = HudMode::Standard,
                               ColorPalette palette = ColorPalette::TacticalGreen);

    ~TacticalHudFilter() override = default;

    /// @brief Renders the tactical HUD symbology in-place onto the given frame buffer.
    /// @param[in,out] data Pointer to raw packed RGB24 or BGR24 frame buffer.
    /// @param[in] width Frame width in pixels.
    /// @param[in] height Frame height in pixels.
    /// @param[in] format Pixel format of the buffer.
    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    /// @brief Updates all HUD telemetry directly from a MISB ST 0601 UAS Datalink message.
    /// @param[in] message Fully or partially populated telemetry structure.
    void updateTelemetry(const Klv::UasDatalinkMessage& message);

    /// @brief Sets the HUD symbology density mode.
    /// @param[in] mode Presentation density mode.
    void setMode(HudMode mode);

    /// @brief Retrieves the active HUD presentation mode.
    [[nodiscard]] HudMode getMode() const;

    /// @brief Sets the tactical color palette.
    /// @param[in] palette Desired color theme.
    void setColorPalette(ColorPalette palette);

    /// @brief Retrieves the active color palette.
    [[nodiscard]] ColorPalette getColorPalette() const;

    /// @brief Sets coordinate display formatting.
    /// @param[in] format DMS or Decimal Degrees.
    void setCoordinateFormat(CoordinateFormat format);

    /// @brief Retrieves coordinate display formatting.
    [[nodiscard]] CoordinateFormat getCoordinateFormat() const;

    /// @brief Sets whether the artificial horizon pitch ladder is visible.
    /// @param[in] show True to render the artificial horizon.
    void setShowHorizon(bool show);

    /// @brief Checks whether the artificial horizon is enabled.
    [[nodiscard]] bool getShowHorizon() const;

    /// @brief Sets whether the top compass heading ribbon is visible.
    /// @param[in] show True to render the compass tape.
    void setShowCompassTape(bool show);

    /// @brief Checks whether the compass tape is enabled.
    [[nodiscard]] bool getShowCompassTape() const;

    /// @brief Sets whether the MISB ST 0102 security classification banners are visible.
    /// @param[in] show True to render security banners.
    void setShowSecurityBanners(bool show);

    /// @brief Checks whether security banners are enabled.
    [[nodiscard]] bool getShowSecurityBanners() const;

    /// @brief Manually sets platform attitude angles for testing or direct IMU feeds.
    /// @param[in] headingDeg Platform heading angle in degrees [0, 360).
    /// @param[in] pitchDeg Platform pitch angle in degrees [-20, +20].
    /// @param[in] rollDeg Platform roll angle in degrees [-50, +50].
    void setPlatformAttitude(double headingDeg, double pitchDeg, double rollDeg);

    /// @brief Manually sets sensor gimbal angles and optical FOV.
    /// @param[in] azimuthDeg Sensor azimuth angle in degrees.
    /// @param[in] elevationDeg Sensor elevation angle in degrees.
    /// @param[in] hfovDeg Horizontal field of view in degrees.
    /// @param[in] zoomMagnification Optical zoom factor (e.g. 1.0x to 30.0x).
    void setSensorOrientation(double azimuthDeg, double elevationDeg, double hfovDeg, double zoomMagnification = 1.0);

    /// @brief Manually sets target coordinate data.
    /// @param[in] target Target telemetry data.
    void setTargetData(const TargetData& target);

    /// @brief Manually sets MISB ST 0102 security classification.
    /// @param[in] security Security classification metadata.
    void setSecurityMetadata(const Klv::SecurityMetadata& security);

private:
    mutable std::mutex m_mutex;
    HudMode m_mode { HudMode::Standard };
    ColorPalette m_palette { ColorPalette::TacticalGreen };
    CoordinateFormat m_coordFormat { CoordinateFormat::Dms };
    bool m_showHorizon { true };
    bool m_showCompassTape { true };
    bool m_showSecurityBanners { true };

    PlatformData m_platform;
    TargetData m_target;
    std::optional<Klv::SecurityMetadata> m_security;
    std::optional<Klv::FrustumCorners> m_footprintCorners;
};

} // namespace Video::Filters

namespace Video {
using Filters::TacticalHudFilter;
} // namespace Video

#endif // PELCOD_HAS_FILTERS
