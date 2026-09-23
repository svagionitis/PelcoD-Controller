#pragma once

/// @file MpegTsKlvExtractor.h
/// @brief MPEG-2 Transport Stream (188-byte TS packet) demuxer and KLV PES packet extractor.

#include "KlvStreamScanner.h"
#include "KlvTypes.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace Klv {

/// @class MpegTsKlvExtractor
/// @brief Ingests 188-byte MPEG-2 Transport Stream packets, demultiplexes metadata PES streams, and forwards KLV packets.
class MpegTsKlvExtractor {
public:
    /// @brief TS packet fixed size in bytes.
    static constexpr std::size_t kTsPacketSize { 188U };

    /// @brief TS packet sync byte (0x47).
    static constexpr std::uint8_t kTsSyncByte { 0x47U };

    /// @brief Callback for extracted KLV payloads.
    using KlvPayloadCallback = std::function<void(const std::uint8_t* data, std::size_t size)>;

    /// @brief Default constructor.
    MpegTsKlvExtractor();

    /// @brief Explicitly configures the metadata PID to extract (bypassing PAT/PMT auto-discovery).
    /// @param[in] pid 13-bit Elementary Stream PID.
    void setMetadataPid(std::uint16_t pid) noexcept;

    /// @brief Sets callback for extracted KLV payload buffers.
    /// @param[in] callback Function called with raw KLV data.
    void setKlvPayloadCallback(KlvPayloadCallback callback);

    /// @brief Sets callback for decoded UasDatalinkMessage structs.
    /// @param[in] callback Function called with decoded telemetry.
    void setMessageCallback(KlvStreamScanner::MessageCallback callback);

    /// @brief Ingests arbitrary chunks of an MPEG-TS byte stream (syncs to 0x47 boundaries).
    /// @param[in] data Pointer to the input stream buffer.
    /// @param[in] size Number of bytes available.
    /// @return Number of KLV packets dispatched.
    std::size_t processStream(const std::uint8_t* data, std::size_t size);

    /// @brief Ingests a single 188-byte aligned MPEG-TS packet.
    /// @param[in] packet Pointer to 188 bytes starting with 0x47.
    /// @return Number of KLV packets dispatched (0 or 1).
    std::size_t processTsPacket(const std::uint8_t* packet);

    /// @brief Resets all demuxer state, PID tables, and reassembly buffers.
    void reset();

    /// @brief Returns the discovered or manually set metadata PID, if any.
    [[nodiscard]] std::optional<std::uint16_t> metadataPid() const noexcept;

private:
    std::optional<std::uint16_t> m_metadataPid;
    std::optional<std::uint16_t> m_pmtPid;
    std::vector<std::uint8_t> m_streamBuffer;
    std::vector<std::uint8_t> m_pesReassemblyBuffer;
    int m_lastContinuityCounter { -1 };

    KlvPayloadCallback m_payloadCallback;
    KlvStreamScanner m_scanner;

    void parsePat(const std::uint8_t* payload, std::size_t size);
    void parsePmt(const std::uint8_t* payload, std::size_t size);
    void handlePesPacket(const std::uint8_t* pesData, std::size_t pesSize);
};

} // namespace Klv
