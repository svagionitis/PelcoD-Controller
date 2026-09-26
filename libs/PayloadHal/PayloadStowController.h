#pragma once

/// @file PayloadStowController.h
/// @brief Payload Stow, De-Ice/Wiper Routine & Emergency Park Controller.

#include "ICameraPayload.h"
#include "IPanTiltUnit.h"
#include "IPtzPresetManager.h"
#include "PayloadTypes.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace PayloadHal {

/// @enum StowState
/// @brief Operational lifecycle and stow stance of the payload gimbal.
enum class StowState : std::uint8_t {
    Stowed,              ///< Securely docked in protective stow cavity; safe for vehicle motion.
    Stowing,             ///< Actively slewing to stow orientation with retracted optics.
    Deployed,            ///< Operational in normal field of regard.
    Deploying,           ///< Slewing to operational deploy orientation.
    Maintenance,         ///< Stationary in technician servicing stance.
    MovingToMaintenance, ///< Slewing to maintenance stance.
    EmergencyParking,    ///< High-priority slew to emergency park pose.
    EmergencyParked,     ///< Securely parked following an emergency stop/park command.
    Zeroizing,           ///< Purging mission data and slewing to emergency park.
    Zeroized             ///< All sensitive coordinates purged; payload sterile and locked.
};

/// @enum HeaterMode
/// @brief Operating mode for optical window de-icing and defogging heating elements.
enum class HeaterMode : std::uint8_t {
    Off,            ///< Heating elements disabled.
    ManualOn,       ///< Continuous heating active.
    AutoThermostat  ///< Automatically engages below temperature threshold.
};

/// @enum WiperMode
/// @brief Windshield wiper operational mode.
enum class WiperMode : std::uint8_t {
    Off,          ///< Wiper parked outside optical aperture.
    SingleWipe,   ///< Executes a single complete wiping stroke and parks.
    Continuous,   ///< Continuous cyclical wiping.
    IntervalWipe  ///< Wipes periodically at configured intervals.
};

/// @enum WiperState
/// @brief Real-time positional state of the wiper mechanism.
enum class WiperState : std::uint8_t {
    Parked,  ///< Wiper blade held in park position outside sensor FOV.
    Wiping,  ///< Wiper actively traversing optical window.
    Halted   ///< Wiper stopped mid-stroke (e.g. failure or emergency).
};

/// @enum WasherState
/// @brief Coordinated washer jet sequence phase.
enum class WasherState : std::uint8_t {
    Idle,           ///< Washer inactive.
    Spraying,       ///< Pressurized fluid jet actively spraying window.
    Soaking,        ///< Fluid dwell / soak delay prior to wiping.
    ClearingWipes,  ///< Multi-stroke wipe cycle clearing fluid and debris.
    Complete        ///< Cycle completed; wiper returning to park.
};

/// @enum ZeroizeReason
/// @brief Cause trigger for cryptographic and tactical mission coordinate zeroization.
enum class ZeroizeReason : std::uint8_t {
    OperatorCommand,     ///< Manual operator panic / purge command.
    TamperDetected,      ///< Hardware enclosure switch / tamper sensor triggered.
    HostileCaptureAlert, ///< Host platform geofence breach / capture condition.
    SystemDecommission   ///< Routine end-of-mission sterilization.
};

/// @struct StowOrientation
/// @brief Angular coordinate configuration for a designated gimbal stance.
struct StowOrientation {
    double panDeg { 180.0 };            ///< Azimuth angle in degrees (e.g. 180° = facing backward/dock).
    double tiltDeg { -90.0 };           ///< Elevation angle in degrees (e.g. -90° = pointing straight down/up into bay).
    double rollDeg { 0.0 };             ///< Roll angle in degrees.
    double arrivalToleranceDeg { 1.5 }; ///< Acceptance threshold for stance arrival in degrees.
    float slewSpeedRatio { 0.5f };      ///< Slew speed multiplier [0.1 to 1.0].
};

/// @struct WindowCleaningConfig
/// @brief Parameters for window de-icing, heating, wiping, and fluid washing.
struct WindowCleaningConfig {
    // Heater / De-Ice
    double autoDeIceOnTempC { 2.0 };        ///< Ambient temp below which auto-heating activates (°C).
    double autoDeIceOffTempC { 8.0 };       ///< Ambient temp above which auto-heating deactivates (°C).
    double maxContinuousHeatSec { 900.0 };  ///< Maximum continuous run time (15 mins) to prevent drain.

    // Wiper
    double wipeStrokeDurationSec { 1.2 };  ///< Time for one full forward/return wipe cycle (s).
    double intervalWipeDelaySec { 10.0 };  ///< Delay between wipes in IntervalWipe mode (s).

    // Washer Routine
    double washSprayDurationSec { 1.5 };       ///< Fluid spray duration (s).
    double washSoakDwellSec { 0.5 };           ///< Fluid dwell before wiping begins (s).
    std::uint32_t washWipeCycleCount { 4U };   ///< Number of wipe strokes to clear fluid.
    double fluidConsumptionPerWash01 { 0.02 }; ///< Fraction of total tank used per wash.
};

/// @struct StowControllerStatus
/// @brief Real-time status report of the stow controller and environmental clearing devices.
struct StowControllerStatus {
    StowState stowState { StowState::Deployed };
    bool vehicleMotionSafe { false };
    bool vehicleMotionActive { false };
    bool mechanicalLockEngaged { false };

    // Environmental / Cleaning
    HeaterMode heaterMode { HeaterMode::Off };
    bool heaterActive { false };
    double currentWindowTempC { 20.0 };

    WiperMode wiperMode { WiperMode::Off };
    WiperState wiperState { WiperState::Parked };

    WasherState washerState { WasherState::Idle };
    double washerFluidLevel01 { 1.0 };  ///< Remaining fluid [0.0 = Empty, 1.0 = Full].
    bool washerFluidLow { false };      ///< Warning when level <= 0.15.

    // Emergency & Zeroize
    bool isZeroized { false };
};

/// @class PayloadStowController
/// @brief Controller managing gimbal lifecycle stances, safety interlocks, de-icing, wiping, and emergency zeroization.
class PayloadStowController {
public:
    /// @brief Constructor binding gimbal, preset manager, and primary camera.
    /// @param[in] ptu Gimbal pan-tilt unit interface.
    /// @param[in] presetMgr Optional PTZ preset manager for zeroization and stowing.
    /// @param[in] primaryCamera Optional camera payload for optical zoom retraction.
    explicit PayloadStowController(
        std::shared_ptr<IPanTiltUnit> ptu,
        std::shared_ptr<IPtzPresetManager> presetMgr = nullptr,
        std::shared_ptr<ICameraPayload> primaryCamera = nullptr);

    /// @brief Destructor.
    ~PayloadStowController();

    // Non-copyable, movable
    PayloadStowController(const PayloadStowController&) = delete;
    PayloadStowController& operator=(const PayloadStowController&) = delete;
    PayloadStowController(PayloadStowController&&) noexcept = default;
    PayloadStowController& operator=(PayloadStowController&&) noexcept = default;

    // --- Orientations & Lifecycle Commands ---

    /// @brief Configures target angles for the docked Stow stance.
    /// @param[in] orientation Desired pan, tilt, roll, and speed.
    void setStowOrientation(const StowOrientation& orientation) noexcept;

    /// @brief Queries current Stow stance orientation configuration.
    [[nodiscard]] StowOrientation stowOrientation() const noexcept;

    /// @brief Configures target angles for the operational Deploy stance.
    /// @param[in] orientation Desired pan, tilt, roll, and speed.
    void setDeployOrientation(const StowOrientation& orientation) noexcept;

    /// @brief Queries current Deploy stance orientation configuration.
    [[nodiscard]] StowOrientation deployOrientation() const noexcept;

    /// @brief Configures target angles for the technician Maintenance stance.
    /// @param[in] orientation Desired pan, tilt, roll, and speed.
    void setMaintenanceOrientation(const StowOrientation& orientation) noexcept;

    /// @brief Queries current Maintenance stance orientation configuration.
    [[nodiscard]] StowOrientation maintenanceOrientation() const noexcept;

    /// @brief Initiates graceful stowing of the payload.
    /// @details Retracts optical zoom to wide and slews gimbal to StowOrientation.
    /// @return True if stow command was dispatched.
    bool stow();

    /// @brief Deploys the payload into operational field of regard.
    /// @details Releases mechanical locks if disarmed and slews to DeployOrientation.
    /// @return True if deploy command was accepted.
    bool deploy();

    /// @brief Commands the gimbal to slew to the maintenance stance.
    /// @return True if command was dispatched.
    bool moveToMaintenance();

    /// @brief Queries the current lifecycle stow state.
    [[nodiscard]] StowState state() const noexcept;

    /// @brief Checks if payload is fully docked in Stow stance.
    [[nodiscard]] bool isStowed() const noexcept;

    /// @brief Checks if payload is in operational Deployed stance.
    [[nodiscard]] bool isDeployed() const noexcept;

    // --- Vehicle Motion Safety Interlocks ---

    /// @brief Queries whether the payload is currently safe for host vehicle transit / maneuver.
    /// @return True only when in Stowed or EmergencyParked state.
    [[nodiscard]] bool isSafeForVehicleMotion() const noexcept;

    /// @brief Notifies the controller whether the host vehicle is currently in motion.
    /// @param[in] moving True if vehicle is cruising, flying, or sailing.
    void setVehicleMotionActive(bool moving) noexcept;

    /// @brief Checks whether the host vehicle is currently flagged as in motion.
    [[nodiscard]] bool isVehicleMotionActive() const noexcept;

    /// @brief Enables or disables automatic stowing when vehicle motion starts.
    /// @param[in] enable True to automatically stow on vehicle movement.
    void setAutoStowOnVehicleMotion(bool enable) noexcept;

    /// @brief Queries auto-stow on vehicle motion configuration.
    [[nodiscard]] bool isAutoStowOnVehicleMotionEnabled() const noexcept;

    /// @brief Enables or disables strict vehicle motion interlock.
    /// @param[in] enable True to reject deploy commands while vehicle is in motion.
    void setVehicleMotionInterlockEnabled(bool enable) noexcept;

    /// @brief Queries strict vehicle motion interlock status.
    [[nodiscard]] bool isVehicleMotionInterlockEnabled() const noexcept;

    // --- Mechanical Locking Pin / Gimbal Brake ---

    /// @brief Engages physical/solenoid mechanical gimbal lock.
    /// @return True if lock was engaged while stationary.
    bool engageMechanicalLock();

    /// @brief Disengages physical mechanical gimbal lock.
    /// @return True if released.
    bool releaseMechanicalLock();

    /// @brief Checks whether mechanical lock is currently engaged.
    [[nodiscard]] bool isMechanicalLockEngaged() const noexcept;

    // --- Window Heater & De-Ice Routine ---

    /// @brief Configures cleaning, wiper, and heater parameters.
    /// @param[in] config Window cleaning configuration structure.
    void setCleaningConfig(const WindowCleaningConfig& config) noexcept;

    /// @brief Queries current window cleaning configuration.
    [[nodiscard]] WindowCleaningConfig cleaningConfig() const noexcept;

    /// @brief Sets window heater / de-icing mode.
    /// @param[in] mode Off, ManualOn, or AutoThermostat.
    void setHeaterMode(HeaterMode mode) noexcept;

    /// @brief Queries active window heater mode.
    [[nodiscard]] HeaterMode heaterMode() const noexcept;

    /// @brief Checks if heating elements are currently energized.
    [[nodiscard]] bool isHeaterActive() const noexcept;

    /// @brief Updates current ambient / window temperature for thermostat logic.
    /// @param[in] tempC Ambient temperature in degrees Celsius.
    void updateAmbientTemperature(double tempC) noexcept;

    // --- Windshield Wiper Routine ---

    /// @brief Sets wiper operational mode.
    /// @param[in] mode Off, SingleWipe, Continuous, or IntervalWipe.
    void setWiperMode(WiperMode mode) noexcept;

    /// @brief Queries active wiper mode.
    [[nodiscard]] WiperMode wiperMode() const noexcept;

    /// @brief Queries real-time positional state of wiper blade.
    [[nodiscard]] WiperState wiperState() const noexcept;

    /// @brief Triggers an immediate single wipe stroke.
    /// @return True if wipe stroke was initiated.
    bool triggerSingleWipe();

    // --- Pressurized Washer Fluid Routine ---

    /// @brief Starts an autonomous fluid spray and clearing wipe sequence.
    /// @return True if washer routine was initiated.
    bool startWasherRoutine();

    /// @brief Aborts an active washer routine and parks the wiper.
    void cancelWasherRoutine();

    /// @brief Queries active washer routine state.
    [[nodiscard]] WasherState washerState() const noexcept;

    /// @brief Queries remaining washer fluid level [0.0 = Empty, 1.0 = Full].
    [[nodiscard]] double washerFluidLevel() const noexcept;

    /// @brief Refills washer fluid reservoir.
    /// @param[in] level01 Normalized fluid level [0.0 to 1.0].
    void refillWasherFluid(double level01 = 1.0) noexcept;

    // --- Emergency Park & Mission Zeroization ---

    /// @brief Commands immediate high-rate emergency park to stow pose.
    /// @return True if emergency park was dispatched.
    bool emergencyPark();

    /// @brief Triggers mission zeroization: purges coordinates and emergency parks.
    /// @param[in] reason Cause trigger for zeroization.
    /// @return True if zeroization routine was executed.
    bool zeroize(ZeroizeReason reason);

    /// @brief Checks if controller is currently in zeroized lock state.
    [[nodiscard]] bool isZeroized() const noexcept;

    /// @brief Administrative unlock resetting zeroized lock state.
    /// @return True if unlocked.
    bool resetZeroizeLock();

    // --- Periodic Update Loop ---

    /// @brief Advances timers, state transitions, and cleaning sequences.
    /// @param[in] dtSeconds Time delta in seconds.
    void update(double dtSeconds);

    // --- Status Telemetry ---

    /// @brief Retrieves comprehensive status snapshot.
    [[nodiscard]] StowControllerStatus status() const noexcept;

    // --- Callbacks ---

    /// @brief State transition observer callback.
    using StateChangedCallback = std::function<void(StowState prev, StowState current, const std::string& reason)>;

    /// @brief Interlock warning callback.
    using InterlockAlertCallback = std::function<void(bool safeForMotion, const std::string& msg)>;

    /// @brief Anti-tamper zeroization audit callback.
    using ZeroizeCallback = std::function<void(ZeroizeReason reason, const std::string& auditDetails)>;

    /// @brief Registers state transition callback.
    void setStateChangedCallback(StateChangedCallback cb);

    /// @brief Registers interlock alert callback.
    void setInterlockAlertCallback(InterlockAlertCallback cb);

    /// @brief Registers zeroize audit callback.
    void setZeroizeCallback(ZeroizeCallback cb);

private:
    mutable std::mutex m_mutex;

    std::shared_ptr<IPanTiltUnit> m_ptu {};
    std::shared_ptr<IPtzPresetManager> m_presetMgr {};
    std::shared_ptr<ICameraPayload> m_primaryCam {};

    // Stances
    StowOrientation m_stowPose {};
    StowOrientation m_deployPose { 0.0, 0.0, 0.0, 1.5, 1.0f };
    StowOrientation m_maintenancePose { 90.0, 0.0, 0.0, 1.5, 0.5f };

    StowState m_state { StowState::Deployed };
    bool m_vehicleMotionActive { false };
    bool m_autoStowOnVehicleMotion { false };
    bool m_vehicleMotionInterlockEnabled { true };
    bool m_mechanicalLockEngaged { false };
    bool m_isZeroized { false };

    // Window Cleaning
    WindowCleaningConfig m_cleaningConfig {};
    HeaterMode m_heaterMode { HeaterMode::Off };
    bool m_heaterActive { false };
    double m_currentWindowTempC { 20.0 };
    double m_heaterTimerSec { 0.0 };

    WiperMode m_wiperMode { WiperMode::Off };
    WiperState m_wiperState { WiperState::Parked };
    double m_wiperTimerSec { 0.0 };
    double m_wiperIntervalTimerSec { 0.0 };

    WasherState m_washerState { WasherState::Idle };
    double m_washerTimerSec { 0.0 };
    std::uint32_t m_currentWashWipeCycles { 0U };
    double m_washerFluidLevel01 { 1.0 };

    // Callbacks
    StateChangedCallback m_stateCallback {};
    InterlockAlertCallback m_interlockCallback {};
    ZeroizeCallback m_zeroizeCallback {};

    // Internal helpers
    [[nodiscard]] bool checkArrivalLocked(const StowOrientation& targetPose) const noexcept;
    void executeStowMoveLocked(const StowOrientation& targetPose, StowState transitioningState);
};

} // namespace PayloadHal
