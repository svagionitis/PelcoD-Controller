#include "StanagStreamIndexer.h"
#include "KlvCrc.h"
#include "KlvEncoder.h"
#include "KlvParser.h"
#include "VideoNaluParser.h"

#include <cstdint>
#include <cstring>

#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Klv {

namespace {

    constexpr std::size_t kTsPacketSize { 188U };
    constexpr std::uint8_t kTsSyncByte { 0x47U };
    constexpr std::uint32_t kSidxMagic { 0x58444953U }; // "SIDX" in little-endian
    constexpr std::uint32_t kSidxVersion { 1U };

    // 16-byte MISB ST 0601 Universal Label prefix: 06 0E 2B 34 02 0B 01 01 0E 01 03 01 01 00 00 00
    constexpr std::uint8_t kMisb0601UlPrefix[] = { 0x06U, 0x0EU, 0x2BU, 0x34U, 0x02U, 0x0BU, 0x01U, 0x01U };

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
    m_videoPes.buffer.clear();
    m_videoPes.fileOffset = 0U;
    m_videoPes.active = false;
    m_klvPes.buffer.clear();
    m_klvPes.fileOffset = 0U;
    m_klvPes.active = false;
}

void StanagStreamIndexer::flushVideoPes()
{
    if (m_videoPes.active && !m_videoPes.buffer.empty()) {
        processVideoPes(m_videoPes.buffer.data(), m_videoPes.buffer.size(), m_videoPes.fileOffset);
        m_videoPes.buffer.clear();
        m_videoPes.active = false;
    }
}

void StanagStreamIndexer::flushKlvPes()
{
    if (m_klvPes.active && !m_klvPes.buffer.empty()) {
        processKlvPes(m_klvPes.buffer.data(), m_klvPes.buffer.size(), m_klvPes.fileOffset);
        m_klvPes.buffer.clear();
        m_klvPes.active = false;
    }
}

void StanagStreamIndexer::checkVideoPesComplete()
{
    if (m_videoPes.active && m_videoPes.buffer.size() >= 6U) {
        if (m_videoPes.buffer[0] == 0x00U && m_videoPes.buffer[1] == 0x00U && m_videoPes.buffer[2] == 0x01U) {
            const std::size_t pesLen = (static_cast<std::size_t>(m_videoPes.buffer[4]) << 8U)
                | static_cast<std::size_t>(m_videoPes.buffer[5]);
            if (pesLen > 0U && m_videoPes.buffer.size() >= 6U + pesLen) {
                flushVideoPes();
            }
        }
    }
}

void StanagStreamIndexer::checkKlvPesComplete()
{
    if (m_klvPes.active && m_klvPes.buffer.size() >= 6U) {
        if (m_klvPes.buffer[0] == 0x00U && m_klvPes.buffer[1] == 0x00U && m_klvPes.buffer[2] == 0x01U) {
            const std::size_t pesLen
                = (static_cast<std::size_t>(m_klvPes.buffer[4]) << 8U) | static_cast<std::size_t>(m_klvPes.buffer[5]);
            if (pesLen > 0U && m_klvPes.buffer.size() >= 6U + pesLen) {
                flushKlvPes();
            }
        }
    }
}

void StanagStreamIndexer::flush()
{
    flushVideoPes();
    flushKlvPes();
    m_timeIndex.finalize();
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
    flushVideoPes();
    flushKlvPes();
    return m_timeIndex;
}

bool StanagStreamIndexer::indexFile(std::string_view filePath, ProgressCallback progress)
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

    flush();
    return true;
}

std::size_t StanagStreamIndexer::indexChunk(const std::uint8_t* data, std::size_t size, std::uint64_t baseOffset)
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
        } else if (m_videoPid && pid == *m_videoPid) {
            if (pusi) {
                flushVideoPes();
                m_videoPes.active = true;
                m_videoPes.fileOffset = filePacketOffset;
                m_videoPes.buffer.assign(payload, payload + payloadSize);
                checkVideoPesComplete();
            } else if (m_videoPes.active) {
                if (m_videoPes.buffer.size() + payloadSize <= 10485760U) {
                    m_videoPes.buffer.insert(m_videoPes.buffer.end(), payload, payload + payloadSize);
                }
                checkVideoPesComplete();
            }
        } else if (m_metadataPid && pid == *m_metadataPid) {
            if (pusi) {
                flushKlvPes();
                m_klvPes.active = true;
                m_klvPes.fileOffset = filePacketOffset;
                m_klvPes.buffer.assign(payload, payload + payloadSize);
                checkKlvPesComplete();
            } else if (m_klvPes.active) {
                if (m_klvPes.buffer.size() + payloadSize <= 2097152U) {
                    m_klvPes.buffer.insert(m_klvPes.buffer.end(), payload, payload + payloadSize);
                }
                checkKlvPesComplete();
            }
        } else if (!m_metadataPid && pusi && payloadSize >= 16U) {
            // Check for unannounced KLV stream
            if (std::memcmp(payload, kMisb0601UlPrefix, sizeof(kMisb0601UlPrefix)) == 0) {
                m_metadataPid = pid;
                flushKlvPes();
                m_klvPes.active = true;
                m_klvPes.fileOffset = filePacketOffset;
                m_klvPes.buffer.assign(payload, payload + payloadSize);
                checkKlvPesComplete();
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

    const std::size_t sectionLen
        = static_cast<std::size_t>(((static_cast<std::uint16_t>(payload[sectionOffset + 1] & 0x0FU)) << 8U)
            | static_cast<std::uint16_t>(payload[sectionOffset + 2]));

    const std::size_t sectionEnd = std::min(size, sectionOffset + 3U + sectionLen);
    if (sectionEnd < 4U) {
        return;
    }
    const std::size_t dataEnd = sectionEnd - 4U; // Exclude CRC32

    std::size_t pos = sectionOffset + 8U;
    while (pos + 4U <= dataEnd) {
        const std::uint16_t progNum = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(payload[pos]) << 8U) | static_cast<std::uint16_t>(payload[pos + 1]));
        const std::uint16_t progPid
            = static_cast<std::uint16_t>(((static_cast<std::uint16_t>(payload[pos + 2] & 0x1FU)) << 8U)
                | static_cast<std::uint16_t>(payload[pos + 3]));

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

    const std::size_t sectionLen
        = static_cast<std::size_t>(((static_cast<std::uint16_t>(payload[sectionOffset + 1] & 0x0FU)) << 8U)
            | static_cast<std::uint16_t>(payload[sectionOffset + 2]));

    const std::size_t sectionEnd = std::min(size, sectionOffset + 3U + sectionLen);
    if (sectionEnd < 4U) {
        return;
    }
    const std::size_t dataEnd = sectionEnd - 4U; // Exclude CRC32

    const std::size_t progInfoLen
        = static_cast<std::size_t>(((static_cast<std::uint16_t>(payload[sectionOffset + 10] & 0x0FU)) << 8U)
            | static_cast<std::uint16_t>(payload[sectionOffset + 11]));

    std::size_t pos = sectionOffset + 12U + progInfoLen;
    while (pos + 5U <= dataEnd) {
        const std::uint8_t streamType = payload[pos];
        const std::uint16_t elemPid
            = static_cast<std::uint16_t>(((static_cast<std::uint16_t>(payload[pos + 1] & 0x1FU)) << 8U)
                | static_cast<std::uint16_t>(payload[pos + 2]));
        const std::size_t esInfoLen
            = static_cast<std::size_t>(((static_cast<std::uint16_t>(payload[pos + 3] & 0x0FU)) << 8U)
                | static_cast<std::uint16_t>(payload[pos + 4]));

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

void StanagStreamIndexer::processVideoPes(const std::uint8_t* pesData, std::size_t size, std::uint64_t fileOffset)
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
            | (static_cast<std::uint64_t>(pesData[11] & 0xFEU) << 14U) | (static_cast<std::uint64_t>(pesData[12]) << 7U)
            | (static_cast<std::uint64_t>(pesData[13] & 0xFEU) >> 1U);
        m_lastVideoPts = ptsTicks;
        dtsTicks = ptsTicks;
    }

    if (ptsDtsFlags == 0x03U && size >= 19U) {
        dtsTicks = (static_cast<std::uint64_t>(pesData[14] & 0x0EU) << 29U)
            | (static_cast<std::uint64_t>(pesData[15]) << 22U)
            | (static_cast<std::uint64_t>(pesData[16] & 0xFEU) << 14U) | (static_cast<std::uint64_t>(pesData[17]) << 7U)
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

                if (nalTypeAvc == 5U || nalTypeAvc == 7U || nalTypeHevc == 19U || nalTypeHevc == 20U
                    || nalTypeHevc == 33U) {
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

void StanagStreamIndexer::processKlvPes(const std::uint8_t* pesData, std::size_t size, std::uint64_t fileOffset)
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
            | (static_cast<std::uint64_t>(pesData[11] & 0xFEU) << 14U) | (static_cast<std::uint64_t>(pesData[12]) << 7U)
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

std::optional<std::uint64_t> StanagStreamIndexer::peekUtcTimestamp(const std::uint8_t* data, std::size_t size) noexcept
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

    const double dur = m_timeIndex.durationSeconds();
    std::uint64_t durBits = 0U;
    std::memcpy(&durBits, &dur, sizeof(dur));
    append64(durBits);

    const auto& videoEntries = m_timeIndex.videoEntries();
    const auto& klvEntries = m_timeIndex.klvEntries();

    append32(static_cast<std::uint32_t>(videoEntries.size()));
    append32(static_cast<std::uint32_t>(klvEntries.size()));

    for (const auto& v : videoEntries) {
        append64(v.ptsTicks);
        append64(v.dtsTicks);
        append64(v.fileByteOffset);
        append32(v.packetSizeBytes);
        append32(v.frameIndex);
        payload.push_back(v.isKeyframe ? 1U : 0U);
        payload.push_back(0U);
        payload.push_back(0U);
        payload.push_back(0U);
    }

    for (const auto& k : klvEntries) {
        append64(k.ptsTicks);
        append64(k.utcTimestampUs);
        append64(k.fileByteOffset);
        append32(k.packetSizeBytes);
        append32(k.messageIndex);
        if (k.message.has_value()) {
            payload.push_back(1U);
            const auto encoded = KlvEncoder::encode(*k.message);
            append32(static_cast<std::uint32_t>(encoded.size()));
            payload.insert(payload.end(), encoded.begin(), encoded.end());
        } else {
            payload.push_back(0U);
            append32(0U);
        }
    }

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
    if (fileSize < 44) {
        return false;
    }

    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(fileSize));
    in.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    if (!in.good()) {
        return false;
    }

    const std::size_t payloadSize = buffer.size() - 4U;
    const std::uint32_t storedCrc = static_cast<std::uint32_t>(buffer[payloadSize])
        | (static_cast<std::uint32_t>(buffer[payloadSize + 1]) << 8U)
        | (static_cast<std::uint32_t>(buffer[payloadSize + 2]) << 16U)
        | (static_cast<std::uint32_t>(buffer[payloadSize + 3]) << 24U);

    const std::uint32_t calculatedCrc = KlvCrc::calculateCrc32Mpeg(buffer.data(), payloadSize);
    if (storedCrc != calculatedCrc) {
        return false;
    }

    std::size_t pos = 0U;
    auto read32 = [&buffer, &pos]() -> std::uint32_t {
        const std::uint32_t v = static_cast<std::uint32_t>(buffer[pos])
            | (static_cast<std::uint32_t>(buffer[pos + 1]) << 8U) | (static_cast<std::uint32_t>(buffer[pos + 2]) << 16U)
            | (static_cast<std::uint32_t>(buffer[pos + 3]) << 24U);
        pos += 4U;
        return v;
    };

    auto read64 = [&buffer, &pos]() -> std::uint64_t {
        std::uint64_t v = 0U;
        for (int i = 0; i < 8; ++i) {
            v |= (static_cast<std::uint64_t>(buffer[pos + static_cast<std::size_t>(i)]) << (i * 8));
        }
        pos += 8U;
        return v;
    };

    const std::uint32_t magic = read32();
    if (magic != kSidxMagic) {
        return false;
    }

    const std::uint32_t version = read32();
    if (version != kSidxVersion) {
        return false;
    }

    const std::uint64_t basePts = read64();
    const std::uint64_t baseUtc = read64();
    const std::uint64_t durBits = read64();
    double durationSec = 0.0;
    std::memcpy(&durationSec, &durBits, sizeof(durationSec));

    const std::uint32_t videoCount = read32();
    const std::uint32_t klvCount = read32();

    constexpr std::size_t kVideoEntryBytes = 36U;
    if (pos + (static_cast<std::size_t>(videoCount) * kVideoEntryBytes) > payloadSize) {
        return false;
    }

    std::vector<VideoIndexEntry> videoEntries {};
    videoEntries.reserve(videoCount);
    for (std::uint32_t i = 0U; i < videoCount; ++i) {
        VideoIndexEntry v {};
        v.ptsTicks = read64();
        v.dtsTicks = read64();
        v.fileByteOffset = read64();
        v.packetSizeBytes = read32();
        v.frameIndex = read32();
        v.isKeyframe = (buffer[pos] != 0U);
        pos += 4U;
        videoEntries.push_back(v);
    }

    std::vector<KlvIndexEntry> klvEntries {};
    klvEntries.reserve(klvCount);
    for (std::uint32_t i = 0U; i < klvCount; ++i) {
        constexpr std::size_t kKlvHeaderBytes = 33U;
        if (pos + kKlvHeaderBytes + 4U > payloadSize) {
            return false;
        }
        KlvIndexEntry k {};
        k.ptsTicks = read64();
        k.utcTimestampUs = read64();
        k.fileByteOffset = read64();
        k.packetSizeBytes = read32();
        k.messageIndex = read32();
        const bool hasMsg = (buffer[pos++] != 0U);
        const std::uint32_t klvLen = read32();
        if (hasMsg && klvLen > 0U) {
            if (pos + klvLen > payloadSize) {
                return false;
            }
            UasDatalinkMessage msg {};
            if (KlvParser::parse(buffer.data() + pos, klvLen, msg, false) == KlvStatus::Success) {
                k.message = msg;
            }
            pos += klvLen;
        }
        klvEntries.push_back(k);
    }

    if (pos != payloadSize) {
        return false;
    }

    m_timeIndex.loadEntries(std::move(videoEntries), std::move(klvEntries), basePts, baseUtc, durationSec);
    m_videoFrameCount = static_cast<std::uint32_t>(m_timeIndex.videoFrameCount());
    m_klvPacketCount = static_cast<std::uint32_t>(m_timeIndex.klvPacketCount());
    return true;
}

} // namespace Klv
