#pragma once

/// @file SightlineStabilizationBuilder.h
/// @brief Serializer for Sightline video stabilization and frame enhancement commands.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineStabilizationBuilder
/// @brief Encodes electronic image stabilization, frame registration, and sensor blending.
class SightlineStabilizationBuilder {
public:
    /// @brief Encodes video stabilization configuration (Message ID 0x02).
    /// @param[in] msg Stabilization parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetStabilization(
        const MsgSetStabilizationParameters& msg);

    /// @brief Encodes stabilization filter reset (Message ID 0x04).
    /// @param[in] msg Reset parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildResetStabilization(
        const MsgResetStabilizationParameters& msg);

    /// @brief Encodes stabilization bias correction (Message ID 0x12).
    /// @param[in] msg Bias parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetStabilizationBias(
        const MsgSetStabilizationBias& msg);

    /// @brief Encodes frame registration parameters (Message ID 0x0E).
    /// @param[in] msg Registration parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetRegistration(
        const MsgSetRegistrationParameters& msg);

    /// @brief Encodes multi-sensor blending fusion (Message ID 0x2F).
    /// @param[in] msg Blend parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetBlendParameters(
        const MsgSetBlendParameters& msg);

    /// @brief Encodes 3D spatio-temporal noise reduction (Message ID 0xAF).
    /// @param[in] msg Noise reduction parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetNoise3D(
        const MsgNoise3D& msg);

    /// @brief Encodes query for active stabilization parameters (Message ID 0x03).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetStabilization(
        std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active registration parameters (Message ID 0x0F).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetRegistration(
        std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active stabilization bias (Message ID 0x28 query 0x12).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetStabilizationBias(
        std::uint8_t cameraIndex = 0U);
};

} // namespace Sightline
