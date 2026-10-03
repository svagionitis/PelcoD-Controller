#pragma once

/// @file SightlineCompression.h
/// @brief Sightline SLA Compression Module (H.264/H.265 encoding and streaming control).
/// @see https://knowledge.sightlineintelligence.com/releases/IDD/current/group__compress.html

#include "../SightlineTypes.h"

#include <cstdint>

namespace Sightline {

/// @enum H264Profile
/// @brief H.264 video compression profiles supported across SLA OEM boards.
enum class H264Profile : std::uint8_t {
    Baseline = 0U, ///< Baseline Profile (BP) - Supported on all OEMs (1500 default)
    Main = 1U,     ///< Main Profile (MP) - Supported on 3000/17xx/4000/41xx
    High = 2U      ///< High Profile (HP) - Supported on 3000/17xx/4000/41xx (default for 17xx/4000/41xx)
};

/// @enum BitrateControlMode
/// @brief Encoder bitrate regulation algorithms.
enum class BitrateControlMode : std::uint8_t {
    Legacy = 0U,      ///< Constant bitrate attempt over time (suitable for static scenes)
    Variable = 1U,    ///< Variable bitrate (VBR, recommended for 17xx/4000/41xx)
    Constrained = 2U, ///< Average constant bitrate with parameter flexibility (3000-OEM only)
    Balanced = 3U     ///< Balanced P-frame prioritization for low-bandwidth links (17xx-OEM only)
};

/// @enum DeblockingFilter
/// @brief In-loop deblocking filter modes.
enum class DeblockingFilter : std::uint8_t {
    FilterAll = 0U,   ///< Filter all edges (default for most applications)
    DisableAll = 1U,  ///< Disable all deblocking
    DisableSlice = 2U ///< Disable slice edges
};

/// @brief Combines profile and bitrate control mode into flags byte.
/// @param[in] profile Target H.264 profile.
/// @param[in] mode Rate control mode.
/// @return Flags byte conforming to SLASetH264Parameters_t specification.
[[nodiscard]] constexpr std::uint8_t makeH264Flags(H264Profile profile, BitrateControlMode mode) noexcept
{
    return static_cast<std::uint8_t>(
        (static_cast<std::uint8_t>(mode) << 4U) | (static_cast<std::uint8_t>(profile) & 0x03U));
}

/// @struct MsgSetH264Parameters
/// @brief Encoder bitrate, GOP structure, profile, and QP settings (Message ID 0x23) and telemetry (0x56).
/// @details Conforms to official Sightline SLASetH264Parameters_t / SLACurrentH264Parameters_t.
struct MsgSetH264Parameters {
    std::uint32_t targetBitrateBps { 3000000U }; ///< Target bit rate in bps (default 3 Mbps)
    std::uint8_t intraFrameInterval { 30U }; ///< 1-255: I-frame interval, 0: Intra Refresh
    std::uint8_t lfDisableIdc { 0U }; ///< In-loop filter control (see DeblockingFilter)
    std::uint8_t airMbPeriod { 0U }; ///< Cyclic Intra Refresh mega-block period (default 20 when 0)
    std::uint8_t sliceRefreshRowNumber { 0U }; ///< Slice refresh row number
    std::uint8_t flags { 0x12U }; ///< Profile (bits 0..1), Bitrate Mode (bits 4..5)
    std::uint16_t displayId { 0x0002U }; ///< Network Display ID (0x0002 = Net0, 0x0080 = Net1, 0x0200 = Net2)
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
