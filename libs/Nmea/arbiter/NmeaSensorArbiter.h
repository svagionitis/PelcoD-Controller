#pragma once

/// @file NmeaSensorArbiter.h
/// @brief Multi-sensor redundancy and failover arbiter for dual GPS and dual gyrocompass feeds.

#include "NmeaTypes.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace Nmea::Arbiter {

/// @brief Identifier for dual GNSS input feeds.
enum class GpsSourceId : std::uint8_t { Primary = 0U, Secondary = 1U };

/// @brief Identifier for dual Gyro/Compass input feeds.
enum class HeadingSourceId : std::uint8_t { Primary = 0U, Secondary = 1U };

/// @brief Operational failover arbitration policy.
enum class FailoverPolicy : std::uint8_t {
    PrimarySecondaryAutoRevert
    = 0U, ///< Default to Primary; failover to Secondary if Primary degrades; auto-revert when Primary recovers
    HighestQualityFirst = 1U, ///< Route feed with highest health score; tie breaks in favor of Primary
    ManualOverride = 2U ///< Locked to designated manual source
};

/// @brief Configuration parameters for the sensor arbiter.
struct ArbiterConfig {
    FailoverPolicy policy { FailoverPolicy::PrimarySecondaryAutoRevert };
    GpsSourceId manualGpsSelection { GpsSourceId::Primary };
    HeadingSourceId manualHeadingSelection { HeadingSourceId::Primary };
    std::chrono::milliseconds gpsTimeout { 2500 }; ///< Max interval before GNSS feed considered offline
    std::chrono::milliseconds headingTimeout { 1500 }; ///< Max interval before Gyro feed considered offline
    double maxPositionDivergenceMeters { 25.0 }; ///< Position deviation threshold triggering cross-sensor alarm
    double maxHeadingDivergenceDeg { 4.0 }; ///< Heading deviation threshold triggering cross-sensor alarm
    double maxSanitySpeedMps { 50.0 }; ///< Kinematic jump speed limit (~97 knots)
    double maxSanityTurnRateDegPerSec { 60.0 }; ///< Maximum rotational rate limit
    double minimumHealthyScore { 50.0 }; ///< Minimum health score required for active selection (0..100)
};

/// @brief Real-time status, telemetry, and health metrics for a GNSS feed.
struct GpsChannelStatus {
    GpsSourceId source { GpsSourceId::Primary };
    bool online { false };
    bool positionValid { false };
    double latitudeDeg { 0.0 };
    double longitudeDeg { 0.0 };
    double altitudeMeters { 0.0 };
    double sogKnots { 0.0 };
    double cogDegrees { 0.0 };
    NmeaFixQuality fixQuality { NmeaFixQuality::Invalid };
    std::uint8_t satellites { 0U };
    double hdop { 99.9 };
    double healthScore { 0.0 }; ///< 0.0 .. 100.0%
    bool kinematicJumpDetected { false };
    std::chrono::steady_clock::time_point lastUpdate {};
};

/// @brief Real-time status, telemetry, and health metrics for a heading/gyro feed.
struct HeadingChannelStatus {
    HeadingSourceId source { HeadingSourceId::Primary };
    bool online { false };
    bool headingValid { false };
    double headingDegrees { 0.0 };
    double pitchDegrees { 0.0 };
    double rollDegrees { 0.0 };
    bool hasAttitude { false };
    double healthScore { 0.0 }; ///< 0.0 .. 100.0%
    bool angularJumpDetected { false };
    std::chrono::steady_clock::time_point lastUpdate {};
};

/// @brief Cross-sensor divergence monitoring state.
struct DivergenceStatus {
    bool positionDiverged { false };
    double positionDeltaMeters { 0.0 };
    bool headingDiverged { false };
    double headingDeltaDeg { 0.0 };
};

/// @class NmeaSensorArbiter
/// @brief Failover arbiter evaluating dual GPS and dual Gyro telemetry feeds with health scoring,
///        anti-spoofing kinematic jump rejection, cross-sensor divergence monitoring, and seamless snapshot output.
class NmeaSensorArbiter {
public:
    using GpsFailoverCallback
        = std::function<void(GpsSourceId oldSource, GpsSourceId newSource, const std::string& reason)>;
    using HeadingFailoverCallback
        = std::function<void(HeadingSourceId oldSource, HeadingSourceId newSource, const std::string& reason)>;
    using DivergenceCallback = std::function<void(const DivergenceStatus& status)>;

    explicit NmeaSensorArbiter(const ArbiterConfig& config = {});
    virtual ~NmeaSensorArbiter() = default;

    // Non-copyable, non-movable
    NmeaSensorArbiter(const NmeaSensorArbiter&) = delete;
    NmeaSensorArbiter& operator=(const NmeaSensorArbiter&) = delete;
    NmeaSensorArbiter(NmeaSensorArbiter&&) = delete;
    NmeaSensorArbiter& operator=(NmeaSensorArbiter&&) = delete;

    // --- Configuration ---

    void setConfig(const ArbiterConfig& config);
    [[nodiscard]] ArbiterConfig config() const;

    // --- Callback Registration ---

    void setGpsFailoverCallback(GpsFailoverCallback cb);
    void setHeadingFailoverCallback(HeadingFailoverCallback cb);
    void setDivergenceCallback(DivergenceCallback cb);

    // --- Ingestion Handlers ---

    /// @brief Ingests parsed GGA sentence for a specific GNSS source.
    void updateGps(GpsSourceId source, const GgaData& gga);

    /// @brief Ingests parsed RMC sentence for a specific GNSS source.
    void updateGps(GpsSourceId source, const RmcData& rmc);

    /// @brief Updates GNSS coordinates directly.
    void updateGpsCoordinates(GpsSourceId source, double lat, double lon, double altMeters, NmeaFixQuality fix,
        std::uint8_t sats, double hdop, double sogKnots = 0.0, double cogDeg = 0.0);

    /// @brief Ingests heading update for a specific gyro/compass source.
    void updateHeading(HeadingSourceId source, double headingDeg);

    /// @brief Ingests pitch and roll attitude update for a specific gyro source.
    void updateAttitude(HeadingSourceId source, double pitchDeg, double rollDeg);

    /// @brief Evaluates channel timeouts, health scores, divergence alarms, and failover transitions.
    /// @param[in] now Reference time point (defaults to steady_clock::now()).
    void evaluate(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    // --- State and Arbitration Queries ---

    /// @brief Returns the currently active GNSS source selected by arbitration.
    [[nodiscard]] GpsSourceId activeGpsSource() const;

    /// @brief Returns the currently active Heading source selected by arbitration.
    [[nodiscard]] HeadingSourceId activeHeadingSource() const;

    /// @brief Produces a glitch-free unified own-ship navigation snapshot from active sources.
    [[nodiscard]] NmeaNavSnapshot activeNavSnapshot() const;

    /// @brief Retrieves the status and health metrics of a specific GNSS feed.
    [[nodiscard]] GpsChannelStatus gpsStatus(GpsSourceId source) const;

    /// @brief Retrieves the status and health metrics of a specific Heading feed.
    [[nodiscard]] HeadingChannelStatus headingStatus(HeadingSourceId source) const;

    /// @brief Retrieves current divergence status between primary and secondary feeds.
    [[nodiscard]] DivergenceStatus divergenceStatus() const;

    /// @brief Calculates the great-circle Haversine distance in meters between two coordinates.
    [[nodiscard]] static double calculateDistanceMeters(double lat1, double lon1, double lat2, double lon2) noexcept;

    /// @brief Calculates the minimal angular difference in degrees between two headings [0.0 .. 180.0].
    [[nodiscard]] static double calculateHeadingDeltaDeg(double h1, double h2) noexcept;

private:
    void computeGpsHealth(GpsChannelStatus& status, std::chrono::steady_clock::time_point now);
    void computeHeadingHealth(HeadingChannelStatus& status, std::chrono::steady_clock::time_point now);
    void arbitrateGps(std::chrono::steady_clock::time_point now);
    void arbitrateHeading(std::chrono::steady_clock::time_point now);
    void checkDivergence();

    mutable std::mutex m_mutex {};
    ArbiterConfig m_config {};

    GpsFailoverCallback m_gpsFailoverCb {};
    HeadingFailoverCallback m_headingFailoverCb {};
    DivergenceCallback m_divergenceCb {};

    GpsChannelStatus m_primaryGps {};
    GpsChannelStatus m_secondaryGps {};
    GpsSourceId m_activeGps { GpsSourceId::Primary };

    HeadingChannelStatus m_primaryHeading {};
    HeadingChannelStatus m_secondaryHeading {};
    HeadingSourceId m_activeHeading { HeadingSourceId::Primary };

    DivergenceStatus m_divergence {};
};

} // namespace Nmea::Arbiter
