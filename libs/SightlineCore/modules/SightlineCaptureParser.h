#pragma once

/// @file SightlineCaptureParser.h
/// @brief Parser deserializing raw Sightline SLA camera acquisition and video mode telemetry frames.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineCaptureParser
/// @brief Deserializes video acquisition parameters and mode configurations.
class SightlineCaptureParser {
public:
    /// @brief Parses active video capture parameters (Message ID 0x46).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized video parameters.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseVideoParameters(ByteView packet, MsgSetVideoParameters& out);

    /// @brief Parses active video mode parameters (Message ID 0x4B).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized video mode structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseVideoMode(ByteView packet, MsgSetVideoMode& out);

    /// @brief Parses camera switch command / status (Message ID 0x82).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized camera switch structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseCameraSwitch(ByteView packet, MsgCameraSwitch& out);

    /// @brief Parses advanced capture deserializer hardware parameters (Message ID 0x7B).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized advanced capture structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseAdvCaptureParams(ByteView packet, MsgAdvancedCaptureParameters& out);

    /// @brief Parses digital video framing decoder parameters (Message ID 0x91).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized digital video parser structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseDigiVideoParser(ByteView packet, MsgDigitalVideoParserParameters& out);

    /// @brief Parses camera hardware capabilities and limits (Message ID 0xBB).
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized camera capabilities structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseCameraCapabilities(ByteView packet, MsgCameraCapabilities& out);
};

} // namespace Sightline
