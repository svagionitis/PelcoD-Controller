#pragma once

/// @file ThermalTuningAdvisor.h
/// @brief Atmospheric and Sea State Sensor Analyzer for Thermal Camera Tuning.

#include "NmeaTypes.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace Nmea {

/// @enum ThermalAgcPreset
/// @brief Recommended Automatic Gain Control (AGC) and image processing profile for thermal camera cores.
enum class ThermalAgcPreset : std::uint8_t {
    DefaultNormal = 0, ///< Balanced dynamic range for standard maritime conditions
    CrossoverEnhanced, ///< High contrast plateau equalization for low delta-T thermal crossover
    MarineFogPenetration, ///< High-pass spatial filtering and boosted detail for fog/humidity
    SeaClutterSuppression ///< Elevated temporal noise filtering for whitecap wave clutter
};

/// @enum ThermalPolarityAdvice
/// @brief Recommended thermal display polarity based on background water temperature.
enum class ThermalPolarityAdvice : std::uint8_t {
    WhiteHot = 0, ///< Targets hotter than background appear white
    BlackHot ///< Targets colder than background appear black
};

/// @struct ThermalTuningAdvice
/// @brief Recommended camera settings synthesized from current maritime atmospheric physics.
struct ThermalTuningAdvice {
    ThermalAgcPreset agcPreset { ThermalAgcPreset::DefaultNormal };
    double ddeStrengthPercent { 30.0 }; ///< Recommended Digital Detail Enhancement [0.0 .. 100.0]
    ThermalPolarityAdvice polarity { ThermalPolarityAdvice::WhiteHot };
    double washoutRiskScore { 0.0 }; ///< Thermal crossover washout risk [0.0 = Minimal, 1.0 = Extreme]
    double fogRiskScore { 0.0 }; ///< Marine fog / condensation risk [0.0 = Minimal, 1.0 = Dense Fog]
    std::string rationale {}; ///< Human-readable explanation of active environmental recommendation
    std::chrono::steady_clock::time_point timestamp {};
};

/// @class ThermalTuningAdvisor
/// @brief Analyzes MTW, MDA, MMB meteorological data to dynamically advise thermal camera image pipelines.
class ThermalTuningAdvisor {
public:
    using AdviceCallback = std::function<void(const ThermalTuningAdvice&)>;
    using SnapshotCallback = std::function<void(const NmeaEnvironmentSnapshot&)>;

    ThermalTuningAdvisor() = default;
    ~ThermalTuningAdvisor() = default;

    // Non-copyable, movable
    ThermalTuningAdvisor(const ThermalTuningAdvisor&) = delete;
    ThermalTuningAdvisor& operator=(const ThermalTuningAdvisor&) = delete;
    ThermalTuningAdvisor(ThermalTuningAdvisor&&) noexcept = default;
    ThermalTuningAdvisor& operator=(ThermalTuningAdvisor&&) noexcept = default;

    /// @brief Ingests Mean Water Temperature ($--MTW).
    /// @param[in] mtw Deserialized MtwData.
    void ingestMtw(const MtwData& mtw);

    /// @brief Ingests Barometric Pressure ($--MMB).
    /// @param[in] mmb Deserialized MmbData.
    void ingestMmb(const MmbData& mmb);

    /// @brief Ingests Meteorological Composite Data ($--MDA).
    /// @param[in] mda Deserialized MdaData.
    void ingestMda(const MdaData& mda);

    /// @brief Manually updates environmental parameters.
    /// @param[in] waterTempC Optional water temperature in Celsius.
    /// @param[in] airTempC Optional air temperature in Celsius.
    /// @param[in] relHumPercent Optional relative humidity percentage.
    /// @param[in] dewPointC Optional dew point in Celsius.
    /// @param[in] windSpeedKts Optional wind speed in knots.
    void updateEnvironment(std::optional<double> waterTempC, std::optional<double> airTempC,
        std::optional<double> relHumPercent, std::optional<double> dewPointC, std::optional<double> windSpeedKts);

    /// @brief Retrieves the current environmental snapshot.
    [[nodiscard]] NmeaEnvironmentSnapshot snapshot() const;

    /// @brief Retrieves the current thermal tuning advice.
    [[nodiscard]] ThermalTuningAdvice advice() const;

    /// @brief Subscribes to new thermal tuning advice updates.
    void setAdviceCallback(AdviceCallback cb);

    /// @brief Subscribes to environmental snapshot updates.
    void setSnapshotCallback(SnapshotCallback cb);

private:
    void recomputeAdviceLocked();

    mutable std::mutex m_mutex {};
    NmeaEnvironmentSnapshot m_snapshot {};
    ThermalTuningAdvice m_currentAdvice {};

    AdviceCallback m_adviceCallback {};
    SnapshotCallback m_snapshotCallback {};
};

} // namespace Nmea
