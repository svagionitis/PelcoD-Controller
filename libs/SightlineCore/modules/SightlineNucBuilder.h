#pragma once

/// @file SightlineNucBuilder.h
/// @brief Serializer for Sightline non-uniformity correction (NUC) and dead pixel replacement commands.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineNucBuilder
/// @brief Encodes NUC / DPR commands per Sightline IDD v3.11.
/// @details All encoders are stateless and thread-safe. Encoders that can receive structurally
///          invalid input return an empty vector instead of a malformed packet; callers must check
///          `empty()` before transmitting.
class SightlineNucBuilder {
public:
    /// @brief Encodes NUC/DPR parameters and calibration action (Message ID 0x35).
    /// @details Always serialises the full IDD v3.11 layout (35 fixed bytes + nucName).
    /// @param[in] msg NUC parameters.
    /// @return Framed binary packet.
    /// @retval empty `msg.nucName` exceeds 255 characters.
    [[nodiscard]] static std::vector<std::uint8_t> buildNucParameters(const MsgNucParameters& msg);

    /// @brief Encodes a raw dead pixel list operation (Message ID 0xA8).
    /// @details Prefer buildAddDeadPixel / buildRemoveDeadPixel / buildDynamicDead.
    /// @param[in] msg Dead pixel operation.
    /// @return Framed binary packet (8-byte payload).
    [[nodiscard]] static std::vector<std::uint8_t> buildDeadPixel(const MsgDeadPixel& msg);

    /// @brief Encodes "add pixel to dead list" (Message ID 0xA8, mode 0).
    /// @details See EAN-NUC-and-DPR Appendix A1. When sending a list, set @p deferUpdate on every
    ///          pixel except the last so the dead map is rebuilt once.
    /// @param[in] cameraIndex Camera index.
    /// @param[in] column Pixel column (x), 0 = left.
    /// @param[in] row Pixel row (y), 0 = top.
    /// @param[in] deferUpdate True if more pixels follow (c = 1).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildAddDeadPixel(
        std::uint8_t cameraIndex, std::uint16_t column, std::uint16_t row, bool deferUpdate);

    /// @brief Encodes "remove pixel from dead list" (Message ID 0xA8, mode 1).
    /// @details Only remove pixels that were added manually (EAN-NUC-and-DPR section 5.3).
    /// @param[in] cameraIndex Camera index.
    /// @param[in] column Pixel column (x).
    /// @param[in] row Pixel row (y).
    /// @param[in] deferUpdate True if more pixels follow (c = 1).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildRemoveDeadPixel(
        std::uint8_t cameraIndex, std::uint16_t column, std::uint16_t row, bool deferUpdate);

    /// @brief Encodes dynamic dead pixel detection (Message ID 0xA8, mode 2).
    /// @param[in] cameraIndex Camera index.
    /// @param[in] kernelSize Neighbourhood width/height; 0 selects the firmware default (15).
    /// @param[in] maxPixelDiff Dead threshold; 0 selects an automatic per-neighbourhood value.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDynamicDead(
        std::uint8_t cameraIndex, std::uint8_t kernelSize, std::uint8_t maxPixelDiff);

    /// @brief Encodes query for active NUC parameters (Message ID 0x28 query 0x35).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetNucParameters(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes NUC / dead table save, load, interpolate, and default command (Message ID 0x36).
    /// @details mode byte = (defaultOp << 4) | fileOp. secondaryFileName and interpolationRatio are
    ///          always appended (empty / 0 when unused).
    /// @param[in] msg Read/write NUC parameters.
    /// @return Framed binary packet.
    /// @retval empty fileName is blank and the command is not ClearNuc / ClearDead with
    ///         fileOp None (IDD: blank names only accepted for mode 0x30 / 0x40), or a name
    ///         exceeds 255 characters.
    [[nodiscard]] static std::vector<std::uint8_t> buildReadWriteNuc(const MsgReadWriteNuc& msg);

    /// @brief Encodes query for loaded / default table names (Message ID 0x28 query 0x36).
    /// @param[in] query Table selector (payload0).
    /// @param[in] cameraIndex Target camera index (payload1).
    /// @return Framed binary packet; the reply is a MsgReadWriteNuc (0x36).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetReadWriteNuc(
        NucTableQuery query, std::uint8_t cameraIndex = 0U);

    /// @brief Encodes custom pseudo-color thermal palette table (Message ID 0x72).
    /// @param[in] msg User palette table.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetUserPalette(const MsgUserPalette& msg);

    /// @brief Encodes query for active user palette (Message ID 0x28 query 0x72).
    /// @param[in] paletteIndex Palette slot index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetUserPalette(std::uint8_t paletteIndex = 0U);

    /// @brief Encodes query for dead pixel detection statistics (Message ID 0x28 query 0xA1).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet; the reply is a MsgDeadPixelStats (0xA1).
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDeadPixelStats(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes geometric camera intrinsic calibration parameters (Message ID 0xC0).
    /// @param[in] msg Camera calibration parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCameraCalibration(const MsgCameraCalibration& msg);

    /// @brief Encodes query for camera calibration parameters (Message ID 0x28 query 0xC0).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCameraCalibration(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes camera parameter file load/save command (Message ID 0xC2).
    /// @param[in] msg Camera parameter file parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCameraParameterFile(const MsgCameraParameterFile& msg);
};

} // namespace Sightline
