#pragma once

/// @file MpegTsH264Demuxer.h
/// @brief Zero-allocation streaming MPEG-2 Transport Stream demuxer for H.264 elementary streams.

#include <cstddef>
#include <cstdint>

#include <functional>
#include <optional>
#include <vector>

namespace Video {

/// @class MpegTsH264Demuxer
/// @brief Demultiplexes 188-byte MPEG-2 Transport Stream packets to extract H.264 Annex-B NAL units.
class MpegTsH264Demuxer {
public:
    /// @brief TS packet fixed size in bytes.
    static constexpr std::size_t kTsPacketSize { 188U };

    /// @brief TS packet sync byte (0x47).
    static constexpr std::uint8_t kTsSyncByte { 0x47U };

    /// @brief H.264 AVC stream type identifier in PMT.
    static constexpr std::uint8_t kStreamTypeH264 { 0x1BU };

    /// @brief Callback invoked when a complete Annex-B NAL unit is ready for decoding.
    using NaluCallback = std::function<void(const std::uint8_t* data, std::size_t size, double ptsSeconds)>;

    /// @brief Constructs demuxer with designated NALU output callback.
    /// @param[in] callback Function invoked per complete Annex-B slice/frame.
    explicit MpegTsH264Demuxer(NaluCallback callback);

    /// @brief Destructor.
    ~MpegTsH264Demuxer() = default;

    /// @brief Ingests arbitrary chunks of TS stream data (re-syncs to 0x47 boundaries).
    /// @param[in] data Pointer to raw byte buffer.
    /// @param[in] size Number of bytes available.
    /// @return Number of NAL units dispatched.
    [[nodiscard]] std::size_t processData(const std::uint8_t* data, std::size_t size);

    /// @brief Ingests a single 188-byte aligned TS packet.
    /// @param[in] packet Pointer to 188-byte buffer starting with 0x47.
    /// @return Number of NAL units dispatched.
    [[nodiscard]] std::size_t processPacket(const std::uint8_t* packet);

    /// @brief Flushes any currently reassembled PES packet buffer.
    void flush();

    /// @brief Resets demuxer internal state, PID caches, and reassembly buffers.
    void reset() noexcept;

    /// @brief Manually specifies the video elementary stream PID (bypassing PAT/PMT discovery).
    /// @param[in] pid 13-bit video PID.
    void setVideoPid(std::uint16_t pid) noexcept;

    /// @brief Queries currently discovered video PID.
    /// @return Video PID if discovered, std::nullopt otherwise.
    [[nodiscard]] std::optional<std::uint16_t> videoPid() const noexcept;

    /// @brief Queries currently discovered PMT PID.
    /// @return PMT PID if discovered, std::nullopt otherwise.
    [[nodiscard]] std::optional<std::uint16_t> pmtPid() const noexcept;

private:
    void parsePat(const std::uint8_t* payload, std::size_t size);
    void parsePmt(const std::uint8_t* payload, std::size_t size);
    std::size_t handlePesData(const std::uint8_t* payload, std::size_t size, bool pusi);
    std::size_t dispatchNalus();

    NaluCallback m_naluCallback;
    std::optional<std::uint16_t> m_pmtPid { std::nullopt };
    std::optional<std::uint16_t> m_videoPid { std::nullopt };

    std::vector<std::uint8_t> m_streamBuffer;
    std::vector<std::uint8_t> m_pesBuffer;
    double m_currentPts { 0.0 };
};

} // namespace Video
