#pragma once

/// @file MpegTsMuxerTypes.h
/// @brief Configuration and enumerations for STANAG 4609 / MISB ST 1402 MPEG-TS KLV multiplexer.

#include <cstddef>
#include <cstdint>

#include <optional>
#include <string>
#include <vector>

#include "St1607Types.h"
#include "VideoTypes.h"

namespace Klv {

/// @enum KlvStreamType
/// @brief MPEG-2 TS elementary stream type for metadata.
enum class KlvStreamType : std::uint8_t {
    PesPrivateData = 0x06U, ///< Legacy / synchronous STANAG 4609 stream type
    MetadataInPes = 0x15U ///< Standard STANAG 4609 Ed. 3/4 & MISB ST 1402 stream type
};

/// @enum KlvStreamId
/// @brief PES packet stream identifier.
enum class KlvStreamId : std::uint8_t {
    PrivateStream1 = 0xBDU, ///< Standard private_stream_1 (most common for KLV)
    MetadataStream = 0xFCU ///< ISO/IEC 13818-1 metadata_stream
};

/// @struct MetadataStreamConfig
/// @brief Configuration descriptor for an individual metadata elementary stream.
struct MetadataStreamConfig {
    /// @brief 13-bit Elementary Stream PID (valid range 0x0020 - 0x1FFE).
    std::uint16_t pid { 0x01E0U };

    /// @brief Stream type in PMT (0x15 for Metadata in PES, 0x06 for PES private data).
    KlvStreamType streamType { KlvStreamType::MetadataInPes };

    /// @brief PES packet stream identifier (0xBD for PrivateStream1, 0xFC for MetadataStream).
    KlvStreamId streamId { KlvStreamId::PrivateStream1 };

    /// @brief 4-byte ISO/IEC 13818-1 Registration Descriptor format identifier ("KLVA", "VMTI").
    std::string formatIdentifier { "KLVA" };

    /// @brief Human-readable name or subsystem descriptor (e.g., "Primary EO/IR", "Radar Tracks").
    std::string streamName { "Primary Telemetry" };

    /// @brief Optional MISB ST 1607 Item 143 Metadata Substream Identifier.
    std::optional<MetadataSubstreamId> msid { std::nullopt };

    /// @brief Whether to inject 33-bit PTS timestamps into PES headers for this stream.
    bool emitPts { true };

    /// @brief Validates conformance of stream settings.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return (pid >= 0x0020U && pid <= 0x1FFEU) && (formatIdentifier.size() >= 4U);
    }
};

/// @struct MpegTsMuxerConfig
/// @brief Configuration settings for MpegTsKlvMuxer.
struct MpegTsMuxerConfig {
    /// @brief Program number in PAT/PMT (default 1).
    std::uint16_t programNumber { 1U };

    /// @brief PMT Elementary PID (default 0x0100 / 256).
    std::uint16_t pmtPid { 0x0100U };

    /// @brief Video Elementary PID (default 0x0101 / 257).
    std::uint16_t videoPid { 0x0101U };

    /// @brief Metadata Elementary PID (default 0x01E0 / 480). Used as default when metadataStreams is empty.
    std::uint16_t metadataPid { 0x01E0U };

    /// @brief PCR PID (defaults to metadata PID for standalone, or video PID when video enabled).
    std::uint16_t pcrPid { 0x01E0U };

    /// @brief Transport Stream ID in PAT (default 1).
    std::uint16_t transportStreamId { 1U };

    /// @brief Video codec to advertise in PMT and packetize (None = metadata-only mode).
    VideoCodec videoCodec { VideoCodec::None };

    /// @brief Whether PCR timestamps should be stamped on the Video PID when video is active.
    bool pcrOnVideo { true };

    /// @brief Elementary stream type for metadata (0x06 or 0x15). Default for single-stream mode.
    KlvStreamType streamType { KlvStreamType::MetadataInPes };

    /// @brief PES stream ID for metadata (0xBD or 0xFC). Default for single-stream mode.
    KlvStreamId streamId { KlvStreamId::PrivateStream1 };

    /// @brief Registration descriptor format identifier ("KLVA" or "KLVN"). Default for single-stream mode.
    std::string formatIdentifier { "KLVA" };

    /// @brief Periodic interval (in metadata packets) to inject PAT and PMT.
    std::size_t patPmtPeriodPackets { 40U };

    /// @brief Target PCR insertion period in milliseconds (< 100ms per ISO/IEC 13818-1).
    std::uint32_t pcrIntervalMs { 40U };

    /// @brief Maximum allowed PTS desynchronization in milliseconds (STANAG 4609 <= 50ms).
    std::uint32_t maxSyncDeltaMs { 50U };

    /// @brief Whether to emit 33-bit Presentation Time Stamp (PTS) in PES headers.
    bool emitPts { true };

    /// @brief Dedicated metadata elementary streams for multi-PID multiplexing (MISB ST 1402 / ST 1607).
    std::vector<MetadataStreamConfig> metadataStreams { };

    /// @brief Resolves metadata stream configuration for a given PID.
    [[nodiscard]] const MetadataStreamConfig* findStream(std::uint16_t pid) const noexcept
    {
        for (const auto& stream : metadataStreams) {
            if (stream.pid == pid) {
                return &stream;
            }
        }
        return nullptr;
    }

    /// @brief Resolves metadata stream configuration by MISB ST 1607 MSID local identifier.
    [[nodiscard]] const MetadataStreamConfig* findStreamByMsid(std::uint32_t localId) const noexcept
    {
        for (const auto& stream : metadataStreams) {
            if (stream.msid.has_value() && stream.msid->localId == localId) {
                return &stream;
            }
        }
        return nullptr;
    }

    /// @brief Returns the effective primary metadata PID.
    [[nodiscard]] std::uint16_t primaryMetadataPid() const noexcept
    {
        if (!metadataStreams.empty()) {
            return metadataStreams.front().pid;
        }
        return metadataPid;
    }

    /// @brief Validates overall multiplexer configuration.
    [[nodiscard]] bool validate() const noexcept
    {
        if (pmtPid < 0x0010U || pmtPid >= 0x1FFFU) {
            return false;
        }
        if (videoCodec != VideoCodec::None && (videoPid == pmtPid || videoPid >= 0x1FFFU)) {
            return false;
        }
        if (metadataStreams.empty()) {
            if (metadataPid == pmtPid || (videoCodec != VideoCodec::None && metadataPid == videoPid)) {
                return false;
            }
        } else {
            for (std::size_t i = 0U; i < metadataStreams.size(); ++i) {
                const auto& s1 = metadataStreams[i];
                if (!s1.isValid() || s1.pid == pmtPid || (videoCodec != VideoCodec::None && s1.pid == videoPid)) {
                    return false;
                }
                for (std::size_t j = i + 1U; j < metadataStreams.size(); ++j) {
                    if (s1.pid == metadataStreams[j].pid) {
                        return false; // Duplicate stream PID
                    }
                }
            }
        }
        return true;
    }
};

} // namespace Klv
