#pragma once

/// @file MpegTsKlvMuxer.h
/// @brief STANAG 4609 / MISB ST 1402 compliant MPEG-2 Transport Stream KLV multiplexer.

#include "KlvTypes.h"
#include "MpegTsMuxerTypes.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace Klv {

/// @class MpegTsKlvMuxer
/// @brief Multiplexes KLV telemetry packets into 188-byte MPEG-2 Transport Stream packets.
class MpegTsKlvMuxer {
public:
    /// @brief TS packet fixed size in bytes.
    static constexpr std::size_t kTsPacketSize { 188U };

    /// @brief TS packet sync byte (0x47).
    static constexpr std::uint8_t kTsSyncByte { 0x47U };

    /// @brief Callback signature for emitted 188-byte TS packets.
    using TsPacketCallback = std::function<void(const std::uint8_t* packet, std::size_t size)>;

    /// @brief Constructs muxer with optional configuration.
    /// @param[in] config Initial configuration parameters.
    explicit MpegTsKlvMuxer(const MpegTsMuxerConfig& config = {});

    /// @brief Configures output streaming callback.
    /// @param[in] callback Function invoked whenever one or more TS packets are ready.
    void setPacketCallback(TsPacketCallback callback);

    /// @brief Updates muxer configuration.
    /// @param[in] config New configuration parameters.
    void setConfig(const MpegTsMuxerConfig& config);

    /// @brief Gets active configuration.
    /// @return Const reference to active configuration.
    [[nodiscard]] const MpegTsMuxerConfig& config() const noexcept;

    /// @brief Multiplexes a raw pre-encoded KLV packet into TS packets.
    /// @param[in] klvData Pointer to raw KLV bytes (starting with 16-byte UL).
    /// @param[in] size Size of KLV packet in bytes.
    /// @param[in] timestampUs Optional microsecond timestamp for PTS/PCR calculation.
    /// @return Number of 188-byte TS packets emitted.
    [[nodiscard]] std::size_t muxKlvPacket(const std::uint8_t* klvData,
                                           std::size_t size,
                                           std::optional<std::uint64_t> timestampUs = std::nullopt);

    /// @brief Encodes and multiplexes a UasDatalinkMessage into TS packets.
    /// @param[in] message High-level telemetry message.
    /// @return Number of 188-byte TS packets emitted.
    [[nodiscard]] std::size_t muxMessage(const UasDatalinkMessage& message);

    /// @brief Multiplexes a KLV packet directly into an allocated byte buffer.
    /// @param[in] klvData Pointer to raw KLV bytes.
    /// @param[in] size Size of KLV packet in bytes.
    /// @param[in] timestampUs Optional microsecond timestamp.
    /// @return Vector of contiguous 188-byte TS packets.
    [[nodiscard]] std::vector<std::uint8_t> muxToBuffer(const std::uint8_t* klvData,
                                                        std::size_t size,
                                                        std::optional<std::uint64_t> timestampUs = std::nullopt);

    /// @brief Multiplexes a UasDatalinkMessage directly into an allocated byte buffer.
    /// @param[in] message High-level telemetry message.
    /// @return Vector of contiguous 188-byte TS packets.
    [[nodiscard]] std::vector<std::uint8_t> muxMessageToBuffer(const UasDatalinkMessage& message);

    /// @brief Forces immediate generation and emission of PAT and PMT packets.
    /// @return Number of TS packets emitted (normally 2: PAT + PMT).
    [[nodiscard]] std::size_t emitPsiTables();

    /// @brief Resets continuity counters and internal stream states.
    void reset() noexcept;

private:
    MpegTsMuxerConfig m_config;
    TsPacketCallback m_callback;
    std::map<std::uint16_t, std::uint8_t> m_continuityCounters;
    std::size_t m_packetCounter { 0U };
    std::uint64_t m_lastPcrTimestampUs { 0U };

    [[nodiscard]] std::uint8_t nextCc(std::uint16_t pid) noexcept;
    [[nodiscard]] std::vector<std::uint8_t> buildPatPacket();
    [[nodiscard]] std::vector<std::uint8_t> buildPmtPacket();
    [[nodiscard]] std::vector<std::uint8_t> buildPesPacket(const std::uint8_t* klvData,
                                                           std::size_t size,
                                                           std::optional<std::uint64_t> timestampUs);

    std::size_t emitTsPacket(std::uint16_t pid,
                             bool pusi,
                             const std::uint8_t* payload,
                             std::size_t payloadSize,
                             std::optional<std::uint64_t> pcrUs);

    void dispatchPacket(const std::vector<std::uint8_t>& packet);
};

} // namespace Klv
