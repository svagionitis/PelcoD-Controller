#pragma once

/// @file LaserDesignatorTypes.h
/// @brief Tactical Laser Target Designator (LTD), STANAG 3733 PRF, and Spot Tracker (LST) data structures.
/// @details Defines pulse repetition frequency (PRF) code formulas, capacitor bank states,
///          diode lumped thermal parameters, MIL-HDBK-828 hazard fan geometry, and LST quadrant tracking types.

#include "Klv/KlvTypes.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace PayloadHal {

/// @enum PrfBand
/// @brief NATO STANAG 3733 Laser Pulse Repetition Frequency (PRF) Band designation.
enum class PrfBand : std::uint8_t {
    BandI,  ///< Standard PRF: Codes 1111 - 1488 (10 Hz - 20 Hz)
    BandII  ///< High PRF: Codes 1511 - 1788 (20 Hz - 33.3 Hz)
};

/// @enum DesignatorState
/// @brief Operational state of the tactical Laser Target Designator.
enum class DesignatorState : std::uint8_t {
    Disarmed,       ///< Master arm open; laser drive power inhibited
    Armed,          ///< Master arm closed; safety interlocks verified
    Charging,       ///< Capacitor bank charging to operational fire voltage
    ReadyToFire,    ///< Capacitor bank fully charged; primed for pulse sequence
    Designating,    ///< Actively firing high-energy laser pulses at commanded PRF
    CoolingDown,    ///< Enforced thermal dwell interval following designation burst
    ThermalCutoff,  ///< Over-temperature emergency cutoff tripped (> 65°C)
    Fault           ///< Hardware or safety interlock fault
};

/// @enum CapacitorState
/// @brief Dielectric storage capacitor bank state.
enum class CapacitorState : std::uint8_t {
    Discharged,     ///< Energy depleted (safe voltage < 5V)
    Charging,       ///< High-voltage converter actively charging capacitor bank
    Ready,          ///< Reached required firing threshold (e.g. >= 800V)
    Dumping         ///< Internal bleed resistor active (emergency discharge)
};

/// @enum LtdThermalZone
/// @brief Thermal health regime for the laser pump diodes and Nd:YAG / fiber rod.
enum class LtdThermalZone : std::uint8_t {
    Nominal,        ///< Normal operating regime (< 50°C)
    Warning,        ///< Elevated temperature warning (50°C - 65°C)
    Critical        ///< Critical over-temperature cutoff (>= 65°C)
};

/// @enum LstState
/// @brief Operational tracking state of the Laser Spot Tracker (LST) seeker.
enum class LstState : std::uint8_t {
    Off,            ///< Seeker quadrant receiver inactive
    Searching,      ///< Scanning field-of-view for optical energy pulses
    Acquired,       ///< Spot detected with matching STANAG 3733 PRF code
    Tracking,       ///< Closed-loop servo slaving line-of-sight to laser spot
    Coasting,       ///< Spot temporarily obscured; predictive lead maintained
    Lost            ///< Spot lost beyond coast duration threshold
};

// =============================================================================
// STANAG 3733 Validation & Timing Helpers
// =============================================================================

namespace Stanag3733 {

    /// @brief Checks whether a 4-digit integer is a valid NATO STANAG 3733 PRF code (1111 - 1788).
    /// @param[in] code 4-digit decimal PRF code.
    /// @return True if valid per STANAG 3733 rules.
    [[nodiscard]] constexpr bool isValidCode(std::uint16_t code) noexcept
    {
        if (code < 1111U || code > 1788U) {
            return false;
        }
        const unsigned d1 = (code / 1000U) % 10U;
        const unsigned d2 = (code / 100U) % 10U;
        const unsigned d3 = (code / 10U) % 10U;
        const unsigned d4 = code % 10U;

        if (d1 != 1U) {
            return false;
        }
        if (d2 < 1U || d2 > 7U) {
            return false;
        }
        if (d3 < 1U || d3 > 8U) {
            return false;
        }
        if (d4 < 1U || d4 > 8U) {
            return false;
        }
        return true;
    }

    /// @brief Determines the PRF Band for a valid STANAG 3733 code.
    /// @param[in] code Valid 4-digit code.
    /// @return PrfBand::BandI or PrfBand::BandII.
    [[nodiscard]] constexpr PrfBand bandOf(std::uint16_t code) noexcept
    {
        const unsigned d2 = (code / 100U) % 10U;
        return (d2 <= 4U) ? PrfBand::BandI : PrfBand::BandII;
    }

    /// @brief Computes the nominal pulse repetition interval (PRI) in microseconds.
    /// @param[in] code Valid STANAG 3733 code.
    /// @return Pulse interval in microseconds [30,000.0 .. 100,000.0].
    [[nodiscard]] inline double pulseIntervalUs(std::uint16_t code) noexcept
    {
        if (!isValidCode(code)) {
            return 100000.0; // 10 Hz fallback
        }
        const unsigned d2 = (code / 100U) % 10U;
        const unsigned d3 = (code / 10U) % 10U;
        const unsigned d4 = code % 10U;

        if (d2 <= 4U) {
            // Band I: 50,000 to 100,000 us (20 Hz down to 10 Hz)
            const unsigned idx = ((d2 - 1U) * 64U) + ((d3 - 1U) * 8U) + (d4 - 1U);
            return 50000.0 + (static_cast<double>(idx) * (50000.0 / 255.0));
        } else {
            // Band II: 30,000 to 50,000 us (33.3 Hz down to 20 Hz)
            const unsigned idx = ((d2 - 5U) * 64U) + ((d3 - 1U) * 8U) + (d4 - 1U);
            return 30000.0 + (static_cast<double>(idx) * (20000.0 / 191.0));
        }
    }

    /// @brief Computes the pulse repetition frequency in Hertz.
    /// @param[in] code Valid STANAG 3733 code.
    /// @return Repetition rate in Hz [10.0 .. 33.33].
    [[nodiscard]] inline double pulseFrequencyHz(std::uint16_t code) noexcept
    {
        const double intervalUs = pulseIntervalUs(code);
        return (intervalUs > 0.0) ? (1000000.0 / intervalUs) : 10.0;
    }

} // namespace Stanag3733

// =============================================================================
// Telemetry & Data Structures
// =============================================================================

/// @struct LtdThermalTelemetry
/// @brief Instantaneous thermal health and duty-cycle budget telemetry.
struct LtdThermalTelemetry {
    double diodeTempC { 25.0 };                 ///< Laser diode junction temperature in Celsius
    double heatSinkTempC { 25.0 };              ///< Thermal heat sink temperature in Celsius
    double capacityUtilization01 { 0.0 };       ///< Ratio of current temp to max limit [0.0, 1.0]
    double burstTimeRemainingSec { 60.0 };      ///< Maximum allowed continuous designation time remaining
    double coolingTimeRemainingSec { 0.0 };     ///< Enforced cooldown duration before re-arming permitted
    LtdThermalZone zone { LtdThermalZone::Nominal }; ///< Current thermal safety zone
    std::uint32_t thermalTripCount { 0U };      ///< Lifetime over-temperature protection trip count
};

/// @struct CapacitorTelemetry
/// @brief Instantaneous capacitor bank electrical state.
struct CapacitorTelemetry {
    double voltageVolts { 0.0 };                ///< Measured capacitor bank potential in Volts
    double targetVoltageVolts { 850.0 };        ///< Fully charged firing threshold in Volts
    double energyJoules { 0.0 };                ///< Stored electrical energy (1/2 * C * V^2) in Joules
    double chargePercent { 0.0 };               ///< Normalized charge progress [0.0 .. 100.0%]
    CapacitorState state { CapacitorState::Discharged }; ///< Active charging state
};

/// @struct LaserHazardFan
/// @brief MIL-HDBK-828 Laser Safety Corridor and surface hazard footprint on terrain.
struct LaserHazardFan {
    double nohdMeters { 12000.0 };              ///< Nominal Ocular Hazard Distance (naked eye) in meters
    double enohdMeters { 35000.0 };             ///< Extended NOHD for magnifying optical binoculars in meters
    double bufferHalfAngleDeg { 2.0 };          ///< Lateral safety buffer half-angle in degrees
    Klv::GeoPoint3D targetCoordinate {};        ///< Designated target 3D coordinate (impact point)
    std::vector<Klv::GeoPoint2D> hazardPolygon {}; ///< Ground intersection hazard footprint boundary vertices
    double minSafeAttackHeadingDeg { 0.0 };     ///< Weapon delivery entry corridor minimum true heading
    double maxSafeAttackHeadingDeg { 0.0 };     ///< Weapon delivery entry corridor maximum true heading
    bool isDesignatorInhibited { false };       ///< True if pointing into keep-out zone or vehicle structure
};

/// @struct LstSpotMeasurement
/// @brief 4-Quadrant detector measurement of acquired laser energy spot.
struct LstSpotMeasurement {
    bool valid { false };                       ///< True if laser spot energy detected above optical threshold
    double errorX { 0.0 };                      ///< Normalized horizontal displacement [-1.0 (Left) .. +1.0 (Right)]
    double errorY { 0.0 };                      ///< Normalized vertical displacement [-1.0 (Down) .. +1.0 (Up)]
    std::uint16_t detectedPrfCode { 0U };       ///< Decoded pulse repetition code
    bool prfMatched { false };                  ///< True if detected code matches commanded seeker code
    double irradianceWattsPerM2 { 0.0 };        ///< Detected optical irradiance in Watts/m^2
    double snrDb { 0.0 };                       ///< Signal-to-Noise Ratio in decibels
    std::chrono::system_clock::time_point timestamp {}; ///< Acquisition timestamp
};

/// @struct DesignatorConfig
/// @brief Configuration and calibration parameters for the Laser Target Designator.
struct DesignatorConfig {
    std::uint16_t defaultPrfCode { 1688U };     ///< Default tactical PRF code (STANAG 3733)
    double pulseEnergyJoules { 0.080 };         ///< Optical pulse energy (80 mJ)
    double opticalEfficiency { 0.18 };          ///< Electrical-to-optical conversion efficiency (18%)
    double thermalCapacitanceJPerC { 450.0 };    ///< Laser diode/rod lumped thermal mass in J/°C
    double thermalDissipationWPerC { 8.5 };     ///< Heat sink convective cooling rate in W/°C
    double warningTempThresholdC { 50.0 };      ///< Elevated thermal warning threshold in °C
    double cutoffTempThresholdC { 65.0 };       ///< Critical emergency cutoff threshold in °C
    double safeRecoveryTempThresholdC { 45.0 }; ///< Temperature required before clearing thermal lockout
    double maxContinuousBurstSec { 60.0 };      ///< Maximum single designation burst time in seconds
    double enforcedCooldownRatio { 2.0 };       ///< Cooldown-to-burst time ratio (e.g. 2:1 cooldown)
    double capacitorCapacitanceFarads { 0.00045 }; ///< 450 uF high-voltage storage capacitor
    double firingVoltageVolts { 850.0 };        ///< Nominal discharge firing potential
    double chargingCurrentAmps { 0.15 };        ///< HV converter charge current
    double beamDivergenceMrad { 0.35 };         ///< Laser beam divergence in milliradians
    double safetyBufferAngleDeg { 2.5 };        ///< MIL-HDBK-828 lateral safety buffer angle in degrees
    double lstFieldOfViewDeg { 6.0 };           ///< LST quadrant seeker total instantaneous optical FOV
    double lstTrackingGain { 1.5 };             ///< Proportional servo rate gain for auto-cueing
    double lstCoastTimeoutSec { 2.0 };          ///< Maximum coast duration on beam loss before declaring lost
};

} // namespace PayloadHal
