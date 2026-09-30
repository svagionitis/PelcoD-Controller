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
    [[nodiscard]] static std::vector<std::uint8_t> buildNucParameters(
        const MsgNucParameters& msg);

    /// @brief Encodes dead pixel replacement configuration (Message ID 0xA8).
    /// @param[in] msg Dead pixel parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildDeadPixel(
        const MsgDeadPixel& msg);

    /// @brief Encodes query for active NUC parameters (Message ID 0x28 query 0x35).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetNucParameters(
        std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for dead pixel replacement configuration (Message ID 0x28 query 0xA8).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDeadPixel(
        std::uint8_t cameraIndex = 0U);
};

} // namespace Sightline
