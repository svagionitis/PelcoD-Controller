#pragma once

/// @file VesselAttitudeCompensator.h
/// @brief Dynamic host vessel attitude tracking, multi-protocol ingestion, wave jitter filtering,
///        kinematic line-of-sight stabilization, and active horizon counter-roll calculations.

#include "GeoreferenceUtils.h"
#include "Klv/KlvTypes.h"
#include "PlatformLeverArmCompensator.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

// Forward declarations for NMEA types
namespace Nmea {
struct PashrData;
struct PfecAttitudeData;
struct XdrData;
struct AttitudeData;
namespace N2k {
struct Attitude;
} // namespace N2k
} // namespace Nmea

namespace PayloadHal {

/// @struct VesselAttitudeState
/// @brief Full kinematic attitude state of the host vessel.
struct VesselAttitudeState {
    double headingDeg { 0.0 };          ///< Compass heading / yaw in degrees [0.0 .. 360.0)
    double pitchDeg { 0.0 };            ///< Pitch angle in degrees (positive = bow up)
    double rollDeg { 0.0 };             ///< Roll angle in degrees (positive = starboard down)
    double heaveMeters { 0.0 };         ///< Vertical heave in meters (positive = up)
    double rateOfTurnDegPerSec { 0.0 }; ///< Yaw angular velocity (deg/s)
    double pitchRateDegPerSec { 0.0 };  ///< Pitch angular velocity (deg/s)
    double rollRateDegPerSec { 0.0 };   ///< Roll angular velocity (deg/s)
    std::chrono::steady_clock::time_point timestamp {};
    bool hasHeading { false };
    bool isValid { false };
};

/// @class VesselAttitudeCompensator
/// @brief Ingests dynamic vessel attitude from N2K/NMEA sources, applies low-pass smoothing,
///        evaluates timeouts, and computes compensated line-of-sight look angles and horizon roll.
class VesselAttitudeCompensator {
public:
    VesselAttitudeCompensator() = default;
    virtual ~VesselAttitudeCompensator() = default;

    // Non-copyable, non-movable
    VesselAttitudeCompensator(const VesselAttitudeCompensator&) = delete;
    VesselAttitudeCompensator& operator=(const VesselAttitudeCompensator&) = delete;
    VesselAttitudeCompensator(VesselAttitudeCompensator&&) = delete;
    VesselAttitudeCompensator& operator=(VesselAttitudeCompensator&&) = delete;

    // --- Ingestion APIs ---

    /// @brief Updates vessel attitude with raw angular state.
    /// @param[in] headingDeg Compass heading in degrees [0.0 .. 360.0).
    /// @param[in] pitchDeg Vessel pitch in degrees (positive = bow up).
    /// @param[in] rollDeg Vessel roll in degrees (positive = starboard down).
    /// @param[in] heaveMeters Vertical heave in meters.
    /// @param[in] now Timestamp of update.
    void updateAttitude(
        double headingDeg,
        double pitchDeg,
        double rollDeg,
        double heaveMeters = 0.0,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) noexcept;

    /// @brief Ingests NMEA 2000 PGN 127257 Attitude telemetry.
    /// @param[in] att N2K Attitude structure (yaw, pitch, roll in radians).
    /// @param[in] now Timestamp of update.
    void updateFromN2k(
        const Nmea::N2k::Attitude& att,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) noexcept;

    /// @brief Ingests NMEA 0183 $PASHR inertial attitude telemetry.
    /// @param[in] pashr Deserialized PashrData structure.
    /// @param[in] now Timestamp of update.
    void updateFromPashr(
        const Nmea::PashrData& pashr,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) noexcept;

    /// @brief Ingests FLIR proprietary $PFEC,GPatt attitude telemetry.
    /// @param[in] pfec Deserialized PfecAttitudeData structure.
    /// @param[in] now Timestamp of update.
    void updateFromPfec(
        const Nmea::PfecAttitudeData& pfec,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) noexcept;

    /// @brief Ingests NMEA 0183 $--XDR transducer telemetry (matches PITCH / ROLL transducers).
    /// @param[in] xdr Deserialized XdrData structure.
    /// @param[in] now Timestamp of update.
    void updateFromXdr(
        const Nmea::XdrData& xdr,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) noexcept;

    /// @brief Ingests unified NMEA attitude report.
    /// @param[in] att Deserialized AttitudeData structure.
    /// @param[in] now Timestamp of update.
    void updateFromAttitude(
        const Nmea::AttitudeData& att,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) noexcept;

    // --- State Queries ---

    /// @brief Retrieves the latest filtered vessel attitude state.
    /// @return Current VesselAttitudeState.
    [[nodiscard]] VesselAttitudeState attitudeState() const noexcept;

    /// @brief Checks whether attitude telemetry is active and within the valid timeout window.
    /// @param[in] now Current evaluation timestamp.
    /// @return True if attitude was updated within attitudeTimeout().
    [[nodiscard]] bool hasValidAttitude(
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const noexcept;

    /// @brief Sets maximum elapsed time before attitude telemetry is considered stale.
    /// @param[in] timeout Maximum valid age (default: 1000 ms).
    void setAttitudeTimeout(std::chrono::milliseconds timeout) noexcept;

    /// @brief Gets the attitude timeout duration.
    /// @return Timeout in milliseconds.
    [[nodiscard]] std::chrono::milliseconds attitudeTimeout() const noexcept;

    /// @brief Enables or disables exponential moving average (EMA) smoothing.
    /// @param[in] enabled True to enable smoothing filter.
    /// @param[in] alpha Filter weight [0.0 .. 1.0], where 1.0 means no smoothing.
    void setSmoothing(bool enabled, double alpha = 0.8) noexcept;

    /// @brief Checks whether smoothing is enabled.
    /// @return True if smoothing filter is active.
    [[nodiscard]] bool isSmoothingEnabled() const noexcept;

    // --- Kinematic Line-of-Sight & Leveling Calculations ---

    /// @brief Computes fully attitude-compensated gimbal look angles to acquire a 3D target.
    /// @details If attitude is stale or uninitialized, falls back gracefully to a 2D level model.
    /// @param[in] platformPos Platform geodetic position (GPS antenna).
    /// @param[in] targetPos Target geodetic 3D coordinate.
    /// @param[in] config Gimbal mounting lever-arm configuration.
    /// @param[in] now Timestamp to check attitude validity.
    /// @return Compensated GimbalLookAngles.
    [[nodiscard]] GimbalLookAngles compensateLookAngles(
        const Klv::GeoPoint3D& platformPos,
        const Klv::GeoPoint3D& targetPos,
        const PlatformLeverArmConfig& config,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const noexcept;

    /// @brief Computes the active 3-axis horizon counter-roll angle for the gimbal.
    /// @param[in] gimbalPanDeg Current or commanded gimbal pan angle in degrees.
    /// @param[in] gimbalTiltDeg Current or commanded gimbal tilt angle in degrees.
    /// @param[in] now Timestamp to check attitude validity.
    /// @return Counter-roll angle in degrees [-180.0, +180.0]. Returns 0.0 if attitude is invalid.
    [[nodiscard]] double computeHorizonRoll(
        double gimbalPanDeg,
        double gimbalTiltDeg,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const noexcept;

    /// @brief Clears attitude state and resets filtering history.
    void reset() noexcept;

private:
    void applyUpdate(
        double headingDeg,
        double pitchDeg,
        double rollDeg,
        double heaveMeters,
        bool hasHeading,
        std::chrono::steady_clock::time_point now) noexcept;

    mutable std::mutex m_mutex {};
    VesselAttitudeState m_state {};
    std::chrono::milliseconds m_timeout { 1000 };
    bool m_smoothingEnabled { false };
    double m_alpha { 0.8 };
};

} // namespace PayloadHal
