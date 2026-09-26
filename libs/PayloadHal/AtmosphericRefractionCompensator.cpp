#include "AtmosphericRefractionCompensator.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kEpsilon = 1e-9;

template <typename T>
T clampVal(T val, T minVal, T maxVal) {
    return std::max(minVal, std::min(val, maxVal));
}

} // namespace

AtmosphericRefractionCompensator::AtmosphericRefractionCompensator(AtmosphericEnvironment env)
    : m_env(env) {}

void AtmosphericRefractionCompensator::setEnvironment(const AtmosphericEnvironment& env) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (env.isValid()) {
        m_env = env;
    }
}

AtmosphericEnvironment AtmosphericRefractionCompensator::environment() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_env;
}

double AtmosphericRefractionCompensator::computeWavelengthDispersion(OpticalBand band) const noexcept {
    // Central wavelength lambda in micrometers
    double lambdaUm = 0.55;
    switch (band) {
        case OpticalBand::Visible:   lambdaUm = 0.55; break;
        case OpticalBand::Swir:      lambdaUm = 1.30; break;
        case OpticalBand::Mwir:      lambdaUm = 4.00; break;
        case OpticalBand::Lwir:      lambdaUm = 10.00; break;
        case OpticalBand::Lrf1064nm: lambdaUm = 1.064; break;
        case OpticalBand::Lrf1550nm: lambdaUm = 1.55; break;
    }

    // Edlén / Ciddor dispersion function: sigma = 1 / lambda (in um^-1)
    const double sigma = 1.0 / lambdaUm;
    const double sigmaSq = sigma * sigma;

    const double nDisp = 0.05792105 + (1.67917 / (57.362 - sigmaSq)) + (0.05451 / (167.9 - sigmaSq));

    // Reference dispersion at visible wavelength (0.55 um):
    // sigma = 1 / 0.55, sigma^2 = 3.305785
    constexpr double kNDispRef = 0.08931563;
    return nDisp / kNDispRef;
}

double AtmosphericRefractionCompensator::computeBaseRefractivity() const noexcept {
    const double tk = m_env.temperatureCelsius + 273.15;
    const double p = m_env.pressureHpa;
    const double h = m_env.relativeHumidityPercent;

    if (tk <= kEpsilon) {
        return 315.0;
    }

    // Dry air refractivity component
    const double nDry = 77.6 * (p / tk);

    // Water vapor saturation pressure via Magnus-Tetens formula (hPa)
    const double tc = m_env.temperatureCelsius;
    const double es = 6.1121 * std::exp((17.502 * tc) / (240.97 + tc));
    const double e = (h / 100.0) * es;

    // Wet refractivity component
    const double nWet = 72.0 * (e / tk) + 3.75e5 * (e / (tk * tk));

    return nDry + nWet;
}

double AtmosphericRefractionCompensator::refractivity(OpticalBand band) const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    const double baseN = computeBaseRefractivity();
    const double dispersion = computeWavelengthDispersion(band);
    return baseN * dispersion;
}

double AtmosphericRefractionCompensator::refractiveIndex(OpticalBand band) const noexcept {
    return 1.0 + refractivity(band) * 1e-6;
}

double AtmosphericRefractionCompensator::effectiveKFactor(OpticalBand band) const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    const double dispersion = computeWavelengthDispersion(band);

    if (!m_env.autoComputeKFactor) {
        // Scale user-specified base kFactor by dispersion
        return 1.0 + (m_env.baseKFactor - 1.0) * dispersion;
    }

    const double tk = m_env.temperatureCelsius + 273.15;
    const double p = m_env.pressureHpa;

    // Optical vertical refractive index gradient dn/dz (approx -2.5e-8 m^-1 at sea level)
    const double tempFactor = (288.15 / tk) * (288.15 / tk);
    const double pressFactor = p / 1013.25;
    const double dnDz = -2.5e-8 * pressFactor * tempFactor * dispersion;

    const double denominator = 1.0 + kEarthRadiusMeters * dnDz;
    if (denominator <= 0.1) {
        return 1.333; // Fallback
    }

    const double kCalc = 1.0 / denominator;
    return clampVal(kCalc, 1.05, 1.35);
}

double AtmosphericRefractionCompensator::refractionAngle(
    double slantRangeMeters,
    double elevationDeg,
    OpticalBand band) const noexcept {

    if (slantRangeMeters <= kEpsilon) {
        return 0.0;
    }

    const double elRad = elevationDeg * kDegToRad;
    const double d = std::max(0.0, slantRangeMeters * std::cos(elRad));
    const double k = effectiveKFactor(band);

    if (k <= 1.0) {
        return 0.0;
    }

    // Bending angle in radians: delta_theta = d * (k - 1) / (2 * k * R_E)
    const double deltaThetaRad = (d * (k - 1.0)) / (2.0 * k * kEarthRadiusMeters);
    return deltaThetaRad * kRadToDeg;
}

double AtmosphericRefractionCompensator::apparentToTrueElevation(
    double apparentElevationDeg,
    double slantRangeMeters,
    OpticalBand band) const noexcept {

    const double deltaEl = refractionAngle(slantRangeMeters, apparentElevationDeg, band);
    return apparentElevationDeg - deltaEl;
}

double AtmosphericRefractionCompensator::trueToApparentElevation(
    double trueElevationDeg,
    double slantRangeMeters,
    OpticalBand band) const noexcept {

    double appEl = trueElevationDeg;
    for (int i = 0; i < 2; ++i) {
        appEl = trueElevationDeg + refractionAngle(slantRangeMeters, appEl, band);
    }
    return appEl;
}

double AtmosphericRefractionCompensator::geometricEarthDrop(double groundDistanceMeters) noexcept {
    if (groundDistanceMeters <= 0.0) {
        return 0.0;
    }
    // h_drop = d^2 / (2 * R_E)
    return (groundDistanceMeters * groundDistanceMeters) / (2.0 * kEarthRadiusMeters);
}

double AtmosphericRefractionCompensator::effectiveEarthDrop(
    double groundDistanceMeters,
    OpticalBand band) const noexcept {

    if (groundDistanceMeters <= 0.0) {
        return 0.0;
    }
    const double k = effectiveKFactor(band);
    return (groundDistanceMeters * groundDistanceMeters) / (2.0 * k * kEarthRadiusMeters);
}

double AtmosphericRefractionCompensator::opticalHorizonDistance(
    double observerAltitudeMsl,
    double targetAltitudeMsl,
    OpticalBand band) const noexcept {

    const double hObs = std::max(0.0, observerAltitudeMsl);
    const double hTgt = std::max(0.0, targetAltitudeMsl);
    const double k = effectiveKFactor(band);

    const double twoKeRe = 2.0 * k * kEarthRadiusMeters;
    return std::sqrt(twoKeRe * hObs) + std::sqrt(twoKeRe * hTgt);
}

bool AtmosphericRefractionCompensator::isTargetVisibleOverHorizon(
    double observerAltitudeMsl,
    double targetAltitudeMsl,
    double groundDistanceMeters,
    OpticalBand band) const noexcept {

    const double horizonDist = opticalHorizonDistance(observerAltitudeMsl, targetAltitudeMsl, band);
    return groundDistanceMeters <= horizonDist;
}

RefractionCorrectionResult AtmosphericRefractionCompensator::evaluateRefraction(
    double observerAltitudeMsl,
    double apparentElevationDeg,
    double slantRangeMeters,
    OpticalBand band) const noexcept {

    RefractionCorrectionResult res;
    res.apparentElevationDeg = apparentElevationDeg;

    const double elRad = apparentElevationDeg * kDegToRad;
    const double groundDistance = std::max(0.0, slantRangeMeters * std::cos(elRad));

    res.refractionAngleDeg = refractionAngle(slantRangeMeters, apparentElevationDeg, band);
    res.trueElevationDeg = apparentElevationDeg - res.refractionAngleDeg;

    res.earthCurvatureDropMeters = geometricEarthDrop(groundDistance);
    res.effectiveEarthDropMeters = effectiveEarthDrop(groundDistance, band);

    // Target altitude estimated from elevation and drop
    const double targetAltEst = observerAltitudeMsl + slantRangeMeters * std::sin(elRad) - res.effectiveEarthDropMeters;
    res.opticalHorizonRangeMeters = opticalHorizonDistance(observerAltitudeMsl, std::max(0.0, targetAltEst), band);

    res.targetBelowHorizon = (groundDistance > res.opticalHorizonRangeMeters);

    return res;
}

} // namespace PayloadHal
