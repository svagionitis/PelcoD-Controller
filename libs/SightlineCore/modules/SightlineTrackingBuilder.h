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

    /// @brief Encodes target tracking termination command (Message ID 0x09).
    /// @param[in] msg Stop tracking parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildStopTracking(const MsgStopTracking& msg);

    /// @brief Encodes active track modification command (Message ID 0x05).
    /// @param[in] msg Modify tracking parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildModifyTracking(const MsgModifyTracking& msg);

    /// @brief Encodes sub-pixel tracking nudge command (Message ID 0x0A).
    /// @param[in] msg Coordinate trim nudge parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildNudgeTracking(const MsgNudgeTrackingCoordinate& msg);

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
