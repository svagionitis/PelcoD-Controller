#pragma once

/// @file SightlineDetectionBuilder.h
/// @brief Serializer for Sightline automatic object and MTI moving target detection commands (IDD Detection module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineDetectionBuilder
/// @brief Encodes MTI moving target detection thresholds, modes, and target size bounds (Message ID 0x2D).
class SightlineDetectionBuilder {
public:
    /// @brief Encodes MTI moving target detection configuration (Message ID 0x2D).
    /// @param[in] msg Detection parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDetectionParams(
        const MsgSetDetectionParameters& msg);

    /// @brief Encodes query for active detection parameters (Message ID 0x2E).
    /// @param[in] cameraIndex Target camera index.
    /// @param[in] detIdx Detection index (0 or 1).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDetectionParams(
        std::uint8_t cameraIndex = 0U, std::uint8_t detIdx = 0U);
};

} // namespace Sightline
