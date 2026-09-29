#pragma once

/// @file TargetThreatEvaluator.h
/// @brief Multi-sensor maritime target threat evaluation, CPA/TCPA computation, and priority queueing engine.

#include "GeoreferenceUtils.h"
#include "Klv/KlvGeodesy.h"
#include "Klv/KlvTypes.h"
#include "Nmea/AisTypes.h"
#include "Nmea/NmeaTypes.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace PayloadHal {

/// @enum TargetTrackSource
/// @brief Sensor origin of the evaluated target track.
enum class TargetTrackSource : std::uint8_t { None = 0, RadarArpa, Ais, FusedRadarAis };

/// @enum ThreatLevel
/// @brief Tactical threat classification level for marine targets.
enum class ThreatLevel : std::uint8_t {
    None = 0, ///< Target is distant, stationary, or diverging safely
    Informational, ///< Target detected within situational awareness horizon
    Warning, ///< Target breached warning perimeter or projected CPA < warning distance
    Critical, ///< Target breached security perimeter or imminent collision risk (low CPA/TCPA)
    Emergency ///< Emergency distress beacon (AIS-SART, MOB, EPIRB) requiring immediate response
};

/// @struct ThreatAssessmentConfig
/// @brief Configurable parameters and weighting factors for marine threat scoring.
struct ThreatAssessmentConfig {
    double warningZoneRadiusMeters { 3704.0 }; ///< 2.0 NM perimeter boundary
    double securityZoneRadiusMeters { 926.0 }; ///< 0.5 NM inner perimeter boundary
    double criticalCpaMeters { 500.0 }; ///< Critical CPA threshold distance
    double warningCpaMeters { 1852.0 }; ///< Warning CPA threshold distance (1.0 NM)
    double criticalTcpaSeconds { 300.0 }; ///< Critical TCPA threshold time (5 minutes)
    double warningTcpaSeconds { 900.0 }; ///< Warning TCPA threshold time (15 minutes)
    double associationMaxDistMeters { 250.0 }; ///< Spatial tolerance for radar-AIS fusion
    double associationMaxSpeedKnots { 3.0 }; ///< Speed tolerance for radar-AIS fusion
    double associationMaxCourseDeg { 30.0 }; ///< Course tolerance for radar-AIS fusion
    double highSpeedThresholdKnots { 20.0 }; ///< Velocity above which target is considered High-Speed Craft
    double weightCpa { 25.0 }; ///< Maximum score contribution for low CPA
    double weightTcpa { 25.0 }; ///< Maximum score contribution for imminent TCPA
    double weightProximity { 25.0 }; ///< Maximum score contribution for physical closeness
    double weightDarkVessel { 20.0 }; ///< Score boost for radar target lacking AIS
    double weightHighSpeed { 10.0 }; ///< Score boost for high-speed craft
    double weightEmergency { 1000.0 }; ///< Score for emergency beacons
};

/// @struct EvaluatedTarget
/// @brief Complete tactical threat profile and kinematic assessment of a marine target.
struct EvaluatedTarget {
    std::uint32_t targetId { 0U }; ///< Primary tracking identifier (Radar track number or AIS MMSI)
    std::optional<std::uint32_t> associatedRadarId { std::nullopt }; ///< Associated radar target ID if fused
    std::optional<std::uint32_t> associatedAisMmsi { std::nullopt }; ///< Associated AIS MMSI if fused
    TargetTrackSource source { TargetTrackSource::None }; ///< Sensor source
    std::string targetName {}; ///< Target vessel name or tactical callsign
    Klv::GeoPoint3D position {}; ///< Estimated 3D geodetic position (lat, lon, alt MSL)
    double rangeMeters { 0.0 }; ///< Slant/ground distance from own platform in meters
    double trueBearingDeg { 0.0 }; ///< Geographic true bearing from own ship [0.0, 360.0)
    double relativeBearingDeg { 0.0 }; ///< Relative bearing from own ship heading [-180.0, +180.0]
    double sogKnots { 0.0 }; ///< Target Speed Over Ground in knots
    double cogDegrees { 0.0 }; ///< Target Course Over Ground in degrees [0.0, 360.0)
    double cpaMeters { 0.0 }; ///< Distance at Closest Point of Approach in meters
    double tcpaSeconds { 0.0 }; ///< Time to CPA in seconds (positive = closing, negative = past)
    bool isClosing { false }; ///< True if target distance to own ship is currently decreasing
    bool isDarkVessel { false }; ///< True if radar target does not match any active AIS vessel
    bool isEmergency { false }; ///< True if emergency beacon (AIS-SART, MOB, EPIRB)
    Nmea::AisBeaconType emergencyType { Nmea::AisBeaconType::None }; ///< Emergency beacon subtype
    ThreatLevel threatLevel { ThreatLevel::None }; ///< Discretized threat level
    double threatScore { 0.0 }; ///< Continuous normalized threat score (higher = higher priority)
    std::chrono::steady_clock::time_point lastUpdate {}; ///< Timestamp of latest telemetry update
};

/// @struct InspectedTargetRecord
/// @brief Historical record of a target inspection to enforce dwell cooldowns.
struct InspectedTargetRecord {
    std::uint32_t targetId { 0U }; ///< Target identifier (radar track ID or AIS MMSI)
    TargetTrackSource source { TargetTrackSource::None }; ///< Sensor source
    std::chrono::steady_clock::time_point inspectedAt {}; ///< Timestamp when inspection completed
    std::chrono::milliseconds cooldownDuration { 60000 }; ///< Duration before target can be re-cued
};

/// @class TargetThreatEvaluator
/// @brief Analyzes marine targets from radar and AIS, calculates CPA/TCPA, correlates contacts, and ranks threats.
/// @details Thread-safe engine that ingests navigation telemetry and outputs a prioritized candidate list
///          for autonomous PTZ camera slew-to-cue and visual inspection.
class TargetThreatEvaluator {
public:
    using ThreatAlertCallback = std::function<void(const EvaluatedTarget&)>;

    /// @brief Default constructor with standard maritime threat thresholds.
    /// @param[in] config Assessment thresholds and scoring weights.
    explicit TargetThreatEvaluator(const ThreatAssessmentConfig& config = {});

    /// @brief Virtual destructor.
    virtual ~TargetThreatEvaluator() = default;

    // Non-copyable, movable
    TargetThreatEvaluator(const TargetThreatEvaluator&) = delete;
    TargetThreatEvaluator& operator=(const TargetThreatEvaluator&) = delete;
    TargetThreatEvaluator(TargetThreatEvaluator&&) noexcept = default;
    TargetThreatEvaluator& operator=(TargetThreatEvaluator&&) noexcept = default;

    /// @brief Updates threat assessment configuration.
    /// @param[in] config New configuration parameters.
    void setConfig(const ThreatAssessmentConfig& config);

    /// @brief Retrieves the active configuration parameters.
    /// @return Current ThreatAssessmentConfig snapshot.
    [[nodiscard]] ThreatAssessmentConfig config() const;

    /// @brief Evaluates all active radar and AIS contacts against own-ship navigation telemetry.
    /// @param[in] ownNav Latest own-ship navigation snapshot.
    /// @param[in] radarTargets Active ARPA radar targets from NmeaDevice.
    /// @param[in] aisTargets Active AIS vessel targets from NmeaDevice.
    /// @return Number of evaluated targets updated.
    std::size_t evaluate(const Nmea::NmeaNavSnapshot& ownNav, const std::vector<Nmea::TtmData>& radarTargets,
        const std::vector<Nmea::AisVesselTarget>& aisTargets);

    /// @brief Retrieves all evaluated targets sorted in descending order of threat score.
    /// @return Vector of EvaluatedTarget sorted from highest priority to lowest.
    [[nodiscard]] std::vector<EvaluatedTarget> getPrioritizedTargets() const;

    /// @brief Retrieves the single highest-priority target candidate for automated cueing.
    /// @return Highest threat target, or std::nullopt if no targets exist.
    [[nodiscard]] std::optional<EvaluatedTarget> getHighestThreatTarget() const;

    /// @brief Retrieves an evaluated target by its ID and sensor source.
    /// @param[in] targetId Radar track ID or AIS MMSI.
    /// @param[in] source Target sensor source.
    /// @return EvaluatedTarget struct if present, std::nullopt otherwise.
    [[nodiscard]] std::optional<EvaluatedTarget> getTarget(std::uint32_t targetId, TargetTrackSource source) const;

    /// @brief Marks a target as recently inspected to prevent immediate re-cueing.
    /// @param[in] targetId Target identifier (radar ID or AIS MMSI).
    /// @param[in] source Target sensor source.
    /// @param[in] cooldown Duration before target can be re-inspected.
    void markTargetInspected(std::uint32_t targetId, TargetTrackSource source,
        std::chrono::milliseconds cooldown = std::chrono::milliseconds(60000));

    /// @brief Checks whether a target is currently within its inspection cooldown window.
    /// @param[in] targetId Target identifier.
    /// @param[in] source Sensor track source.
    /// @param[in] now Current time point for cooldown evaluation.
    /// @return True if target is in cooldown, false otherwise.
    [[nodiscard]] bool isTargetInCooldown(std::uint32_t targetId, TargetTrackSource source,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const;

    /// @brief Retrieves the next highest-threat target that is not currently in cooldown.
    /// @param[in] minScore Minimum threat score required for candidacy.
    /// @param[in] now Current time point for cooldown evaluation.
    /// @return Highest threat uninspected target, or std::nullopt if none qualify.
    [[nodiscard]] std::optional<EvaluatedTarget> getNextUninspectedCandidate(double minScore = 30.0,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const;

    /// @brief Clears all target cooldown records.
    void clearCooldownHistory();

    /// @brief Removes expired records from cooldown history.
    /// @param[in] now Current time point for evaluation.
    void cleanupExpiredCooldowns(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    /// @brief Clears all cached evaluated targets.
    void clear();

    /// @brief Computes Closest Point of Approach (CPA) distance and Time to CPA (TCPA) in 2D local tangent plane.
    /// @param[in] ownPos Own-ship geodetic coordinate.
    /// @param[in] ownSogKnots Own-ship Speed Over Ground in knots.
    /// @param[in] ownCogDeg Own-ship Course Over Ground in degrees.
    /// @param[in] targetPos Target geodetic coordinate.
    /// @param[in] targetSogKnots Target Speed Over Ground in knots.
    /// @param[in] targetCogDeg Target Course Over Ground in degrees.
    /// @param[out] outCpaMeters Calculated distance at CPA in meters.
    /// @param[out] outTcpaSeconds Calculated time to CPA in seconds (negative if diverging / past).
    /// @note Thread-safe and stateless pure calculation.
    static void calculateCpa(const Klv::GeoPoint3D& ownPos, double ownSogKnots, double ownCogDeg,
        const Klv::GeoPoint3D& targetPos, double targetSogKnots, double targetCogDeg, double& outCpaMeters,
        double& outTcpaSeconds) noexcept;

    /// @brief Sets callback triggered whenever a Critical or Emergency threat is evaluated.
    /// @param[in] cb Callback accepting the critical EvaluatedTarget.
    void setThreatAlertCallback(ThreatAlertCallback cb);

private:
    [[nodiscard]] double computeThreatScore(double rangeMeters, double cpaMeters, double tcpaSeconds, bool isClosing,
        bool isDarkVessel, bool isEmergency, double sogKnots) const noexcept;

    [[nodiscard]] ThreatLevel determineThreatLevel(double threatScore, double rangeMeters, double cpaMeters,
        double tcpaSeconds, bool isClosing, bool isEmergency) const noexcept;

    mutable std::mutex m_mutex {};
    ThreatAssessmentConfig m_config {};
    std::vector<EvaluatedTarget> m_evaluatedTargets {};
    std::vector<InspectedTargetRecord> m_inspectedTargets {};
    ThreatAlertCallback m_alertCallback {};
};

} // namespace PayloadHal
