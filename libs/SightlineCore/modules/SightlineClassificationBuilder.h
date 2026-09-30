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
    [[nodiscard]] static std::vector<std::uint8_t> buildCustomAIDetect(
        const MsgCustomAIDetect& msg);

    /// @brief Encodes query for custom AI detect parameters (Message ID 0x28 query 0xBA).
    /// @return Framed binary packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildGetCustomAIDetect();
};

} // namespace Sightline
