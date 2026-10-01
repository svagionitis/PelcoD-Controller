#pragma once

/// @file SightlineDisplayBuilder.h
/// @brief Serializer for Sightline video output display layout and scaling commands (IDD Display module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineDisplayBuilder
/// @brief Encodes display window offset, layout dimensions, and target display index (Message ID 0x16).
class SightlineDisplayBuilder {
public:
    /// @brief Encodes display layout and scaling (Message ID 0x16).
    /// @param[in] msg Display parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDisplayParams(const MsgSetDisplayParameters& msg);

    /// @brief Encodes query for active display parameters (Message ID 0x3A).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDisplayParams(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes multi-channel video display routing and aspect ratio (Message ID 0xA4).
    /// @param[in] msg Video display parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVideoDisplay(const MsgVideoDisplay& msg);

    /// @brief Encodes query for video display routing (Message ID 0x28 query 0xA4).
    /// @param[in] displayIndex Target display index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVideoDisplay(std::uint8_t displayIndex = 0U);

    /// @brief Encodes multi-display split screen and PiP window routing (Message ID 0xA5).
    /// @param[in] msg Multi-display parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetMultiDisplay(const MsgMultiDisplay& msg);

    /// @brief Encodes query for multi-display configuration (Message ID 0x28 query 0xA5).
    /// @param[in] displayIndex Target display index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetMultiDisplay(std::uint8_t displayIndex = 0U);
};

} // namespace Sightline
