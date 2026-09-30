#pragma once

/// @file SightlineCompression.h
/// @brief Sightline SLA Compression Module (H.264/H.265 encoding and streaming control).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__compress.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgSetH264Parameters
/// @brief Encoder bitrate, GOP structure, and profile settings (Message ID 0x23).
struct MsgSetH264Parameters {
    std::uint8_t streamIndex { 0U };
    std::uint32_t targetBitrateBps { 4000000U }; // 4 Mbps
    std::uint16_t gopLength { 30U };
    std::uint8_t qualityLevel { 1U };
    std::uint8_t rateControl { 0U }; // 0: CBR, 1: VBR
};

/// @struct MsgStreamingControl
/// @brief Start / pause / stop network video stream pipelines (Message ID 0x90).
struct MsgStreamingControl {
    std::uint8_t streamIndex { 0U };
    std::uint8_t action { 1U }; // 0: Stop, 1: Start, 2: Pause
};

} // namespace Sightline
