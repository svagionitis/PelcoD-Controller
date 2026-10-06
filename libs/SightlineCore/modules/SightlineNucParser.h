#pragma once

/// @file SightlineNucParser.h
/// @brief Parser deserializing raw Sightline SLA NUC and dead pixel configuration telemetry packets.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineNucParser
/// @brief Deserializes NUC parameters and dead pixel replacement status frames.
class SightlineNucParser {
public:
    /// @brief Parses NUC/DPR parameters (Message ID 0x35).
    /// @details Requires at least the 4 leading bytes. Tail fields added in later firmware are decoded
    ///          when present; a reply truncated on a field boundary (e.g. 28-byte legacy layout) leaves
    ///          the remaining fields at their struct defaults. @p out is only written on success.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized NUC structure.
    /// @return True on successful parse.
    /// @retval false Wrong message ID, fewer than 4 bytes, enumerator outside the IDD range,
    ///         field cut mid-way, or nucName length overruns the payload.
    [[nodiscard]] static bool parseNucParameters(ByteView packet, MsgNucParameters& out);

    /// @brief Parses a dead pixel list operation (Message ID 0xA8).
    /// @details The board never emits 0xA8; this decoder exists for traffic inspection and simulators.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized dead pixel structure.
    /// @return True on successful parse.
    /// @retval false Wrong message ID, payload shorter than 8 bytes, or mode outside 0..2.
    [[nodiscard]] static bool parseDeadPixel(ByteView packet, MsgDeadPixel& out);

    /// @brief Parses a NUC / dead table read-write reply (Message ID 0x36).
    /// @details Reply to GetParameters [0x36, NucTableQuery, cam]. secondaryFileName and
    ///          interpolationRatio are optional. @p out is only written on success.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized read/write NUC structure.
    /// @return True on successful parse.
    /// @retval false Wrong message ID, reserved mode nibble, or a string overruns the payload.
    [[nodiscard]] static bool parseReadWriteNuc(ByteView packet, MsgReadWriteNuc& out);

    /// @brief Parses custom thermal pseudo-color palette (Message ID 0x72 / 0x73).
    /// @details Deserializes palette slot index and lookup table bytes.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized user palette structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseUserPalette(ByteView packet, MsgUserPalette& out);

    /// @brief Parses dead pixel calibration statistics (Message ID 0xA1).
    /// @details Decodes the 33-byte IDD layout: nDead (s32) and seven per-criterion u32 counters.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized dead pixel stats structure.
    /// @return True on successful parse.
    /// @retval false Wrong message ID or payload shorter than 33 bytes.
    [[nodiscard]] static bool parseDeadPixelStats(ByteView packet, MsgDeadPixelStats& out);

    /// @brief Parses camera intrinsic geometric calibration (Message ID 0xC0).
    /// @details Deserializes focal lengths (fx, fy), principal points (cx, cy), and distortion (k1, k2, p1, p2).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized camera calibration structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseCameraCalibration(ByteView packet, MsgCameraCalibration& out);

    /// @brief Parses camera parameter file status or reply (Message ID 0xC2).
    /// @details Deserializes camera index, action code, and filename string.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized parameter file structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseCameraParameterFile(ByteView packet, MsgCameraParameterFile& out);
};

} // namespace Sightline
