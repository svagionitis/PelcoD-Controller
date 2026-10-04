#include "SightlineRadiometry.h"

#include <algorithm>
#include <cmath>

namespace Sightline {

namespace {

[[nodiscard]] constexpr float getSensorScale(RadiometricSensor sensor, float customScale) noexcept
{
    switch (sensor) {
    case RadiometricSensor::FlirTau2LowRes:
        return 0.4F;
    case RadiometricSensor::FlirTau2HighRes:
        return 0.04F;
    case RadiometricSensor::FlirBosonHighGain:
        return 0.01F;
    case RadiometricSensor::FlirBosonLowGain:
        return 0.02F;
    case RadiometricSensor::DrsTamarisk:
        return 1.0F / 32.0F; // 0.03125 K per count (11.5 fixed point)
    case RadiometricSensor::CustomLinear:
        return customScale;
    default:
        return 1.0F;
    }
}

[[nodiscard]] constexpr std::uint16_t getMaxRawCounts(RadiometricSensor sensor) noexcept
{
    switch (sensor) {
    case RadiometricSensor::FlirTau2LowRes:
    case RadiometricSensor::FlirTau2HighRes:
        return 16383U; // 14-bit unsigned
    default:
        return 65535U; // 16-bit unsigned
    }
}

} // namespace

float SightlineRadiometry::rawToKelvin(
    std::uint16_t raw, RadiometricSensor sensor, float scaleA, float offsetB) noexcept
{
    switch (sensor) {
    case RadiometricSensor::FlirTau2LowRes:
        return static_cast<float>(raw) * 0.4F;

    case RadiometricSensor::FlirTau2HighRes:
        return static_cast<float>(raw) * 0.04F;

    case RadiometricSensor::FlirBosonHighGain:
        return static_cast<float>(raw) / 100.0F;

    case RadiometricSensor::FlirBosonLowGain:
        return static_cast<float>(raw) / 50.0F;

    case RadiometricSensor::DrsTamarisk:
        return static_cast<float>(raw) / 32.0F;

    case RadiometricSensor::CustomLinear:
        return (static_cast<float>(raw) * scaleA) + offsetB;

    default:
        return static_cast<float>(raw);
    }
}

std::uint16_t SightlineRadiometry::kelvinToRaw(
    float kelvin, RadiometricSensor sensor, float scaleA, float offsetB) noexcept
{
    if (!std::isfinite(kelvin) || kelvin <= 0.0F) {
        return 0U;
    }

    const auto maxCount = static_cast<float>(getMaxRawCounts(sensor));
    float rawValue = 0.0F;

    switch (sensor) {
    case RadiometricSensor::FlirTau2LowRes:
        rawValue = kelvin / 0.4F;
        break;

    case RadiometricSensor::FlirTau2HighRes:
        rawValue = kelvin / 0.04F;
        break;

    case RadiometricSensor::FlirBosonHighGain:
        rawValue = kelvin * 100.0F;
        break;

    case RadiometricSensor::FlirBosonLowGain:
        rawValue = kelvin * 50.0F;
        break;

    case RadiometricSensor::DrsTamarisk:
        rawValue = kelvin * 32.0F;
        break;

    case RadiometricSensor::CustomLinear:
        if (std::abs(scaleA) > 1e-6F) {
            rawValue = (kelvin - offsetB) / scaleA;
        } else {
            rawValue = 0.0F;
        }
        break;

    default:
        rawValue = kelvin;
        break;
    }

    if (!std::isfinite(rawValue) || rawValue <= 0.0F) {
        return 0U;
    }
    if (rawValue >= maxCount) {
        return static_cast<std::uint16_t>(maxCount);
    }

    return static_cast<std::uint16_t>(rawValue + 0.5F);
}

RadiometricSpotStats SightlineRadiometry::calcSpotStats(
    const MsgTrackingBoxPixelStats& stats, RadiometricSensor sensor,
    float scaleA, float offsetB) noexcept
{
    RadiometricSpotStats out {};
    out.cameraIndex = stats.cameraIndex;
    out.trackId = stats.trackId;

    const float minK = rawToKelvin(stats.minIntensity, sensor, scaleA, offsetB);
    const float maxK = rawToKelvin(stats.maxIntensity, sensor, scaleA, offsetB);
    const float meanK = rawToKelvin(stats.meanIntensity, sensor, scaleA, offsetB);

    out.minTemp = fromKelvin(minK);
    out.maxTemp = fromKelvin(maxK);
    out.meanTemp = fromKelvin(meanK);

    const float scale = getSensorScale(sensor, scaleA);
    out.stdDevKelvin = static_cast<float>(stats.stdDevIntensity) * scale;

    return out;
}

IsothermAgcRange SightlineRadiometry::calcIsothermAgc(
    float minKelvin, float maxKelvin, RadiometricSensor sensor,
    float scaleA, float offsetB) noexcept
{
    IsothermAgcRange range {};

    const float clampedMin = std::max(0.0F, minKelvin);
    const float clampedMax = std::max(clampedMin, maxKelvin);

    range.agHoldmin = kelvinToRaw(clampedMin, sensor, scaleA, offsetB);
    range.agHoldmax = kelvinToRaw(clampedMax, sensor, scaleA, offsetB);

    const float deltaK = clampedMax - clampedMin;
    range.degreesPerCount = (range.agHoldmax > range.agHoldmin) ? (deltaK / 255.0F) : 0.0F;

    return range;
}

} // namespace Sightline
