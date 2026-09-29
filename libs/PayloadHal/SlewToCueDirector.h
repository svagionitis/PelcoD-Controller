#pragma once

/// @file SlewToCueDirector.h
/// @brief Tactical orchestrator managing automated PTZ slew-to-cue, range-adaptive framing, and optical tracker
/// handover.

#include "AutoFramingController.h"
#include "GeoLockController.h"
#include "ICameraPayload.h"
#include "NmeaSlavingBridge.h"
#include "PayloadAutoTrackerBridge.h"
#include "TargetThreatEvaluator.h"
#include "TourEngine.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace PayloadHal {

/// @enum CueingState
/// @brief State of the automated Slew-to-Cue lifecycle.
enum class CueingState : std::uint8_t {
    Idle = 0, ///< Standby or executing background patrol
    SlewingToTarget, ///< Gimbal actively slewing towards target line-of-sight
    FramingTarget, ///< Boresight on target; adjusting optical zoom based on range and dimensions
    AcquiringOpticalLock, ///< Optical video tracker attempting to acquire visual centroid
    OpticalTracking, ///< Closed-loop video tracking actively holding target in frame
    GeodeticTrackingFallback, ///< Optical lock unavailable/lost; tracking on Geo-Lock / dead reckoning
    DwellInspection, ///< Target locked and framed; executing inspection dwell timer
    TargetCompleted ///< Dwell completed; transitioning to next threat or resuming patrol
};

/// @struct SlewToCueConfig
/// @brief Operational thresholds and automation settings for Slew-to-Cue.
struct SlewToCueConfig {
    bool autonomousEngagement { true }; ///< True to automatically cue highest-ranked threat
    bool autoFraming { true }; ///< True to automatically adjust camera zoom
    bool autoOpticalHandover { true }; ///< True to engage video tracker upon gimbal convergence
    double lockToleranceDeg { 1.5 }; ///< Angular boresight error considered "on-target"
    double minThreatScoreToCue { 35.0 }; ///< Minimum threat score required for autonomous cueing
    std::chrono::milliseconds maxSlewWaitTimeout { 8000 }; ///< Timeout waiting for gimbal to slew on target
    std::chrono::milliseconds opticalAcquisitionTimeout { 3000 }; ///< Timeout for video tracker to lock
    std::chrono::milliseconds inspectionDwellDuration { 15000 }; ///< Time to inspect target before next
    bool allowPreemptionByHigherThreat { true }; ///< True to pre-empt active target if higher threat appears
    double preemptionScoreDelta { 25.0 }; ///< Score delta required to pre-empt an active target
    bool waitForZoomConvergence { false }; ///< True to wait for zoom profiler convergence before tracker handover
    std::chrono::milliseconds maxFramingDuration { 4000 }; ///< Timeout waiting for zoom convergence
    std::chrono::milliseconds targetCooldownDuration { 60000 }; ///< Cooldown before completed target can be re-cued
};

/// @struct SlewToCueStatus
/// @brief Real-time telemetry snapshot of the Slew-to-Cue director.
struct SlewToCueStatus {
    CueingState state { CueingState::Idle };
    bool isEngaged { false };
    std::uint32_t activeTargetId { 0U };
    TargetTrackSource activeTargetSource { TargetTrackSource::None };
    std::string activeTargetName {};
    double slantRangeMeters { 0.0 };
    double trueBearingDeg { 0.0 };
    double currentHfovDeg { 60.0 };
    bool isOpticalTrackerLocked { false };
    bool isEmergencyTarget { false };
    bool isTourSuspended { false };
    bool isGeodeticFallback { false };
    bool isPaused { false };
    double activeThreatScore { 0.0 };
    std::chrono::milliseconds dwellElapsed { 0 };
    std::chrono::milliseconds dwellRemaining { 0 };
};

/// @class SlewToCueDirector
/// @brief Autonomous coordinator managing the full Slew-to-Cue lifecycle for marine surface targets.
/// @details Evaluates target threats, directs NmeaSlavingBridge, configures AutoFramingController,
///          initiates PayloadAutoTrackerBridge handoff, and controls dwell duration.
class SlewToCueDirector {
public:
    using StateChangeCallback = std::function<void(CueingState oldState, CueingState newState)>;
    using StatusCallback = std::function<void(const SlewToCueStatus& status)>;

    /// @brief Constructs a SlewToCueDirector with required subsystem dependencies.
    /// @param[in] threatEvaluator Shared pointer to TargetThreatEvaluator.
    /// @param[in] slavingBridge Shared pointer to active NmeaSlavingBridge.
    /// @param[in] autoFraming Shared pointer to AutoFramingController (optional).
    /// @param[in] autoTracker Shared pointer to PayloadAutoTrackerBridge (optional).
    /// @param[in] camera Shared pointer to ICameraPayload for optical zoom (optional).
    /// @param[in] geoLock Shared pointer to GeoLockController (optional).
    /// @param[in] config Initial configuration parameters.
    SlewToCueDirector(std::shared_ptr<TargetThreatEvaluator> threatEvaluator,
        std::shared_ptr<NmeaSlavingBridge> slavingBridge, std::shared_ptr<AutoFramingController> autoFraming = nullptr,
        std::shared_ptr<PayloadAutoTrackerBridge> autoTracker = nullptr,
        std::shared_ptr<ICameraPayload> camera = nullptr, std::shared_ptr<GeoLockController> geoLock = nullptr,
        const SlewToCueConfig& config = {});

    /// @brief Constructs a SlewToCueDirector with TourEngine integration.
    /// @param[in] threatEvaluator Shared pointer to TargetThreatEvaluator.
    /// @param[in] slavingBridge Shared pointer to active NmeaSlavingBridge.
    /// @param[in] autoFraming Shared pointer to AutoFramingController (optional).
    /// @param[in] autoTracker Shared pointer to PayloadAutoTrackerBridge (optional).
    /// @param[in] camera Shared pointer to ICameraPayload for optical zoom (optional).
    /// @param[in] geoLock Shared pointer to GeoLockController (optional).
    /// @param[in] tourEngine Shared pointer to TourEngine (optional).
    /// @param[in] config Initial configuration parameters.
    SlewToCueDirector(std::shared_ptr<TargetThreatEvaluator> threatEvaluator,
        std::shared_ptr<NmeaSlavingBridge> slavingBridge, std::shared_ptr<AutoFramingController> autoFraming,
        std::shared_ptr<PayloadAutoTrackerBridge> autoTracker,
        std::shared_ptr<ICameraPayload> camera, std::shared_ptr<GeoLockController> geoLock,
        std::shared_ptr<TourEngine> tourEngine,
        const SlewToCueConfig& config = {});

    virtual ~SlewToCueDirector();

    // Non-copyable, non-movable
    SlewToCueDirector(const SlewToCueDirector&) = delete;
    SlewToCueDirector& operator=(const SlewToCueDirector&) = delete;
    SlewToCueDirector(SlewToCueDirector&&) = delete;
    SlewToCueDirector& operator=(SlewToCueDirector&&) = delete;

    /// @brief Updates configuration parameters.
    /// @param[in] config New configuration.
    void setConfig(const SlewToCueConfig& config);

    /// @brief Retrieves the active configuration.
    /// @return Current SlewToCueConfig snapshot.
    [[nodiscard]] SlewToCueConfig config() const;

    /// @brief Sets the TourEngine instance for background patrol suspension and resumption.
    /// @param[in] tourEngine Shared pointer to TourEngine.
    void setTourEngine(std::shared_ptr<TourEngine> tourEngine);

    /// @brief Retrieves the assigned TourEngine instance.
    /// @return Shared pointer to TourEngine, or nullptr if none assigned.
    [[nodiscard]] std::shared_ptr<TourEngine> tourEngine() const;

    /// @brief Checks whether a background patrol tour was suspended and awaits resumption.
    /// @return True if a tour is currently paused awaiting cue completion.
    [[nodiscard]] bool isTourSuspended() const noexcept;

    /// @brief Checks whether the system is actively operating in geodetic fallback mode.
    /// @return True if tracking via geodetic coasting/slaving rather than optical lock.
    [[nodiscard]] bool isGeodeticFallback() const noexcept;

    /// @brief Extends the inspection dwell duration for the active target.
    /// @param[in] duration Additional time to remain locked on the target.
    void extendDwell(std::chrono::milliseconds duration);

    /// @brief Temporarily pauses active cueing and dwell countdown.
    void pause();

    /// @brief Resumes paused cueing and dwell countdown.
    void resume();

    /// @brief Checks whether cueing operations are temporarily paused.
    /// @return True if currently paused.
    [[nodiscard]] bool isPaused() const noexcept;

    /// @brief Manually commands the director to slew to and track an evaluated target.
    /// @param[in] target Target evaluation record.
    /// @return True if cue was initiated.
    bool cueTarget(const EvaluatedTarget& target);

    /// @brief Manually commands the director to cue a specific radar target.
    /// @param[in] targetId Radar track number (00..99).
    /// @return True if target accepted.
    bool cueRadarTarget(std::uint32_t targetId);

    /// @brief Manually commands the director to cue a specific AIS vessel.
    /// @param[in] mmsi 9-digit MMSI.
    /// @return True if target accepted.
    bool cueAisTarget(std::uint32_t mmsi);

    /// @brief Dismisses the active target immediately and advances to next threat or idle.
    void dismissActiveTarget();

    /// @brief Disengages all cueing and slaving activities safely.
    void disengage();

    /// @brief Checks whether autonomous cueing is actively tracking or slewing.
    /// @return True if in any non-idle state.
    [[nodiscard]] bool isCueingActive() const noexcept;

    /// @brief Retrieves the current operational telemetry status.
    /// @return SlewToCueStatus struct.
    [[nodiscard]] SlewToCueStatus status() const;

    /// @brief Executes a single update evaluation cycle.
    /// @details Evaluates priority threats, checks gimbal convergence, drives auto-framing,
    ///          and manages state machine transitions.
    void update();

    /// @brief Sets callback for state transition notifications.
    /// @param[in] cb Callable accepting old and new CueingState.
    void setStateChangeCallback(StateChangeCallback cb);

    /// @brief Sets callback for periodic status reports.
    /// @param[in] cb Callable receiving SlewToCueStatus.
    void setStatusCallback(StatusCallback cb);

private:
    void transitionTo(CueingState newState);
    [[nodiscard]] bool checkGimbalConvergence() const noexcept;
    void executeAutoFraming(double rangeMeters, double targetLengthMeters);
    void handleTargetFinished();

    std::shared_ptr<TargetThreatEvaluator> m_threatEvaluator;
    std::shared_ptr<NmeaSlavingBridge> m_slavingBridge;
    std::shared_ptr<AutoFramingController> m_autoFraming;
    std::shared_ptr<PayloadAutoTrackerBridge> m_autoTracker;
    std::shared_ptr<ICameraPayload> m_camera;
    std::shared_ptr<GeoLockController> m_geoLock;
    std::shared_ptr<TourEngine> m_tourEngine { nullptr };

    mutable std::recursive_mutex m_mutex {};
    SlewToCueConfig m_config {};
    CueingState m_state { CueingState::Idle };

    std::uint32_t m_activeTargetId { 0U };
    TargetTrackSource m_activeTargetSource { TargetTrackSource::None };
    std::string m_activeTargetName {};
    double m_activeTargetLengthMeters { 20.0 };
    double m_activeTargetBeamMeters { 6.0 };
    double m_activeTargetHeightMeters { 4.0 };
    double m_activeTargetCogDeg { 0.0 };
    double m_activeThreatScore { 0.0 };
    bool m_isEmergencyTarget { false };

    bool m_wasTourRunning { false };
    bool m_isGeodeticFallbackActive { false };
    bool m_isPaused { false };
    std::chrono::milliseconds m_activeDwellDuration { 15000 };
    std::chrono::steady_clock::time_point m_pauseStartTime {};

    std::chrono::steady_clock::time_point m_stateEntryTime {};
    std::chrono::steady_clock::time_point m_dwellStartTime {};
    std::chrono::steady_clock::time_point m_lastUpdateTick {};

    StateChangeCallback m_stateChangeCb {};
    StatusCallback m_statusCb {};
};

} // namespace PayloadHal
