#pragma once

#include <chrono>
#include <cmath>
#include <mutex>
#include <string>

namespace PayloadHal {

/// @enum OpticalBand
/// @brief Optical wavelength band for chromatic refraction and dispersion scaling.
enum class OpticalBand {
    Visible,    ///< Daylight Visible EO (nominal 0.55 um)
    Swir,       ///< Short-Wave Infrared (nominal 1.3 um)
    Mwir,       ///< Mid-Wave Infrared (nominal 4.0 um)
    Lwir,       ///< Long-Wave Infrared (nominal 10.0 um)
    Lrf1064nm,  ///< Nd:YAG Laser Rangefinder (1.064 um)
    Lrf1550nm   ///< Eye-Safe Erbium Glass LRF (1.55 um)
};

/// @struct AtmosphericEnvironment
/// @brief Barometric and meteorological environmental parameters for refractivity modeling.
struct AtmosphericEnvironment {
    double pressureHpa{1013.25};          ///< Barometric surface pressure in hPa/mbar (default 1013.25).
    double temperatureCelsius{15.0};      ///< Ambient temperature in degrees Celsius (default 15.0 C).
    double relativeHumidityPercent{50.0};    ///< Relative humidity in percentage [0, 100] (default 50%).
    double baseKFactor{1.17};             ///< Base effective Earth radius factor (default 1.17 for optical).
    bool autoComputeKFactor{true};        ///< If true, dynamically calculates kFactor from environment.

    /// @brief Checks if environmental parameters are physically reasonable.
    /// @return True if all values are within realistic meteorological limits.
    [[nodiscard]] bool isValid() const noexcept {
        return pressureHpa >= 300.0 && pressureHpa <= 1150.0 &&
               temperatureCelsius >= -60.0 && temperatureCelsius <= 70.0 &&
               relativeHumidityPercent >= 0.0 && relativeHumidityPercent <= 100.0 &&
               baseKFactor > 1.0 && baseKFactor < 2.0;
    }
};

/// @struct RefractionCorrectionResult
/// @brief Comprehensive solution containing angular and geometric refraction offsets.
struct RefractionCorrectionResult {
    double apparentElevationDeg{0.0};     ///< Apparent elevation angle as seen by sensor in degrees.
    double trueElevationDeg{0.0};         ///< True geometric line-of-sight elevation in degrees.
    double refractionAngleDeg{0.0};       ///< Angular refraction offset (apparent - true) in degrees.
    double earthCurvatureDropMeters{0.0};    ///< Geometric Earth drop below horizontal tangent in meters.
    double effectiveEarthDropMeters{0.0};    ///< Net drop combining Earth curvature and ray refraction in meters.
    double opticalHorizonRangeMeters{0.0};   ///< Maximum line-of-sight horizon distance in meters.
    bool targetBelowHorizon{false};       ///< True if target ground distance exceeds the optical horizon.
};

/// @class AtmosphericRefractionCompensator
/// @brief Atmospheric Refraction & Earth Curvature Optical Compensator.
/// @details Models WGS-84 Earth curvature, effective Earth radius (k-factor) ray bending,
///          wavelength-dependent atmospheric dispersion (Visible, SWIR, MWIR, LWIR, LRF),
///          apparent-to-true elevation conversions, and optical horizon line-of-sight analysis.
class AtmosphericRefractionCompensator {
public:
    /// @brief Mean Earth radius according to WGS-84 in meters.
    static constexpr double kEarthRadiusMeters = 6371008.8;

    /// @brief Constructs an atmospheric refraction compensator with given environment.
    /// @param env Atmospheric meteorological parameters.
    explicit AtmosphericRefractionCompensator(AtmosphericEnvironment env = {});

    /// @brief Destructor.
    ~AtmosphericRefractionCompensator() = default;

    /// @brief Updates atmospheric environmental conditions.
    /// @param env New atmospheric environment.
    void setEnvironment(const AtmosphericEnvironment& env);

    /// @brief Returns the current atmospheric environment.
    [[nodiscard]] AtmosphericEnvironment environment() const;

    /// @brief Computes atmospheric refractivity N = (n - 1) * 1e6 for the given optical band.
    /// @param band Optical wavelength band.
    /// @return Refractivity in N-units.
    [[nodiscard]] double refractivity(OpticalBand band = OpticalBand::Visible) const noexcept;

    /// @brief Computes optical index of refraction n for the given optical band.
    /// @param band Optical wavelength band.
    /// @return Refractive index (n > 1.0).
    [[nodiscard]] double refractiveIndex(OpticalBand band = OpticalBand::Visible) const noexcept;

    /// @brief Returns the effective Earth radius factor (k) for the given optical band.
    /// @param band Optical wavelength band.
    /// @return Effective Earth radius factor k (typically 1.15 to 1.25 for optical).
    [[nodiscard]] double effectiveKFactor(OpticalBand band = OpticalBand::Visible) const noexcept;

    /// @brief Converts apparent elevation (as observed by sensor) to true geometric elevation.
    /// @param apparentElevationDeg Apparent elevation angle in degrees.
    /// @param slantRangeMeters Slant range to target in meters.
    /// @param band Optical wavelength band.
    /// @return True geometric elevation angle in degrees.
    [[nodiscard]] double apparentToTrueElevation(
        double apparentElevationDeg,
        double slantRangeMeters,
        OpticalBand band = OpticalBand::Visible) const noexcept;

    /// @brief Converts true geometric elevation to apparent elevation (as observed by sensor).
    /// @param trueElevationDeg True geometric elevation angle in degrees.
    /// @param slantRangeMeters Slant range to target in meters.
    /// @param band Optical wavelength band.
    /// @return Apparent elevation angle in degrees.
    [[nodiscard]] double trueToApparentElevation(
        double trueElevationDeg,
        double slantRangeMeters,
        OpticalBand band = OpticalBand::Visible) const noexcept;

    /// @brief Computes angular refraction bending offset in degrees.
    /// @param slantRangeMeters Slant range in meters.
    /// @param elevationDeg Elevation angle in degrees.
    /// @param band Optical wavelength band.
    /// @return Refraction bending angle in degrees (>= 0).
    [[nodiscard]] double refractionAngle(
        double slantRangeMeters,
        double elevationDeg = 0.0,
        OpticalBand band = OpticalBand::Visible) const noexcept;

    /// @brief Calculates geometric Earth drop below horizontal tangent plane.
    /// @param groundDistanceMeters Ground distance in meters.
    /// @return Geometric drop in meters.
    [[nodiscard]] static double geometricEarthDrop(double groundDistanceMeters) noexcept;

    /// @brief Calculates effective Earth drop combining curvature and ray refraction.
    /// @param groundDistanceMeters Ground distance in meters.
    /// @param band Optical wavelength band.
    /// @return Effective drop in meters.
    [[nodiscard]] double effectiveEarthDrop(
        double groundDistanceMeters,
        OpticalBand band = OpticalBand::Visible) const noexcept;

    /// @brief Evaluates full refraction, curvature drop, and horizon status for an observation.
    /// @param observerAltitudeMsl Observer altitude above Mean Sea Level (MSL) in meters.
    /// @param apparentElevationDeg Apparent elevation observed by sensor in degrees.
    /// @param slantRangeMeters Slant range to target in meters.
    /// @param band Optical wavelength band.
    /// @return RefractionCorrectionResult solution.
    [[nodiscard]] RefractionCorrectionResult evaluateRefraction(
        double observerAltitudeMsl,
        double apparentElevationDeg,
        double slantRangeMeters,
        OpticalBand band = OpticalBand::Visible) const noexcept;

    /// @brief Computes maximum optical line-of-sight horizon distance.
    /// @param observerAltitudeMsl Observer altitude MSL in meters.
    /// @param targetAltitudeMsl Target altitude MSL in meters (default 0.0).
    /// @param band Optical wavelength band.
    /// @return Optical horizon distance in meters.
    [[nodiscard]] double opticalHorizonDistance(
        double observerAltitudeMsl,
        double targetAltitudeMsl = 0.0,
        OpticalBand band = OpticalBand::Visible) const noexcept;

    /// @brief Checks whether a target at a given distance and altitude is visible over the horizon.
    /// @param observerAltitudeMsl Observer altitude MSL in meters.
    /// @param targetAltitudeMsl Target altitude MSL in meters.
    /// @param groundDistanceMeters Ground distance between observer and target in meters.
    /// @param band Optical wavelength band.
    /// @return True if target is within line-of-sight, false if occluded below Earth curvature.
    [[nodiscard]] bool isTargetVisibleOverHorizon(
        double observerAltitudeMsl,
        double targetAltitudeMsl,
        double groundDistanceMeters,
        OpticalBand band = OpticalBand::Visible) const noexcept;

private:
    mutable std::mutex m_mutex;
    AtmosphericEnvironment m_env;

    [[nodiscard]] double computeWavelengthDispersion(OpticalBand band) const noexcept;
    [[nodiscard]] double computeBaseRefractivity() const noexcept;
};

} // namespace PayloadHal
