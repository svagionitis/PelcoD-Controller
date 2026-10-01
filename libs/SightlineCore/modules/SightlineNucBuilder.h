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
/// @brief Encodes thermal sensor NUC calibrations, shutter actions, and dead pixel replacement.
class SightlineNucBuilder {
public:
    /// @brief Encodes NUC calibration and shutter mode command (Message ID 0x35).
    /// @param[in] msg NUC parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildNucParameters(const MsgNucParameters& msg);

    /// @brief Encodes dead pixel replacement configuration (Message ID 0xA8).
    /// @param[in] msg Dead pixel parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDeadPixel(const MsgDeadPixel& msg);

    /// @brief Encodes query for active NUC parameters (Message ID 0x28 query 0x35).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetNucParameters(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for dead pixel replacement configuration (Message ID 0x28 query 0xA8).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDeadPixel(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes NUC flash read/write/restore command (Message ID 0x36).
    /// @param[in] msg Read/write NUC parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildReadWriteNuc(const MsgReadWriteNuc& msg);

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
    /// @return Framed binary packet.
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
