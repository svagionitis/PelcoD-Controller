#pragma once

/// @file GimbalSectorBlanking.h
/// @brief Spatial Sector Blanking & Laser Safety Keep-Out Zones preventing gimbal collisions
///        with host platform structures and inhibiting high-energy laser emissions in restricted sectors.

#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace PayloadHal {

/// @enum SectorZoneType
/// @brief Functional restriction enforced by a spatial sector zone.
enum class SectorZoneType : std::uint8_t {
    MechanicalKeepOut, ///< Prevents gimbal motion into physical vehicle structure
    LaserInhibit,      ///< Eye-safety & sensor hazard zone inhibiting LRF and Illuminators
    VideoBlanking,     ///< Privacy or structural masking zone blanking the video feed
    TotalExclusion     ///< Mechanical keep-out + Laser inhibit + Video blanking
};

/// @enum SectorReferenceFrame
/// @brief Coordinate frame in which the sector boundary angles are expressed.
enum class SectorReferenceFrame : std::uint8_t {
    GimbalMount,  ///< Relative to gimbal base pedestal (Pan, Tilt)
    PlatformBody, ///< Relative to platform forward nose/horizon
    GeodeticNorth ///< True geographic bearing and horizon elevation
};

/// @struct AngularPoint2D
/// @brief 2D spherical coordinate point in degrees.
struct AngularPoint2D {
    double azimuthDeg { 0.0 };   ///< Azimuth in degrees [-180, 180] or [0, 360)
    double elevationDeg { 0.0 }; ///< Elevation in degrees [-90, +90]
};

/// @struct BlankingZone
/// @brief Comprehensive specification for an angular exclusion zone.
struct BlankingZone {
    std::string id {};                             ///< Unique identifier (e.g. "TailBoomKeepOut")
    std::string description {};                    ///< Human-readable description
    SectorZoneType type { SectorZoneType::LaserInhibit };
    SectorReferenceFrame frame { SectorReferenceFrame::GimbalMount };
    bool enabled { true };                         ///< Master enable flag for zone
    double safetyMarginDeg { 1.5 };                ///< Pre-warning / buffer margin around boundary in degrees

    // Rectangular specification (used if polygonVertices is empty)
    double azMinDeg { 0.0 };
    double azMaxDeg { 0.0 };
    double elMinDeg { -90.0 };
    double elMaxDeg { +90.0 };

    // Arbitrary polygonal boundary specification
    std::vector<AngularPoint2D> polygonVertices {}; ///< Ordered polygon vertices
};

/// @struct SectorEvaluationResult
/// @brief Instantaneous safety and restriction status for given gimbal angles.
struct SectorEvaluationResult {
    bool mechanicalAllowed { true };
    bool laserAllowed { true };
    bool videoBlanked { false };
    bool inWarningMargin { false };
    double distanceToNearestZoneDeg { 999.0 };
    std::vector<std::string> activeZoneIds {};
};

/// @struct PathValidationResult
/// @brief Result of checking a proposed slew motion path across keep-out zones.
struct PathValidationResult {
    bool pathClear { true };
    bool clamped { false };
    double safeTargetAzDeg { 0.0 };
    double safeTargetElDeg { 0.0 };
    std::string blockingZoneId {};
};

/// @class GimbalSectorBlanking
/// @brief Spatial exclusion engine managing mechanical travel limits, ANSI Z136 laser
///        inhibition interlocks, and video masking across configurable spherical angular sectors.
class GimbalSectorBlanking {
public:
    GimbalSectorBlanking() = default;
    explicit GimbalSectorBlanking(std::vector<BlankingZone> initialZones);
    virtual ~GimbalSectorBlanking() = default;

    // --- Zone Management ---

    /// @brief Registers a new blanking zone.
    /// @param[in] zone Zone configuration to add.
    /// @return true if zone was added, false if ID already exists or geometry is invalid.
    bool addZone(const BlankingZone& zone);

    /// @brief Updates an existing blanking zone.
    /// @param[in] zone Updated zone specification (matched by ID).
    /// @return true if updated, false if zone ID was not found.
    bool updateZone(const BlankingZone& zone);

    /// @brief Removes a zone by unique identifier.
    /// @param[in] zoneId Identifier of zone to delete.
    /// @return true if found and removed.
    bool removeZone(const std::string& zoneId);

    /// @brief Clears all registered blanking zones.
    void clearZones();

    /// @brief Enables or disables an existing zone without removing it.
    /// @param[in] zoneId Identifier of zone.
    /// @param[in] enabled Target active state.
    /// @return true if zone was found and state updated.
    bool setZoneEnabled(const std::string& zoneId, bool enabled);

    /// @brief Queries all currently registered blanking zones.
    [[nodiscard]] std::vector<BlankingZone> zones() const;

    /// @brief Retrieves a specific zone by ID.
    [[nodiscard]] std::optional<BlankingZone> zone(const std::string& zoneId) const;

    // --- Safety State Evaluation ---

    /// @brief Evaluates all active zones for a given orientation.
    /// @param[in] azDeg Current azimuth angle in degrees.
    /// @param[in] elDeg Current elevation angle in degrees.
    /// @param[in] frame Coordinate frame of the input angles.
    /// @return SectorEvaluationResult with instantaneous safety restrictions.
    [[nodiscard]] SectorEvaluationResult evaluate(
        double azDeg, double elDeg,
        SectorReferenceFrame frame = SectorReferenceFrame::GimbalMount) const;

    /// @brief Checks whether high-energy laser firing (LRF / Illuminator) is permitted.
    /// @param[in] azDeg Azimuth angle in degrees.
    /// @param[in] elDeg Elevation angle in degrees.
    /// @param[in] frame Coordinate frame.
    /// @return true if clear to fire laser, false if inhibited.
    [[nodiscard]] bool isLaserAllowed(
        double azDeg, double elDeg,
        SectorReferenceFrame frame = SectorReferenceFrame::GimbalMount) const;

    /// @brief Checks whether gimbal motion / dwell is permitted at the specified orientation.
    /// @param[in] azDeg Azimuth angle in degrees.
    /// @param[in] elDeg Elevation angle in degrees.
    /// @param[in] frame Coordinate frame.
    /// @return true if clear to position gimbal, false if in mechanical keep-out zone.
    [[nodiscard]] bool isMotionAllowed(
        double azDeg, double elDeg,
        SectorReferenceFrame frame = SectorReferenceFrame::GimbalMount) const;

    /// @brief Checks whether video feed should be blanked/masked.
    /// @param[in] azDeg Azimuth angle in degrees.
    /// @param[in] elDeg Elevation angle in degrees.
    /// @param[in] frame Coordinate frame.
    /// @return true if video blanking is active.
    [[nodiscard]] bool isVideoBlanked(
        double azDeg, double elDeg,
        SectorReferenceFrame frame = SectorReferenceFrame::GimbalMount) const;

    /// @brief Validates a proposed slew trajectory from start orientation to target orientation.
    /// @details Detects whether the motion path traverses any active keep-out zones and
    ///          computes safe clamped arrival angles outside the obstruction perimeter.
    /// @param[in] fromAzDeg Starting azimuth in degrees.
    /// @param[in] fromElDeg Starting elevation in degrees.
    /// @param[in] toAzDeg Proposed target azimuth in degrees.
    /// @param[in] toElDeg Proposed target elevation in degrees.
    /// @param[in] checkType Type of zone constraint to check against.
    /// @param[in] frame Coordinate frame.
    /// @return PathValidationResult with status and safe clamped coordinates.
    [[nodiscard]] PathValidationResult validatePath(
        double fromAzDeg, double fromElDeg,
        double toAzDeg, double toElDeg,
        SectorZoneType checkType = SectorZoneType::MechanicalKeepOut,
        SectorReferenceFrame frame = SectorReferenceFrame::GimbalMount) const;

    // --- Interlock Callback ---

    /// @brief Signature for laser interlock transition notification.
    using InterlockCallback = std::function<void(bool laserAllowed, const std::string& activeZoneId)>;

    /// @brief Registers a callback triggered when laser interlock state transitions.
    /// @param[in] cb Callback to notify.
    void registerInterlockCallback(InterlockCallback cb);

private:
    mutable std::mutex m_mutex {};
    std::vector<BlankingZone> m_zones {};
    InterlockCallback m_interlockCb {};
    mutable bool m_lastLaserAllowed { true };

    [[nodiscard]] static bool isPointInZone(const BlankingZone& zone, double azDeg, double elDeg);
    [[nodiscard]] static double distanceToZone(const BlankingZone& zone, double azDeg, double elDeg);
    [[nodiscard]] static double normalizeAzimuth(double azDeg) noexcept;
};

} // namespace PayloadHal
