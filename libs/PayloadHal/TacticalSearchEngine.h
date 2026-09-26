#pragma once

/// @file TacticalSearchEngine.h
/// @brief Automated tactical search pattern generator and prioritized Slew-to-Cue engine.

#include "ICameraPayload.h"
#include "ILaserRangeFinder.h"
#include "IPanTiltUnit.h"
#include "Klv/KlvTypes.h"
#include "PayloadTypes.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace PayloadHal {

/// @enum SearchPatternType
/// @brief Tactical scanning patterns supported by the pattern generator.
enum class SearchPatternType : std::uint8_t {
    None,            ///< No active pattern
    SectorScan,      ///< Horizontal back-and-forth raster scan with elevation stepping
    ExpandingSquare, ///< IAMSAR concentric expanding square search around datum
    CreepingLine,    ///< Parallel sweep lines advancing along a search baseline
    SpiralScan       ///< Archimedean expanding continuous spiral from datum
};

/// @struct SectorScanConfig
/// @brief Configuration parameters for a horizontal/vertical sector raster scan.
struct SectorScanConfig {
    double minAzimuthDeg { -45.0 };          ///< Left azimuth bound in degrees [-180, 180]
    double maxAzimuthDeg { 45.0 };           ///< Right azimuth bound in degrees [-180, 180]
    double minElevationDeg { -10.0 };        ///< Lower elevation bound in degrees [-90, 90]
    double maxElevationDeg { 10.0 };         ///< Upper elevation bound in degrees [-90, 90]
    double scanSpeedDegPerSec { 15.0 };      ///< Horizontal slew speed in degrees/second
    double elevationStepDeg { 0.0 };         ///< Elevation step per raster (0.0 = auto from camera VFOV)
    double overlapRatio { 0.15 };            ///< Inter-raster overlap ratio [0.05, 0.50]
    bool bidirectional { true };             ///< True for boustrophedon (alternating left/right), false for raster
    std::uint32_t repeatCount { 0U };        ///< Max cycles (0 = continuous)
};

/// @struct ExpandingSquareConfig
/// @brief Configuration parameters for an expanding square SAR search pattern.
struct ExpandingSquareConfig {
    double centerAzimuthDeg { 0.0 };         ///< Datum center azimuth in degrees
    double centerElevationDeg { 0.0 };       ///< Datum center elevation in degrees
    double maxRadiusDeg { 30.0 };            ///< Maximum search radius from center in degrees
    double stepSizeDeg { 0.0 };              ///< Square leg step size (0.0 = auto from camera HFOV)
    double scanSpeedDegPerSec { 10.0 };      ///< Slew speed along legs in degrees/second
    double overlapRatio { 0.15 };            ///< Overlap fraction between successive square loops
    std::uint32_t repeatCount { 0U };        ///< Repeat cycles (0 = continuous)
};

/// @struct CreepingLineConfig
/// @brief Configuration parameters for creeping line parallel search sweeps.
struct CreepingLineConfig {
    double baselineHeadingDeg { 0.0 };       ///< Axis along which the pattern advances [0, 360)
    double sweepWidthDeg { 60.0 };           ///< Total cross-track sweep width in degrees
    double stepSizeDeg { 0.0 };              ///< Step increment along baseline (0.0 = auto from camera HFOV)
    double elevationDeg { 0.0 };             ///< Fixed scan elevation in degrees
    double scanSpeedDegPerSec { 12.0 };      ///< Slew speed in degrees/second
    double overlapRatio { 0.15 };            ///< Sweep leg overlap ratio
    std::uint32_t repeatCount { 0U };        ///< Repeat cycles (0 = continuous)
};

/// @struct SpiralScanConfig
/// @brief Configuration parameters for an Archimedean spiral search pattern.
struct SpiralScanConfig {
    double centerAzimuthDeg { 0.0 };         ///< Datum center azimuth in degrees
    double centerElevationDeg { 0.0 };       ///< Datum center elevation in degrees
    double maxRadiusDeg { 25.0 };            ///< Outer radius cutoff in degrees
    double expansionRateDeg { 0.0 };         ///< Radial expansion per revolution (0.0 = auto from camera HFOV)
    double angularVelocityDegPerSec { 20.0 };///< Rotation rate in degrees/second
    std::uint32_t repeatCount { 0U };        ///< Repeat cycles (0 = continuous)
};

/// @enum CueSource
/// @brief Source classification of incoming Slew-to-Cue target contacts.
enum class CueSource : std::uint8_t {
    Manual,          ///< Operator click-to-point or manual designation
    Radar,           ///< Surface or air search radar track
    Adsb,            ///< Secondary surveillance radar / ADS-B transponder
    Ais,             ///< Automatic Identification System maritime contact
    Acoustic,        ///< Acoustic gunshot / drone sound detector
    OpticalCrossCue  ///< Auxiliary camera or thermal optical detection
};

/// @enum CuePriority
/// @brief Priority ranking governing target preemption and queue sorting.
enum class CuePriority : std::uint8_t {
    Flash = 0,       ///< Immediate preemption of all active scans and cues (e.g. gunshot, hostile UAS)
    Immediate = 1,   ///< Preempts active search patterns immediately (e.g. radar track)
    Priority = 2,    ///< Executed after current leg/dwell completes
    Routine = 3      ///< Background or periodic track check
};

/// @struct TargetCue
/// @brief Target contact cue descriptor for prioritized line-of-sight slew.
struct TargetCue {
    std::string cueId {};                                    ///< Unique cue identifier
    CueSource source { CueSource::Radar };                   ///< Sensor originating the cue
    CuePriority priority { CuePriority::Immediate };         ///< Slew priority rank
    std::optional<double> panAngleDeg {};                    ///< Relative pan angle in degrees
    std::optional<double> tiltAngleDeg {};                   ///< Relative tilt angle in degrees
    std::optional<Klv::GeoPoint3D> geoTarget {};             ///< Geodetic 3D coordinate target
    double estimatedSpeedMps { 0.0 };                        ///< Target velocity magnitude
    double estimatedHeadingDeg { 0.0 };                      ///< Target course over ground
    std::chrono::milliseconds dwellTime { 10000 };           ///< Hold duration once arrived
    std::chrono::system_clock::time_point timestamp {};      ///< Generation timestamp
    std::chrono::seconds timeToLive { 30 };                  ///< Expiration if unserviced
    std::string metadata {};                                 ///< Optional telemetry notes
};

/// @enum TacticalEngineState
/// @brief Operational state of the Tactical Search Engine.
enum class TacticalEngineState : std::uint8_t {
    Idle,             ///< Stationary; neither pattern nor cue active
    ExecutingPattern, ///< Running active automated wide-area search pattern
    SlewingToCue,     ///< Slewing towards high-priority target cue
    DwellingAtCue,    ///< Holding line-of-sight on acquired cue
    TrackingCue,      ///< Active hand-off with Auto-Tracker / LRF pinging
    Paused,           ///< Halted due to operator intervention or pause command
    Fault             ///< PTU or communication error occurred
};

/// @struct TacticalEngineStatus
/// @brief Live operational status and telemetry of the search and cueing engine.
struct TacticalEngineStatus {
    TacticalEngineState state { TacticalEngineState::Idle };
    SearchPatternType activePattern { SearchPatternType::None };
    std::string activeCueId {};
    std::size_t pendingCueCount { 0U };
    double currentAzimuthDeg { 0.0 };
    double currentElevationDeg { 0.0 };
    std::chrono::milliseconds remainingDwell { 0 };
    std::uint32_t completedLoops { 0U };
    std::string statusMessage {};
};

/// @class TacticalSearchEngine
/// @brief High-level tactical scanning and Slew-to-Cue coordinator.
class TacticalSearchEngine {
public:
    using StatusCallback = std::function<void(const TacticalEngineStatus& status)>;
    using CueAcquiredCallback = std::function<void(const TargetCue& cue)>;

    TacticalSearchEngine(std::shared_ptr<IPanTiltUnit> ptu,
                         std::shared_ptr<ICameraPayload> camera,
                         std::shared_ptr<ILaserRangeFinder> lrf = nullptr);
    ~TacticalSearchEngine();

    // --- Search Pattern Controls ---
    bool startSectorScan(const SectorScanConfig& config);
    bool startExpandingSquare(const ExpandingSquareConfig& config);
    bool startCreepingLine(const CreepingLineConfig& config);
    bool startSpiralScan(const SpiralScanConfig& config);
    void pausePattern();
    void resumePattern();
    void stopPattern();

    // --- Slew-to-Cue Priority Queue ---
    bool enqueueCue(const TargetCue& cue);
    bool cancelCue(const std::string& cueId);
    void clearCues();
    [[nodiscard]] std::optional<TargetCue> activeCue() const;
    [[nodiscard]] std::vector<TargetCue> pendingCues() const;

    // --- Operator Overrides & Interlocks ---
    void notifyManualIntervention();

    // --- Telemetry & Observers ---
    [[nodiscard]] TacticalEngineStatus status() const;
    void registerStatusCallback(StatusCallback cb);
    void registerCueAcquiredCallback(CueAcquiredCallback cb);

    // --- Georeference Coordinates ---
    void updateHostPlatform(const Klv::GeoPoint3D& platformPos, double headingDeg);

    // --- Configuration Accessors ---
    void setArrivalThresholdDeg(double degrees) noexcept;
    [[nodiscard]] double arrivalThresholdDeg() const noexcept;

private:
    void engineLoop();
    void stepPattern(double dtSec);
    void processCues();
    bool checkArrival(double targetPanDeg, double targetTiltDeg);
    double effectiveHFOV() const;
    double effectiveVFOV() const;

    std::shared_ptr<IPanTiltUnit> m_ptu;
    std::shared_ptr<ICameraPayload> m_camera;
    std::shared_ptr<ILaserRangeFinder> m_lrf;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_running { false };
    std::thread m_workerThread;

    TacticalEngineStatus m_status {};
    StatusCallback m_statusCb {};
    CueAcquiredCallback m_cueAcquiredCb {};

    // Slew-to-cue priority queue
    std::vector<TargetCue> m_cueQueue {};
    std::optional<TargetCue> m_currentCue {};

    // Active pattern configurations
    SectorScanConfig m_sectorConfig {};
    ExpandingSquareConfig m_squareConfig {};
    CreepingLineConfig m_creepingConfig {};
    SpiralScanConfig m_spiralConfig {};

    // Pattern generator internal state
    double m_patternTimeSec { 0.0 };
    double m_patternCurrentPanDeg { 0.0 };
    double m_patternCurrentTiltDeg { 0.0 };
    bool m_patternSweepForward { true };
    std::size_t m_patternLegIndex { 0U };
    double m_patternLegLengthDeg { 0.0 };
    double m_patternLegTraversedDeg { 0.0 };

    // Slew & Arrival tracking
    double m_arrivalThresholdDeg { 0.5 };
    std::chrono::steady_clock::time_point m_dwellStartTime {};

    // Host platform reference for geodetic cues
    Klv::GeoPoint3D m_platformPos {};
    double m_platformHeadingDeg { 0.0 };
};

} // namespace PayloadHal
