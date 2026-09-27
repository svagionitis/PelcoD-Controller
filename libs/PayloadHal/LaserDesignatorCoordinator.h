#pragma once

/// @file LaserDesignatorCoordinator.h
/// @brief Tactical Laser Target Designator (LTD), STANAG 3733 PRF, and Spot Tracker (LST) coordinator.
/// @details Orchestrates high-energy laser pulse generation, capacitor charging, lumped thermal dissipation,
///          MIL-HDBK-828 hazard fan projection, 4-quadrant spot tracking, and closed-loop gimbal slaving.

#include "IDemProvider.h"
#include "IPanTiltUnit.h"
#include "GimbalSectorBlanking.h"
#include "LaserDesignatorTypes.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace PayloadHal {

/// @class LaserDesignatorCoordinator
/// @brief Manages high-energy target designation, safety interlocks, thermal duty cycles,
///        MIL-HDBK-828 hazard corridors, and LST seeker slaving.
class LaserDesignatorCoordinator {
public:
    using DesignatorStateCallback = std::function<void(DesignatorState state, const std::string& reason)>;
    using LstStateCallback = std::function<void(LstState state, const LstSpotMeasurement& measurement)>;

    /// @brief Constructs the coordinator binding gimbal, DEM terrain provider, and sector blanking interlocks.
    /// @param[in] ptu Optional gimbal interface for line-of-sight orientation and LST auto-cueing.
    /// @param[in] dem Optional digital elevation model provider for terrain hazard fan intersection.
    /// @param[in] blanking Optional keep-out sector blanking engine for mechanical and laser safety.
    /// @param[in] config Operational and thermal calibration configuration.
    explicit LaserDesignatorCoordinator(
        std::shared_ptr<IPanTiltUnit> ptu = nullptr,
        std::shared_ptr<IDemProvider> dem = nullptr,
        std::shared_ptr<GimbalSectorBlanking> blanking = nullptr,
        DesignatorConfig config = {}) noexcept;

    virtual ~LaserDesignatorCoordinator();

    // Disable copy semantics to prevent hardware interlock aliasing
    LaserDesignatorCoordinator(const LaserDesignatorCoordinator&) = delete;
    LaserDesignatorCoordinator& operator=(const LaserDesignatorCoordinator&) = delete;
    LaserDesignatorCoordinator(LaserDesignatorCoordinator&&) noexcept = default;
    LaserDesignatorCoordinator& operator=(LaserDesignatorCoordinator&&) noexcept = default;

    // --- Configuration ---

    void setConfig(const DesignatorConfig& config);
    [[nodiscard]] DesignatorConfig config() const;

    // --- STANAG 3733 PRF Code Management ---

    /// @brief Sets the 4-digit NATO STANAG 3733 PRF code (1111 - 1788).
    /// @param[in] code Valid 4-digit code.
    /// @return True if code was accepted, false if invalid per STANAG 3733 specification.
    bool setPrfCode(std::uint16_t code);

    /// @brief Retrieves the active STANAG 3733 PRF code.
    [[nodiscard]] std::uint16_t prfCode() const noexcept;

    /// @brief Queries the PRF Band (Band I or Band II).
    [[nodiscard]] PrfBand prfBand() const noexcept;

    /// @brief Computes the nominal pulse repetition interval (PRI) in microseconds.
    [[nodiscard]] double pulseIntervalMicroseconds() const noexcept;

    /// @brief Computes the optical pulse repetition frequency in Hertz.
    [[nodiscard]] double pulseFrequencyHz() const noexcept;

    // --- Designator Lifecycle & Safety Interlocks ---

    /// @brief Arms the laser target designator and begins charging the capacitor bank.
    /// @return True if arming succeeded and safety interlocks were cleared.
    bool armDesignator();

    /// @brief Disarms the designator, halts active emission, and safely dumps capacitor energy.
    /// @return True if disarmed safely.
    bool disarmDesignator();

    /// @brief Queries whether the laser designator is currently armed.
    [[nodiscard]] bool isArmed() const noexcept;

    /// @brief Initiates high-energy laser target designation pulse firing.
    /// @details Fails immediately if not armed, capacitor not ready, or in keep-out sector.
    /// @return True if designation started.
    bool startDesignation();

    /// @brief Halts active designation pulse firing and enters thermal cooldown.
    /// @return True if designation was stopped.
    bool stopDesignation();

    /// @brief Queries whether the laser is actively designating.
    [[nodiscard]] bool isDesignating() const noexcept;

    /// @brief Queries current operational designator state.
    [[nodiscard]] DesignatorState designatorState() const noexcept;

    /// @brief Triggers immediate emergency discharge of the high-voltage capacitor bank.
    void triggerEmergencyDump();

    // --- Thermal & Electrical Physics Simulation ---

    /// @brief Advances thermal dissipation, capacitor charging, and LST tracking dynamics.
    /// @param[in] deltaSeconds Elapsed time step in seconds.
    /// @param[in] ambientTempC Ambient air temperature in Celsius (default 25.0°C).
    void update(double deltaSeconds, double ambientTempC = 25.0);

    /// @brief Retrieves current thermal health and duty-cycle budget telemetry.
    [[nodiscard]] LtdThermalTelemetry thermalTelemetry() const noexcept;

    /// @brief Retrieves current capacitor bank potential and energy telemetry.
    [[nodiscard]] CapacitorTelemetry capacitorTelemetry() const noexcept;

    // --- Laser Safety Hazard Fan & Terrain Footprint ---

    /// @brief Computes the MIL-HDBK-828 Laser Hazard Fan and surface ground polygon on terrain.
    /// @param[in] platformPos Platform 3D coordinate (latitude, longitude, altitude MSL).
    /// @param[in] platformHeadingDeg Platform true compass heading in degrees [0, 360).
    /// @return Populated LaserHazardFan structure with NOHD, footprint polygon, and entry corridors.
    [[nodiscard]] LaserHazardFan computeHazardFan(
        const Klv::GeoPoint3D& platformPos, double platformHeadingDeg) const;

    // --- Laser Spot Tracker (LST) Coordination ---

    /// @brief Sets the STANAG 3733 PRF code that the seeker is searching for.
    /// @param[in] code Commanded PRF code.
    void setLstTargetPrfCode(std::uint16_t code);

    /// @brief Queries the seeker commanded PRF code.
    [[nodiscard]] std::uint16_t lstTargetPrfCode() const noexcept;

    /// @brief Activates the Laser Spot Tracker seeker search mode.
    void startLstSearch();

    /// @brief Deactivates the Laser Spot Tracker seeker.
    void stopLstSearch();

    /// @brief Queries current LST seeker state (Off, Searching, Acquired, Tracking, Coasting, Lost).
    [[nodiscard]] LstState lstState() const noexcept;

    /// @brief Retrieves the latest quadrant measurement, if valid.
    [[nodiscard]] std::optional<LstSpotMeasurement> currentSpotMeasurement() const noexcept;

    /// @brief Injects synthetic spot detection parameters (for simulation and unit testing).
    /// @param[in] errorX Horizontal displacement [-1.0 .. +1.0].
    /// @param[in] errorY Vertical displacement [-1.0 .. +1.0].
    /// @param[in] detectedCode Detected optical PRF code.
    /// @param[in] irradianceWattsPerM2 Spot optical irradiance.
    /// @param[in] snrDb Signal to noise ratio.
    void injectSimulatedSpot(double errorX, double errorY, std::uint16_t detectedCode,
                             double irradianceWattsPerM2 = 1.5e-3, double snrDb = 24.0);

    /// @brief Injects raw 4-quadrant photodiode energy readings (A, B, C, D).
    /// @param[in] a Top-Left quadrant intensity.
    /// @param[in] b Top-Right quadrant intensity.
    /// @param[in] c Bottom-Left quadrant intensity.
    /// @param[in] d Bottom-Right quadrant intensity.
    /// @param[in] detectedCode Detected PRF code.
    void inject4QuadrantEnergies(double a, double b, double c, double d, std::uint16_t detectedCode);

    /// @brief Clears active spot detection signal.
    void clearSimulatedSpot();

    /// @brief Enables or disables autonomous closed-loop line-of-sight auto-cueing.
    /// @param[in] enable True to drive PTU towards detected laser spot.
    void setAutoCueingEnabled(bool enable) noexcept;

    /// @brief Queries whether auto-cueing servo slaving is active.
    [[nodiscard]] bool isAutoCueingEnabled() const noexcept;

    // --- Callbacks ---

    void setDesignatorStateCallback(DesignatorStateCallback cb);
    void setLstStateCallback(LstStateCallback cb);

private:
    void transitionDesignatorState(DesignatorState newState, const std::string& reason);
    void transitionLstState(LstState newState);
    [[nodiscard]] bool isKeepOutInhibited() const noexcept;

    std::shared_ptr<IPanTiltUnit> m_ptu {};
    std::shared_ptr<IDemProvider> m_dem {};
    std::shared_ptr<GimbalSectorBlanking> m_blanking {};

    mutable std::recursive_mutex m_mutex;
    DesignatorConfig m_config {};

    // Designator State
    std::uint16_t m_prfCode { 1688U };
    DesignatorState m_state { DesignatorState::Disarmed };
    double m_burstDurationSec { 0.0 };
    double m_cooldownRemainingSec { 0.0 };
    std::uint32_t m_thermalTripCount { 0U };

    // Thermal Simulation
    double m_diodeTempC { 25.0 };
    double m_heatSinkTempC { 25.0 };

    // Capacitor Bank
    CapacitorState m_capState { CapacitorState::Discharged };
    double m_capVoltageVolts { 0.0 };

    // Laser Spot Tracker (LST)
    std::uint16_t m_lstTargetCode { 1688U };
    LstState m_lstState { LstState::Off };
    std::optional<LstSpotMeasurement> m_currentSpot {};
    double m_coastTimerSec { 0.0 };
    bool m_autoCueingEnabled { false };

    // Callbacks
    DesignatorStateCallback m_stateCallback {};
    LstStateCallback m_lstCallback {};
};

} // namespace PayloadHal
