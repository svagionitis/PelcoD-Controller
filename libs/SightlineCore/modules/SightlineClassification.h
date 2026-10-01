#pragma once

/// @file SightlineClassification.h
/// @brief Sightline SLA Classification Module (AI & Deep learning classifiers).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__classification.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgCustomAIDetect
/// @brief AI inference engine configuration and model execution (Message ID 0xBA).
struct MsgCustomAIDetect {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t modelId { 0U };
    std::uint8_t confidenceThreshold { 50U };
    std::uint8_t nmsThreshold { 45U };
};

/// @struct MsgVMTIChips
/// @brief Extracted thumbnail chips of moving detections for downlinks (Message ID 0xAD).
struct MsgVMTIChips {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
    std::uint8_t chipIndex { 0U };
    std::uint8_t totalChips { 1U };
    std::uint16_t chipWidth { 0U };
    std::uint16_t chipHeight { 0U };
    std::vector<std::uint8_t> chipData {};
};

/// @struct MsgVMTIFields
/// @brief Configures MISB ST 0903 VMTI metadata field insertion into KLV streams (Message ID 0xBF).
struct MsgVMTIFields {
    std::uint8_t cameraIndex { 0U };
    std::uint32_t enabledFieldsMask { 0xFFFFFFFFU };
    std::uint8_t reportRate { 1U };
};

/// @struct MsgKlvClassFilters
/// @brief Filter criteria dictating detected object classes generating KLV (Message ID 0xC1).
struct MsgKlvClassFilters {
    std::uint8_t cameraIndex { 0U };
    std::uint16_t classMask { 0xFFFFU };
    std::uint8_t minConfidence { 50U };
};

/// @struct MsgTrackingMultiClass
/// @brief Deep learning multi-class categorization telemetry (Message ID 0xBD).
struct MsgTrackingMultiClass {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t trackId { 0U };
    std::uint8_t primaryClass { 0U };
    std::uint8_t confidence { 0U };
    std::uint8_t flags { 0U };
};

/// @struct MsgCustomClassifier
/// @brief Custom neural network classifier configuration (Message ID 0xA7).
struct MsgCustomClassifier {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t classifierType { 0U };
    std::uint8_t enable { 0U };
    std::uint8_t confidence { 50U };
};

/// @struct MsgClassifierParameters
/// @brief Classifier execution bounds and non-max suppression thresholds (Message ID 0xA9).
struct MsgClassifierParameters {
    std::uint8_t cameraIndex { 0U };
    std::uint8_t modelIndex { 0U };
    std::uint8_t nmsThreshold { 45U };
    std::uint16_t maxDetections { 100U };
};

} // namespace Sightline
