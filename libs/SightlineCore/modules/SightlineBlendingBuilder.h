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
    [[nodiscard]] static std::vector<std::uint8_t> buildSetBlendParameters(const MsgSetBlendParameters& msg);

    /// @brief Encodes query for active blend parameters (Message ID 0x30).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetBlendParameters();

    /// @brief Encodes 4-point projective homography calibration (Message ID 0x95).
    /// @param[in] msg Four align points parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildFourAlignPoints(const MsgFourAlignPoints& msg);

    /// @brief Encodes query for 4-point projective calibration (Message ID 0x28 query 0x95).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetFourAlignPoints(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes fine-tune alignment offsets and automated registration (Message ID 0xB9).
    /// @param[in] msg Blend align parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetBlendAlign(const MsgBlendAlign& msg);

    /// @brief Encodes query for blend alignment parameters (Message ID 0x28 query 0xB9).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetBlendAlign(std::uint8_t cameraIndex = 0U);
};

} // namespace Sightline
