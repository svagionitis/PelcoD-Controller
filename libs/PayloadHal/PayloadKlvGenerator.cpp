/// @file PayloadKlvGenerator.cpp
/// @brief Implementation of PayloadKlvGenerator for PayloadHal.

#include "PayloadKlvGenerator.h"
#include "GeoreferenceUtils.h"

#include <cmath>
#include <utility>

namespace PayloadHal {

namespace {
    constexpr double DEG_TO_RAD = 3.14159265358979323846 / 180.0;

    double normalize360(double deg) noexcept
    {
        double d = std::fmod(deg, 360.0);
        if (d < 0.0) {
            d += 360.0;
        }
        return d;
    }
} // namespace

PayloadKlvGenerator::PayloadKlvGenerator(std::shared_ptr<IPayload> payload,
                                         PayloadKlvConfig config) noexcept
    : m_payload(std::move(payload))
    , m_config(std::move(config))
{
}

void PayloadKlvGenerator::setConfig(const PayloadKlvConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

PayloadKlvConfig PayloadKlvGenerator::config() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void PayloadKlvGenerator::setActiveCamera(std::shared_ptr<ICameraPayload> camera)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeCamera = std::move(camera);
}

std::shared_ptr<ICameraPayload> PayloadKlvGenerator::activeCamera() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_activeCamera) {
        return m_activeCamera;
    }
    if (m_payload) {
        return m_payload->primaryCamera();
    }
    return nullptr;
}

void PayloadKlvGenerator::setDemProvider(std::shared_ptr<IDemProvider> dem)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.demProvider = std::move(dem);
}

std::shared_ptr<IDemProvider> PayloadKlvGenerator::demProvider() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_config.demProvider) {
        return m_config.demProvider;
    }
    if (m_payload) {
        return m_payload->demProvider();
    }
    return nullptr;
}

Klv::UasDatalinkMessage PayloadKlvGenerator::buildMessage(
    const PlatformNavData& nav,
    std::optional<std::uint64_t> timestampUs) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    Klv::UasDatalinkMessage msg;

    // Tag 2: Precision Time Stamp (µs since epoch)
    if (timestampUs.has_value()) {
        msg.precisionTimeStampUs = *timestampUs;
    } else {
        const auto now = std::chrono::system_clock::now();
        msg.precisionTimeStampUs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count());
    }

    // Static & administrative metadata
    if (!m_config.missionId.empty()) {
        msg.missionId = m_config.missionId;
    }
    if (!m_config.platformTailNumber.empty()) {
        msg.platformTailNumber = m_config.platformTailNumber;
    }
    if (!m_config.platformDesignation.empty()) {
        msg.platformDesignation = m_config.platformDesignation;
    }
    if (!m_config.imageSourceSensor.empty()) {
        msg.imageSourceSensor = m_config.imageSourceSensor;
    }
    if (!m_config.imageCoordinateSystem.empty()) {
        msg.imageCoordinateSystem = m_config.imageCoordinateSystem;
    }

    // Platform Navigation Telemetry
    msg.platformHeadingDeg = normalize360(nav.headingDeg);
    msg.platformPitchDeg = nav.pitchDeg;
    msg.platformRollDeg = nav.rollDeg;
    msg.sensorLatitudeDeg = nav.position.latitudeDeg;
    msg.sensorLongitudeDeg = nav.position.longitudeDeg;
    msg.sensorTrueAltitudeM = nav.position.altitudeM;

    // Active Camera Telemetry
    std::shared_ptr<ICameraPayload> cam = m_activeCamera;
    if (!cam && m_payload) {
        cam = m_payload->primaryCamera();
    }

    double hfovDeg = 0.0;
    double vfovDeg = 0.0;
    if (cam) {
        const auto camTelem = cam->currentTelemetry();
        hfovDeg = camTelem.horizontalFovDeg;
        vfovDeg = camTelem.verticalFovDeg;
        if (hfovDeg > 0.0) {
            msg.sensorHfovDeg = hfovDeg;
        }
        if (vfovDeg > 0.0) {
            msg.sensorVfovDeg = vfovDeg;
        }
    }

    // Pan-Tilt Gimbal Telemetry
    double panDeg = 0.0;
    double tiltDeg = 0.0;
    double rollDeg = 0.0;
    if (m_payload && m_payload->panTilt()) {
        const auto ptuTelem = m_payload->panTilt()->currentTelemetry();
        panDeg = ptuTelem.panAngleDeg;
        tiltDeg = ptuTelem.tiltAngleDeg;
        rollDeg = ptuTelem.rollAngleDeg;
        msg.sensorRelAzimuthDeg = normalize360(panDeg);
        msg.sensorRelElevationDeg = tiltDeg;
        msg.sensorRelRollDeg = normalize360(rollDeg);
    }

    const auto dem = (m_config.demProvider ? m_config.demProvider : (m_payload ? m_payload->demProvider() : nullptr));

    // Laser Range Finder & Slant Range Resolution
    std::optional<double> slantRangeM;
    if (m_payload && m_payload->lrf()) {
        const auto lrfMeas = m_payload->lrf()->lastMeasurement();
        if (lrfMeas && lrfMeas->valid && lrfMeas->slantRangeMeters > 0.0) {
            slantRangeM = lrfMeas->slantRangeMeters;
        }
    }

    // If LRF echo is unavailable, fall back to terrain DEM ray intersection or ground plane
    if (!slantRangeM.has_value() && tiltDeg < 0.0) {
        if (dem) {
            const auto demRes = DemRayCaster::intersect(*dem, nav.position, nav.headingDeg, panDeg, tiltDeg);
            if (demRes) {
                slantRangeM = demRes->slantRangeMeters;
                msg.frameCenterLatDeg = demRes->targetPosition.latitudeDeg;
                msg.frameCenterLonDeg = demRes->targetPosition.longitudeDeg;
                msg.frameCenterElevM = demRes->targetPosition.altitudeM;
            }
        } else {
            const auto groundTarget = GeoreferenceUtils::computeTargetFromGroundIntersection(
                nav.position, nav.headingDeg, panDeg, tiltDeg, m_config.fallbackGroundElevationM);
            if (groundTarget) {
                const auto look = GeoreferenceUtils::computeLookAnglesToTarget(nav.position, nav.headingDeg, *groundTarget);
                if (look.slantRangeMeters > 0.0) {
                    slantRangeM = look.slantRangeMeters;
                }
            }
        }
    }

    if (slantRangeM.has_value() && *slantRangeM > 0.0) {
        msg.slantRangeM = *slantRangeM;

        // Optical Target Width (Tag 22)
        if (hfovDeg > 0.0) {
            msg.targetWidthM = 2.0 * (*slantRangeM) * std::tan((hfovDeg * DEG_TO_RAD) * 0.5);
        }

        // Frame Center Target Projection (Tags 23-25) if not already set by DEM
        if (!msg.frameCenterLatDeg.has_value()) {
            const auto targetPos = GeoreferenceUtils::computeTargetFromSlantRange(
                nav.position, nav.headingDeg, panDeg, tiltDeg, *slantRangeM);
            if (targetPos) {
                msg.frameCenterLatDeg = targetPos->latitudeDeg;
                msg.frameCenterLonDeg = targetPos->longitudeDeg;
                msg.frameCenterElevM = targetPos->altitudeM;
            }
        }
    } else if (tiltDeg < 0.0 && !msg.frameCenterLatDeg.has_value()) {
        const auto groundPos = GeoreferenceUtils::computeTargetFromGroundIntersection(
            nav.position, nav.headingDeg, panDeg, tiltDeg, m_config.fallbackGroundElevationM);
        if (groundPos) {
            msg.frameCenterLatDeg = groundPos->latitudeDeg;
            msg.frameCenterLonDeg = groundPos->longitudeDeg;
            msg.frameCenterElevM = groundPos->altitudeM;
        }
    }

    // Footprint Frustum Corners (Tags 26-33)
    if (m_config.enableFrustumCorners && cam && tiltDeg < 0.0 && hfovDeg > 0.0) {
        if (dem) {
            const auto corners = GeoreferenceUtils::computeFrustumCorners(
                *dem, nav.position, nav.headingDeg, panDeg, tiltDeg, hfovDeg, vfovDeg, rollDeg);
            if (corners) {
                msg.cornerCoordinates = corners;
            }
        } else if (m_payload) {
            const auto corners = m_payload->computeFrustumCorners(
                cam, nav.position, nav.headingDeg, m_config.fallbackGroundElevationM);
            if (corners) {
                msg.cornerCoordinates = corners;
            }
        }
    }

    // Security Metadata (Tag 48)
    msg.security = m_config.security;

    // Version Number (Tag 65)
    msg.uasLsVersion = m_config.uasLsVersion;

    return msg;
}

std::vector<std::uint8_t> PayloadKlvGenerator::generatePacket(
    const PlatformNavData& nav,
    std::optional<std::uint64_t> timestampUs) const
{
    const auto message = buildMessage(nav, timestampUs);
    return Klv::KlvEncoder::encode(message);
}

} // namespace PayloadHal
