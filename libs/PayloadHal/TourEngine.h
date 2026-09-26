#pragma once

/// @file TourEngine.h
/// @brief Automated cyclical guard patrol and tour execution engine across PTZ presets.

#include "ICameraPayload.h"
#include "IPanTiltUnit.h"
#include "IPtzPresetManager.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace PayloadHal {

/// @struct TourWaypoint
/// @brief Single stop within an automated cyclical patrol route.
struct TourWaypoint {
    std::uint32_t presetId { 0U };                ///< Target preset to slew to
    std::chrono::milliseconds dwellTime { 5000 }; ///< Observation hold duration once arrived
    float slewSpeedRatio { 1.0f };                ///< Slew velocity ratio (0.1 to 1.0)
};

/// @struct TourDefinition
/// @brief Multi-waypoint patrol mission profile.
struct TourDefinition {
    std::string tourId {};                         ///< Unique identifier (e.g. "patrol_perimeter_night")
    std::string name {};                           ///< User-facing label (e.g. "Perimeter Guard Night")
    std::vector<TourWaypoint> waypoints {};        ///< Ordered sequence of preset stops
    bool loop { true };                            ///< True if patrol repeats indefinitely
    std::uint32_t maxLoops { 0U };                 ///< Max repeat cycles (0 = infinite)
    std::optional<std::uint32_t> homePresetId {};  ///< Home preset to return to on stop/timeout
    std::chrono::seconds inactivityHomeTimeout { 60 }; ///< Seconds of manual idle before returning Home
};

/// @enum TourState
/// @brief Operational state of the automated tour execution engine.
enum class TourState : std::uint8_t {
    Idle,               ///< No active tour running
    SlewingToWaypoint,  ///< Gimbal actively moving towards target preset
    DwellingAtWaypoint, ///< Stationary hold at waypoint during observation window
    Paused,             ///< Tour temporarily halted due to operator or auto-tracker intervention
    Completed,          ///< Finite tour reached maxLoops
    Fault               ///< Hardware or communication fault occurred
};

/// @struct TourStatus
/// @brief Real-time telemetry reporting tour progression.
struct TourStatus {
    TourState state { TourState::Idle };
    std::string activeTourId {};
    std::size_t currentWaypointIndex { 0U };
    std::uint32_t currentPresetId { 0U };
    std::chrono::milliseconds remainingDwell { 0 };
    std::uint32_t completedLoops { 0U };
    std::string statusMessage {};
};

/// @class TourEngine
/// @brief Background execution engine coordinating sequential preset patrols with dwell timers and manual override interlocks.
class TourEngine {
public:
    using StatusCallback = std::function<void(const TourStatus& status)>;

    TourEngine(std::shared_ptr<IPanTiltUnit> ptu,
               std::shared_ptr<ICameraPayload> camera,
               std::shared_ptr<IPtzPresetManager> presetMgr);
    ~TourEngine();

    /// @brief Registers a patrol tour profile into the engine.
    bool registerTour(const TourDefinition& tour);

    /// @brief Removes a registered tour profile.
    bool removeTour(const std::string& tourId);

    /// @brief Queries a registered tour definition.
    [[nodiscard]] std::optional<TourDefinition> getTour(const std::string& tourId) const;

    /// @brief Lists all registered patrol tour profiles.
    [[nodiscard]] std::vector<TourDefinition> listTours() const;

    /// @brief Starts executing the specified tour profile from waypoint 0.
    bool startTour(const std::string& tourId);

    /// @brief Pauses the active tour at the current position.
    bool pauseTour();

    /// @brief Resumes a paused tour towards the current waypoint.
    bool resumeTour();

    /// @brief Stops the active tour and transitions to Idle.
    void stopTour();

    /// @brief Signals operator manual joystick/keyboard deflection to pause active patrol without motor fight.
    void notifyManualIntervention();

    /// @brief Queries the real-time operational status and telemetry of the tour engine.
    [[nodiscard]] TourStatus status() const;

    /// @brief Registers an observer callback for tour state transitions.
    void registerStatusCallback(StatusCallback cb);

    /// @brief Configures angular arrival tolerance threshold in degrees.
    void setArrivalThresholdDeg(double degrees) noexcept;

    /// @brief Retrieves the angular arrival tolerance threshold in degrees.
    [[nodiscard]] double arrivalThresholdDeg() const noexcept;

private:
    void executionLoop();
    bool checkArrival(const PtzPreset& target);

    std::shared_ptr<IPanTiltUnit> m_ptu;
    std::shared_ptr<ICameraPayload> m_camera;
    std::shared_ptr<IPtzPresetManager> m_presetMgr;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::map<std::string, TourDefinition> m_tours;
    TourStatus m_status {};
    StatusCallback m_callback {};

    double m_arrivalThresholdDeg { 0.5 };
    std::chrono::steady_clock::time_point m_dwellStartTime {};

    std::atomic<bool> m_running { false };
    std::thread m_workerThread;
};

} // namespace PayloadHal
