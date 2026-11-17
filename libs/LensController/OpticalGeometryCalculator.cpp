/// @file OpticalGeometryCalculator.cpp
#include "OpticalGeometryCalculator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace LensControl {

OpticalGeometryCalculator::OpticalGeometryCalculator(const OpticsConfig& config) noexcept
    : m_config { config }
{
}

DistanceMeters OpticalGeometryCalculator::calculateHyperfocal(FocalLengthMm focalLength) const noexcept
{
    const float f = focalLength.value();
    const float n = m_config.aperture.value();
    const float c = m_config.coc.value();

    if (f <= 0.0F || n <= 0.0F || c <= 0.0F) {
        return DistanceMeters { 0.0F };
    }

    // H = (f^2 / (N * c)) + f, converted to meters (f in mm, c in mm -> H in mm / 1000)
    const float fMeters = f * 0.001F;
    const float hMeters = ((f * f * 0.001F) / (n * c)) + fMeters;
    return DistanceMeters { hMeters };
}

DofLimits OpticalGeometryCalculator::calculateDof(DistanceMeters subjectDist, FocalLengthMm focalLength) const noexcept
{
    DofLimits result { };
    const float d = subjectDist.value();
    const float fMm = focalLength.value();
    const float fMeters = fMm * 0.001F;

    if (d <= fMeters || fMm <= 0.0F) {
        return result;
    }

    const DistanceMeters h = calculateHyperfocal(focalLength);
    const float hMeters = h.value();
    result.hyperfocalDistance = h;

    if (hMeters <= 0.0F) {
        return result;
    }

    const float nearDenom = hMeters + (d - fMeters);
    if (nearDenom > 0.0F) {
        result.nearLimit = DistanceMeters { (hMeters * d) / nearDenom };
    } else {
        result.nearLimit = DistanceMeters { 0.0F };
    }

    const float farDenom = hMeters - (d - fMeters);
    if (farDenom <= 0.0F) {
        result.farLimit = DistanceMeters::infinity();
        result.depthOfField = DistanceMeters::infinity();
        result.isFarInfinite = true;
    } else {
        const float farVal = (hMeters * d) / farDenom;
        result.farLimit = DistanceMeters { farVal };
        result.depthOfField = DistanceMeters { farVal - result.nearLimit.value() };
        result.isFarInfinite = false;
    }

    return result;
}

bool OpticalGeometryCalculator::isWideAngle(MotorPosition zoomPos) const noexcept
{
    return zoomPos.value() <= m_config.wideLimitZoom.value();
}

AdaptiveStepConfig OpticalGeometryCalculator::calculateSteps(
    FocalLengthMm focalLength, std::uint16_t baseCoarse, std::uint16_t baseMid, std::uint16_t baseFine) const noexcept
{
    AdaptiveStepConfig config { };
    config.coarseStep = baseCoarse;
    config.midStep = baseMid;
    config.fineStep = baseFine;
    config.scaleFactor = 1.0F;

    const DofLimits teleDof = calculateDof(m_config.nominalSubjectDist, m_config.maxFocalLength);
    const DofLimits currentDof = calculateDof(m_config.nominalSubjectDist, focalLength);

    if (teleDof.depthOfField.value() <= 0.0F) {
        return config;
    }

    float scale = 1.0F;
    if (currentDof.isFarInfinite) {
        scale = 10.0F;
    } else {
        scale = currentDof.depthOfField.value() / teleDof.depthOfField.value();
        scale = std::clamp(scale, 1.0F, 10.0F);
    }

    config.scaleFactor = scale;

    const float scaledCoarse = std::round(static_cast<float>(baseCoarse) * scale);
    const float scaledMid = std::round(static_cast<float>(baseMid) * scale);
    const float scaledFine = std::round(static_cast<float>(baseFine) * scale);

    config.coarseStep = static_cast<std::uint16_t>(std::clamp(scaledCoarse, 1.0F, 65535.0F));
    config.midStep = static_cast<std::uint16_t>(std::clamp(scaledMid, 1.0F, 65535.0F));
    config.fineStep = static_cast<std::uint16_t>(std::clamp(scaledFine, 1.0F, 65535.0F));

    return config;
}

void OpticalGeometryCalculator::setConfig(const OpticsConfig& config) noexcept
{
    m_config = config;
}

OpticsConfig OpticalGeometryCalculator::config() const noexcept
{
    return m_config;
}

} // namespace LensControl
