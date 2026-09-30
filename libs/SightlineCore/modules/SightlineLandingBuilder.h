#pragma once

/// @file SightlineLandingBuilder.h
/// @brief Serializer for Sightline autonomous visual landing aid commands and position updates.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineLandingBuilder
/// @brief Encodes landing aid search/track parameters and landing relative position frames.
class SightlineLandingBuilder {
public:
    /// @brief Encodes landing aid search and tracking command (Message ID 0x81).
    /// @param[in] msg Landing aid parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildLandingAid(
        const MsgLandingAid& msg);

    /// @brief Encodes landing target relative position and orientation (Message ID 0x83).
    /// @param[in] msg Landing position telemetry.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildLandingPosition(
        const MsgLandingPosition& msg);

    /// @brief Encodes query for landing aid parameters (Message ID 0x28 query 0x81).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetLandingAid();
};

} // namespace Sightline
