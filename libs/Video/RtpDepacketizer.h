#pragma once

/// @file RtpDepacketizer.h
/// @brief Zero-transcode RFC 6184 (H.264) and RFC 7798 (H.265) RTP depacketizer.

#include <cstddef>
#include <cstdint>

#include <functional>
#include <string_view>
#include <vector>

#include "Klv/VideoTypes.h"
#include "RtspPassthroughTypes.h"

namespace Video {

/// @class RtpDepacketizer
/// @brief Parses RFC 6184 and RFC 7798 RTP packets into Annex B VideoAccessUnits.
class RtpDepacketizer {
public:
    /// @brief Callback invoked when a complete video Access Unit is ready.
    using AccessUnitCallback = std::function<void(const Klv::VideoAccessUnit& au)>;

    /// @brief Callback invoked when raw Annex B byte stream chunks are ready.
    using RawChunkCallback = std::function<void(const std::uint8_t* data, std::size_t size, std::uint64_t ptsUs)>;

    /// @brief Constructs depacketizer for designated codec.
    /// @param[in] codec Video elementary stream codec format.
    explicit RtpDepacketizer(Klv::VideoCodec codec = Klv::VideoCodec::H264) noexcept;

    /// @brief Sets callback for assembled VideoAccessUnit delivery.
    /// @param[in] callback Functional callback.
    void setUnitCallback(AccessUnitCallback callback);

    /// @brief Sets callback for raw Annex B byte stream chunk delivery.
    /// @param[in] callback Functional callback.
    void setChunkCallback(RawChunkCallback callback);

    /// @brief Configures active video codec.
    /// @param[in] codec Video elementary stream codec.
    void setCodec(Klv::VideoCodec codec) noexcept;

    /// @brief Gets active video codec.
    /// @return Current VideoCodec.
    [[nodiscard]] Klv::VideoCodec codec() const noexcept;

    /// @brief Injects out-of-band parameter sets (SPS / PPS / VPS) extracted from SDP.
    /// @param[in] paramSets Vector of raw parameter set NAL units without start codes.
    void setParameterSets(const std::vector<std::vector<std::uint8_t>>& paramSets);

    /// @brief Parses comma-separated Base64 SDP sprop parameter strings into raw NALUs.
    /// @param[in] sprop Comma-separated Base64 parameter string.
    /// @return Vector of raw binary parameter set byte vectors.
    [[nodiscard]] static std::vector<std::vector<std::uint8_t>> parseSprop(std::string_view sprop);

    /// @brief Ingests and processes an RFC 3550 RTP packet.
    /// @param[in] packet Pointer to raw RTP packet buffer.
    /// @param[in] size Size of RTP packet in bytes.
    /// @return True if packet was valid and processed without corruption.
    bool processPacket(const std::uint8_t* packet, std::size_t size);

    /// @brief Flushes currently accumulated Access Unit buffer.
    void flush();

    /// @brief Resets depacketizer state and clears internal buffers.
    void reset() noexcept;

    /// @brief Gets total detected packet loss count.
    /// @return Number of dropped packets detected via sequence continuity checks.
    [[nodiscard]] std::size_t packetLossCount() const noexcept;

    /// @brief Gets total processed RTP packets count.
    /// @return Total received valid RTP packets.
    [[nodiscard]] std::size_t packetsReceived() const noexcept;

private:
    Klv::VideoCodec m_codec { Klv::VideoCodec::H264 };
    AccessUnitCallback m_unitCallback { };
    RawChunkCallback m_chunkCallback { };
    std::vector<std::vector<std::uint8_t>> m_paramSets { };

    std::vector<std::uint8_t> m_auBuffer { };
    std::uint32_t m_lastRtpTimestamp { 0U };
    std::uint64_t m_unwrappedTimestamp { 0U };
    std::uint32_t m_baseRtpTimestamp { 0U };
    std::uint64_t m_basePtsUs { 0U };
    std::uint64_t m_currentPtsUs { 0U };
    bool m_hasBaseTimestamp { false };

    std::uint16_t m_lastSequence { 0U };
    bool m_hasLastSequence { false };
    std::size_t m_packetLossCount { 0U };
    std::size_t m_packetsReceived { 0U };

    // Fragmentation Unit (FU) accumulation
    std::vector<std::uint8_t> m_fuBuffer { };
    bool m_fuInProgress { false };

    bool parseH264(const std::uint8_t* payload, std::size_t size, bool marker);
    bool parseH265(const std::uint8_t* payload, std::size_t size, bool marker);
    void appendNalu(const std::uint8_t* nalu, std::size_t size);
    void emitAccessUnit(std::uint64_t ptsUs);
    [[nodiscard]] std::uint64_t unwrapTimestamp(std::uint32_t rtpTs) noexcept;
};

} // namespace Video
