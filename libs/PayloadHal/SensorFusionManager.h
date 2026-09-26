#pragma once

/// @file SensorFusionManager.h
/// @brief Optical Sensor Switching, Digital Match-Zoom & Fusion Manager for multi-spectral EO/IR payloads.

#include "ICameraPayload.h"
#include "PayloadTypes.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace PayloadHal {

/// @enum OpticalChannel
/// @brief Active optical camera channel designation.
enum class OpticalChannel : std::uint8_t {
    Primary,    ///< Primary daylight / visible sensor
    Secondary,  ///< Secondary thermal / IR sensor
    Auxiliary   ///< Optional SWIR or situational camera
};

/// @enum OpticalPalette
/// @brief Extended false-color lookup tables and rendering modes.
enum class OpticalPalette : std::uint8_t {
    DaylightColor,      ///< Standard true-color RGB (Visible)
    DaylightMonochrome, ///< Grayscale NIR / low-light mode
    WhiteHot,           ///< Thermal: Higher temperature rendered whiter
    BlackHot,           ///< Thermal: Higher temperature rendered darker
    Ironbow,            ///< Thermal: High dynamic range purple-red-yellow
    Rainbow,            ///< Thermal: Full rainbow spectrum
    Sepia,              ///< Surveillance night-vision sepia tone
    Arctic,             ///< Cold-environment cyan/blue-gold palette
    HazePenetration     ///< Atmospheric dehaze / CLAHE high-contrast mode
};

/// @enum FusionLayoutMode
/// @brief Multi-sensor video composition mode.
enum class FusionLayoutMode : std::uint8_t {
    SingleChannel,    ///< Sole display of active optical channel
    PictureInPicture, ///< Main view with secondary inset window
    SideBySideSplit,  ///< 50/50 horizontal split
    TopBottomSplit,   ///< 50/50 vertical split
    AlphaBlend,       ///< Weighted transparency overlay (Visible + Thermal)
    EdgeDetailBlend   ///< Thermal background overlaid with high-pass Visible edges
};

/// @struct MatchZoomResult
/// @brief Computed optical and electronic zoom parameters matching source FOV.
struct MatchZoomResult {
    double targetOpticalZoom01 { 0.0 };   ///< Normalized target camera zoom [0.0, 1.0]
    double digitalCropFactor { 1.0 };     ///< Electronic crop magnification (>= 1.0)
    double achievedHfovDeg { 0.0 };       ///< Actual resulting horizontal FOV in degrees
    double fovDeltaDeg { 0.0 };           ///< Difference between requested and achieved FOV
    bool isClamped { false };             ///< True if target camera reached wide/tele limit
};

/// @struct EnvironmentalSceneMetrics
/// @brief Ambient environmental and optical scene measurements.
struct EnvironmentalSceneMetrics {
    double ambientIlluminanceLux { 500.0 }; ///< Ambient light level in Lux (0.01 = night, 10000 = bright sun)
    double sceneContrastRatio { 0.8 };     ///< Optical contrast metric (0.0 = total haze/obscuration, 1.0 = sharp)
    double sunElevationDeg { 45.0 };       ///< Solar elevation angle in degrees (-90 to +90)
    bool isObscuredBySmokeHaze { false };  ///< True if optical scattering/obscuration is detected
};

/// @struct AutoSwitchConfig
/// @brief Configuration governing automated day/night and environmental sensor handoff.
struct AutoSwitchConfig {
    bool enabled { false };                ///< Master enable flag for auto-switching
    double lowLightThresholdLux { 1.5 };   ///< Lux level below which thermal is engaged
    double daylightReturnThresholdLux { 5.0 }; ///< Lux level above which daylight is restored
    double lowContrastThreshold { 0.25 };  ///< Contrast below which thermal is engaged
    double hysteresisTimeSec { 3.0 };      ///< Minimum time condition must persist before switching
    double cooldownTimeSec { 10.0 };       ///< Minimum cooldown between switches
};

/// @struct PipWindowConfig
/// @brief Coordinates and size for Picture-in-Picture inset window.
struct PipWindowConfig {
    double normalizedX { 0.70 };           ///< Inset top-left X [0.0, 1.0]
    double normalizedY { 0.05 };           ///< Inset top-left Y [0.0, 1.0]
    double normalizedWidth { 0.25 };       ///< Inset width [0.0, 1.0]
    double normalizedHeight { 0.25 };      ///< Inset height [0.0, 1.0]
    bool borderEnabled { true };           ///< Draw contrasting border around inset
};

/// @class SensorFusionManager
/// @brief Governs optical channel selection, digital match-zoom synchronization,
///        tactical color palettes, environmental auto-switching, and display fusion.
class SensorFusionManager {
public:
    SensorFusionManager();
    SensorFusionManager(
        std::shared_ptr<ICameraPayload> primaryCam,
        std::shared_ptr<ICameraPayload> secondaryCam);
    virtual ~SensorFusionManager();

    // --- Camera Binding ---

    /// @brief Binds primary and secondary optical sensors to the manager.
    /// @param[in] primaryCam Primary daylight/visible camera.
    /// @param[in] secondaryCam Secondary thermal/IR camera.
    void setCameras(
        std::shared_ptr<ICameraPayload> primaryCam,
        std::shared_ptr<ICameraPayload> secondaryCam) noexcept;

    /// @brief Queries the bound primary camera payload.
    [[nodiscard]] std::shared_ptr<ICameraPayload> primaryCamera() const noexcept;

    /// @brief Queries the bound secondary camera payload.
    [[nodiscard]] std::shared_ptr<ICameraPayload> secondaryCamera() const noexcept;

    /// @brief Retrieves the currently active optical camera based on active channel.
    [[nodiscard]] std::shared_ptr<ICameraPayload> activeCamera() const noexcept;

    // --- Channel Selection & Switching ---

    /// @brief Switches the active optical display channel.
    /// @param[in] channel Target channel (Primary or Secondary).
    /// @param[in] applyMatchZoom If true, automatically synchronizes target camera FOV.
    /// @return True if channel switched successfully.
    bool setActiveChannel(OpticalChannel channel, bool applyMatchZoom = true);

    /// @brief Queries the currently active optical channel.
    [[nodiscard]] OpticalChannel activeChannel() const noexcept;

    // --- Digital Match-Zoom ---

    /// @brief Enables or disables automatic match-zoom on channel switch.
    /// @param[in] enabled Desired match-zoom state.
    void setMatchZoomEnabled(bool enabled) noexcept;

    /// @brief Checks whether digital match-zoom is enabled.
    [[nodiscard]] bool isMatchZoomEnabled() const noexcept;

    /// @brief Computes the required target camera zoom and digital crop factor to match source FOV.
    /// @param[in] sourceChannel Source camera channel.
    /// @param[in] targetChannel Target camera channel.
    /// @return MatchZoomResult with calculated optical zoom, digital crop, and resulting HFOV.
    [[nodiscard]] MatchZoomResult computeMatchZoom(
        OpticalChannel sourceChannel,
        OpticalChannel targetChannel) const;

    /// @brief Computes and commands match-zoom directly on the target camera.
    /// @param[in] sourceChannel Source camera channel.
    /// @param[in] targetChannel Target camera channel.
    /// @return True if target camera zoom was dispatched.
    bool applyMatchZoom(OpticalChannel sourceChannel, OpticalChannel targetChannel);

    /// @brief Queries the currently active digital crop magnification factor.
    [[nodiscard]] double currentDigitalCropFactor() const noexcept;

    // --- Optical Palette & False-Color LUT ---

    /// @brief Configures false-color palette / rendering LUT.
    /// @param[in] palette Target palette mode.
    /// @return True if applied.
    bool setPalette(OpticalPalette palette);

    /// @brief Queries the current optical palette mode.
    [[nodiscard]] OpticalPalette palette() const noexcept;

    /// @brief Configures thermal isotherm highlighting.
    /// @param[in] enabled True to enable isotherm color emphasis.
    /// @param[in] minTempC Minimum temperature threshold in Celsius.
    /// @param[in] maxTempC Maximum temperature threshold in Celsius.
    void setIsothermHighlight(bool enabled, double minTempC = 35.0, double maxTempC = 42.0) noexcept;

    /// @brief Checks whether isotherm mode is active.
    [[nodiscard]] bool isIsothermEnabled() const noexcept;

    /// @brief Queries the isotherm temperature window in Celsius.
    void getIsothermRange(double& minTempC, double& maxTempC) const noexcept;

    // --- Environmental Auto-Switching ---

    /// @brief Configures the automated day/night and low-contrast handoff engine.
    /// @param[in] config Environmental thresholds and hysteresis parameters.
    void setAutoSwitchConfig(const AutoSwitchConfig& config) noexcept;

    /// @brief Queries the active auto-switch configuration.
    [[nodiscard]] AutoSwitchConfig autoSwitchConfig() const noexcept;

    /// @brief Ingests fresh environmental and optical scene measurements.
    /// @param[in] metrics Ambient light, contrast, and sun angles.
    void updateSceneMetrics(const EnvironmentalSceneMetrics& metrics);

    /// @brief Queries the last reported environmental scene metrics.
    [[nodiscard]] EnvironmentalSceneMetrics sceneMetrics() const noexcept;

    // --- Display Layout & Fusion Composition ---

    /// @brief Configures multi-sensor video display layout.
    /// @param[in] mode Desired fusion layout mode.
    void setFusionLayoutMode(FusionLayoutMode mode) noexcept;

    /// @brief Queries the active fusion layout mode.
    [[nodiscard]] FusionLayoutMode fusionLayoutMode() const noexcept;

    /// @brief Configures visible/thermal blend ratio for AlphaBlend layout.
    /// @param[in] visibleWeight01 Weight of visible channel [0.0 = 100% Thermal, 1.0 = 100% Visible].
    void setAlphaBlendWeight(double visibleWeight01) noexcept;

    /// @brief Queries the active alpha blend weight.
    [[nodiscard]] double alphaBlendWeight() const noexcept;

    /// @brief Configures Picture-in-Picture window position and size.
    /// @param[in] config Normalized coordinates and dimensions.
    void setPipConfig(const PipWindowConfig& config) noexcept;

    /// @brief Queries Picture-in-Picture window configuration.
    [[nodiscard]] PipWindowConfig pipConfig() const noexcept;

    // --- Periodic Update ---

    /// @brief Executes periodic environmental evaluation and hysteresis timers.
    /// @param[in] dtSeconds Elapsed time since last call in seconds.
    void update(double dtSeconds);

    // --- Callbacks ---

    /// @brief Observer callback signature for optical channel switch notifications.
    using ChannelSwitchCallback = std::function<void(OpticalChannel prevChannel,
                                                     OpticalChannel newChannel,
                                                     const std::string& reason)>;

    /// @brief Registers an observer callback for channel switch events.
    void setChannelSwitchCallback(ChannelSwitchCallback callback);

private:
    mutable std::mutex m_mutex;
    std::shared_ptr<ICameraPayload> m_primaryCam {};
    std::shared_ptr<ICameraPayload> m_secondaryCam {};

    OpticalChannel m_activeChannel { OpticalChannel::Primary };
    bool m_matchZoomEnabled { true };
    double m_currentDigitalCropFactor { 1.0 };

    OpticalPalette m_palette { OpticalPalette::DaylightColor };
    bool m_isothermEnabled { false };
    double m_isothermMinTempC { 35.0 };
    double m_isothermMaxTempC { 42.0 };

    AutoSwitchConfig m_autoConfig {};
    EnvironmentalSceneMetrics m_sceneMetrics {};
    double m_hysteresisTimerSec { 0.0 };
    double m_cooldownTimerSec { 0.0 };
    OpticalChannel m_pendingSwitchChannel { OpticalChannel::Primary };

    FusionLayoutMode m_layoutMode { FusionLayoutMode::SingleChannel };
    double m_alphaBlendWeight { 0.5 };
    PipWindowConfig m_pipConfig {};

    ChannelSwitchCallback m_switchCallback {};

    [[nodiscard]] std::shared_ptr<ICameraPayload> getCameraForChannelLocked(OpticalChannel channel) const noexcept;
    [[nodiscard]] static MatchZoomResult computeMatchZoomInternal(
        const std::shared_ptr<ICameraPayload>& srcCam,
        const std::shared_ptr<ICameraPayload>& tgtCam);
};

} // namespace PayloadHal
