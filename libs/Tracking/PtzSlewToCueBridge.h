#pragma once

/// @file PtzSlewToCueBridge.h
/// @brief Bridges geodetic target cues into automated Pelco-D PTZ slewing and optical framing.

#include "PtzAutoTracker.h"
#include "Klv/KlvGeodesy.h"
#include "PelcoDCore/PelcoDTypes.h"
#include "PelcoDCore/ProtocolBuilder.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Tracking {

/// @enum SlewState
/// @brief Slew execution state of the bridge.
enum class SlewState : std::uint8_t {
    Idle = 0U,              ///< No active cue or tracking disabled.
    CoarseAcquisition = 1U, ///< Slew error exceeds coarse threshold; executing absolute angle positioning.
    FineTracking = 2U,      ///< Slew error within coarse threshold; executing closed-loop rate tracking.
    Settled = 3U,           ///< Target acquired and centered within settle tolerance.
    TargetLost = 4U         ///< Cue timed out or tracking lost.
};

/// @struct PlatformNavState
/// @brief Navigation state of the sensor host platform.
struct PlatformNavState {
    Klv::GeoPoint3D position{};       ///< WGS-84 location (latitude, longitude, altitude MSL/HAE in meters).
    double headingDeg{0.0};           ///< Platform true heading [0, 360) deg.
    double pitchDeg{0.0};             ///< Platform pitch [-90, +90] deg (nose up positive).
    double rollDeg{0.0};              ///< Platform roll [-180, +180] deg (starboard down positive).
};

/// @struct TargetCue
/// @brief Geodetic target point and metadata for slew-to-cue targeting.
struct TargetCue {
    Klv::GeoPoint3D position{};       ///< Target WGS-84 coordinate.
    double targetRadiusM{2.0};        ///< Estimated physical target radius in meters for auto-framing.
    std::uint64_t timestampUs{0U};    ///< Cue timestamp in microseconds.
    std::string cueSourceId{};        ///< Origin identifier (e.g., "ST0601", "CoT", "RADAR").
};

/// @struct SlewBridgeConfig
/// @brief Operational thresholds and configuration parameters for slew-to-cue execution.
struct SlewBridgeConfig {
    double coarseThresholdDeg{4.0};   ///< Boundary between coarse slew and fine rate tracking.
    double settleToleranceDeg{0.25};  ///< Error threshold to transition from FineTracking to Settled.
    double zoomMarginFactor{1.5};     ///< Safety bounding factor applied to target radius.
    double minHfovDeg{2.0};           ///< Minimum camera horizontal field of view (full telephoto).
    double maxHfovDeg{60.0};          ///< Maximum camera horizontal field of view (full wide).
    double zoomChangeDeadbandDeg{0.5};///< Minimum HFOV delta before issuing zoom adjustments.
    bool enableAutoZoom{true};        ///< Enable automated optical zoom framing.
    std::uint8_t pelcoAddress{1U};    ///< Target Pelco-D RS-485 device bus address (1 - 255).
};

/// @struct SlewCommandBatch
/// @brief Output commands and diagnostics generated on each bridge evaluation tick.
struct SlewCommandBatch {
    std::vector<std::uint8_t> coarsePanCmd{};  ///< Pelco-D absolute pan command frame (if coarse slew).
    std::vector<std::uint8_t> coarseTiltCmd{}; ///< Pelco-D absolute tilt command frame (if coarse slew).
    std::vector<std::uint8_t> rateMotionCmd{}; ///< Pelco-D variable velocity motion frame (if fine tracking).
    std::vector<std::uint8_t> zoomCmd{};       ///< Pelco-D zoom tele/wide/stop command frame.
    SlewState state{SlewState::Idle};          ///< Current bridge operational state.
    double targetGimbalAzDeg{0.0};             ///< Target relative pan angle in gimbal body frame [0, 360).
    double targetGimbalElDeg{0.0};             ///< Target relative tilt angle in gimbal body frame [-90, +90].
    double errorAzimuthDeg{0.0};               ///< Signed azimuth error (Target Az - Current Pan) in [-180, +180].
    double errorElevationDeg{0.0};             ///< Signed elevation error (Target El - Current Tilt) in [-90, +90].
    double slantRangeMeters{0.0};              ///< Line-of-sight slant range to target cue in meters.
    double desiredHfovDeg{0.0};                ///< Calculated optimal HFOV for target auto-framing.
};

/// @class PtzSlewToCueBridge
/// @brief Coordinates geodetic cue ingestion, platform attitude compensation, and Pelco-D actuation.
class PtzSlewToCueBridge {
public:
    /// @brief Constructs the bridge with tracker reference and configuration.
    /// @param[in] tracker Reference to angular velocity PID tracker.
    /// @param[in] config Operational configuration settings.
    explicit PtzSlewToCueBridge(PtzAutoTracker& tracker,
                                const SlewBridgeConfig& config = {}) noexcept;

    /// @brief Ingests or updates the active target geodetic cue.
    /// @param[in] cue Geodetic location and metadata.
    void updateCue(const TargetCue& cue) noexcept;

    /// @brief Clears active target cue and transitions bridge to Idle.
    void clearCue() noexcept;

    /// @brief Updates platform navigation telemetry.
    /// @param[in] platform Position and attitude of sensor host platform.
    void updatePlatform(const PlatformNavState& platform) noexcept;

    /// @brief Periodic control update evaluating line-of-sight errors and generating Pelco-D commands.
    /// @param[in] currentPanDeg Current measured gimbal pan angle [0, 360) deg.
    /// @param[in] currentTiltDeg Current measured gimbal tilt angle [-90, +90] deg.
    /// @param[in] currentHfovDeg Current camera horizontal field of view in degrees.
    /// @param[in] dt Elapsed time in seconds since previous evaluation.
    /// @return Actionable Pelco-D command batch with telemetry.
    [[nodiscard]] SlewCommandBatch update(double currentPanDeg,
                                          double currentTiltDeg,
                                          double currentHfovDeg,
                                          double dt) noexcept;

    /// @brief Gets the current bridge operational state.
    /// @return Current SlewState enum.
    [[nodiscard]] SlewState getState() const noexcept;

    /// @brief Checks whether the bridge has an active target cue.
    /// @return True if active cue is assigned.
    [[nodiscard]] bool hasCue() const noexcept;

    /// @brief Gets the active configuration.
    /// @return Const reference to SlewBridgeConfig.
    [[nodiscard]] const SlewBridgeConfig& getConfig() const noexcept;

    /// @brief Sets new operational configuration.
    /// @param[in] config New configuration parameters.
    void setConfig(const SlewBridgeConfig& config) noexcept;

    /// @brief Computes topocentric gimbal target angles given platform and target geodetic positions.
    /// @param[in] platform Platform position and 3D attitude.
    /// @param[in] target Target geodetic coordinate.
    /// @param[out] outGimbalPanDeg Target gimbal pan angle [0, 360) deg.
    /// @param[out] outGimbalTiltDeg Target gimbal tilt angle [-90, +90] deg.
    /// @param[out] outSlantRangeM Slant range in meters.
    /// @return True if geometry was successfully solved.
    [[nodiscard]] static bool solveGimbalAngles(const PlatformNavState& platform,
                                                const Klv::GeoPoint3D& target,
                                                double& outGimbalPanDeg,
                                                double& outGimbalTiltDeg,
                                                double& outSlantRangeM) noexcept;

    /// @brief Calculates the signed shortest angular error across 360-degree wrapping.
    /// @param[in] targetDeg Target angle in degrees.
    /// @param[in] currentDeg Current measured angle in degrees.
    /// @return Signed difference in [-180, +180] degrees (positive = target clockwise/right of current).
    [[nodiscard]] static double shortestAngleDelta(double targetDeg, double currentDeg) noexcept;

private:
    PtzAutoTracker& m_tracker;
    SlewBridgeConfig m_config{};
    PlatformNavState m_platform{};
    std::optional<TargetCue> m_activeCue{std::nullopt};
    SlewState m_state{SlewState::Idle};
    double m_lastDesiredHfovDeg{0.0};
};

} // namespace Tracking
