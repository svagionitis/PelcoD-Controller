#pragma once

/// @file MpegTsKlvExtractor.h
/// @brief MPEG-2 Transport Stream (188-byte TS packet) demuxer and KLV PES packet extractor.

#include <cstddef>
#include <cstdint>

#include <functional>
#include <optional>
#include <vector>

#include "KlvStreamScanner.h"
#include "KlvTypes.h"

namespace Klv {

/// @class MpegTsKlvExtractor
/// @brief Ingests 188-byte MPEG-2 Transport Stream packets, demultiplexes metadata PES streams, and forwards KLV
/// packets.
class MpegTsKlvExtractor {
public:
    /// @brief TS packet fixed size in bytes.
    static constexpr std::size_t kTsPacketSize { 188U };

    /// @brief TS packet sync byte (0x47).
    static constexpr std::uint8_t kTsSyncByte { 0x47U };

    /// @brief Callback for extracted KLV payloads.
    using KlvPayloadCallback = std::function<void(const std::uint8_t* data, std::size_t size)>;

    /// @brief Callback for extracted KLV payloads with elementary stream PID.
    using MultiPidPayloadCallback = std::function<void(std::uint16_t pid, const std::uint8_t* data, std::size_t size)>;

    /// @brief Callback for decoded telemetry messages with elementary stream PID.
    using MultiPidMessageCallback = std::function<void(std::uint16_t pid, const UasDatalinkMessage& message)>;

    /// @brief Default constructor.
    MpegTsKlvExtractor();

    /// @brief Explicitly configures the metadata PID to extract (bypassing PAT/PMT auto-discovery).
    /// @param[in] pid 13-bit Elementary Stream PID.
    void setMetadataPid(std::uint16_t pid) noexcept;

    /// @brief Adds a metadata PID to explicitly extract.
    /// @param[in] pid 13-bit Elementary Stream PID.
    void addMetadataPid(std::uint16_t pid);

    /// @brief Sets callback for extracted KLV payload buffers.
    /// @param[in] callback Function called with raw KLV data.
    void setKlvPayloadCallback(KlvPayloadCallback callback);

    /// @brief Sets callback for extracted KLV payloads tagged with elementary PID.
    /// @param[in] callback Function called with elementary PID and raw KLV data.
    void setMultiPidPayloadCallback(MultiPidPayloadCallback callback);

    /// @brief Sets callback for decoded UasDatalinkMessage structs.
    /// @param[in] callback Function called with decoded telemetry.
    void setMessageCallback(KlvStreamScanner::MessageCallback callback);

    /// @brief Sets callback for decoded UasDatalinkMessage structs tagged with elementary PID.
    /// @param[in] callback Function called with elementary PID and decoded telemetry.
    void setMultiPidMessageCallback(MultiPidMessageCallback callback);

    /// @brief Ingests arbitrary chunks of an MPEG-TS byte stream (syncs to 0x47 boundaries).
    /// @param[in] data Pointer to the input stream buffer.
    /// @param[in] size Number of bytes available.
    /// @return Number of KLV packets dispatched.
    [[nodiscard]] std::size_t processStream(const std::uint8_t* data, std::size_t size);

    /// @brief Ingests a single 188-byte aligned MPEG-TS packet.
    /// @param[in] packet Pointer to 188 bytes starting with 0x47.
    /// @return Number of KLV packets dispatched (0 or 1).
    [[nodiscard]] std::size_t processTsPacket(const std::uint8_t* packet);

    /// @brief Resets all demuxer state, PID tables, and reassembly buffers.
    void reset();

    /// @brief Flushes any remaining buffered PES packet data.
    /// @details Ingests any complete or unconsumed PES data currently in the reassembly buffer.
    /// @return Number of KLV packets dispatched during flush.
    [[nodiscard]] std::size_t flush();

    /// @brief Returns the discovered or manually set primary metadata PID, if any.
    [[nodiscard]] std::optional<std::uint16_t> metadataPid() const noexcept;

    /// @brief Returns all active/discovered metadata PIDs.
    [[nodiscard]] std::vector<std::uint16_t> metadataPids() const;

    /// @brief Maximum allowed PES reassembly buffer size in bytes.
    static constexpr std::size_t kMaxPesBufferSize { 2097152U }; // 2 MB

private:
    struct SubstreamReassembly {
        std::uint16_t pid { 0U };
        int lastContinuityCounter { -1 };
        std::vector<std::uint8_t> buffer { };
    };

    std::optional<std::uint16_t> m_metadataPid { };
    std::vector<std::uint16_t> m_metadataPids { };
    std::optional<std::uint16_t> m_pmtPid { };
    std::vector<std::uint8_t> m_streamBuffer { };
    std::vector<SubstreamReassembly> m_substreams { };

    KlvPayloadCallback m_payloadCallback { };
    MultiPidPayloadCallback m_multiPayloadCallback { };
    KlvStreamScanner::MessageCallback m_messageCallback { };
    MultiPidMessageCallback m_multiMessageCallback { };
    KlvStreamScanner m_scanner { };

    [[nodiscard]] bool isMetadataPid(std::uint16_t pid) const noexcept;
    [[nodiscard]] SubstreamReassembly& getOrCreateSubstream(std::uint16_t pid);

    void parsePat(const std::uint8_t* payload, std::size_t size);
    void parsePmt(const std::uint8_t* payload, std::size_t size);
    [[nodiscard]] std::size_t handlePesPacket(std::uint16_t pid, const std::uint8_t* pesData, std::size_t pesSize);
};

} // namespace Klv
