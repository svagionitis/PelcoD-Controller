#pragma once

/// @file PayloadSlavingCoordinator.h
/// @brief Multi-Payload Master/Slave Slaving & Predictive Blind-Zone Handover coordinator
///        supporting inter-turret baseline parallax compensation, canted mounts, and autonomous handoff.

#include "GeoreferenceUtils.h"
#include "IPayload.h"
#include "Klv/KlvTypes.h"
#include "PlatformLeverArmCompensator.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace PayloadHal {

/// @enum SlavingMode
/// @brief Operational synchronization mode between payloads.
enum class SlavingMode : std::uint8_t {
    Disabled,          ///< Payloads operate independently
    MasterSlave3D,     ///< Slave tracks master's 3D target point with baseline parallax compensation
    CollimatedLOS,     ///< Slave matches master line-of-sight vector at infinity (parallel pointing)
    GeodeticTarget,    ///< All payloads slave to an absolute WGS-84 coordinate (Lat, Lon, Alt)
    AutonomousHandoff  ///< MasterSlave3D with automatic predictive blind-zone handover enabled
};

/// @enum HandoverState
/// @brief Progress state of the blind-zone or manual handover sequence.
enum class HandoverState : std::uint8_t {
    Idle,                ///< No active slaving or normal tracking with clear LOS
    Tracking,            ///< Master actively tracking target; slaves aligned
    ApproachingBlindZone,///< Master LOS within pre-warning margin of keep-out zone or travel stop
    CandidateSelected,   ///< Optimal candidate slave selected based on visibility clearance
    CueingSlave,         ///< Candidate slave commanded to slew to target 3D intercept point
    SlaveConverging,     ///< Candidate slave slewing, awaiting angular alignment within lock tolerance
    TransferringControl, ///< Atomic promotion of candidate slave to new Master
    HandoverComplete,    ///< Transition complete; previous master released to standby
    HandoffFailed        ///< Timeout or all slaves obstructed; raises warning and retains best-effort tracking
};

/// @struct StationCapability
/// @brief Functional capabilities of a registered payload station.
struct StationCapability {
    bool canBeMaster { true };            ///< Whether this station can act as Master
    bool canBeSlave { true };             ///< Whether this station can receive slave commands
    bool hasLrf { false };                ///< Whether station is equipped with active LRF
    bool hasAutoTracker { false };        ///< Whether station has video auto-tracker
    double maxSlewRateDegPerSec { 60.0 }; ///< Maximum slaving slew velocity
    double lockToleranceDeg { 0.5 };      ///< Alignment tolerance confirming slave on target
};

/// @struct PayloadStationRecord
/// @brief Registry configuration and status for a payload station.
struct PayloadStationRecord {
    std::string stationId {};
    std::shared_ptr<IPayload> payload {};
    Vector3D platformOffsetM { 0.0, 0.0, 0.0 }; ///< 3D lever-arm from platform reference
    MountingOrientation mountOrientation {};      ///< Mounting Euler angles relative to platform body
    StationCapability capability {};
    bool isMaster { false };
    bool isSlaved { false };
    bool isEnabled { true };
};

/// @struct SlavedLookAngles
/// @brief Result of slave look-angle kinematics.
struct SlavedLookAngles {
    double panDeg { 0.0 };          ///< Pan / azimuth look angle in degrees
    double tiltDeg { 0.0 };         ///< Tilt / elevation look angle in degrees
    double slantRangeMeters { 0.0 };///< Slant range to target in meters
    bool isCollimated { false };    ///< True if pointing parallel to master at optical infinity
};

/// @struct SlavingStatusReport
/// @brief Real-time telemetry report for the multi-payload coordinator.
struct SlavingStatusReport {
    SlavingMode mode { SlavingMode::Disabled };
    std::string activeMasterId {};
    std::vector<std::string> activeSlaveIds {};
    HandoverState handoverState { HandoverState::Idle };
    std::string candidateSlaveId {};
    double masterDistanceToBlindZoneDeg { 999.0 };
    double slaveConvergenceErrorDeg { 0.0 };
    std::string lastHandoverReason {};
};

/// @class PayloadSlavingCoordinator
/// @brief Coordinates multiple gimbaled payload stations for synchronized line-of-sight tracking,
///        inter-station baseline parallax compensation, and autonomous blind-zone handover.
class PayloadSlavingCoordinator {
public:
    PayloadSlavingCoordinator();
    virtual ~PayloadSlavingCoordinator();

    // --- Station Management ---

    /// @brief Registers a payload station in the coordinator registry.
    /// @param[in] record Station configuration and payload reference.
    /// @return True if registered, false if stationId is empty or already registered.
    bool registerStation(const PayloadStationRecord& record);

    /// @brief Unregisters a payload station by identifier.
    /// @param[in] stationId Identifier of station to remove.
    /// @return True if found and removed.
    bool unregisterStation(const std::string& stationId);

    /// @brief Retrieves the configuration record of a registered station.
    /// @param[in] stationId Identifier of station.
    /// @return Station record if found, or std::nullopt.
    [[nodiscard]] std::optional<PayloadStationRecord> station(const std::string& stationId) const;

    /// @brief Retrieves snapshots of all currently registered payload stations.
    [[nodiscard]] std::vector<PayloadStationRecord> stations() const;

    /// @brief Clears all registered stations.
    void clearStations();

    // --- Slaving Mode & Master / Slave Control ---

    /// @brief Sets the active multi-payload slaving synchronization mode.
    /// @param[in] mode Desired slaving mode.
    void setSlavingMode(SlavingMode mode) noexcept;

    /// @brief Queries the currently configured slaving mode.
    [[nodiscard]] SlavingMode slavingMode() const noexcept;

    /// @brief Designates the active Master station.
    /// @param[in] stationId Identifier of station to make Master.
    /// @return True if station is registered and capable of being Master.
    bool setMasterStation(const std::string& stationId);

    /// @brief Queries the identifier of the active Master station.
    [[nodiscard]] std::string masterStationId() const;

    /// @brief Enables or disables slaving for a specific registered station.
    /// @param[in] stationId Station identifier.
    /// @param[in] slaved True to participate as a slave to the active Master.
    /// @return True if station was found and capability allows slaving.
    bool setStationSlaved(const std::string& stationId, bool slaved);

    /// @brief Checks whether a specific station is currently slaved.
    [[nodiscard]] bool isStationSlaved(const std::string& stationId) const;

    // --- Geodetic Target Slaving ---

    /// @brief Sets an absolute 3D WGS-84 geographic target coordinate for GeodeticTarget mode.
    /// @param[in] targetGps Target latitude, longitude, and altitude MSL.
    void setGeodeticTarget(const Klv::GeoPoint3D& targetGps);

    /// @brief Queries the current geodetic target coordinate if set.
    [[nodiscard]] std::optional<Klv::GeoPoint3D> geodeticTarget() const;

    /// @brief Clears the active geodetic target coordinate.
    void clearGeodeticTarget();

    // --- Default Engagement Slant Range ---

    /// @brief Sets the default engagement slant range used when Master LRF is unavailable or unequipped.
    /// @param[in] rangeMeters Engagement range in meters (default 1000.0m).
    void setDefaultEngagementRange(double rangeMeters) noexcept;

    /// @brief Gets the default engagement range in meters.
    [[nodiscard]] double defaultEngagementRange() const noexcept;

    // --- Thresholds & Handover Configuration ---

    /// @brief Sets the safety pre-warning margin and timeout for predictive blind-zone handovers.
    /// @param[in] warningMarginDeg Angular margin in degrees triggering handoff (default 5.0°).
    /// @param[in] timeoutSec Maximum time allowed for slave convergence before failing (default 5.0s).
    void setHandoverThresholds(double warningMarginDeg, double timeoutSec = 5.0) noexcept;

    /// @brief Gets the configured blind-zone warning margin in degrees.
    [[nodiscard]] double warningMarginDeg() const noexcept;

    /// @brief Gets the handover timeout duration in seconds.
    [[nodiscard]] double handoverTimeoutSec() const noexcept;

    // --- Mathematical Kinematics ---

    /// @brief Computes convergent pan/tilt look-angles for a slave station given master look angles and range.
    /// @param[in] masterId Identifier of master station.
    /// @param[in] slaveId Identifier of slave station.
    /// @param[in] masterPanDeg Master pan angle in degrees.
    /// @param[in] masterTiltDeg Master tilt angle in degrees.
    /// @param[in] slantRangeMeters Slant range to target in meters (if <= 0.0 or infinite, collimated LOS is computed).
    /// @return SlavedLookAngles containing convergent pan, tilt, and range, or std::nullopt if IDs are invalid.
    [[nodiscard]] std::optional<SlavedLookAngles> computeSlaveLookAngles(
        const std::string& masterId,
        const std::string& slaveId,
        double masterPanDeg,
        double masterTiltDeg,
        double slantRangeMeters) const;

    // --- Handover Protocol & Execution ---

    /// @brief Requests an explicit operator-directed handoff of the Master role to a specific target station.
    /// @param[in] targetStationId Identifier of candidate station to promote to Master.
    /// @return True if candidate is registered, capable of being Master, and handoff initiated.
    bool requestHandover(const std::string& targetStationId);

    /// @brief Aborts an in-progress handover sequence and reverts to tracking or idle.
    void cancelHandover();

    /// @brief Queries current handover state machine phase.
    [[nodiscard]] HandoverState handoverState() const noexcept;

    /// @brief Generates a comprehensive real-time status and telemetry report.
    [[nodiscard]] SlavingStatusReport statusReport() const;

    // --- Periodic Cycle Update ---

    /// @brief Main periodic update step (e.g. called at 20-50 Hz by controller or background loop).
    /// @param[in] dtSeconds Elapsed time since last update call in seconds.
    void update(double dtSeconds);

    // --- Callbacks ---

    /// @brief Callback signature notifying observers of handover state transitions.
    using HandoverCallback = std::function<void(const std::string& oldMasterId,
                                                const std::string& newMasterId,
                                                HandoverState state)>;

    /// @brief Registers a callback for handover state events.
    void setHandoverCallback(HandoverCallback callback);

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, PayloadStationRecord> m_stations;
    SlavingMode m_mode { SlavingMode::Disabled };
    std::string m_masterId {};
    std::optional<Klv::GeoPoint3D> m_geodeticTarget {};
    double m_defaultEngagementRangeM { 1000.0 };
    double m_warningMarginDeg { 5.0 };
    double m_handoverTimeoutSec { 5.0 };

    // Handover state tracking
    HandoverState m_handoverState { HandoverState::Idle };
    std::string m_candidateSlaveId {};
    double m_handoverTimerSec { 0.0 };
    std::string m_lastHandoverReason {};
    HandoverCallback m_handoverCallback {};

    // Internal helper methods (must be called with m_mutex locked)
    [[nodiscard]] std::optional<SlavedLookAngles> computeSlaveLookAnglesLocked(
        const std::string& masterId,
        const std::string& slaveId,
        double masterPanDeg,
        double masterTiltDeg,
        double slantRangeMeters) const;

    [[nodiscard]] std::string selectBestCandidateSlaveLocked(
        const std::string& currentMasterId,
        double targetPanDeg,
        double targetTiltDeg,
        double slantRangeMeters) const;

    [[nodiscard]] bool isStationClearOfBlindZonesLocked(
        const std::string& stationId,
        double panDeg,
        double tiltDeg) const;

    [[nodiscard]] double distanceToNearestBlindZoneLocked(
        const std::string& stationId,
        double panDeg,
        double tiltDeg) const;

    void notifyHandoverStateLocked(const std::string& oldMaster, const std::string& newMaster, HandoverState state);
};

} // namespace PayloadHal
