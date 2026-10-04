#pragma once

/// @file VideoPesPacketizer.h
/// @brief ISO/IEC 13818-1 and STANAG 4609 Video PES packetizer for H.264 and H.265.

#include "VideoTypes.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Klv {

/// @class VideoPesPacketizer
/// @brief Encapsulates video Access Units into standard MPEG-2 PES packets (Stream ID 0xE0).
class VideoPesPacketizer {
public:
    /// @brief Video Stream ID per ISO/IEC 13818-1 (0xE0 = Video Stream 0).
    static constexpr std::uint8_t kVideoStreamId { 0xE0U };

    /// @brief Constructs a video PES packetizer.
    /// @param[in] prependAud If true, automatically injects an AUD NAL unit if missing.
    explicit VideoPesPacketizer(bool prependAud = true) noexcept;

    /// @brief Converts a VideoAccessUnit into a standard MPEG-2 PES packet.
    /// @param[in] au Populated VideoAccessUnit.
    /// @return Byte vector containing complete PES packet.
    [[nodiscard]] std::vector<std::uint8_t> packetize(const VideoAccessUnit& au) const;

    /// @brief Encapsulates a raw Annex B video frame into a PES packet.
    /// @param[in] data Pointer to raw Annex B byte buffer.
    /// @param[in] size Size of frame in bytes.
    /// @param[in] ptsUs Presentation timestamp in microseconds.
    /// @param[in] dtsUs Optional decode timestamp in microseconds.
    /// @param[in] isKeyframe True if frame contains IDR / keyframe.
    /// @return Byte vector containing complete PES packet.
    [[nodiscard]] std::vector<std::uint8_t> buildPesPacket(const std::uint8_t* data,
                                                           std::size_t size,
                                                           std::uint64_t ptsUs,
                                                           std::optional<std::uint64_t> dtsUs = std::nullopt,
                                                           bool isKeyframe = false) const;

    /// @brief Converts microsecond timestamp to 33-bit 90 kHz PES timestamp.
    /// @param[in] timestampUs Microseconds since epoch.
    /// @return 33-bit integer scaled to 90,000 ticks per second.
    [[nodiscard]] static constexpr std::uint64_t toPts90kHz(std::uint64_t timestampUs) noexcept {
        return ((timestampUs * 90ULL) / 1000ULL) & 0x1FFFFFFFFULL;
    }

private:
    bool m_prependAud { true };

    [[nodiscard]] static std::vector<std::uint8_t> generateAud(VideoCodec codec,
                                                               VideoSliceType sliceType);

    [[nodiscard]] static bool hasLeadingAud(const std::uint8_t* data,
                                            std::size_t size,
                                            VideoCodec codec) noexcept;
};

} // namespace Klv
