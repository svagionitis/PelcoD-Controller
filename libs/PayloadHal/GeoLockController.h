#pragma once

/// @file GeoLockController.h
/// @brief Closed-loop geographic coordinate tracking and line-of-sight stabilization controller.

#include "IPayload.h"
#include "Klv/KlvTypes.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace PayloadHal {

/// @struct GeoLockStatus
/// @brief Telemetry snapshot of active Geo-Lock tracking state.
struct GeoLockStatus {
    bool engaged { false }; ///< true if Geo-Lock mode is actively engaged
    std::optional<Klv::GeoPoint3D> target {}; ///< Current geographic 3D target coordinate
    double commandedPanDeg { 0.0 }; ///< Last pan angle dispatched to gimbal in degrees
    double commandedTiltDeg { 0.0 }; ///< Last tilt angle dispatched to gimbal in degrees
    double currentPanDeg { 0.0 }; ///< Gimbal current pan angle in degrees
    double currentTiltDeg { 0.0 }; ///< Gimbal current tilt angle in degrees
    double trackingErrorDeg { 0.0 }; ///< Angular error between target line-of-sight and gimbal boresight
    double slantRangeMeters { 0.0 }; ///< Line-of-sight distance to target in meters
    uint64_t updateCount { 0 }; ///< Total number of tracking update cycles processed
};

/// @class GeoLockController
/// @brief High-level controller managing geodetic point-to-slew and continuous Geo-Lock (GeoHold).
/// @details Dynamically compensates for platform translation and yaw rotation to maintain line-of-sight
///          pointing accurately towards a static or slowly moving terrestrial ground target.
class GeoLockController {
public:
    /// @brief Callback providing current host platform position and true heading (heading in degrees [0, 360)).
    using PlatformNavProvider = std::function<std::optional<std::pair<Klv::GeoPoint3D, double>>()>;

    /// @brief Constructs a GeoLockController bound to the specified payload.
    /// @param[in] payload Shared pointer to IPayload instance.
    explicit GeoLockController(std::shared_ptr<IPayload> payload) noexcept;

    /// @brief Destructor; terminates any active background tracking thread.
    virtual ~GeoLockController();

    // Disable copy semantics; allow move
    GeoLockController(const GeoLockController&) = delete;
    GeoLockController& operator=(const GeoLockController&) = delete;
    GeoLockController(GeoLockController&&) noexcept = default;
    GeoLockController& operator=(GeoLockController&&) noexcept = default;

    /// @brief Sets the angular deadband threshold in degrees to suppress actuator jitter.
    /// @param[in] deadbandDeg Minimum angle delta in degrees required to issue a new slew command (default 0.05).
    void setDeadbandDeg(double deadbandDeg) noexcept;

    /// @brief Gets the current deadband threshold in degrees.
    /// @return Deadband threshold in degrees.
    [[nodiscard]] double deadbandDeg() const noexcept;

    /// @brief Configures the platform navigation provider callback for periodic tracking loops.
    /// @param[in] provider Function returning latest platform (GeoPoint3D, headingDeg).
    void setPlatformNavProvider(PlatformNavProvider provider);

    /// @brief Engages Geo-Lock tracking on the specified geodetic coordinate.
    /// @param[in] targetPos Geodetic coordinate (lat, lon, alt) to lock onto.
    /// @return true if target was set and payload is valid, false otherwise.
    bool engage(const Klv::GeoPoint3D& targetPos);

    /// @brief Disengages Geo-Lock tracking mode.
    void disengage();

    /// @brief Checks whether Geo-Lock is currently engaged.
    /// @return true if actively holding a target.
    [[nodiscard]] bool isEngaged() const noexcept;

    /// @brief Retrieves the currently locked geographic target, if any.
    /// @return Target 3D coordinate if engaged, std::nullopt otherwise.
    [[nodiscard]] virtual std::optional<Klv::GeoPoint3D> currentTarget() const noexcept;

    /// @brief Updates tracking line-of-sight based on host platform position and heading.
    /// @details Calculates look angles to the target. If angular error exceeds deadband, commands PTU.
    /// @param[in] platformPos Platform 3D coordinate (latitude, longitude, altitude MSL in meters).
    /// @param[in] platformHeadingDeg Platform true compass heading in degrees [0.0, 360.0).
    /// @return true if update succeeded, false if not engaged or command failed.
    bool updatePlatform(const Klv::GeoPoint3D& platformPos, double platformHeadingDeg);

    /// @brief Starts an internal background tracking thread updating line-of-sight at rateHz.
    /// @details Requires a valid PlatformNavProvider to be set via setPlatformNavProvider.
    /// @param[in] rateHz Update frequency in Hertz (default 20.0 Hz).
    /// @return true if tracking loop thread was started, false if already running or no provider set.
    bool startTrackingLoop(double rateHz = 20.0);

    /// @brief Stops the internal background tracking loop thread.
    void stopTrackingLoop();

    /// @brief Checks whether the internal tracking thread is running.
    /// @return true if tracking thread is active.
    [[nodiscard]] bool isTrackingLoopRunning() const noexcept;

    /// @brief Retrieves a detailed status snapshot of the Geo-Lock tracking loop.
    /// @return GeoLockStatus structure containing current angles, errors, and target.
    [[nodiscard]] GeoLockStatus status() const;

private:
    void trackingWorker(std::chrono::milliseconds interval);

    std::shared_ptr<IPayload> m_payload {};
    PlatformNavProvider m_navProvider {};

    mutable std::mutex m_mutex;
    std::optional<Klv::GeoPoint3D> m_target {};
    bool m_engaged { false };
    double m_deadbandDeg { 0.05 };

    double m_lastCommandedPan { 0.0 };
    double m_lastCommandedTilt { 0.0 };
    bool m_hasCommanded { false };

    double m_currentPanDeg { 0.0 };
    double m_currentTiltDeg { 0.0 };
    double m_trackingErrorDeg { 0.0 };
    double m_slantRangeMeters { 0.0 };
    uint64_t m_updateCount { 0 };

    std::thread m_workerThread;
    std::condition_variable m_cv;
    std::atomic<bool> m_loopRunning { false };
    std::atomic<bool> m_stopRequested { false };
};

} // namespace PayloadHal
