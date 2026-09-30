#pragma once

/// @file SightlineTracking.h
/// @brief Sightline SLA Tracking Module (User designated object tracking).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__track.html

#include "../SightlineTypes.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace Sightline {

/// @struct MsgStartTracking
/// @brief Primary or secondary target acquisition initiation (Message ID 0x08).
struct MsgStartTracking {
    std::uint8_t cameraIndex { 0U };
    std::uint16_t centerCol { 320U };
    std::uint16_t centerRow { 240U };
    std::uint16_t width { 64U };
    std::uint16_t height { 64U };
    std::uint8_t flags { 0x01U }; // 0x01: Primary, 0x02: Secondary, 0x04: Auto-size
};

/// @struct MsgStopTracking
/// @brief Terminate target tracking (Message ID 0x09).
struct MsgStopTracking {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0xFFU }; // 0xFF: Stop all tracks
};

/// @struct MsgModifyTracking
/// @brief Adjust active tracking parameters or mode (Message ID 0x05).
struct MsgModifyTracking {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
    std::uint8_t mode { 0U };
    std::uint8_t flags { 0U };
};

/// @struct MsgNudgeTrackingCoordinate
/// @brief Sub-pixel manual trim adjustment for primary target track (Message ID 0x0A).
struct MsgNudgeTrackingCoordinate {
    std::uint8_t cameraIndex { 0U };
    std::int16_t deltaCol { 0 };
    std::int16_t deltaRow { 0 };
};

/// @struct MsgSetTrackingParameters
/// @brief Algorithmic parameters for search window, motion models, and gating (Message ID 0x0C).
struct MsgSetTrackingParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t mode { 0U };
    std::uint16_t acquisitionSearchCol { 128U };
    std::uint16_t acquisitionSearchRow { 96U };
    std::uint8_t flags { 0U };
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
struct MsgTrackingPosition {
    std::uint8_t cameraIndex { 0U };
    double col { 0.0 };
    double row { 0.0 };
    double translationCol { 0.0 };
    double translationRow { 0.0 };
    double rotationDeg { 0.0 };
    double scale { 1.0 };
    std::uint8_t confidence { 0U };
    std::uint8_t trackFlags { 0U };
};

/// @struct MsgTrackingPositions
/// @brief High-rate target position and velocity telemetry for all tracks (Message ID 0x51).
struct MsgTrackingPositions {
    std::uint8_t cameraIndex { 0U };
    std::uint64_t timestampUs { 0U };
    std::uint32_t frameNumber { 0U };
    std::vector<TrackCoordinate> tracks {};
};

/// @struct MsgTrackingPositionsExtended
/// @brief Multi-track report with deep learning classifier classifications (Message ID 0xA0).
struct MsgTrackingPositionsExtended {
    std::uint8_t cameraIndex { 0U };
    std::uint64_t timestampUs { 0U };
    std::uint32_t frameNumber { 0U };
    std::vector<TrackCoordinate> tracks {};
    std::vector<std::uint8_t> classIds {};
};

/// @struct MsgTrackTrails
/// @brief Historic positions trail for rendering target movement path (Message ID 0x9D).
struct MsgTrackTrails {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
    std::vector<std::pair<double, double>> historyPoints {};
};

} // namespace Sightline
