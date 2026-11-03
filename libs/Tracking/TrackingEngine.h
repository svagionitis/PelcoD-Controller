#pragma once

/// @file TrackingEngine.h
/// @brief Multi-target tracking master orchestrator and telemetry generator.

#include <cstddef>
#include <cstdint>

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Klv/VmtiTypes.h"
#include "PtzAutoTracker.h"
#include "PtzCameraModel.h"
#include "PtzSphericalEstimator.h"

namespace Tracking {

/// @struct TrackingEngineConfig
/// @brief Operational configuration for multi-target tracking and data association.
struct TrackingEngineConfig {
    double iouThreshold { 0.3 }; ///< Bounding box IoU threshold for target association [0.0, 1.0].
    double maxGatingDistancePx { 120.0 }; ///< Pixel centroid gating distance threshold.
    std::uint32_t minHitsToConfirm { 3U }; ///< Consecutive hits required to promote New to Active.
    std::uint32_t maxCoastingFrames { 15U }; ///< Maximum coasting frames before dropping lost track.
    std::size_t maxHistoryPoints { 30U }; ///< Maximum historical trajectory waypoints per track.
    std::optional<std::uint32_t> primaryTrackId {}; ///< Primary target ID designated for PTZ framing.
};

/// @class TrackingEngine
/// @brief Multi-target tracking master orchestrator, kinematics estimator, and VMTI telemetry generator.
class TrackingEngine {
public:
    using TrackingCommand = PtzAutoTracker::TrackingCommand;

    explicit TrackingEngine(const TrackingEngineConfig& config = {}) noexcept;
    ~TrackingEngine() = default;

    TrackingEngine(const TrackingEngine&) = delete;
    TrackingEngine& operator=(const TrackingEngine&) = delete;
    TrackingEngine(TrackingEngine&&) = delete;
    TrackingEngine& operator=(TrackingEngine&&) = delete;

    /// @brief Reset all active tracks, associations, and filters.
    void reset() noexcept;

    /// @brief Reconfigure tracking engine parameters at runtime.
    void configure(const TrackingEngineConfig& config) noexcept;

    /// @brief Ingest a frame of visual detections and update all active tracks.
    void updateDetections(const std::vector<Klv::VTargetPack>& detections, std::uint64_t timestampUs, double dtSec,
        const CameraIntrinsics& intrinsics, double camPanRad = 0.0, double camTiltRad = 0.0, double zoom = 1.0,
        double slantRangeMeters = 0.0, const std::optional<Klv::GeoPoint3D>& platformPos = std::nullopt);

    /// @brief Assign or override STANAG taxonomy for a specific track.
    [[nodiscard]] bool setTrackTaxonomy(std::uint32_t trackId, Klv::StanagCategory category, std::string_view label,
        std::uint8_t confidence, std::string_view sidc = "") noexcept;

    /// @brief Designate the primary target track of interest for auto-framing.
    void setPrimaryTrack(std::uint32_t trackId) noexcept;

    /// @brief Clear the designated primary target track.
    void clearPrimaryTrack() noexcept;

    /// @brief Retrieve the current primary target track of interest, if set.
    [[nodiscard]] std::optional<std::uint32_t> getPrimaryTrack() const noexcept;

    /// @brief Retrieve total count of currently active or coasting tracks.
    [[nodiscard]] std::size_t getTrackCount() const noexcept;

    /// @brief Build a comprehensive MISB ST 0903 VmtiLocalSet.
    [[nodiscard]] Klv::VmtiLocalSet buildVmtiSet(
        std::uint64_t timestampUs, std::uint32_t frameWidth, std::uint32_t frameHeight) const;

    /// @brief Retrieve snapshot of all tracked entities.
    [[nodiscard]] std::vector<Klv::VTrackPack> getActiveTracks() const;

    /// @brief Retrieve snapshot of all active ontologies.
    [[nodiscard]] std::vector<Klv::OntologyLocalSet> getOntologies() const;

    /// @brief Get actionable tracking command for the designated primary target.
    [[nodiscard]] std::optional<TrackingCommand> computePtzCommand(PtzAutoTracker& tracker, std::uint32_t frameWidth,
        std::uint32_t frameHeight, double dtSec, double currentZoom = 1.0) const;

private:
    struct TrackedEntity {
        std::uint32_t trackId { 0U };
        Klv::TrackStatus status { Klv::TrackStatus::New };
        std::uint32_t hits { 1U };
        std::uint32_t coastFrames { 0U };
        std::uint64_t startTimeUs { 0U };
        std::uint64_t lastUpdateTimeUs { 0U };

        std::unique_ptr<PtzSphericalEstimator> estimator {};
        SphericalTargetState lastState {};
        Klv::PixelCoord lastPixelLocation {};
        std::optional<Klv::PixelBoundingBox> lastBoundingBox {};
        std::optional<Klv::GeoPoint3D> lastGeodeticLocation {};
        std::uint8_t confidence { 0U };

        std::optional<double> velocityEastMps {};
        std::optional<double> velocityNorthMps {};
        std::optional<double> velocityUpMps {};
        std::optional<double> groundSpeedMps {};
        std::optional<double> groundCourseDeg {};
        std::optional<double> accelerationEastMps2 {};
        std::optional<double> accelerationNorthMps2 {};

        std::vector<Klv::TrackHistoryPoint> historyTrail {};
        std::optional<Klv::OntologyLocalSet> ontology {};
    };

    mutable std::mutex mutex_ {};
    TrackingEngineConfig config_ {};
    std::uint32_t nextTrackId_ { 1U };
    std::vector<std::unique_ptr<TrackedEntity>> tracks_ {};
    std::vector<Klv::VTargetPack> lastDetections_ {};

    [[nodiscard]] static double computeIoU(const Klv::PixelBoundingBox& a, const Klv::PixelBoundingBox& b) noexcept;

    [[nodiscard]] static double computeCentroidDistance(const Klv::PixelCoord& a, const Klv::PixelCoord& b) noexcept;
};

} // namespace Tracking
