#pragma once

/// @file SightlineDetectionParser.h
/// @brief Parser deserializing raw Sightline SLA detection telemetry frames.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineDetectionParser
/// @brief Deserializes active MTI detection parameters (Message ID 0x54 / 0x2D).
class SightlineDetectionParser {
public:
    /// @brief Parses active detection parameters (Message ID 0x54 / 0x2D).
    /// @details Deserializes camera index, mode, sensitivity threshold, and min/max target pixel dimensions.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized detection parameters structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseDetectionParams(ByteView packet, MsgSetDetectionParameters& out);

    /// @brief Parses Video Moving Target Indication (VMTI) parameters (Message ID 0x84).
    /// @details Deserializes enable flag, sensitivity, target area bounds, and mode.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized VMTI structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseVMTI(ByteView packet, MsgSetVMTI& out);

    /// @brief Parses detection region of interest (ROI) parameters (Message ID 0x7C / 0x7D).
    /// @details Deserializes camera index, ROI slot, type (inclusion/exclusion), and bounding box.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized ROI structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseDetectionROI(ByteView packet, MsgDetectionROI& out);

    /// @brief Parses advanced detection parameters (Message ID 0x76 / 0x77).
    /// @details Deserializes velocity thresholds, persistence frames, and merge proximity.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized advanced detection structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseAdvDetectionParams(ByteView packet, MsgAdvancedDetectionParameters& out);

    /// @brief Parses tracking gate luminance pixel statistics (Message ID 0x78).
    /// @details Deserializes mean luminance, standard deviation, and min/max values.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized pixel stats structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseTrackingPixelStats(ByteView packet, MsgTrackingBoxPixelStats& out);
};

} // namespace Sightline
