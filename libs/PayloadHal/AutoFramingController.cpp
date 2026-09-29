#include "AutoFramingController.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

namespace {
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
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

double AutoFramingController::estimateTargetDimension(const Nmea::AisDimensions& dimensions) const noexcept
{
    const auto len = dimensions.lengthMeters();
    if (len > 0U) {
        return std::clamp(
            static_cast<double>(len), m_config.minTargetDimensionMeters, m_config.maxTargetDimensionMeters);
    }
    return m_config.defaultTargetLengthMeters;
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

    return camera.setZoomNormalized(zoom01);
}

} // namespace PayloadHal
