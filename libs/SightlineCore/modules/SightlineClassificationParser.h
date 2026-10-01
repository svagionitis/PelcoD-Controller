#pragma once

/// @file SightlineClassificationParser.h
/// @brief Parser deserializing raw Sightline SLA AI classification telemetry frames.

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineClassificationParser
/// @brief Deserializes custom AI inference configurations.
class SightlineClassificationParser {
public:
    /// @brief Parses AI detection configuration (Message ID 0xBA).
    /// @details Deserializes model ID, confidence threshold, and NMS suppression threshold.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized AI detect structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseCustomAIDetect(ByteView packet, MsgCustomAIDetect& out);

    /// @brief Parses target thumbnail chip image payload (Message ID 0xAD).
    /// @details Deserializes track ID, chip index, dimensions, and image pixel buffer.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized VMTI chips structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseVMTIChips(ByteView packet, MsgVMTIChips& out);

    /// @brief Parses MISB ST 0903 VMTI field insertion mask (Message ID 0xBF).
    /// @details Deserializes camera index, 32-bit field bitmask, and reporting decimation rate.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized VMTI fields structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseVMTIFields(ByteView packet, MsgVMTIFields& out);

    /// @brief Parses STANAG 4609 KLV target class filtering rules (Message ID 0xC1).
    /// @details Deserializes class bitmask and minimum confidence threshold.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized KLV class filters structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseKlvClassFilters(ByteView packet, MsgKlvClassFilters& out);

    /// @brief Parses multi-class deep learning categorization report (Message ID 0xBD).
    /// @details Deserializes track ID, primary object class, confidence score, and flags.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized multi-class structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseTrackingMultiClass(ByteView packet, MsgTrackingMultiClass& out);

    /// @brief Parses custom neural network classifier pipeline configuration (Message ID 0xA7).
    /// @details Deserializes classifier type, enable flag, and minimum confidence threshold.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized custom classifier structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseCustomClassifier(ByteView packet, MsgCustomClassifier& out);

    /// @brief Parses classifier execution bounds and threshold parameters (Message ID 0xA9).
    /// @details Deserializes model index, NMS threshold, and maximum detections count.
    /// @param[in] packet Validated framed packet bytes or view.
    /// @param[out] out Deserialized classifier parameters structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseClassifierParams(ByteView packet, MsgClassifierParameters& out);
};

} // namespace Sightline
