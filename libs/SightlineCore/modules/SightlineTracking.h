#pragma once

/// @file SightlineTracking.h
/// @brief Sightline SLA Tracking Module (User designated object tracking).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__track.html

#include "../SightlineTypes.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace Sightline {

// Note: TrackingMode, ModifyMode, ForcedCoastingMode, and TrackingFlags are
// canonically defined in SightlineTypes.h (included above).

/// @struct MsgStartTracking
/// @brief Primary or secondary target acquisition initiation (Message ID 0x08).
/// @details Conforms to official Sightline SLAStartTracking_t struct layout.
struct MsgStartTracking {
    std::uint8_t cameraIndex { 0U };
    std::uint16_t centerCol { 320U };
    std::uint16_t centerRow { 240U };
    std::uint16_t width { 64U };
    std::uint16_t height { 64U };
    std::uint8_t flags { 0x01U }; // 0x01: Primary, 0x02: Secondary, 0x04: Auto-size
    std::uint16_t nearVal { 0U };
    std::uint8_t userTrackId { 0U };
    std::uint64_t framePts { 0ULL };
};

/// @struct MsgStopTracking
/// @brief Terminate target tracking (Message ID 0x09).
/// @details Official Sightline SLAStopTracking_t format: reserved1(0), reserved2(0), reserved3(0), cameraIndex.
struct MsgStopTracking {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0xFFU }; // 0xFF: Stop all tracks via 0x09; individual IDs use 0x17
};

/// @struct MsgModifyTrackIndex
/// @brief Modify a particular track by its index (stop or designate as primary) (Message ID 0x17).
/// @details Conforms to official Sightline SLAModifyTrackIndex_t struct layout.
struct MsgModifyTrackIndex {
    std::uint8_t trackIndex { 0U };
    std::uint8_t flags { 0U }; // 0: Stop, 1: Primary, 2: Reinit, 3: Coast off, 4: Coast on, 8: Resize no AA, 9: Resize AA
    std::uint8_t cameraIndex { 0U };
    std::uint16_t width { 0U };
    std::uint16_t height { 0U };
};

/// @struct MsgModifyTracking
/// @brief Adjust active tracking parameters or mode (Message ID 0x05).
/// @details Official Sightline SLAModifyTracking_t format: col, row, flags, width, height, cameraIndex.
struct MsgModifyTracking {
    std::uint16_t col { 0U };
    std::uint16_t row { 0U };
    std::uint8_t flags { 0U };
    std::uint8_t width { 0U };
    std::uint8_t height { 0U };
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
    std::uint8_t mode { 0U };
};

/// @struct MsgNudgeTrackingCoordinate
/// @brief Sub-pixel manual trim adjustment for primary target track (Message ID 0x0A).
/// @details Official Sightline SLANudgeTrackingCoordinate_t format.
struct MsgNudgeTrackingCoordinate {
    std::int8_t offsetCol { 0 };
    std::int8_t offsetRow { 0 };
    std::uint8_t rotate { 0U };
    std::uint8_t cameraIndex { 0U };
    std::int16_t deltaCol { 0 };
    std::int16_t deltaRow { 0 };
};

/// @struct MsgSetTrackingParameters
/// @brief Algorithmic parameters for search window, motion models, and gating (Message ID 0x0C).
/// @details Conforms to official Sightline SLASetTrackingParameters_t struct layout.
struct MsgSetTrackingParameters {
    std::uint8_t objectSize { 32U };
    std::uint8_t mode { 1U };
    std::uint8_t mode2 { 0U };
    std::uint8_t maxMisses { 45U }; // Default is 45 frames (1.5s @ 30Hz) per EAN Sec 7.2
    std::uint16_t nearVal { 0U };
    std::uint8_t objectHeight { 0U };
    std::uint8_t cameraIndex { 0U };
    std::uint8_t zoomSmoothing { 5U }; // Default is 5 per EAN Sec 5.2
    std::uint8_t rollSmoothing { 5U }; // Default is 5 per EAN Sec 5.2
    std::uint8_t maxTracks { 10U };
    std::uint16_t acquisitionSearchCol { 128U };
    std::uint16_t acquisitionSearchRow { 96U };
    std::uint8_t flags { 0U };
    std::uint8_t maxPauseTime { 0U }; // Measured in seconds (0..20) per EAN Sec 4.9.1
};

/// @struct MsgDesignateSelectedTrackPrimary
/// @brief Elevates an existing secondary track to primary status (Message ID 0x32).
struct MsgDesignateSelectedTrackPrimary {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
};

/// @struct MsgShiftSelectedTrack
/// @brief Shifts track gate position relative to target centroid (Message ID 0x33).
struct MsgShiftSelectedTrack {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
    std::int16_t shiftCol { 0 };
    std::int16_t shiftRow { 0 };
};

/// @struct MsgStopSelectedTrack
/// @brief Stops a single identified track (Message ID 0x3C).
struct MsgStopSelectedTrack {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
};

/// @struct MsgTrackingPosition
/// @brief Single primary track position and scene motion telemetry (Message ID 0x43).
/// @details Conforms to official Sightline SLATrackingPosition_t struct layout with subpixel accuracy.
struct MsgTrackingPosition {
    std::uint8_t cameraIndex { 0U };
    double col { 0.0 };
    double row { 0.0 };
    double translationCol { 0.0 };
    double translationRow { 0.0 };
    double offsetCol { 0.0 };
    double offsetRow { 0.0 };
    double rotationDeg { 0.0 };
    double scale { 1.0 };
    std::uint8_t confidence { 0U }; // Masked correlation score 0..100%
    bool isCoasting { false };      // True if target occluded (confidence MSB 0x80)
    std::uint8_t sceneConfidence { 0U };
    std::uint8_t trackFlags { 0U };
    std::uint8_t userTrackId { 0U };
    std::uint64_t timestampUs { 0U };
    std::uint32_t frameNumber { 0U };
};

/// @struct MsgTrackingPositions
/// @brief High-rate target position and velocity telemetry for all tracks (Message ID 0x51).
/// @details Conforms to official Sightline SLATrackingPositions_t struct layout.
struct MsgTrackingPositions {
    std::uint8_t cameraIndex { 0U };
    std::uint64_t timestampUs { 0U };
    std::uint32_t frameNumber { 0U };
    std::vector<TrackCoordinate> tracks {};
};

/// @struct MsgTrackingPositionsExtended
/// @brief Multi-track report with deep learning classifier classifications (Message ID 0xA0).
/// @details Conforms to official Sightline SLATrackingPositionsExtended_t struct layout.
struct MsgTrackingPositionsExtended {
    std::uint8_t cameraIndex { 0U };
    std::uint64_t timestampUs { 0U };
    std::uint32_t frameNumber { 0U };
    std::vector<TrackCoordinate> tracks {};
    std::vector<std::uint8_t> classIds {};
};

/// @struct MsgTrackTrails
/// @brief Target tracking trails configuration and history (Message ID 0x9D).
/// @details Conforms to official Sightline SLATrackTrails_t struct layout.
struct MsgTrackTrails {
    std::uint8_t cameraIndex { 0U };
    std::uint16_t flags { 0U };
    std::uint16_t tracksLen { 0U };
    std::uint16_t detectionLen { 0U };
    std::uint8_t trackId { 0U };
    std::vector<std::pair<double, double>> historyPoints {};
};

} // namespace Sightline
