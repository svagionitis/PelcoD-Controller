#pragma once

/// @file SightlineClassificationBuilder.h
/// @brief Serializer for Sightline AI deep learning classification commands (IDD Classification module).

#include "SightlineFraming.h"
#include "SightlineMessages.h"
#include "SightlineTypes.h"

#include <cstdint>
#include <vector>

namespace Sightline {

/// @class SightlineClassificationBuilder
/// @brief Encodes custom AI deep learning model inference parameters (Message ID 0xBA).
class SightlineClassificationBuilder {
public:
    /// @brief Encodes custom AI inference model execution (Message ID 0xBA).
    /// @param[in] msg AI detect parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildCustomAIDetect(const MsgCustomAIDetect& msg);

    /// @brief Encodes query for custom AI detect parameters (Message ID 0x28 query 0xBA).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCustomAIDetect();

    /// @brief Encodes extracted target thumbnail image chip packet (Message ID 0xAD).
    /// @param[in] msg VMTI chip parameters and raw payload buffer.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildVMTIChips(const MsgVMTIChips& msg);

    /// @brief Encodes MISB ST 0903 KLV stream VMTI field insertion mask (Message ID 0xBF).
    /// @param[in] msg VMTI fields configuration.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVMTIFields(const MsgVMTIFields& msg);

    /// @brief Encodes query for active VMTI fields configuration (Message ID 0x28 query 0xBF).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetVMTIFields(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes STANAG 4609 KLV target class filtering rules (Message ID 0xC1).
    /// @param[in] msg KLV class filter parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetKlvClassFilters(const MsgKlvClassFilters& msg);

    /// @brief Encodes query for active KLV class filter rules (Message ID 0x28 query 0xC1).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetKlvClassFilters(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes deep learning multi-class categorization report (Message ID 0xBD).
    /// @param[in] msg Multi-class telemetry data.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildTrackingMultiClass(const MsgTrackingMultiClass& msg);

    /// @brief Encodes custom neural network classifier pipeline activation (Message ID 0xA7).
    /// @param[in] msg Custom classifier settings.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetCustomClassifier(const MsgCustomClassifier& msg);

    /// @brief Encodes query for custom classifier configuration (Message ID 0x28 query 0xA7).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCustomClassifier(std::uint8_t cameraIndex = 0U);

    /// @brief Encodes classifier execution bounds and threshold parameters (Message ID 0xA9).
    /// @param[in] msg Classifier parameters.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetClassifierParams(const MsgClassifierParameters& msg);

    /// @brief Encodes query for classifier execution parameters (Message ID 0x28 query 0xA9).
    /// @param[in] cameraIndex Target camera index.
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetClassifierParams(std::uint8_t cameraIndex = 0U);
};

} // namespace Sightline
