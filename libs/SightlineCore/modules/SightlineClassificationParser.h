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
    /// @param[in] packet Validated framed packet bytes.
    /// @param[out] out Deserialized AI detect structure.
    /// @return True on successful parse.
    [[nodiscard]] static bool parseCustomAIDetect(
        const std::vector<std::uint8_t>& packet, MsgCustomAIDetect& out);
};

} // namespace Sightline
