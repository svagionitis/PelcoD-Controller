/// @file DjiSt0601Mapper.cpp
/// @brief Implementation of DJI telemetry to MISB ST 0601 mapping.

#include "DjiSt0601Mapper.h"

#include <cmath>
#include <cstdlib>

namespace Dji {

namespace {

    constexpr double kMaxLatDeg { 90.0 };
    constexpr double kMaxLonDeg { 180.0 };
    constexpr double kMinAltM { -900.0 }; // ST 0601 Tags 15/75 lower bound
    constexpr double kMaxAltM { 19000.0 }; // ST 0601 Tags 15/75 upper bound
    constexpr double kMaxElevDeg { 180.0 }; // ST 0601 Tag 19 bound
    constexpr double kMaxFovDeg { 180.0 }; // ST 0601 Tags 16/17 bound
    constexpr double kNoFixEpsDeg { 1e-9 };
    constexpr double kFullFrameDiagMm { 43.266615305567875 }; // hypot(36, 24)
    constexpr double kPi { 3.14159265358979323846 };
    constexpr double kRadToDeg { 180.0 / kPi };
    constexpr std::int64_t kUsPerMinute { 60000000 };
    constexpr std::int64_t kQuarterHourUs { 15 * kUsPerMinute };
    constexpr std::int64_t kMaxOffsetMin { 14 * 60 };
    constexpr std::int64_t kMaxSaneUs { 100000000000000000 }; // ~year 5138; guards overflow

    /// @brief Returns true if @p v lies in [lo, hi].
    /// @param[in] v Value.
    /// @param[in] lo Lower bound.
    /// @param[in] hi Upper bound.
    /// @return True if in range.
    [[nodiscard]] constexpr bool inRange(double v, double lo, double hi) noexcept
    {
        return (v >= lo) && (v <= hi);
    }

    /// @brief Maps latitude/longitude, rejecting out-of-range and 0/0 "no fix" values.
    /// @param[in] s Source sample.
    /// @param[in,out] msg Destination message.
    void mapPosition(const DjiTelemetrySample& s, Klv::UasDatalinkMessage& msg)
    {
        if (!s.latitudeDeg.has_value() || !s.longitudeDeg.has_value()) {
            return;
        }
        const double lat { *s.latitudeDeg };
        const double lon { *s.longitudeDeg };
        const bool noFix { (std::fabs(lat) < kNoFixEpsDeg) && (std::fabs(lon) < kNoFixEpsDeg) };
        if (noFix || !inRange(lat, -kMaxLatDeg, kMaxLatDeg) || !inRange(lon, -kMaxLonDeg, kMaxLonDeg)) {
            return;
        }
        msg.sensorLatitudeDeg = lat;
        msg.sensorLongitudeDeg = lon;
    }

    /// @brief Derives horizontal/vertical FOV from a 35 mm-equivalent focal length.
    /// @param[in] s Source sample.
    /// @param[in] cfg Options (aspect ratio).
    /// @param[in,out] msg Destination message.
    void mapFov(const DjiTelemetrySample& s, const DjiMapConfig& cfg, Klv::UasDatalinkMessage& msg)
    {
        if (!cfg.deriveFov || !(cfg.sensorAspect > 0.0) || !s.focalLenMm.has_value() || !(*s.focalLenMm > 0.0)) {
            return;
        }
        const double zoom { (s.digitalZoom.has_value() && (*s.digitalZoom > 0.0)) ? *s.digitalZoom : 1.0 };
        const double halfDiagTan { kFullFrameDiagMm / (2.0 * (*s.focalLenMm) * zoom) };
        const double diagUnits { std::sqrt(1.0 + (cfg.sensorAspect * cfg.sensorAspect)) };
        const double hfov { 2.0 * std::atan(halfDiagTan * cfg.sensorAspect / diagUnits) * kRadToDeg };
        const double vfov { 2.0 * std::atan(halfDiagTan / diagUnits) * kRadToDeg };
        if (inRange(hfov, 0.0, kMaxFovDeg) && inRange(vfov, 0.0, kMaxFovDeg)) {
            msg.sensorHfovDeg = hfov;
            msg.sensorVfovDeg = vfov;
        }
    }

} // namespace

double wrapDegrees360(double deg) noexcept
{
    double r { std::fmod(deg, 360.0) };
    if (r < 0.0) {
        r += 360.0;
    }
    if (r >= 360.0) {
        r = 0.0; // -tiny + 360 can round up to 360
    }
    return r;
}

std::optional<std::int32_t> deriveUtcOffset(std::int64_t localUs, std::uint64_t utcUs) noexcept
{
    if ((localUs < 0) || (localUs > kMaxSaneUs) || (utcUs > static_cast<std::uint64_t>(kMaxSaneUs))) {
        return std::nullopt;
    }
    const std::int64_t diff { localUs - static_cast<std::int64_t>(utcUs) };
    const std::int64_t half { kQuarterHourUs / 2 };
    const std::int64_t quarters { (diff >= 0) ? ((diff + half) / kQuarterHourUs)
                                              : -(((-diff) + half) / kQuarterHourUs) };
    const std::int64_t minutes { quarters * 15 };
    if (std::llabs(minutes) > kMaxOffsetMin) {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(minutes);
}

Klv::UasDatalinkMessage mapToSt0601(const DjiTelemetrySample& sample, const DjiMapConfig& cfg)
{
    Klv::UasDatalinkMessage msg {};

    if (sample.localTimeUs.has_value()) {
        const std::int64_t utc { *sample.localTimeUs - (static_cast<std::int64_t>(cfg.utcOffsetMin) * kUsPerMinute) };
        if (utc > 0) {
            msg.precisionTimeStampUs = static_cast<std::uint64_t>(utc);
        }
    }

    mapPosition(sample, msg);

    if (sample.absAltM.has_value() && inRange(*sample.absAltM, kMinAltM, kMaxAltM)) {
        if (cfg.absAltIsHae) {
            msg.sensorAltitudeHaeM = *sample.absAltM;
        } else {
            msg.sensorTrueAltitudeM = *sample.absAltM;
        }
    }

    if (sample.gimbalYawDeg.has_value()) {
        msg.sensorRelAzimuthDeg = wrapDegrees360(*sample.gimbalYawDeg);
    }
    if (sample.gimbalPitchDeg.has_value() && inRange(*sample.gimbalPitchDeg, -kMaxElevDeg, kMaxElevDeg)) {
        msg.sensorRelElevationDeg = *sample.gimbalPitchDeg;
    }
    if (sample.gimbalRollDeg.has_value()) {
        msg.sensorRelRollDeg = wrapDegrees360(*sample.gimbalRollDeg);
    }

    mapFov(sample, cfg, msg);

    if (!cfg.platform.empty()) {
        msg.platformDesignation = cfg.platform;
    }
    return msg;
}

} // namespace Dji
