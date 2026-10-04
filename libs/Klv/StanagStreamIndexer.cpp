#include "StanagStreamIndexer.h"
#include "KlvCrc.h"
#include "KlvParser.h"
#include "VideoNaluParser.h"

#include <cstring>
#include <fstream>

namespace Klv {

namespace {

constexpr std::size_t kTsPacketSize { 188U };
constexpr std::uint8_t kTsSyncByte { 0x47U };
constexpr std::uint32_t kSidxMagic { 0x58444953U }; // "SIDX" in little-endian
constexpr std::uint32_t kSidxVersion { 1U };

// 16-byte MISB ST 0601 Universal Label prefix: 06 0E 2B 34 02 0B 01 01 0E 01 03 01 01 00 00 00
constexpr std::uint8_t kMisb0601UlPrefix[] = {
    0x06U, 0x0EU, 0x2BU, 0x34U, 0x02U, 0x0BU, 0x01U, 0x01U
};

} // namespace

StanagStreamIndexer::StanagStreamIndexer() = default;

void StanagStreamIndexer::reset() noexcept
{
    m_timeIndex.clear();
    m_videoPid.reset();
    m_metadataPid.reset();
    m_pmtPid.reset();
    m_videoFrameCount = 0U;
    m_klvPacketCount = 0U;
    m_lastVideoPts = 0U;
    m_lastKlvPts = 0U;
}

std::optional<std::uint16_t> StanagStreamIndexer::videoPid() const noexcept
{
    return m_videoPid;
}

std::optional<std::uint16_t> StanagStreamIndexer::metadataPid() const noexcept
{
    return m_metadataPid;
}

void StanagStreamIndexer::setVideoPid(std::uint16_t pid) noexcept
{
    m_videoPid = pid;
}

void StanagStreamIndexer::setMetadataPid(std::uint16_t pid) noexcept
{
    m_metadataPid = pid;
}

const TelemetryTimeIndex& StanagStreamIndexer::timeIndex() const noexcept
{
    return m_timeIndex;
}

TelemetryTimeIndex& StanagStreamIndexer::timeIndex() noexcept
{
    return m_timeIndex;
}

bool StanagStreamIndexer::indexFile(std::string_view filePath,
                                    ProgressCallback progress)
{
    reset();

    std::ifstream stream(std::string(filePath), std::ios::binary | std::ios::ate);
    if (!stream.is_open()) {
        return false;
    }

    const auto totalBytes = static_cast<std::uint64_t>(stream.tellg());
    if (totalBytes < kTsPacketSize) {
        return false;
    }

    stream.seekg(0, std::ios::beg);

    constexpr std::size_t kChunkSize = 348 * kTsPacketSize; // 65,424 bytes (~64 KB)
    std::vector<std::uint8_t> buffer(kChunkSize);
    std::uint64_t processedBytes { 0U };
    int lastPercent { -1 };

    while (stream.good()) {
        stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        const auto bytesRead = static_cast<std::size_t>(stream.gcount());
        if (bytesRead == 0U) {
            break;
        }

        static_cast<void>(indexChunk(buffer.data(), bytesRead, processedBytes));
        processedBytes += bytesRead;

        if (progress && totalBytes > 0U) {
            const int percent = static_cast<int>((processedBytes * 100U) / totalBytes);
            if (percent != lastPercent) {
                lastPercent = percent;
                progress(percent);
            }
        }
    }

    m_timeIndex.finalize();
    return true;
}

std::size_t StanagStreamIndexer::indexChunk(const std::uint8_t* data,
                                            std::size_t size,
                                            std::uint64_t baseOffset)
{
    if (data == nullptr || size < kTsPacketSize) {
        return 0U;
    }

    std::size_t offset = 0U;
    std::size_t packetCount = 0U;

    while (offset + kTsPacketSize <= size) {
        if (data[offset] != kTsSyncByte) {
            ++offset;
            continue;
        }

        const std::uint8_t* packet = data + offset;
        const std::uint64_t filePacketOffset = baseOffset + offset;

        // Transport Error Indicator
        if ((packet[1] & 0x80U) != 0U) {
            offset += kTsPacketSize;
            continue;
        }

        const bool pusi = (packet[1] & 0x40U) != 0U;
        const std::uint16_t pid = static_cast<std::uint16_t>(
            ((static_cast<std::uint16_t>(packet[1] & 0x1FU)) << 8U) | static_cast<std::uint16_t>(packet[2]));
        const std::uint8_t afc = (packet[3] >> 4U) & 0x03U;

        if (afc == 0U || afc == 2U) {
            offset += kTsPacketSize;
            continue;
        }

        std::size_t payloadOffset = 4U;
        if (afc == 3U) {
            const std::size_t afLen = static_cast<std::size_t>(packet[4]);
            payloadOffset += 1U + afLen;
            if (payloadOffset > kTsPacketSize) {
                offset += kTsPacketSize;
                continue;
            }
        }

        const std::uint8_t* payload = packet + payloadOffset;
        const std::size_t payloadSize = kTsPacketSize - payloadOffset;

        if (pid == 0x0000U) {
            parsePat(payload, payloadSize);
        } else if (m_pmtPid && pid == *m_pmtPid) {
            parsePmt(payload, payloadSize);
        } else if (m_videoPid && pid == *m_videoPid && pusi) {
            processVideoPes(payload, payloadSize, filePacketOffset);
        } else if (m_metadataPid && pid == *m_metadataPid && pusi) {
            processKlvPes(payload, payloadSize, filePacketOffset);
        } else if (!m_metadataPid && pusi && payloadSize >= 16U) {
            // Check for unannounced KLV stream
            if (std::memcmp(payload, kMisb0601UlPrefix, sizeof(kMisb0601UlPrefix)) == 0) {
                m_metadataPid = pid;
                processKlvPes(payload, payloadSize, filePacketOffset);
            }
        }

        offset += kTsPacketSize;
        ++packetCount;
    }

    return packetCount;
}

void StanagStreamIndexer::parsePat(const std::uint8_t* payload, std::size_t size)
{
    if (size < 8U) {
        return;
    }
    const std::size_t ptrField = static_cast<std::size_t>(payload[0]);
    const std::size_t sectionOffset = 1U + ptrField;
    if (sectionOffset + 8U > size || payload[sectionOffset] != 0x00U) {
        return;
    }

    const std::size_t sectionLen = static_cast<std::size_t>(
        ((static_cast<std::uint16_t>(payload[sectionOffset + 1] & 0x0FU)) << 8U) |
        static_cast<std::uint16_t>(payload[sectionOffset + 2]));

    const std::size_t sectionEnd = std::min(size, sectionOffset + 3U + sectionLen);
    if (sectionEnd < 4U) {
        return;
    }
    const std::size_t dataEnd = sectionEnd - 4U; // Exclude CRC32

    std::size_t pos = sectionOffset + 8U;
    while (pos + 4U <= dataEnd) {
        const std::uint16_t progNum = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(payload[pos]) << 8U) | static_cast<std::uint16_t>(payload[pos + 1]));
        const std::uint16_t progPid = static_cast<std::uint16_t>(
            ((static_cast<std::uint16_t>(payload[pos + 2] & 0x1FU)) << 8U) | static_cast<std::uint16_t>(payload[pos + 3]));

        if (progNum != 0U) {
            m_pmtPid = progPid;
            break;
        }
        pos += 4U;
    }
}

void StanagStreamIndexer::parsePmt(const std::uint8_t* payload, std::size_t size)
{
    if (size < 12U) {
        return;
    }
    const std::size_t ptrField = static_cast<std::size_t>(payload[0]);
    const std::size_t sectionOffset = 1U + ptrField;
    if (sectionOffset + 12U > size || payload[sectionOffset] != 0x02U) {
        return;
    }

    const std::size_t sectionLen = static_cast<std::size_t>(
        ((static_cast<std::uint16_t>(payload[sectionOffset + 1] & 0x0FU)) << 8U) |
        static_cast<std::uint16_t>(payload[sectionOffset + 2]));

    const std::size_t sectionEnd = std::min(size, sectionOffset + 3U + sectionLen);
    if (sectionEnd < 4U) {
        return;
    }
    const std::size_t dataEnd = sectionEnd - 4U; // Exclude CRC32

    const std::size_t progInfoLen = static_cast<std::size_t>(
        ((static_cast<std::uint16_t>(payload[sectionOffset + 10] & 0x0FU)) << 8U) |
        static_cast<std::uint16_t>(payload[sectionOffset + 11]));

    std::size_t pos = sectionOffset + 12U + progInfoLen;
    while (pos + 5U <= dataEnd) {
        const std::uint8_t streamType = payload[pos];
        const std::uint16_t elemPid = static_cast<std::uint16_t>(
            ((static_cast<std::uint16_t>(payload[pos + 1] & 0x1FU)) << 8U) | static_cast<std::uint16_t>(payload[pos + 2]));
        const std::size_t esInfoLen = static_cast<std::size_t>(
            ((static_cast<std::uint16_t>(payload[pos + 3] & 0x0FU)) << 8U) | static_cast<std::uint16_t>(payload[pos + 4]));

        // Video: 0x1B (H.264), 0x24 (H.265), 0x02 (MPEG-2 Video)
        if (streamType == 0x1BU || streamType == 0x24U || streamType == 0x02U) {
            if (!m_videoPid) {
                m_videoPid = elemPid;
            }
        }
        // Metadata: 0x06 (PES private data) or 0x15 (Metadata in PES)
        else if (streamType == 0x06U || streamType == 0x15U) {
            if (!m_metadataPid) {
                m_metadataPid = elemPid;
            }
        }

        pos += 5U + esInfoLen;
    }
}

void StanagStreamIndexer::processVideoPes(const std::uint8_t* pesData,
                                         std::size_t size,
                                         std::uint64_t fileOffset)
{
    // Need at least PES start code (3), stream_id (1), length (2), flags (3) = 9 bytes
    if (size < 9U || pesData[0] != 0x00U || pesData[1] != 0x00U || pesData[2] != 0x01U) {
        return;
    }

    const std::uint8_t ptsDtsFlags = (pesData[7] >> 6U) & 0x03U;
    std::uint64_t ptsTicks { m_lastVideoPts };
    std::uint64_t dtsTicks { m_lastVideoPts };

    std::size_t pesHeaderDataLen = static_cast<std::size_t>(pesData[8]);
    if (size < 9U + pesHeaderDataLen) {
        return;
    }

    if ((ptsDtsFlags & 0x02U) != 0U && size >= 14U) {
        ptsTicks = (static_cast<std::uint64_t>(pesData[9] & 0x0EU) << 29U)
                 | (static_cast<std::uint64_t>(pesData[10]) << 22U)
                 | (static_cast<std::uint64_t>(pesData[11] & 0xFEU) << 14U)
                 | (static_cast<std::uint64_t>(pesData[12]) << 7U)
                 | (static_cast<std::uint64_t>(pesData[13] & 0xFEU) >> 1U);
        m_lastVideoPts = ptsTicks;
        dtsTicks = ptsTicks;
    }

    if (ptsDtsFlags == 0x03U && size >= 19U) {
        dtsTicks = (static_cast<std::uint64_t>(pesData[14] & 0x0EU) << 29U)
                 | (static_cast<std::uint64_t>(pesData[15]) << 22U)
                 | (static_cast<std::uint64_t>(pesData[16] & 0xFEU) << 14U)
                 | (static_cast<std::uint64_t>(pesData[17]) << 7U)
                 | (static_cast<std::uint64_t>(pesData[18] & 0xFEU) >> 1U);
    }

    // Inspect video payload for IDR / keyframe slices
    const std::size_t payloadOffset = 9U + pesHeaderDataLen;
    bool isKeyframe = false;

    if (size > payloadOffset) {
        const std::uint8_t* esData = pesData + payloadOffset;
        const std::size_t esSize = size - payloadOffset;

        std::size_t scanPos = 0U;
        std::size_t prefixLen = 0U;
        while (scanPos < esSize) {
            const std::size_t scOffset = VideoNaluParser::findStartCode(esData, esSize, scanPos, prefixLen);
            if (scOffset >= esSize) {
                break;
            }

            const std::size_t nalHeaderPos = scOffset + prefixLen;
            if (nalHeaderPos < esSize) {
                const std::uint8_t nalHeader = esData[nalHeaderPos];
                // H.264: nal_unit_type = nalHeader & 0x1F. IDR = 5, SPS = 7
                const std::uint8_t nalTypeAvc = nalHeader & 0x1FU;
                // H.265: nal_unit_type = (nalHeader >> 1) & 0x3F. IDR = 19..20, VPS = 32, SPS = 33
                const std::uint8_t nalTypeHevc = (nalHeader >> 1U) & 0x3FU;

                if (nalTypeAvc == 5U || nalTypeAvc == 7U || nalTypeHevc == 19U || nalTypeHevc == 20U || nalTypeHevc == 33U) {
                    isKeyframe = true;
                    break;
                }
            }
            scanPos = nalHeaderPos + 1U;
        }
    }

    VideoIndexEntry entry {};
    entry.ptsTicks = ptsTicks;
    entry.dtsTicks = dtsTicks;
    entry.fileByteOffset = fileOffset;
    entry.packetSizeBytes = static_cast<std::uint32_t>(size);
    entry.frameIndex = m_videoFrameCount++;
    entry.isKeyframe = isKeyframe;

    m_timeIndex.addVideoEntry(entry);
}

void StanagStreamIndexer::processKlvPes(const std::uint8_t* pesData,
                                       std::size_t size,
                                       std::uint64_t fileOffset)
{
    if (size < 9U || pesData[0] != 0x00U || pesData[1] != 0x00U || pesData[2] != 0x01U) {
        return;
    }

    const std::uint8_t ptsDtsFlags = (pesData[7] >> 6U) & 0x03U;
    std::uint64_t ptsTicks { m_lastKlvPts };

    std::size_t pesHeaderDataLen = static_cast<std::size_t>(pesData[8]);
    if (size < 9U + pesHeaderDataLen) {
        return;
    }

    if ((ptsDtsFlags & 0x02U) != 0U && size >= 14U) {
        ptsTicks = (static_cast<std::uint64_t>(pesData[9] & 0x0EU) << 29U)
                 | (static_cast<std::uint64_t>(pesData[10]) << 22U)
                 | (static_cast<std::uint64_t>(pesData[11] & 0xFEU) << 14U)
                 | (static_cast<std::uint64_t>(pesData[12]) << 7U)
                 | (static_cast<std::uint64_t>(pesData[13] & 0xFEU) >> 1U);
        m_lastKlvPts = ptsTicks;
    }

    const std::size_t payloadOffset = 9U + pesHeaderDataLen;
    std::uint64_t utcUs = 0U;
    std::optional<UasDatalinkMessage> parsedMsg {};

    if (size > payloadOffset) {
        const std::uint8_t* klvData = pesData + payloadOffset;
        const std::size_t klvSize = size - payloadOffset;

        const auto optUtc = peekUtcTimestamp(klvData, klvSize);
        if (optUtc) {
            utcUs = *optUtc;
        }

        for (std::size_t i = 0U; i + 16U <= klvSize; ++i) {
            if (KlvParser::isMisb0601(klvData + i, klvSize - i)) {
                UasDatalinkMessage msg {};
                if (KlvParser::parse(klvData + i, klvSize - i, msg, false) == KlvStatus::Success) {
                    parsedMsg = msg;
                    if (msg.precisionTimeStampUs && utcUs == 0U) {
                        utcUs = *msg.precisionTimeStampUs;
                    }
                }
                break;
            }
        }
    }

    KlvIndexEntry entry {};
    entry.ptsTicks = ptsTicks;
    entry.utcTimestampUs = utcUs;
    entry.fileByteOffset = fileOffset;
    entry.packetSizeBytes = static_cast<std::uint32_t>(size);
    entry.messageIndex = m_klvPacketCount++;
    entry.message = parsedMsg;

    m_timeIndex.addKlvEntry(entry);
}

std::optional<std::uint64_t> StanagStreamIndexer::peekUtcTimestamp(const std::uint8_t* data,
                                                                  std::size_t size) noexcept
{
    if (data == nullptr || size < 20U) {
        return std::nullopt;
    }

    // Look for MISB ST 0601 Universal Label: 16 bytes starting with 0x06 0x0E 0x2B 0x34
    std::size_t ulPos = 0U;
    bool foundUl = false;
    for (std::size_t i = 0U; i + 16U <= size; ++i) {
        if (data[i] == 0x06U && data[i + 1] == 0x0EU && data[i + 2] == 0x2BU && data[i + 3] == 0x34U) {
            ulPos = i;
            foundUl = true;
            break;
        }
    }

    if (!foundUl) {
        return std::nullopt;
    }

    std::size_t pos = ulPos + 16U;
    if (pos >= size) {
        return std::nullopt;
    }

    // Parse BER packet length
    std::size_t packetLen = 0U;
    if ((data[pos] & 0x80U) == 0U) {
        packetLen = static_cast<std::size_t>(data[pos]);
        pos += 1U;
    } else {
        const std::size_t numLenBytes = static_cast<std::size_t>(data[pos] & 0x7FU);
        pos += 1U;
        if (numLenBytes > 4U || pos + numLenBytes > size) {
            return std::nullopt;
        }
        for (std::size_t j = 0U; j < numLenBytes; ++j) {
            packetLen = (packetLen << 8U) | static_cast<std::size_t>(data[pos + j]);
        }
        pos += numLenBytes;
    }

    const std::size_t packetEnd = std::min(size, pos + packetLen);

    // Scan tags until Tag 2 (Precision Time Stamp) is found
    while (pos + 2U <= packetEnd) {
        const std::uint8_t tag = data[pos];
        const std::size_t tagLen = static_cast<std::size_t>(data[pos + 1]);
        pos += 2U;

        if (tag == 0x02U && tagLen == 8U && pos + 8U <= packetEnd) {
            std::uint64_t ts = 0U;
            for (std::size_t b = 0U; b < 8U; ++b) {
                ts = (ts << 8U) | static_cast<std::uint64_t>(data[pos + b]);
            }
            return ts;
        }

        pos += tagLen;
    }

    return std::nullopt;
}

bool StanagStreamIndexer::saveIndex(std::string_view indexPath) const
{
    std::ofstream out(std::string(indexPath), std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    std::vector<std::uint8_t> payload {};

    auto append64 = [&payload](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            payload.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFFU));
        }
    };

    auto append32 = [&payload](std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            payload.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFFU));
        }
    };

    append32(kSidxMagic);
    append32(kSidxVersion);
    append64(m_timeIndex.basePts());
    append64(m_timeIndex.baseUtcUs());

    double dur = m_timeIndex.durationSeconds();
    std::uint64_t durBits = 0U;
    std::memcpy(&durBits, &dur, sizeof(dur));
    append64(durBits);

    append32(static_cast<std::uint32_t>(m_timeIndex.videoFrameCount()));
    // We iterate video frames through window
    // (Or save from m_timeIndex directly)
    // Write a dummy record count for now and append CRC
    const std::uint32_t crc = KlvCrc::calculateCrc32Mpeg(payload.data(), payload.size());
    append32(crc);

    out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    return out.good();
}

bool StanagStreamIndexer::loadIndex(std::string_view indexPath)
{
    std::ifstream in(std::string(indexPath), std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    in.seekg(0, std::ios::end);
    const auto fileSize = in.tellg();
    if (fileSize < 36) {
        return false;
    }

    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(fileSize));
    in.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    const std::size_t payloadSize = buffer.size() - 4U;
    const std::uint32_t storedCrc = static_cast<std::uint32_t>(buffer[payloadSize])
        | (static_cast<std::uint32_t>(buffer[payloadSize + 1]) << 8U)
        | (static_cast<std::uint32_t>(buffer[payloadSize + 2]) << 16U)
        | (static_cast<std::uint32_t>(buffer[payloadSize + 3]) << 24U);

    const std::uint32_t calculatedCrc = KlvCrc::calculateCrc32Mpeg(buffer.data(), payloadSize);
    if (storedCrc != calculatedCrc) {
        return false;
    }

    return true;
}

} // namespace Klv
