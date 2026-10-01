#pragma once

/// @file SightlineCaptureBuilder.h
/// @brief Serializer for Sightline camera acquisition and video mode commands (IDD Capture module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineCaptureBuilder
/// @brief Encodes camera acquisition formatting and freeze/zoom/flip video modes.
class SightlineCaptureBuilder {
public:
    /// @brief Encodes video capture format (Message ID 0x10).
    /// @param[in] msg Video parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVideoParameters(const MsgSetVideoParameters& msg);

    /// @brief Encodes video freeze/zoom/flip mode (Message ID 0x1F).
    /// @param[in] msg Mode parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVideoMode(const MsgSetVideoMode& msg);

    /// @brief Encodes query for active video parameters (Message ID 0x11).
    /// @param[in] cameraIndex Target camera index (0-based).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVideoParameters(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for active video mode (Message ID 0x20).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVideoMode();

    /// @brief Encodes camera input channel switch (Message ID 0x82).
    /// @param[in] msg Camera switch parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCameraSwitch(const MsgCameraSwitch& msg);

    /// @brief Encodes advanced capture deserializer hardware registers (Message ID 0x7B).
    /// @param[in] msg Advanced capture parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetAdvCaptureParams(const MsgAdvancedCaptureParameters& msg);

    /// @brief Encodes query for advanced capture parameters (Message ID 0x28 query 0x7B).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetAdvCaptureParams(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes digital video framing decoder parameters (Message ID 0x91).
    /// @param[in] msg Digital video parser parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDigiVideoParser(const MsgDigitalVideoParserParameters& msg);

    /// @brief Encodes query for digital video parser parameters (Message ID 0x28 query 0x91).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetDigiVideoParser(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes query for camera sensor capabilities (Message ID 0x28 query 0xBB).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCameraCapabilities(std::uint8_t cameraIndex = 0U);
};

} // namespace Sightline
