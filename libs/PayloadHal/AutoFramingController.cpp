#include "AutoFramingController.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace PayloadHal {

namespace {
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kRadToDeg = 180.0 / kPi;
    constexpr double kDegToRad = kPi / 180.0;
} // namespace

AutoFramingController::AutoFramingController(const AutoFramingConfig& config)
    : m_config(config)
{
}

void AutoFramingController::setConfig(const AutoFramingConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

AutoFramingConfig AutoFramingController::config() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

double AutoFramingController::calculateApparentWidth(
    double lengthMeters, double beamMeters, double aspectAngleDeg) noexcept
{
    double angle = std::fmod(std::abs(aspectAngleDeg), 360.0);
    if (angle > 180.0) {
        angle = 360.0 - angle;
    }
    const double rad = angle * kDegToRad;
    const double sinVal = std::abs(std::sin(rad));
    const double cosVal = std::abs(std::cos(rad));
    return lengthMeters * sinVal + beamMeters * cosVal;
}

double AutoFramingController::calculateDesiredHfov(
    double slantRangeMeters, double targetDimensionMeters, double occupancyRatio) noexcept
{
    const double r = std::max(slantRangeMeters, 1.0);
    const double l = std::max(targetDimensionMeters, 0.5);
    const double occ = std::clamp(occupancyRatio, 0.05, 0.95);

    // Target subtended angle in radians: 2 * atan( (L / 2) / R )
    const double targetAngularSpanRad = 2.0 * std::atan2(l * 0.5, r);
    const double targetAngularSpanDeg = targetAngularSpanRad * kRadToDeg;

    // Desired HFOV to fill occupancyRatio of frame
    return targetAngularSpanDeg / occ;
}

double AutoFramingController::calculateFramingHfov(
    double slantRangeMeters, double apparentWidthMeters, double apparentHeightMeters,
    double widthRatio, double heightRatio, double aspectRatio) noexcept
{
    const double hfovWidth = calculateDesiredHfov(slantRangeMeters, apparentWidthMeters, widthRatio);
    const double vfovDesired = calculateDesiredHfov(slantRangeMeters, apparentHeightMeters, heightRatio);
    const double hfovHeight = vfovDesired * std::max(aspectRatio, 0.1);

    return std::max(hfovWidth, hfovHeight);
}

double AutoFramingController::calculateNormalizedZoom(double slantRangeMeters, double targetDimensionMeters,
    double occupancyRatio, double wideHfovDeg, double teleHfovDeg) noexcept
{
    const double wide = std::max(wideHfovDeg, teleHfovDeg + 0.1);
    const double tele = std::max(teleHfovDeg, 0.1);

    const double desiredHfov = calculateDesiredHfov(slantRangeMeters, targetDimensionMeters, occupancyRatio);
    const double clampedHfov = std::clamp(desiredHfov, tele, wide);

    // Optical focal length is inversely proportional to tan(HFOV / 2)
    const double tanWide = std::tan(wide * 0.5 * kDegToRad);
    const double tanTele = std::tan(tele * 0.5 * kDegToRad);
    const double tanTarget = std::tan(clampedHfov * 0.5 * kDegToRad);

    if (std::abs(tanWide - tanTele) < 1e-6) {
        return 0.0;
    }

    const double invTele = 1.0 / tanTele;
    const double invWide = 1.0 / tanWide;
    const double invTarget = 1.0 / std::max(tanTarget, 1e-6);

    const double normalized = (invTarget - invWide) / (invTele - invWide);
    return std::clamp(normalized, 0.0, 1.0);
}

double AutoFramingController::calculateNormalizedZoom(
    double desiredHfovDeg, double wideHfovDeg, double teleHfovDeg,
    LensZoomCurveType curveType) noexcept
{
    const double wide = std::max(wideHfovDeg, teleHfovDeg + 0.1);
    const double tele = std::max(teleHfovDeg, 0.1);
    const double clampedHfov = std::clamp(desiredHfovDeg, tele, wide);

    if (curveType == LensZoomCurveType::LinearHfov) {
        const double normalized = (wide - clampedHfov) / (wide - tele);
        return std::clamp(normalized, 0.0, 1.0);
    }

    // Logarithmic focal growth model: f(z) = f_wide * (f_tele / f_wide)^z
    const double tanWide = std::tan(wide * 0.5 * kDegToRad);
    const double tanTele = std::tan(tele * 0.5 * kDegToRad);
    const double tanTarget = std::tan(clampedHfov * 0.5 * kDegToRad);

    if (std::abs(tanWide - tanTele) < 1e-6) {
        return 0.0;
    }

    const double invWide = 1.0 / tanWide;
    const double invTele = 1.0 / tanTele;
    const double invTarget = 1.0 / std::max(tanTarget, 1e-6);

    const double ratioTotal = invTele / invWide;
    const double ratioCurrent = invTarget / invWide;

    if (ratioTotal <= 1.0 || ratioCurrent <= 1.0) {
        return 0.0;
    }

    const double normalized = std::log(ratioCurrent) / std::log(ratioTotal);
    return std::clamp(normalized, 0.0, 1.0);
}

double AutoFramingController::estimateTargetDimension(const Nmea::AisDimensions& dimensions) const noexcept
{
    const auto len = dimensions.lengthMeters();
    if (len > 0U) {
        return std::clamp(
            static_cast<double>(len), m_config.minTargetDimensionMeters, m_config.maxTargetDimensionMeters);
    }
    return m_config.defaultTargetLengthMeters;
}

TargetPhysicalEnvelope AutoFramingController::estimateTargetEnvelope(
    const Nmea::AisDimensions& dimensions,
    std::optional<std::uint8_t> shipType) const noexcept
{
    TargetPhysicalEnvelope env {};
    const auto len = dimensions.lengthMeters();
    const auto beam = dimensions.beamMeters();

    if (len > 0U) {
        env.lengthMeters = static_cast<double>(len);
        env.beamMeters = (beam > 0U) ? static_cast<double>(beam) : std::max(env.lengthMeters / 6.0, 4.0);
        env.heightMeters = std::clamp(env.beamMeters * 0.6, 3.0, 35.0);
    } else if (shipType.has_value()) {
        const std::uint8_t st = *shipType;
        if (st >= 70U && st <= 79U) { // Cargo
            env.lengthMeters = 140.0;
            env.beamMeters = 22.0;
            env.heightMeters = 18.0;
        } else if (st >= 80U && st <= 89U) { // Tanker
            env.lengthMeters = 180.0;
            env.beamMeters = 28.0;
            env.heightMeters = 16.0;
        } else if (st >= 60U && st <= 69U) { // Passenger
            env.lengthMeters = 120.0;
            env.beamMeters = 20.0;
            env.heightMeters = 25.0;
        } else if (st == 30U) { // Fishing
            env.lengthMeters = 25.0;
            env.beamMeters = 7.0;
            env.heightMeters = 6.0;
        } else if (st == 52U) { // Tug
            env.lengthMeters = 32.0;
            env.beamMeters = 10.0;
            env.heightMeters = 8.0;
        } else if (st >= 40U && st <= 49U) { // High Speed Craft
            env.lengthMeters = 40.0;
            env.beamMeters = 11.0;
            env.heightMeters = 7.0;
        } else if (st == 50U || st == 51U) { // Pilot / SAR
            env.lengthMeters = 20.0;
            env.beamMeters = 6.0;
            env.heightMeters = 5.0;
        } else {
            env.lengthMeters = m_config.defaultTargetLengthMeters;
            env.beamMeters = m_config.defaultTargetBeamMeters;
            env.heightMeters = m_config.defaultTargetHeightMeters;
        }
    } else {
        env.lengthMeters = m_config.defaultTargetLengthMeters;
        env.beamMeters = m_config.defaultTargetBeamMeters;
        env.heightMeters = m_config.defaultTargetHeightMeters;
    }

    env.lengthMeters = std::clamp(env.lengthMeters, m_config.minTargetDimensionMeters, m_config.maxTargetDimensionMeters);
    env.beamMeters = std::clamp(env.beamMeters, 1.0, m_config.maxTargetDimensionMeters * 0.5);
    env.heightMeters = std::clamp(env.heightMeters, 1.0, 50.0);

    return env;
}

TargetFramingMetrics AutoFramingController::evaluateFraming(
    double slantRangeMeters, const TargetPhysicalEnvelope& envelope,
    double targetTrueHeadingDeg, double cameraAzimuthDeg) const noexcept
{
    TargetFramingMetrics metrics {};
    double relAspect = std::abs(targetTrueHeadingDeg - cameraAzimuthDeg);
    relAspect = std::fmod(relAspect, 360.0);
    if (relAspect > 180.0) {
        relAspect = 360.0 - relAspect;
    }
    metrics.relativeAspectAngleDeg = relAspect;

    metrics.apparentWidthMeters = calculateApparentWidth(
        envelope.lengthMeters, envelope.beamMeters, relAspect);
    metrics.apparentHeightMeters = envelope.heightMeters;

    const double clampedRange = std::clamp(
        slantRangeMeters, m_config.minSlantRangeMeters, m_config.maxSlantRangeMeters);

    metrics.desiredHfovDeg = calculateFramingHfov(
        clampedRange, metrics.apparentWidthMeters, metrics.apparentHeightMeters,
        m_config.targetFrameOccupancyRatio, m_config.targetVerticalOccupancyRatio,
        m_config.frameAspectRatio);

    metrics.targetNormalizedZoom = calculateNormalizedZoom(
        metrics.desiredHfovDeg, m_config.wideHfovDeg, m_config.teleHfovDeg, m_config.zoomCurve);

    return metrics;
}

bool AutoFramingController::scheduleFraming(
    ICameraPayload& camera, double slantRangeMeters,
    const TargetPhysicalEnvelope& envelope,
    double targetTrueHeadingDeg, double cameraAzimuthDeg)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto metrics = evaluateFraming(
        slantRangeMeters, envelope, targetTrueHeadingDeg, cameraAzimuthDeg);

    // Check range & zoom deadband hysteresis
    if (m_lastFramedRangeMeters > 0.0) {
        const double rangeDeltaRatio = std::abs(slantRangeMeters - m_lastFramedRangeMeters) / m_lastFramedRangeMeters;
        const double zoomDelta = std::abs(metrics.targetNormalizedZoom - m_targetZoom01);

        if (rangeDeltaRatio < m_config.rangeHysteresisRatio && zoomDelta < m_config.zoomDeadband01) {
            // Within deadband; maintain current trajectory
            return true;
        }
    }

    m_lastFramedRangeMeters = slantRangeMeters;
    m_targetZoom01 = metrics.targetNormalizedZoom;

    const auto telem = camera.currentTelemetry();
    m_currentZoom01 = std::clamp(static_cast<double>(telem.normalizedZoom), 0.0, 1.0);

    const double zoomErr = std::abs(m_currentZoom01 - m_targetZoom01);
    m_isConverged = (zoomErr <= m_config.zoomConvergenceTolerance01);
    m_autofocusTriggered = false;
    m_transitionStartTime = std::chrono::steady_clock::now();

    return true;
}

void AutoFramingController::update(ICameraPayload& camera, std::chrono::milliseconds dt)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_isConverged) {
        return;
    }

    const double dtSec = std::chrono::duration<double>(dt).count();
    const double maxStep = std::max(m_config.maxZoomVelocityPerSec * dtSec, 0.001);

    const double diff = m_targetZoom01 - m_currentZoom01;
    if (std::abs(diff) <= maxStep || std::abs(diff) <= m_config.zoomConvergenceTolerance01) {
        m_currentZoom01 = m_targetZoom01;
        m_isConverged = true;
        (void)camera.setZoomNormalized(m_currentZoom01);

        if (m_config.triggerAutofocusOnConvergence && !m_autofocusTriggered) {
            (void)camera.triggerOnePushFocus();
            m_autofocusTriggered = true;
        }
    } else {
        m_currentZoom01 += (diff > 0.0 ? maxStep : -maxStep);
        m_currentZoom01 = std::clamp(m_currentZoom01, 0.0, 1.0);
        (void)camera.setZoomNormalized(m_currentZoom01);
    }
}

bool AutoFramingController::isZoomConverged() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isConverged;
}

double AutoFramingController::targetZoom() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_targetZoom01;
}

double AutoFramingController::currentZoom() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentZoom01;
}

std::chrono::milliseconds AutoFramingController::estimatedConvergenceTime() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isConverged) {
        return std::chrono::milliseconds(0);
    }
    const double remZoom = std::abs(m_targetZoom01 - m_currentZoom01);
    const double speed = std::max(m_config.maxZoomVelocityPerSec, 0.01);
    const double remSec = remZoom / speed;
    return std::chrono::milliseconds(static_cast<std::int64_t>(remSec * 1000.0));
}

void AutoFramingController::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isConverged = true;
    m_lastFramedRangeMeters = 0.0;
    m_autofocusTriggered = false;
}

bool AutoFramingController::frameTarget(ICameraPayload& camera, double slantRangeMeters, double targetDimensionMeters)
{
    AutoFramingConfig cfg;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cfg = m_config;
    }

    const double clampedRange = std::clamp(slantRangeMeters, cfg.minSlantRangeMeters, cfg.maxSlantRangeMeters);
    const double clampedDim
        = std::clamp(targetDimensionMeters, cfg.minTargetDimensionMeters, cfg.maxTargetDimensionMeters);

    const double zoom01 = calculateNormalizedZoom(
        clampedRange, clampedDim, cfg.targetFrameOccupancyRatio, cfg.wideHfovDeg, cfg.teleHfovDeg);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentZoom01 = zoom01;
        m_targetZoom01 = zoom01;
        m_isConverged = true;
        m_lastFramedRangeMeters = clampedRange;
    }

    return camera.setZoomNormalized(zoom01);
}

} // namespace PayloadHal
