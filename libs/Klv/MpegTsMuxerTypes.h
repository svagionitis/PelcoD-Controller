#pragma once

/// @file MpegTsMuxerTypes.h
/// @brief Configuration and enumerations for STANAG 4609 / MISB ST 1402 MPEG-TS KLV multiplexer.

#include <cstddef>
#include <cstdint>
#include <string>

namespace Klv {

/// @enum KlvStreamType
/// @brief MPEG-2 TS elementary stream type for metadata.
enum class KlvStreamType : std::uint8_t {
    PesPrivateData = 0x06U,  ///< Legacy / synchronous STANAG 4609 stream type
    MetadataInPes  = 0x15U   ///< Standard STANAG 4609 Ed. 3/4 & MISB ST 1402 stream type
};

/// @enum KlvStreamId
/// @brief PES packet stream identifier.
enum class KlvStreamId : std::uint8_t {
    PrivateStream1 = 0xBDU,  ///< Standard private_stream_1 (most common for KLV)
    MetadataStream = 0xFCU   ///< ISO/IEC 13818-1 metadata_stream
};

/// @struct MpegTsMuxerConfig
/// @brief Configuration settings for MpegTsKlvMuxer.
struct MpegTsMuxerConfig {
    /// @brief Program number in PAT/PMT (default 1).
    std::uint16_t programNumber { 1U };

    /// @brief PMT Elementary PID (default 0x0100 / 256).
    std::uint16_t pmtPid { 0x0100U };

    /// @brief Metadata Elementary PID (default 0x01E0 / 480).
    std::uint16_t metadataPid { 0x01E0U };

    /// @brief PCR PID (defaults to metadata PID for standalone metadata stream).
    std::uint16_t pcrPid { 0x01E0U };

    /// @brief Transport Stream ID in PAT (default 1).
    std::uint16_t transportStreamId { 1U };

    /// @brief Elementary stream type (0x06 or 0x15).
    KlvStreamType streamType { KlvStreamType::MetadataInPes };

    /// @brief PES stream ID (0xBD or 0xFC).
    KlvStreamId streamId { KlvStreamId::PrivateStream1 };

    /// @brief Registration descriptor format identifier ("KLVA" or "KLVN").
    std::string formatIdentifier { "KLVA" };

    /// @brief Periodic interval (in metadata packets) to inject PAT and PMT.
    std::size_t patPmtPeriodPackets { 40U };

    /// @brief Target PCR insertion period in milliseconds (< 100ms per ISO/IEC 13818-1).
    std::uint32_t pcrIntervalMs { 40U };

    /// @brief Whether to emit 33-bit Presentation Time Stamp (PTS) in PES headers.
    bool emitPts { true };
};

} // namespace Klv
