#pragma once

/// @file SightlineStabilizationBuilder.h
/// @brief Serializer for Sightline video stabilization and frame enhancement commands.
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__stabilize.html
/// @see https://knowledge.sightlineintelligence.com/wp-content/uploads/EAN-Stabilization.pdf

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"
#include "modules/SightlineStabilization.h"

#include <cmath>
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
    [[nodiscard]] static std::vector<std::uint8_t> buildSetStabilization(const MsgSetStabilizationParameters& msg);

    /// @brief Encodes stabilization filter reset (Message ID 0x04).
    /// @param[in] msg Reset parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildResetStabilization(const MsgResetStabilizationParameters& msg);

    /// @brief Encodes stabilization bias correction (Message ID 0x9F).
    /// @param[in] msg Bias parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetStabilizationBias(const MsgSetStabilizationBias& msg);

    /// @brief Encodes frame registration parameters (Message ID 0x9E).
    /// @param[in] msg Registration parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetRegistration(const MsgSetRegistrationParameters& msg);

    /// @brief Encodes multi-sensor blending fusion (Message ID 0x2F).
    /// @note Prefer SightlineBlendingBuilder::buildSetBlendParameters.
    /// @param[in] msg Blend parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetBlendParameters(const MsgSetBlendParameters& msg);

    /// @brief Encodes query for active stabilization parameters (Message ID 0x03).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetStabilization(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active registration parameters (Message ID 0x28 query 0x9E).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetRegistration(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active stabilization bias (Message ID 0x28 query 0x9F).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetStabilizationBias(std::uint8_t cameraIndex = 0U);

    /// @brief Computes feedforward stabilization bias for gimbal movements.
    /// @details Implements EAN-Stabilization section 3.1 formula:
    ///   biasCol = panLeftDegPerSec * horizontalResolution / (horizontalFOVDeg * framesPerSec)
    ///   biasRow = tiltUpDegPerSec * verticalResolution / (verticalFOVDeg * framesPerSec)
    /// @param[in] panLeft Pan angular rate (positive left, negative right) in deg/s.
    /// @param[in] tiltUp Tilt angular rate (positive up, negative down) in deg/s.
    /// @param[in] hRes Camera horizontal resolution in pixels.
    /// @param[in] vRes Camera vertical resolution in pixels.
    /// @param[in] hFov Camera horizontal field of view in degrees.
    /// @param[in] vFov Camera vertical field of view in degrees.
    /// @param[in] fps Frames processed per second.
    /// @param[in] cameraIndex Target camera index (0..3).
    /// @param[in] autoBias Enable automatic bias correction.
    /// @param[in] updateRate Auto bias update rate (0..255).
    /// @return Configured MsgSetStabilizationBias structure.
    [[nodiscard]] static MsgSetStabilizationBias calcGimbalBias(double panLeft, double tiltUp, std::uint16_t hRes,
        std::uint16_t vRes, double hFov, double vFov, double fps, std::uint8_t cameraIndex = 0U,
        std::uint8_t autoBias = 1U, std::uint8_t updateRate = 50U);

    /// @brief Generates registration parameters for designated operational profile.
    /// @param[in] preset Preset operational mode.
    /// @param[in] cameraIndex Target camera index.
    /// @return Configured MsgSetRegistrationParameters.
    [[nodiscard]] static MsgSetRegistrationParameters makeRegistrationPreset(
        StabilizationPreset preset, std::uint8_t cameraIndex = 0U);

    /// @brief Generates stabilization parameters for designated operational profile.
    /// @param[in] preset Preset operational mode.
    /// @param[in] cameraIndex Target camera index.
    /// @return Configured MsgSetStabilizationParameters.
    [[nodiscard]] static MsgSetStabilizationParameters makeStabilizationPreset(
        StabilizationPreset preset, std::uint8_t cameraIndex = 0U);

    /// @brief Generates stabilization bias parameters for designated operational profile.
    /// @param[in] preset Preset operational mode.
    /// @param[in] cameraIndex Target camera index.
    /// @return Configured MsgSetStabilizationBias.
    [[nodiscard]] static MsgSetStabilizationBias makeBiasPreset(
        StabilizationPreset preset, std::uint8_t cameraIndex = 0U);
};

} // namespace Sightline
