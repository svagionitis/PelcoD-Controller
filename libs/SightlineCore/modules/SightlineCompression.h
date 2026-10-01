#pragma once

/// @file SightlineCompression.h
/// @brief Sightline SLA Compression Module (H.264/H.265 encoding and streaming control).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__compress.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @struct MsgSetH264Parameters
/// @brief Encoder bitrate, GOP structure, profile, and QP settings (Message ID 0x23) and telemetry (0x56).
/// @details Conforms to official Sightline SLASetH264Parameters_t / SLACurrentH264Parameters_t.
struct MsgSetH264Parameters {
    std::uint32_t targetBitrateBps { 3000000U }; ///< Target bit rate in bps (default 3 Mbps)
    std::uint8_t intraFrameInterval { 30U }; ///< 1-255: I-frame interval, 0: Intra Refresh
    std::uint8_t lfDisableIdc { 0U }; ///< In-loop filter control (0: filter all, 1: disable all, 2: disable slice)
    std::uint8_t airMbPeriod { 0U }; ///< Cyclic Intra Refresh mega-block period (default 20 when 0)
    std::uint8_t sliceRefreshRowNumber { 0U }; ///< Slice refresh row number
    std::uint8_t flags { 0x12U }; ///< Profile (0: Base, 1: Main, 2: High), Bitrate Mode (0: Legacy, 1: VBR)
    std::uint16_t displayId { 0x0002U }; ///< Network Display ID (0x0002 = Net0, 0x0080 = Net1)
    std::uint8_t minQp { 0U }; ///< Minimum Quantization Parameter (1-30, 0: auto)
    std::uint8_t maxQp { 0U }; ///< Maximum Quantization Parameter (minQp..51, 0: auto)
};

/// @struct MsgStreamingControl
/// @brief Start / pause / stop network video stream pipelines (Message ID 0x90).
struct MsgStreamingControl {
    std::uint8_t streamIndex { 0U };
    std::uint8_t action { 1U }; // 0: Stop, 1: Start, 2: Pause
};

/// @struct MsgDecoderParameters
/// @brief Configures hardware network video stream decoder (Message ID 0x99).
/// @details Conforms to official Sightline SLADecoderParameters_t struct layout.
struct MsgDecoderParameters {
    std::uint8_t decoderIndex { 0U }; ///< Hardware decoder channel index (0..3)
    std::uint8_t enable { 0U }; ///< 0: Disable, 1: Enable
    std::uint8_t codec { 0U }; ///< 0: H.264, 1: H.265, 2: MJPEG
    std::uint16_t networkPort { 15004U }; ///< Listening UDP transport port
    std::uint16_t bufferDepthMs { 100U }; ///< De-jitter buffer latency target in ms
    std::uint32_t multicastIp { 0U }; ///< Optional IPv4 multicast subscription address
};

} // namespace Sightline
