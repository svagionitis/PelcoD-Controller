#pragma once

/// @file SightlineBlendingBuilder.h
/// @brief Serializer for Sightline two-channel video blending commands (IDD Blending module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineBlendingBuilder
/// @brief Encodes multi-sensor blending and registration fusion packets (Message ID 0x2F).
class SightlineBlendingBuilder {
public:
    /// @brief Encodes multi-sensor blending fusion (Message ID 0x2F).
    /// @param[in] msg Blend parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetBlendParameters(
        const MsgSetBlendParameters& msg);

    /// @brief Encodes query for active blend parameters (Message ID 0x30).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetBlendParameters();
};

} // namespace Sightline
