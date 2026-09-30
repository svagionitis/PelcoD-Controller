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

} // namespace Sightline
