#include "TrackingEngine.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Klv/KlvGeodesy.h"
#include "TrackingVmtiBridge.h"

namespace Tracking {

namespace {
    inline constexpr double kPi { 3.14159265358979323846 };
    inline constexpr double kRadToDeg { 180.0 / kPi };
} // namespace

TrackingEngine::TrackingEngine(const TrackingEngineConfig& config) noexcept
    : config_(config)
{
}

void TrackingEngine::reset() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    tracks_.clear();
    lastDetections_.clear();
    nextTrackId_ = 1U;
}

void TrackingEngine::configure(const TrackingEngineConfig& config) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

void TrackingEngine::setPrimaryTrack(std::uint32_t trackId) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_.primaryTrackId = trackId;
}

void TrackingEngine::clearPrimaryTrack() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_.primaryTrackId = std::nullopt;
}

std::optional<std::uint32_t> TrackingEngine::getPrimaryTrack() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.primaryTrackId;
}

std::size_t TrackingEngine::getTrackCount() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t count { 0U };
    for (const auto& trk : tracks_) {
        if (trk && trk->status != Klv::TrackStatus::Dropped) {
            ++count;
        }
    }
    return count;
}

double TrackingEngine::computeIoU(const Klv::PixelBoundingBox& a, const Klv::PixelBoundingBox& b) noexcept
{
    const std::uint32_t xA = std::max(a.topLeft.col, b.topLeft.col);
    const std::uint32_t yA = std::max(a.topLeft.row, b.topLeft.row);
    const std::uint32_t xB = std::min(a.bottomRight.col, b.bottomRight.col);
    const std::uint32_t yB = std::min(a.bottomRight.row, b.bottomRight.row);

    if (xB < xA || yB < yA) {
        return 0.0;
    }

    const double interArea = static_cast<double>(xB - xA + 1U) * static_cast<double>(yB - yA + 1U);
    const double areaA = static_cast<double>(a.width()) * static_cast<double>(a.height());
    const double areaB = static_cast<double>(b.width()) * static_cast<double>(b.height());
    const double unionArea = areaA + areaB - interArea;

    return (unionArea > 0.0) ? (interArea / unionArea) : 0.0;
}

double TrackingEngine::computeCentroidDistance(const Klv::PixelCoord& a, const Klv::PixelCoord& b) noexcept
{
    const double dx = static_cast<double>(a.col) - static_cast<double>(b.col);
    const double dy = static_cast<double>(a.row) - static_cast<double>(b.row);
    return std::hypot(dx, dy);
}

void TrackingEngine::updateDetections(const std::vector<Klv::VTargetPack>& detections, std::uint64_t timestampUs,
    double dtSec, const CameraIntrinsics& intrinsics, double camPanRad, double camTiltRad, double zoom,
    double slantRangeMeters, const std::optional<Klv::GeoPoint3D>& platformPos)
{
    std::lock_guard<std::mutex> lock(mutex_);
    lastDetections_ = detections;

    std::vector<bool> trackMatched(tracks_.size(), false);
    std::vector<bool> detMatched(detections.size(), false);

    // 1. Data Association (Greedy minimum score matching)
    for (std::size_t t = 0U; t < tracks_.size(); ++t) {
        if (!tracks_[t] || tracks_[t]->status == Klv::TrackStatus::Dropped) {
            continue;
        }

        std::size_t bestDetIdx = std::numeric_limits<std::size_t>::max();
        double bestScore = std::numeric_limits<double>::max();

        for (std::size_t d = 0U; d < detections.size(); ++d) {
            if (detMatched[d]) {
                continue;
            }

            const auto& det = detections[d];
            double matchScore = std::numeric_limits<double>::max();

            if (tracks_[t]->lastBoundingBox.has_value() && det.boundingBox.has_value()) {
                const double iou = computeIoU(*tracks_[t]->lastBoundingBox, *det.boundingBox);
                if (iou >= config_.iouThreshold) {
                    matchScore = (1.0 - iou) * 50.0; // Higher weight on IoU
                }
            }

            if (matchScore > 1000.0 && det.centroid.has_value()) {
                const double dist = computeCentroidDistance(tracks_[t]->lastPixelLocation, *det.centroid);
                if (dist <= config_.maxGatingDistancePx) {
                    matchScore = dist;
                }
            }

            if (matchScore < bestScore) {
                bestScore = matchScore;
                bestDetIdx = d;
            }
        }

        if (bestDetIdx < detections.size()) {
            trackMatched[t] = true;
            detMatched[bestDetIdx] = true;

            // Matched track update
            auto& trk = *tracks_[t];
            const auto& det = detections[bestDetIdx];

            trk.hits++;
            trk.coastFrames = 0U;

            if (trk.hits >= config_.minHitsToConfirm && trk.status == Klv::TrackStatus::New) {
                trk.status = Klv::TrackStatus::Active;
            } else if (trk.status == Klv::TrackStatus::Coasting) {
                trk.status = Klv::TrackStatus::Active;
            }

            double detU = 0.0;
            double detV = 0.0;
            if (TrackingVmtiBridge::extractPixelDetection(det, detU, detV)) {
                if (trk.estimator) {
                    trk.estimator->update(detU, detV, camPanRad, camTiltRad, zoom, dtSec);
                }
                trk.lastPixelLocation = Klv::PixelCoord { static_cast<std::uint32_t>(std::lround(detU)) + 1U,
                    static_cast<std::uint32_t>(std::lround(detV)) + 1U };
            }
            if (det.boundingBox.has_value()) {
                trk.lastBoundingBox = det.boundingBox;
            }
            trk.confidence = det.confidence.value_or(static_cast<std::uint8_t>(90U));
            if (trk.estimator) {
                trk.lastState = trk.estimator->getState();
            }

            // Kinematic velocity & course derivation
            if (slantRangeMeters > 0.0) {
                const double vHoriz = slantRangeMeters * std::abs(trk.lastState.azimuthVelocityRadPerSec);
                const double vUp = slantRangeMeters * trk.lastState.elevationVelocityRadPerSec;
                trk.groundSpeedMps = vHoriz;
                trk.velocityUpMps = vUp;

                double course = std::fmod(trk.lastState.azimuthRad * kRadToDeg, 360.0);
                if (course < 0.0) {
                    course += 360.0;
                }
                trk.groundCourseDeg = course;

                const double newVe = vHoriz * std::sin(trk.lastState.azimuthRad);
                const double newVn = vHoriz * std::cos(trk.lastState.azimuthRad);

                if (trk.velocityEastMps.has_value() && dtSec > 0.0) {
                    trk.accelerationEastMps2 = (newVe - *trk.velocityEastMps) / dtSec;
                    trk.accelerationNorthMps2 = (newVn - *trk.velocityNorthMps) / dtSec;
                }
                trk.velocityEastMps = newVe;
                trk.velocityNorthMps = newVn;

                if (platformPos.has_value()) {
                    const double groundDist = slantRangeMeters * std::cos(trk.lastState.elevationRad);
                    const Klv::GeoPoint2D target2D = Klv::KlvGeodesy::directGeodetic(
                        Klv::GeoPoint2D { platformPos->latitudeDeg, platformPos->longitudeDeg }, course,
                        std::max(0.0, groundDist));
                    const double targetHae
                        = platformPos->altitudeM + (slantRangeMeters * std::sin(trk.lastState.elevationRad));
                    trk.lastGeodeticLocation
                        = Klv::GeoPoint3D { target2D.latitudeDeg, target2D.longitudeDeg, targetHae };
                }
            }

            // Append historical trajectory point
            Klv::TrackHistoryPoint histPt;
            histPt.timestampUs = timestampUs;
            histPt.pixelLocation = trk.lastPixelLocation;
            histPt.geodeticLocation = trk.lastGeodeticLocation;
            trk.historyTrail.push_back(histPt);

            if (trk.historyTrail.size() > config_.maxHistoryPoints) {
                trk.historyTrail.erase(trk.historyTrail.begin());
            }

            trk.lastUpdateTimeUs = timestampUs;
        }
    }

    // 2. Coasting / dropped updates for unmatched tracks
    for (std::size_t t = 0U; t < tracks_.size(); ++t) {
        if (!trackMatched[t] && tracks_[t] && tracks_[t]->status != Klv::TrackStatus::Dropped) {
            auto& trk = *tracks_[t];
            trk.coastFrames++;

            if (trk.status == Klv::TrackStatus::Active) {
                trk.status = Klv::TrackStatus::Coasting;
            }

            if (trk.estimator) {
                trk.estimator->predict(dtSec);
                trk.lastState = trk.estimator->getState();
            }

            if (trk.coastFrames > config_.maxCoastingFrames) {
                trk.status = Klv::TrackStatus::Dropped;
            }
        }
    }

    // 3. Initiate new tracks for unmatched detections
    for (std::size_t d = 0U; d < detections.size(); ++d) {
        if (!detMatched[d]) {
            const auto& det = detections[d];
            auto newTrk = std::make_unique<TrackedEntity>();
            newTrk->trackId = nextTrackId_++;
            newTrk->status = Klv::TrackStatus::New;
            newTrk->hits = 1U;
            newTrk->coastFrames = 0U;
            newTrk->startTimeUs = timestampUs;
            newTrk->lastUpdateTimeUs = timestampUs;
            newTrk->confidence = det.confidence.value_or(static_cast<std::uint8_t>(50U));

            SphericalEstimatorConfig estCfg;
            estCfg.intrinsics = intrinsics;
            newTrk->estimator = std::make_unique<PtzSphericalEstimator>(estCfg);

            double detU = 0.0;
            double detV = 0.0;
            if (TrackingVmtiBridge::extractPixelDetection(det, detU, detV)) {
                newTrk->estimator->initFromPixel(detU, detV, camPanRad, camTiltRad, zoom);
                newTrk->lastPixelLocation = Klv::PixelCoord { static_cast<std::uint32_t>(std::lround(detU)) + 1U,
                    static_cast<std::uint32_t>(std::lround(detV)) + 1U };
            }
            newTrk->lastBoundingBox = det.boundingBox;
            newTrk->lastState = newTrk->estimator->getState();

            Klv::TrackHistoryPoint histPt;
            histPt.timestampUs = timestampUs;
            histPt.pixelLocation = newTrk->lastPixelLocation;
            newTrk->historyTrail.push_back(histPt);

            tracks_.push_back(std::move(newTrk));
        }
    }

    // 4. Prune old dropped tracks
    tracks_.erase(
        std::remove_if(tracks_.begin(), tracks_.end(),
            [](const std::unique_ptr<TrackedEntity>& trk) { return !trk || trk->status == Klv::TrackStatus::Dropped; }),
        tracks_.end());
}

bool TrackingEngine::setTrackTaxonomy(std::uint32_t trackId, Klv::StanagCategory category, std::string_view label,
    std::uint8_t confidence, std::string_view sidc) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& trk : tracks_) {
        if (trk && trk->trackId == trackId) {
            Klv::OntologyLocalSet ont;
            ont.ontologyId = trackId;
            ont.ontologyUri = "urn:nato:stanag:1241";
            ont.category = category;
            ont.classLabel = std::string(label);
            ont.confidence = confidence;
            ont.sidcCode = std::string(sidc);
            trk->ontology = ont;
            return true;
        }
    }
    return false;
}

Klv::VmtiLocalSet TrackingEngine::buildVmtiSet(
    std::uint64_t timestampUs, std::uint32_t frameWidth, std::uint32_t frameHeight) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    Klv::VmtiLocalSet vmti;
    vmti.precisionTimeStampUs = timestampUs;
    vmti.version = 6U;
    vmti.frameWidth = frameWidth;
    vmti.frameHeight = frameHeight;
    vmti.targets = lastDetections_;

    for (const auto& trk : tracks_) {
        if (!trk || trk->status == Klv::TrackStatus::Dropped) {
            continue;
        }

        Klv::VTrackPack pack;
        pack.trackId = trk->trackId;
        pack.status = trk->status;
        pack.confidence = trk->confidence;
        pack.velocityEastMps = trk->velocityEastMps;
        pack.velocityNorthMps = trk->velocityNorthMps;
        pack.velocityUpMps = trk->velocityUpMps;
        pack.groundSpeedMps = trk->groundSpeedMps;
        pack.groundCourseDeg = trk->groundCourseDeg;
        pack.accelerationEastMps2 = trk->accelerationEastMps2;
        pack.accelerationNorthMps2 = trk->accelerationNorthMps2;
        pack.historyTrail = trk->historyTrail;
        pack.trackStartTimeUs = trk->startTimeUs;
        if (timestampUs >= trk->startTimeUs) {
            pack.trackDurationUs = static_cast<std::uint32_t>(timestampUs - trk->startTimeUs);
        }
        vmti.tracks.push_back(pack);

        if (trk->ontology.has_value()) {
            vmti.ontologies.push_back(*trk->ontology);
        }
    }

    vmti.totalTargetsDetected = static_cast<std::uint32_t>(vmti.targets.size());
    vmti.numTargetsReported = static_cast<std::uint32_t>(vmti.targets.size());

    return vmti;
}

std::vector<Klv::VTrackPack> TrackingEngine::getActiveTracks() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Klv::VTrackPack> result;
    result.reserve(tracks_.size());
    for (const auto& trk : tracks_) {
        if (trk && trk->status != Klv::TrackStatus::Dropped) {
            Klv::VTrackPack pack;
            pack.trackId = trk->trackId;
            pack.status = trk->status;
            pack.confidence = trk->confidence;
            pack.velocityEastMps = trk->velocityEastMps;
            pack.velocityNorthMps = trk->velocityNorthMps;
            pack.velocityUpMps = trk->velocityUpMps;
            pack.groundSpeedMps = trk->groundSpeedMps;
            pack.groundCourseDeg = trk->groundCourseDeg;
            pack.accelerationEastMps2 = trk->accelerationEastMps2;
            pack.accelerationNorthMps2 = trk->accelerationNorthMps2;
            pack.historyTrail = trk->historyTrail;
            pack.trackStartTimeUs = trk->startTimeUs;
            result.push_back(pack);
        }
    }
    return result;
}

std::vector<Klv::OntologyLocalSet> TrackingEngine::getOntologies() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Klv::OntologyLocalSet> result;
    for (const auto& trk : tracks_) {
        if (trk && trk->status != Klv::TrackStatus::Dropped && trk->ontology.has_value()) {
            result.push_back(*trk->ontology);
        }
    }
    return result;
}

std::optional<PtzAutoTracker::TrackingCommand> TrackingEngine::computePtzCommand(PtzAutoTracker& tracker,
    std::uint32_t frameWidth, std::uint32_t frameHeight, double dtSec, double currentZoom) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_.primaryTrackId.has_value()) {
        return std::nullopt;
    }

    const std::uint32_t targetId = *config_.primaryTrackId;
    for (const auto& trk : tracks_) {
        if (trk && trk->trackId == targetId && trk->status != Klv::TrackStatus::Dropped) {
            Klv::VTargetPack pack;
            pack.targetId = trk->trackId;
            pack.centroid = trk->lastPixelLocation;
            pack.boundingBox = trk->lastBoundingBox;
            pack.confidence = trk->confidence;

            return tracker.updateFromVmti(pack, frameWidth, frameHeight, dtSec, currentZoom);
        }
    }

    return std::nullopt;
}

} // namespace Tracking
