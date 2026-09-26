/// @file SensorParallaxCompensator.cpp
/// @brief Implementation of range-dependent sensor parallax and boresight alignment.

#include "SensorParallaxCompensator.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

namespace {

constexpr double kPi = 3.14159265358979323846;

double degToRad(double deg)
{
    return deg * (kPi / 180.0);
}

double radToDeg(double rad)
{
    return rad * (180.0 / kPi);
}

} // namespace

SensorParallaxCompensator::SensorParallaxCompensator(const SensorOffset3D& baseline,
                                                     const BoresightCalibration& boresight)
    : m_baseline(baseline)
    , m_boresight(boresight)
{
}

void SensorParallaxCompensator::setBaselineOffset(const SensorOffset3D& baseline) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_baseline = baseline;
}

SensorOffset3D SensorParallaxCompensator::baselineOffset() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_baseline;
}

void SensorParallaxCompensator::setBoresightCalibration(const BoresightCalibration& boresight) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_boresight = boresight;
}

BoresightCalibration SensorParallaxCompensator::boresightCalibration() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_boresight;
}

ParallaxDisparity SensorParallaxCompensator::computeDisparity(
    double slantRangeMeters,
    double primaryHfovDeg,
    double primaryVfovDeg,
    std::uint32_t imageWidth,
    std::uint32_t imageHeight) const
{
    SensorOffset3D baseline;
    BoresightCalibration boresight;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        baseline = m_baseline;
        boresight = m_boresight;
    }

    ParallaxDisparity disparity {};
    disparity.rangeMeters = std::max(0.001, slantRangeMeters);

    // Range along line-of-sight
    const double effectiveRange = std::max(0.01, disparity.rangeMeters + baseline.longitudinalOffsetM);

    // Angular disparity (triangulation + static boresight)
    const double azParallaxRad = std::atan2(baseline.lateralOffsetM, effectiveRange);
    const double elParallaxRad = std::atan2(baseline.verticalOffsetM, effectiveRange);

    disparity.azimuthDisparityDeg = radToDeg(azParallaxRad) + boresight.azimuthOffsetDeg;
    disparity.elevationDisparityDeg = radToDeg(elParallaxRad) + boresight.elevationOffsetDeg;

    // Normalized screen offset [-1.0, 1.0]
    const double halfHfovRad = degToRad(std::max(0.1, primaryHfovDeg) / 2.0);
    const double halfVfovRad = degToRad(std::max(0.1, primaryVfovDeg) / 2.0);

    const double tanHalfHfov = std::tan(halfHfovRad);
    const double tanHalfVfov = std::tan(halfVfovRad);

    const double totalAzRad = degToRad(disparity.azimuthDisparityDeg);
    const double totalElRad = degToRad(disparity.elevationDisparityDeg);

    if (tanHalfHfov > 1e-6) {
        disparity.normalizedScreenDeltaX = std::tan(totalAzRad) / tanHalfHfov;
    }
    if (tanHalfVfov > 1e-6) {
        // Screen Y is inverted (top is 0, bottom is H)
        disparity.normalizedScreenDeltaY = -std::tan(totalElRad) / tanHalfVfov;
    }

    // Pixel disparity
    disparity.pixelDeltaX = disparity.normalizedScreenDeltaX * (static_cast<double>(imageWidth) / 2.0);
    disparity.pixelDeltaY = disparity.normalizedScreenDeltaY * (static_cast<double>(imageHeight) / 2.0);

    return disparity;
}

std::pair<double, double> SensorParallaxCompensator::computeReticleOffset(
    double slantRangeMeters,
    double hfovDeg,
    double vfovDeg) const
{
    const auto disp = computeDisparity(slantRangeMeters, hfovDeg, vfovDeg);
    return { disp.normalizedScreenDeltaX, disp.normalizedScreenDeltaY };
}

ScreenRect2D SensorParallaxCompensator::transformBoundingBox(
    const ScreenRect2D& primaryBox,
    double slantRangeMeters,
    double primaryHfovDeg,
    double primaryVfovDeg,
    double secondaryHfovDeg,
    double secondaryVfovDeg) const
{
    const auto disp = computeDisparity(slantRangeMeters, primaryHfovDeg, primaryVfovDeg);

    const double safeSecHfov = std::max(0.1, secondaryHfovDeg);
    const double safeSecVfov = std::max(0.1, secondaryVfovDeg);

    const double scaleX = primaryHfovDeg / safeSecHfov;
    const double scaleY = primaryVfovDeg / safeSecVfov;

    ScreenRect2D secBox {};
    secBox.width = primaryBox.width * scaleX;
    secBox.height = primaryBox.height * scaleY;

    // Shift box position by parallax disparity
    secBox.x = (primaryBox.x * scaleX) + disp.pixelDeltaX;
    secBox.y = (primaryBox.y * scaleY) + disp.pixelDeltaY;

    return secBox;
}

std::pair<double, double> SensorParallaxCompensator::mapPointPrimaryToSecondary(
    double xNorm,
    double yNorm,
    double slantRangeMeters,
    double primaryHfovDeg,
    double primaryVfovDeg,
    double secondaryHfovDeg,
    double secondaryVfovDeg) const
{
    const auto disp = computeDisparity(slantRangeMeters, primaryHfovDeg, primaryVfovDeg);

    const double primHalfHfovRad = degToRad(std::max(0.1, primaryHfovDeg) / 2.0);
    const double primHalfVfovRad = degToRad(std::max(0.1, primaryVfovDeg) / 2.0);

    // Primary ray angles relative to optical axis
    const double primRayAzRad = std::atan(xNorm * std::tan(primHalfHfovRad));
    const double primRayElRad = std::atan(-yNorm * std::tan(primHalfVfovRad));

    // Shift by net disparity
    const double secRayAzRad = primRayAzRad - degToRad(disp.azimuthDisparityDeg);
    const double secRayElRad = primRayElRad - degToRad(disp.elevationDisparityDeg);

    // Secondary screen projection
    const double secHalfHfovRad = degToRad(std::max(0.1, secondaryHfovDeg) / 2.0);
    const double secHalfVfovRad = degToRad(std::max(0.1, secondaryVfovDeg) / 2.0);

    double secNormX = 0.0;
    double secNormY = 0.0;

    const double tanSecHalfHfov = std::tan(secHalfHfovRad);
    const double tanSecHalfVfov = std::tan(secHalfVfovRad);

    if (tanSecHalfHfov > 1e-6) {
        secNormX = std::tan(secRayAzRad) / tanSecHalfHfov;
    }
    if (tanSecHalfVfov > 1e-6) {
        secNormY = -std::tan(secRayElRad) / tanSecHalfVfov;
    }

    return { secNormX, secNormY };
}

std::pair<double, double> SensorParallaxCompensator::computeLrfConvergenceAngles(
    double slantRangeMeters,
    const SensorOffset3D& lrfOffset) const
{
    SensorOffset3D offset = lrfOffset;
    if (std::abs(offset.lateralOffsetM) < 1e-6 && std::abs(offset.verticalOffsetM) < 1e-6
        && std::abs(offset.longitudinalOffsetM) < 1e-6) {
        std::lock_guard<std::mutex> lock(m_mutex);
        offset = m_baseline;
    }

    const double range = std::max(0.01, slantRangeMeters + offset.longitudinalOffsetM);
    const double azCorrectionDeg = radToDeg(std::atan2(offset.lateralOffsetM, range));
    const double elCorrectionDeg = radToDeg(std::atan2(offset.verticalOffsetM, range));

    return { azCorrectionDeg, elCorrectionDeg };
}

} // namespace PayloadHal
