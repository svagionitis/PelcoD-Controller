#pragma once

/// @file SightlineTrackingBuilder.h
/// @brief Serializer for Sightline video tracking and motion commands.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineTrackingBuilder
/// @brief Encodes target acquisition, tracking control, and MTI detection packets.
class SightlineTrackingBuilder {
public:
    /// @brief Encodes target tracking acquisition command (Message ID 0x08).
    /// @param[in] msg Start tracking parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildStartTracking(const MsgStartTracking& msg);

    /// @brief Encodes precision acquisition with MISB timestamp (Message ID 0x08).
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] col Center column coordinate in pixels.
    /// @param[in] row Center row coordinate in pixels.
    /// @param[in] width Target box width.
    /// @param[in] height Target box height.
    /// @param[in] framePtsUs Microsecond MISB Precision Time Stamp.
    /// @param[in] flags Acquisition flags (default 0x01 primary).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildStartPrecision(
        std::uint8_t cameraIndex, std::uint16_t col, std::uint16_t row,
        std::uint16_t width, std::uint16_t height, std::uint64_t framePtsUs,
        std::uint8_t flags = 0x01U);

    /// @brief Encodes target tracking termination command (Message ID 0x09).
    /// @param[in] msg Stop tracking parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildStopTracking(const MsgStopTracking& msg);

    /// @brief Encodes active track modification command (Message ID 0x05).
    /// @param[in] msg Modify tracking parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildModifyTracking(const MsgModifyTracking& msg);

    /// @brief Encodes target cueing and mode modification (Message ID 0x05).
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] col Target column coordinate.
    /// @param[in] row Target row coordinate.
    /// @param[in] mode Algorithmic modify mode (EAN Appendix C).
    /// @param[in] trackId Target track ID.
    /// @param[in] width Target box width (0 for default).
    /// @param[in] height Target box height (0 for default).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildModifyTrackingMode(
        std::uint8_t cameraIndex, std::uint16_t col, std::uint16_t row,
        ModifyMode mode, std::uint8_t trackId = 0U,
        std::uint8_t width = 0U, std::uint8_t height = 0U);

    /// @brief Encodes sub-pixel tracking nudge command (Message ID 0x0A).
    /// @param[in] msg Coordinate trim nudge parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildNudgeTracking(const MsgNudgeTrackingCoordinate& msg);

    /// @brief Encodes coordinate frame trim nudge (Message ID 0x0A).
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] deltaCol Pixel column offset to nudge.
    /// @param[in] deltaRow Pixel row offset to nudge.
    /// @param[in] coordMode Camera or display coordinate selection.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildNudgeTrackingRotated(
        std::uint8_t cameraIndex, std::int16_t deltaCol, std::int16_t deltaRow,
        NudgeCoordinateMode coordMode = NudgeCoordinateMode::DisplayCoordinates);

    /// @brief Encodes coordinate reporting mode configuration (Message ID 0x0B).
    /// @param[in] msg Reporting parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetReportingMode(const MsgCoordinateReportingMode& msg);

    /// @brief Encodes tracking algorithm tuning parameters (Message ID 0x0C).
    /// @param[in] msg Tracking parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTrackingParameters(const MsgSetTrackingParameters& msg);

    /// @brief Encodes track index modification (stop/primary) (Message ID 0x17).
    /// @param[in] msg Track index modify parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildModifyTrackIndex(const MsgModifyTrackIndex& msg);

    /// @brief Encodes track index modification with typed action (Message ID 0x17).
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] trackIndex Zero-based track index.
    /// @param[in] action Operational action code (stop, primary, reinit, forced coast, resize).
    /// @param[in] width Optional target box width for resizing (default 0).
    /// @param[in] height Optional target box height for resizing (default 0).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildModifyTrackIndex(
        std::uint8_t cameraIndex, std::uint8_t trackIndex, TrackIndexAction action,
        std::uint16_t width = 0U, std::uint16_t height = 0U);

    /// @brief Encodes forced coast override mode via ModifyTrackIndex (Message ID 0x17).
    /// @param[in] cameraIndex Zero-based camera index.
    /// @param[in] trackIndex Zero-based track index.
    /// @param[in] mode Forced coasting algorithmic mode.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildForcedCoasting(
        std::uint8_t cameraIndex, std::uint8_t trackIndex, ForcedCoastingMode mode);

    /// @brief Encodes tracking trail parameters (Message ID 0x9D).
    /// @param[in] msg Track trail parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildTrackTrails(const MsgTrackTrails& msg);

    /// @brief Encodes track promotion to primary (Message ID 0x32).
    /// @param[in] msg Designate primary parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDesignatePrimary(const MsgDesignateSelectedTrackPrimary& msg);

    /// @brief Encodes track gate shift relative to centroid (Message ID 0x33).
    /// @param[in] msg Shift parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildShiftSelectedTrack(const MsgShiftSelectedTrack& msg);

    /// @brief Encodes termination of single track (Message ID 0x3C).
    /// @param[in] msg Stop track parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildStopSelectedTrack(const MsgStopSelectedTrack& msg);

    /// @brief Encodes MTI moving target detection configuration (Message ID 0x2D).
    /// @param[in] msg Detection parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDetectionParams(const MsgSetDetectionParameters& msg);

    /// @brief Encodes custom AI inference model execution (Message ID 0xBA).
    /// @param[in] msg AI detect parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCustomAIDetect(const MsgCustomAIDetect& msg);

    /// @brief Encodes query for active tracking parameters (Message ID 0x0D).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetTrackingParameters(
        std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for tracking trails (Message ID 0x28 query 0x9D).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetTrackTrails(
        std::uint8_t cameraIndex = 0U);
};

} // namespace Sightline
