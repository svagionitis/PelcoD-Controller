#pragma once

/// @file PayloadAutoTrackerBridge.h
/// @brief Closed-loop optical visual auto-tracking bridge connecting Tracking::PtzAutoTracker to PayloadHal.

#include "ICameraPayload.h"
#include "IPanTiltUnit.h"
#include "IPayload.h"
#include "Tracking/PtzAutoTracker.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace PayloadHal {

/// @enum TrackerDriveMode
/// @brief Command dispatch mode for pan-tilt motion.
enum class TrackerDriveMode : std::uint8_t {
    NormalizedVelocity, ///< Uses IPanTiltUnit::setNormalizedVelocity([-1.0, 1.0])
    PhysicalRate        ///< Uses IPanTiltUnit::setRate(deg/sec) scaled by max configured speeds
};

/// @struct VisualTargetDetection
/// @brief Input observation from computer vision target detector.
struct VisualTargetDetection {
    double errorX { 0.0 };          ///< Normalized horizontal boresight error [-1.0 (left) .. +1.0 (right)]
    double errorY { 0.0 };          ///< Normalized vertical boresight error [-1.0 (up) .. +1.0 (down)]
    double velocityX { 0.0 };       ///< Target horizontal velocity in normalized units/sec
    double velocityY { 0.0 };       ///< Target vertical velocity in normalized units/sec
    bool isLocked { false };        ///< True if target is detected with positive optical lock
    bool isCoasting { false };      ///< True if target is temporarily occluded and coasting on prediction
    double targetNormHeight { 0.0 };///< Normalized target bounding height [0.0 .. 1.0] for auto-zoom framing
    std::chrono::steady_clock::time_point timestamp {}; ///< Observation timestamp
};

/// @struct AutoTrackerStatus
/// @brief Telemetry snapshot of active visual auto-tracker bridge.
struct AutoTrackerStatus {
    bool engaged { false }; ///< True if tracker is actively engaged
    Tracking::PtzAutoTracker::TrackingState trackingState { Tracking::PtzAutoTracker::TrackingState::Idle }; ///< State
    double lastErrorX { 0.0 };        ///< Last processed error X
    double lastErrorY { 0.0 };        ///< Last processed error Y
    double commandedPanVel { 0.0 };   ///< Last dispatched pan velocity [-1.0 .. 1.0] or deg/s
    double commandedTiltVel { 0.0 };  ///< Last dispatched tilt velocity [-1.0 .. 1.0] or deg/s
    double commandedZoomVel { 0.0 };  ///< Last dispatched continuous zoom velocity [-1.0 .. 1.0]
    double lastLeadOffsetX { 0.0 };   ///< Predictive lead boresight offset X
    double lastLeadOffsetY { 0.0 };   ///< Predictive lead boresight offset Y
    double currentZoomFactor { 1.0 }; ///< Optical magnification used in gain calculation
    uint64_t updateCount { 0U };      ///< Total update cycles completed
};

/// @class PayloadAutoTrackerBridge
/// @brief High-level controller bridging Tracking::PtzAutoTracker with PayloadHal subsystems.
/// @details Automatically binds live camera zoom telemetry to gain scheduling, dispatches
///          deadband-filtered pan/tilt/zoom motor commands, and provides both step-by-step
///          and threaded background tracking execution.
class PayloadAutoTrackerBridge {
public:
    /// @brief Target observation provider callback for autonomous background tracking.
    using TargetProvider = std::function<std::optional<VisualTargetDetection>()>;

    /// @brief Constructs a bridge bound to an aggregate IPayload station.
    /// @param[in] payload Shared pointer to IPayload instance.
    explicit PayloadAutoTrackerBridge(std::shared_ptr<IPayload> payload) noexcept;

    /// @brief Constructs a bridge bound directly to specific Pan-Tilt and Camera subsystems.
    /// @param[in] ptu Shared pointer to IPanTiltUnit instance.
    /// @param[in] camera Shared pointer to ICameraPayload instance (optional, for auto-zoom & gain scheduling).
    PayloadAutoTrackerBridge(std::shared_ptr<IPanTiltUnit> ptu,
                             std::shared_ptr<ICameraPayload> camera = nullptr) noexcept;

    /// @brief Destructor; terminates any active background tracking thread and halts motion.
    virtual ~PayloadAutoTrackerBridge();

    // Disable copy semantics; allow move
    PayloadAutoTrackerBridge(const PayloadAutoTrackerBridge&) = delete;
    PayloadAutoTrackerBridge& operator=(const PayloadAutoTrackerBridge&) = delete;
    PayloadAutoTrackerBridge(PayloadAutoTrackerBridge&&) noexcept = default;
    PayloadAutoTrackerBridge& operator=(PayloadAutoTrackerBridge&&) noexcept = default;

    // --- Control Lifecycle ---

    /// @brief Engages visual auto-tracking mode.
    /// @return True if tracking engaged successfully and PTU is available.
    bool engage();

    /// @brief Disengages visual auto-tracking mode and safely stops all actuator motion.
    void disengage();

    /// @brief Checks whether visual auto-tracking is actively engaged.
    /// @return True if engaged.
    [[nodiscard]] bool isEngaged() const noexcept;

    /// @brief Resets underlying PID controllers, tracking state, and stops actuators.
    void reset();

    // --- Configuration & Tuning ---

    /// @brief Configures command dispatch mode (NormalizedVelocity vs PhysicalRate).
    /// @param[in] mode Drive mode.
    void setDriveMode(TrackerDriveMode mode) noexcept;

    /// @brief Queries current drive mode.
    /// @return Current TrackerDriveMode.
    [[nodiscard]] TrackerDriveMode driveMode() const noexcept;

    /// @brief Configures maximum physical angular rates when in PhysicalRate mode.
    /// @param[in] maxPanDegPerSec Maximum pan speed in degrees per second.
    /// @param[in] maxTiltDegPerSec Maximum tilt speed in degrees per second.
    void setMaxPhysicalRates(double maxPanDegPerSec, double maxTiltDegPerSec) noexcept;

    /// @brief Configures PID and feedforward gains for the Pan (Azimuth) axis.
    void setPanGains(double kp, double ki, double kd, double kff = 0.0) noexcept;

    /// @brief Configures PID and feedforward gains for the Tilt (Elevation) axis.
    void setTiltGains(double kp, double ki, double kd, double kff = 0.0) noexcept;

    /// @brief Configures normalized error deadbands to suppress actuator jitter.
    void setDeadbands(double panDeadband, double tiltDeadband) noexcept;

    /// @brief Enables or disables closed-loop auto-zoom framing.
    void setAutoZoomEnabled(bool enabled) noexcept;

    /// @brief Configures desired target framing height and hysteresis deadband.
    void setTargetFramingHeight(double targetNormHeight, double deadband = 0.04) noexcept;

    /// @brief Configures predictive lead angle deflection.
    void setPredictiveLeadEnabled(bool enabled, double kLead = 0.15, double maxLead = 0.25) noexcept;

    /// @brief Configures adaptive latency compensation.
    void setAdaptiveLatencyEnabled(bool enabled, double estimatedLatencySeconds = 0.10) noexcept;

    // --- Manual Ingestion ---

    /// @brief Ingests normalized optical viewport boresight error and target dynamics.
    /// @param[in] target Visual observation record.
    /// @param[in] dt Elapsed time in seconds since previous update.
    /// @return True if update succeeded and commands were dispatched.
    bool updateVisual(const VisualTargetDetection& target, double dt);

    /// @brief Ingests a normalized bounding box [0.0 .. 1.0] from an object detector.
    /// @param[in] normX Left coordinate of bounding box [0.0 .. 1.0].
    /// @param[in] normY Top coordinate of bounding box [0.0 .. 1.0].
    /// @param[in] normWidth Width of bounding box [0.0 .. 1.0].
    /// @param[in] normHeight Height of bounding box [0.0 .. 1.0].
    /// @param[in] isLocked True if detection has positive optical track lock.
    /// @param[in] dt Elapsed time in seconds since previous update.
    /// @return True if update succeeded and commands were dispatched.
    bool updateBoundingBox(double normX, double normY, double normWidth, double normHeight,
                           bool isLocked, double dt);

    /// @brief Ingests physical spherical angular errors in degrees.
    /// @param[in] errorAzimuthDeg Boresight azimuth error in degrees (positive = target right).
    /// @param[in] errorElevationDeg Boresight elevation error in degrees (positive = target above).
    /// @param[in] omegaAzDegPerSec Target azimuth angular velocity in deg/sec.
    /// @param[in] omegaElDegPerSec Target elevation angular velocity in deg/sec.
    /// @param[in] isLocked True if target is actively acquired.
    /// @param[in] isCoasting True if target is temporarily occluded.
    /// @param[in] dt Elapsed time in seconds since previous update.
    /// @param[in] targetNormHeight Optional normalized target height for auto-framing zoom.
    /// @return True if update succeeded.
    bool updateAngular(double errorAzimuthDeg, double errorElevationDeg,
                       double omegaAzDegPerSec, double omegaElDegPerSec,
                       bool isLocked, bool isCoasting, double dt, double targetNormHeight = 0.0);

    // --- Background Autonomous Loop ---

    /// @brief Sets the target detection provider callback for autonomous loop execution.
    /// @param[in] provider Function returning latest VisualTargetDetection or std::nullopt.
    void setTargetProvider(TargetProvider provider);

    /// @brief Starts an internal background tracking loop thread.
    /// @param[in] rateHz Update frequency in Hertz (default 30.0 Hz).
    /// @return True if loop started, false if already running or no provider set.
    bool startTrackingLoop(double rateHz = 30.0);

    /// @brief Stops the internal background tracking loop thread.
    void stopTrackingLoop();

    /// @brief Checks whether the background tracking thread is running.
    /// @return True if running.
    [[nodiscard]] bool isTrackingLoopRunning() const noexcept;

    /// @brief Retrieves a detailed status snapshot of the auto-tracking controller.
    /// @return AutoTrackerStatus structure.
    [[nodiscard]] AutoTrackerStatus status() const;

    /// @brief Direct reference access to the underlying PtzAutoTracker for specialized tuning.
    [[nodiscard]] Tracking::PtzAutoTracker& tracker() noexcept;

private:
    void trackingWorker(std::chrono::milliseconds interval);
    void dispatchCommand(const Tracking::PtzAutoTracker::TrackingCommand& cmd);
    void stopActuators();
    [[nodiscard]] double queryCurrentZoom() const noexcept;

    std::shared_ptr<IPanTiltUnit> m_ptu {};
    std::shared_ptr<ICameraPayload> m_camera {};
    Tracking::PtzAutoTracker m_tracker;

    mutable std::mutex m_mutex;
    TargetProvider m_targetProvider {};
    TrackerDriveMode m_driveMode { TrackerDriveMode::NormalizedVelocity };
    bool m_engaged { false };

    double m_maxPanRateDegPerSec { 60.0 };
    double m_maxTiltRateDegPerSec { 30.0 };

    float m_lastZoomVel { 0.0f };
    bool m_hasLastBox { false };
    double m_lastBoxCenterX { 0.5 };
    double m_lastBoxCenterY { 0.5 };

    AutoTrackerStatus m_status {};
    std::chrono::steady_clock::time_point m_lastUpdateTime {};

    std::thread m_workerThread;
    std::condition_variable m_cv;
    std::atomic<bool> m_loopRunning { false };
    std::atomic<bool> m_stopRequested { false };
};

} // namespace PayloadHal
