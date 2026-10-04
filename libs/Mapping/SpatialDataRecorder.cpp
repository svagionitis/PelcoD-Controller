/// @file SpatialDataRecorder.cpp
/// @brief Implementation of spatial telemetry buffering and decimation engine.

#include "SpatialDataRecorder.h"
#include "KlvGeodesy.h"

#include <cmath>

namespace Mapping {

namespace {

[[nodiscard]] double computeDistance(const SpatialPoint3D& p1, const SpatialPoint3D& p2) noexcept {
    const Klv::GeoPoint2D g1 { p1.latitudeDeg, p1.longitudeDeg };
    const Klv::GeoPoint2D g2 { p2.latitudeDeg, p2.longitudeDeg };
    return Klv::KlvGeodesy::distanceMeters(g1, g2);
}

[[nodiscard]] double angularDiff(double a, double b) noexcept {
    double diff = std::fmod(std::abs(a - b), 360.0);
    if (diff > 180.0) {
        diff = 360.0 - diff;
    }
    return diff;
}

} // namespace

void SpatialDataRecorder::addTelemetryFrame(const Klv::UasDatalinkMessage& msg) {
    if (!msg.sensorLatitudeDeg.has_value() || !msg.sensorLongitudeDeg.has_value()) {
        return;
    }

    const double platformLat = *msg.sensorLatitudeDeg;
    const double platformLon = *msg.sensorLongitudeDeg;
    const double platformAlt = msg.sensorAltitudeHaeM.value_or(msg.sensorTrueAltitudeM.value_or(0.0));
    const std::uint64_t timestamp = msg.precisionTimeStampUs.value_or(0U);

    // Build Track Point
    SpatialTrackPoint tp {};
    tp.timestampUs = timestamp;
    tp.position = { platformLat, platformLon, platformAlt };
    tp.headingDeg = msg.platformHeadingDeg.value_or(0.0);
    tp.pitchDeg = msg.platformPitchDeg.value_or(0.0);
    tp.rollDeg = msg.platformRollDeg.value_or(0.0);
    tp.tailNumber = msg.platformTailNumber.value_or("");
    tp.missionId = msg.missionId.value_or("");

    // Calculate speed if previous point exists and time elapsed > 0
    if (!m_trackPoints.empty() && timestamp > m_trackPoints.back().timestampUs) {
        const double dist = computeDistance(m_trackPoints.back().position, tp.position);
        const double dtSec = static_cast<double>(timestamp - m_trackPoints.back().timestampUs) / 1000000.0;
        if (dtSec > 0.001) {
            tp.speedMps = dist / dtSec;
        }
    }

    // Ground elevation
    const double groundElev = msg.frameCenterElevHaeM.value_or(msg.frameCenterElevM.value_or(0.0));

    // Build Frustum Mesh
    FrustumMesh3D frustum {};
    frustum.timestampUs = timestamp;
    frustum.apex = tp.position;
    frustum.hfovDeg = msg.sensorHfovDeg.value_or(0.0);
    frustum.vfovDeg = msg.sensorVfovDeg.value_or(0.0);
    frustum.slantRangeM = msg.slantRangeM.value_or(0.0);

    if (msg.frameCenterLatDeg.has_value() && msg.frameCenterLonDeg.has_value()) {
        frustum.targetCenter = { *msg.frameCenterLatDeg, *msg.frameCenterLonDeg, groundElev };
        frustum.hasTargetCenter = true;
    }

    std::optional<Klv::FrustumCorners> corners = msg.cornerCoordinates;
    if (!corners.has_value() && msg.sensorHfovDeg.has_value() && msg.sensorVfovDeg.has_value()) {
        const Klv::GeoPoint3D platform3D { platformLat, platformLon, platformAlt };
        const double heading = msg.platformHeadingDeg.value_or(0.0);
        const double azimuth = msg.sensorRelAzimuthDeg.value_or(0.0);
        const double elevation = msg.sensorRelElevationDeg.value_or(-45.0);

        corners = Klv::KlvGeodesy::computeFrustum(platform3D, heading, azimuth, elevation,
                                                  *msg.sensorHfovDeg, *msg.sensorVfovDeg, groundElev);
    }

    if (corners.has_value()) {
        frustum.base[0] = { corners->topLeft.latitudeDeg, corners->topLeft.longitudeDeg, groundElev };
        frustum.base[1] = { corners->topRight.latitudeDeg, corners->topRight.longitudeDeg, groundElev };
        frustum.base[2] = { corners->bottomRight.latitudeDeg, corners->bottomRight.longitudeDeg, groundElev };
        frustum.base[3] = { corners->bottomLeft.latitudeDeg, corners->bottomLeft.longitudeDeg, groundElev };
        frustum.valid = true;
    }

    m_trackPoints.push_back(tp);
    m_frustums.push_back(frustum);
}

void SpatialDataRecorder::clear() noexcept {
    m_trackPoints.clear();
    m_frustums.clear();
}

void SpatialDataRecorder::decimate(const DecimationConfig& config) {
    if (config.mode == DecimationMode::None || m_trackPoints.size() <= 2U) {
        return;
    }

    std::vector<SpatialTrackPoint> decimatedTracks {};
    std::vector<FrustumMesh3D> decimatedFrustums {};
    decimatedTracks.reserve(m_trackPoints.size());
    decimatedFrustums.reserve(m_frustums.size());

    // Always keep first point
    decimatedTracks.push_back(m_trackPoints.front());
    decimatedFrustums.push_back(m_frustums.front());

    const std::uint64_t minIntervalUs = static_cast<std::uint64_t>(config.intervalSec * 1000000.0);

    for (std::size_t i = 1U; i + 1U < m_trackPoints.size(); ++i) {
        bool retain = false;
        const auto& prevTrack = decimatedTracks.back();
        const auto& curTrack = m_trackPoints[i];

        switch (config.mode) {
        case DecimationMode::UniformTime:
            if ((curTrack.timestampUs - prevTrack.timestampUs) >= minIntervalUs) {
                retain = true;
            }
            break;
        case DecimationMode::DistanceThreshold:
            if (computeDistance(prevTrack.position, curTrack.position) >= config.distanceDeltaM) {
                retain = true;
            }
            break;
        case DecimationMode::HeadingThreshold:
            if (angularDiff(prevTrack.headingDeg, curTrack.headingDeg) >= config.angleDeltaDeg) {
                retain = true;
            }
            break;
        case DecimationMode::None:
            retain = true;
            break;
        }

        if (retain) {
            decimatedTracks.push_back(curTrack);
            decimatedFrustums.push_back(m_frustums[i]);
        }
    }

    // Always keep last point
    if (m_trackPoints.size() > 1U) {
        decimatedTracks.push_back(m_trackPoints.back());
        decimatedFrustums.push_back(m_frustums.back());
    }

    m_trackPoints = std::move(decimatedTracks);
    m_frustums = std::move(decimatedFrustums);
}

const std::vector<SpatialTrackPoint>& SpatialDataRecorder::trackPoints() const noexcept {
    return m_trackPoints;
}

const std::vector<FrustumMesh3D>& SpatialDataRecorder::frustums() const noexcept {
    return m_frustums;
}

std::size_t SpatialDataRecorder::size() const noexcept {
    return m_trackPoints.size();
}

bool SpatialDataRecorder::empty() const noexcept {
    return m_trackPoints.empty();
}

} // namespace Mapping
