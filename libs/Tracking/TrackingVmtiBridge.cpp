#include "TrackingVmtiBridge.h"

#include <cstdint>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Tracking {

namespace {
    inline constexpr double kPi { 3.14159265358979323846 };
    inline constexpr double kRadToDeg { 180.0 / kPi };
} // namespace

Klv::VTargetPack TrackingVmtiBridge::buildTargetPack(const SphericalTargetState& state,
    const CameraIntrinsics& intrinsics, std::uint32_t trackId, double camPanRad, double camTiltRad, double zoom,
    double slantRangeMeters, const std::optional<Klv::GeoPoint3D>& platformPos) noexcept
{
    Klv::VTargetPack pack;
    pack.targetId = trackId;

    // Project target spherical angles to camera pixel space
    PtzCameraModel model(intrinsics);
    const Math::Vector<2> uv = model.project(state.azimuthRad, state.elevationRad, camPanRad, camTiltRad, zoom);

    const double maxU = (intrinsics.imageWidth > 0) ? static_cast<double>(intrinsics.imageWidth - 1) : 0.0;
    const double maxV = (intrinsics.imageHeight > 0) ? static_cast<double>(intrinsics.imageHeight - 1) : 0.0;
    const double clampedU = std::clamp(uv[0], 0.0, maxU);
    const double clampedV = std::clamp(uv[1], 0.0, maxV);

    // 1-indexed pixel coordinates
    Klv::PixelCoord centroid;
    centroid.col = static_cast<std::uint32_t>(std::lround(clampedU)) + 1U;
    centroid.row = static_cast<std::uint32_t>(std::lround(clampedV)) + 1U;
    pack.centroid = centroid;

    // Confidence [0, 100]
    if (state.locked) {
        const double conf = std::clamp(100.0 - (state.mahalanobisDistance * 10.0), 10.0, 99.0);
        pack.confidence = static_cast<std::uint8_t>(std::lround(conf));
    } else {
        pack.confidence = static_cast<std::uint8_t>(0U);
    }

    // Detection status: 1 = Moving, 2 = Stopped
    const double speedRadPerSec = std::hypot(state.azimuthVelocityRadPerSec, state.elevationVelocityRadPerSec);
    pack.detectionStatus = static_cast<std::uint8_t>((speedRadPerSec > 0.01) ? 1U : 2U);

    // Covariance errors
    if (state.ce90Meters > 0.0) {
        pack.targetCe90M = state.ce90Meters;
    }
    if (state.le90Meters > 0.0) {
        pack.targetLe90M = state.le90Meters;
    }

    // Target geodetic location (lat, lon, HAE)
    if (platformPos.has_value() && slantRangeMeters > 0.0) {
        double targetBearing = std::fmod(state.azimuthRad * kRadToDeg, 360.0);
        if (targetBearing < 0.0) {
            targetBearing += 360.0;
        }

        const double groundDist = slantRangeMeters * std::cos(state.elevationRad);
        const Klv::GeoPoint2D target2D
            = Klv::KlvGeodesy::directGeodetic(Klv::GeoPoint2D { platformPos->latitudeDeg, platformPos->longitudeDeg },
                targetBearing, std::max(0.0, groundDist));

        const double targetHae = platformPos->altitudeM + (slantRangeMeters * std::sin(state.elevationRad));
        pack.targetLocation = Klv::GeoPoint3D { target2D.latitudeDeg, target2D.longitudeDeg, targetHae };
        pack.heightAboveEllipsoidM = targetHae;
    }

    return pack;
}

bool TrackingVmtiBridge::extractPixelDetection(const Klv::VTargetPack& pack, double& outU, double& outV) noexcept
{
    if (pack.centroid.has_value()) {
        outU = static_cast<double>(pack.centroid->col > 0U ? pack.centroid->col - 1U : 0U);
        outV = static_cast<double>(pack.centroid->row > 0U ? pack.centroid->row - 1U : 0U);
        return true;
    }

    if (pack.boundingBox.has_value()) {
        const auto& box = *pack.boundingBox;
        const double c1 = static_cast<double>(box.topLeft.col > 0U ? box.topLeft.col - 1U : 0U);
        const double r1 = static_cast<double>(box.topLeft.row > 0U ? box.topLeft.row - 1U : 0U);
        const double c2 = static_cast<double>(box.bottomRight.col > 0U ? box.bottomRight.col - 1U : 0U);
        const double r2 = static_cast<double>(box.bottomRight.row > 0U ? box.bottomRight.row - 1U : 0U);
        outU = (c1 + c2) * 0.5;
        outV = (r1 + r2) * 0.5;
        return true;
    }

    return false;
}

Klv::VTrackPack TrackingVmtiBridge::buildTrackPack(const SphericalTargetState& state, std::uint32_t trackId,
    Klv::TrackStatus status, double slantRangeMeters, const std::vector<Klv::TrackHistoryPoint>& history,
    std::optional<std::uint64_t> startTimeUs, std::optional<std::uint32_t> durationUs) noexcept
{
    Klv::VTrackPack pack;
    pack.trackId = trackId;
    pack.status = status;

    if (state.locked) {
        const double conf = std::clamp(100.0 - (state.mahalanobisDistance * 10.0), 10.0, 99.0);
        pack.confidence = static_cast<std::uint8_t>(std::lround(conf));
    } else {
        pack.confidence = static_cast<std::uint8_t>(0U);
    }

    if (slantRangeMeters > 0.0) {
        const double vHoriz = slantRangeMeters * std::abs(state.azimuthVelocityRadPerSec);
        const double vUp = slantRangeMeters * state.elevationVelocityRadPerSec;
        pack.groundSpeedMps = vHoriz;
        pack.velocityUpMps = vUp;

        // Heading / course in [0, 360) deg
        double course = std::fmod(state.azimuthRad * kRadToDeg, 360.0);
        if (course < 0.0) {
            course += 360.0;
        }
        pack.groundCourseDeg = course;

        // Velocity East & North
        pack.velocityEastMps = vHoriz * std::sin(state.azimuthRad);
        pack.velocityNorthMps = vHoriz * std::cos(state.azimuthRad);
    }

    pack.historyTrail = history;
    pack.trackStartTimeUs = startTimeUs;
    pack.trackDurationUs = durationUs;

    return pack;
}

Klv::OntologyLocalSet TrackingVmtiBridge::buildOntology(std::uint32_t trackId, Klv::StanagCategory category,
    std::string_view label, std::uint8_t confidence, std::string_view sidc, std::string_view uri) noexcept
{
    Klv::OntologyLocalSet ontology;
    ontology.ontologyId = trackId;
    ontology.ontologyUri = std::string(uri);
    ontology.category = category;
    ontology.classLabel = std::string(label);
    ontology.confidence = confidence;
    ontology.sidcCode = std::string(sidc);
    return ontology;
}

} // namespace Tracking
