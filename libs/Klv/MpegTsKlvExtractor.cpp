#include "MpegTsKlvExtractor.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "KlvBer.h"
#include "KlvParser.h"

namespace Klv {

MpegTsKlvExtractor::MpegTsKlvExtractor()
{
    m_scanner.setRawPacketCallback([this](const std::uint8_t* packet, std::size_t size) {
        if (m_payloadCallback) {
            m_payloadCallback(packet, size);
        }
    });
}

void MpegTsKlvExtractor::setMetadataPid(std::uint16_t pid) noexcept
{
    m_metadataPid = pid;
    m_metadataPids.clear();
    m_metadataPids.push_back(pid);
}

void MpegTsKlvExtractor::addMetadataPid(std::uint16_t pid)
{
    if (std::find(m_metadataPids.begin(), m_metadataPids.end(), pid) == m_metadataPids.end()) {
        m_metadataPids.push_back(pid);
    }
    if (!m_metadataPid.has_value()) {
        m_metadataPid = pid;
    }
}

void MpegTsKlvExtractor::setKlvPayloadCallback(KlvPayloadCallback callback)
{
    m_payloadCallback = std::move(callback);
}

void MpegTsKlvExtractor::setMultiPidPayloadCallback(MultiPidPayloadCallback callback)
{
    m_multiPayloadCallback = std::move(callback);
}

void MpegTsKlvExtractor::setMessageCallback(KlvStreamScanner::MessageCallback callback)
{
    m_messageCallback = callback;
    m_scanner.setMessageCallback(std::move(callback));
}

void MpegTsKlvExtractor::setMultiPidMessageCallback(MultiPidMessageCallback callback)
{
    m_multiMessageCallback = std::move(callback);
}

std::optional<std::uint16_t> MpegTsKlvExtractor::metadataPid() const noexcept
{
    return m_metadataPid;
}

std::vector<std::uint16_t> MpegTsKlvExtractor::metadataPids() const
{
    return m_metadataPids;
}

bool MpegTsKlvExtractor::isMetadataPid(std::uint16_t pid) const noexcept
{
    if (m_metadataPid.has_value() && *m_metadataPid == pid) {
        return true;
    }
    return std::find(m_metadataPids.begin(), m_metadataPids.end(), pid) != m_metadataPids.end();
}

MpegTsKlvExtractor::SubstreamReassembly& MpegTsKlvExtractor::getOrCreateSubstream(std::uint16_t pid)
{
    for (auto& sub : m_substreams) {
        if (sub.pid == pid) {
            return sub;
        }
    }
    m_substreams.push_back(SubstreamReassembly { pid, -1, { } });
    return m_substreams.back();
}

void MpegTsKlvExtractor::reset()
{
    m_streamBuffer.clear();
    m_substreams.clear();
    m_scanner.reset();
}

std::size_t MpegTsKlvExtractor::flush()
{
    std::size_t dispatched = 0U;
    for (auto& sub : m_substreams) {
        if (!sub.buffer.empty()) {
            dispatched += handlePesPacket(sub.pid, sub.buffer.data(), sub.buffer.size());
            sub.buffer.clear();
        }
    }
    return dispatched;
}

std::size_t MpegTsKlvExtractor::processStream(const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size == 0U) {
        return 0U;
    }

    m_streamBuffer.insert(m_streamBuffer.end(), data, data + size);
    std::size_t dispatched = 0U;
    std::size_t offset = 0U;

    while (offset + kTsPacketSize <= m_streamBuffer.size()) {
        // Find 0x47 sync byte
        if (m_streamBuffer[offset] != kTsSyncByte) {
            offset++;
            continue;
        }

        // Verify next packet sync if enough bytes are available
        if (offset + 2U * kTsPacketSize <= m_streamBuffer.size()
            && m_streamBuffer[offset + kTsPacketSize] != kTsSyncByte) {
            // False sync byte, advance by 1 byte and re-align
            offset++;
            continue;
        }

        dispatched += processTsPacket(m_streamBuffer.data() + offset);
        offset += kTsPacketSize;
    }

    if (offset > 0U) {
        m_streamBuffer.erase(m_streamBuffer.begin(), m_streamBuffer.begin() + static_cast<std::ptrdiff_t>(offset));
    }

    return dispatched;
}

std::size_t MpegTsKlvExtractor::processTsPacket(const std::uint8_t* packet)
{
    if (packet == nullptr || packet[0] != kTsSyncByte) {
        return 0U;
    }

    // Transport Error Indicator
    if ((packet[1] & 0x80U) != 0U) {
        return 0U;
    }

    const bool pusi = (packet[1] & 0x40U) != 0U;
    const std::uint16_t pid = static_cast<std::uint16_t>(
        ((static_cast<std::uint16_t>(packet[1] & 0x1FU)) << 8U) | static_cast<std::uint16_t>(packet[2]));
    const std::uint8_t afc = (packet[3] >> 4U) & 0x03U;

    // No payload if afc == 0 (reserved) or afc == 2 (adaptation field only)
    if (afc == 0U || afc == 2U) {
        return 0U;
    }

    std::size_t payloadOffset = 4U;
    if (afc == 3U) {
        // Adaptation field present
        const std::size_t afLen = static_cast<std::size_t>(packet[4]);
        payloadOffset += 1U + afLen;
        if (payloadOffset > kTsPacketSize) {
            return 0U; // Corrupt adaptation field
        }
    }

    const std::uint8_t* payload = packet + payloadOffset;
    const std::size_t payloadSize = kTsPacketSize - payloadOffset;
    if (payloadSize == 0U) {
        return 0U;
    }

    // Auto-discover PMT via PAT (PID 0)
    if (pid == 0x0000U) {
        parsePat(payload, payloadSize);
        return 0U;
    }

    // Auto-discover Metadata Elementary Stream via PMT
    if (m_pmtPid.has_value() && pid == *m_pmtPid) {
        parsePmt(payload, payloadSize);
        return 0U;
    }

    // If metadata PID not discovered yet, check if this packet contains MISB UL or PES wrapping MISB UL
    if (!m_metadataPid.has_value() && m_metadataPids.empty()) {
        if (payloadSize >= kUniversalLabelSize
            && std::memcmp(payload, kMisb0601UniversalLabel.data(), kMisb0601PrefixSize) == 0) {
            addMetadataPid(pid);
        } else if (payloadSize >= 9U + kUniversalLabelSize && payload[0] == 0x00U && payload[1] == 0x00U
            && payload[2] == 0x01U) {
            const std::size_t pesHdrLen = static_cast<std::size_t>(payload[8]);
            const std::size_t klvOffset = 9U + pesHdrLen;
            if (klvOffset + kUniversalLabelSize <= payloadSize
                && std::memcmp(payload + klvOffset, kMisb0601UniversalLabel.data(), kMisb0601PrefixSize) == 0) {
                addMetadataPid(pid);
            }
        }
    }

    // Ignore packets that don't match any registered metadata PID
    if (!isMetadataPid(pid)) {
        return 0U;
    }

    auto& sub = getOrCreateSubstream(pid);

    // Continuity Counter checking
    const std::uint8_t cc = packet[3] & 0x0FU;
    bool ccDiscontinuity = false;
    if (sub.lastContinuityCounter >= 0) {
        if (static_cast<int>(cc) == sub.lastContinuityCounter) {
            // Duplicate packet, ignore
            return 0U;
        }
        const int expectedCc = (sub.lastContinuityCounter + 1) & 0x0F;
        if (static_cast<int>(cc) != expectedCc) {
            ccDiscontinuity = true;
        }
    }
    sub.lastContinuityCounter = static_cast<int>(cc);

    if (ccDiscontinuity) {
        sub.buffer.clear();
        if (!pusi) {
            // Fragment following dropped packets cannot be assembled
            return 0U;
        }
    }

    std::size_t dispatched = 0U;

    // On PUSI boundary, flush any previously buffered unbounded or unconsumed PES packet
    if (pusi) {
        if (!sub.buffer.empty()) {
            dispatched += handlePesPacket(pid, sub.buffer.data(), sub.buffer.size());
            sub.buffer.clear();
        }
    }

    if (sub.buffer.size() + payloadSize > kMaxPesBufferSize) {
        sub.buffer.clear();
        return 0U;
    }

    sub.buffer.insert(sub.buffer.end(), payload, payload + payloadSize);

    // Immediate complete PES / KLV packet extraction loop
    while (sub.buffer.size() >= 6U) {
        if (sub.buffer[0] == 0x00U && sub.buffer[1] == 0x00U && sub.buffer[2] == 0x01U) {
            const std::size_t pesLen
                = (static_cast<std::size_t>(sub.buffer[4]) << 8U) | static_cast<std::size_t>(sub.buffer[5]);
            if (pesLen == 0U) {
                // Unbounded PES stream: wait for next PUSI or flush()
                break;
            }

            const std::size_t totalPesSize = 6U + pesLen;
            if (sub.buffer.size() < totalPesSize) {
                // Incomplete PES fragment: wait for more TS packets
                break;
            }

            dispatched += handlePesPacket(pid, sub.buffer.data(), totalPesSize);
            sub.buffer.erase(sub.buffer.begin(), sub.buffer.begin() + static_cast<std::ptrdiff_t>(totalPesSize));
        } else if (sub.buffer.size() >= kUniversalLabelSize + 2U
            && std::memcmp(sub.buffer.data(), kMisb0601UniversalLabel.data(), kMisb0601PrefixSize) == 0) {
            // Raw KLV packet without PES encapsulation
            std::size_t payloadLength { 0U };
            std::size_t berLenConsumed { 0U };
            const std::uint8_t* berPtr = sub.buffer.data() + kUniversalLabelSize;
            const std::size_t avail = sub.buffer.size() - kUniversalLabelSize;
            if (!KlvBer::decodeLength(berPtr, avail, payloadLength, berLenConsumed)) {
                break;
            }

            const std::size_t totalKlvSize = kUniversalLabelSize + berLenConsumed + payloadLength;
            if (sub.buffer.size() < totalKlvSize) {
                break;
            }

            dispatched += handlePesPacket(pid, sub.buffer.data(), totalKlvSize);
            sub.buffer.erase(sub.buffer.begin(), sub.buffer.begin() + static_cast<std::ptrdiff_t>(totalKlvSize));
        } else {
            // If remainder is all 0xFF TS stuffing, clear buffer and finish
            if (std::all_of(sub.buffer.begin(), sub.buffer.end(), [](std::uint8_t b) { return b == 0xFFU; })) {
                sub.buffer.clear();
                break;
            }
            sub.buffer.erase(sub.buffer.begin());
        }
    }

    return dispatched;
}

void MpegTsKlvExtractor::parsePat(const std::uint8_t* payload, std::size_t size)
{
    if (payload == nullptr || size < 8U) {
        return;
    }

    std::size_t offset = 0U;
    // Skip pointer field if present
    const std::size_t pointerField = static_cast<std::size_t>(payload[0]);
    offset += 1U + pointerField;

    if (offset + 7U > size || payload[offset] != 0x00U) { // Table ID 0x00 = PAT
        return;
    }

    const std::size_t sectionLength = ((static_cast<std::size_t>(payload[offset + 1] & 0x0FU)) << 8U)
        | static_cast<std::size_t>(payload[offset + 2]);
    offset += 3U; // Past table_id and section_length

    if (offset + sectionLength > size + 3U || sectionLength < 9U) {
        return;
    }

    offset += 5U; // Skip transport_stream_id, version/current_next, section_number, last_section_number

    const std::size_t programDataEnd = (offset - 5U) + (sectionLength - 4U); // Exclude 4-byte CRC32
    while (offset + 4U <= programDataEnd && offset + 4U <= size) {
        const std::uint16_t programNum = static_cast<std::uint16_t>((payload[offset] << 8U) | payload[offset + 1]);
        const std::uint16_t pmtPid
            = static_cast<std::uint16_t>(((payload[offset + 2] & 0x1FU) << 8U) | payload[offset + 3]);
        if (programNum != 0U) {
            m_pmtPid = pmtPid;
            break;
        }
        offset += 4U;
    }
}

void MpegTsKlvExtractor::parsePmt(const std::uint8_t* payload, std::size_t size)
{
    if (payload == nullptr || size < 12U) {
        return;
    }

    std::size_t offset = 0U;
    const std::size_t pointerField = static_cast<std::size_t>(payload[0]);
    offset += 1U + pointerField;

    if (offset + 11U > size || payload[offset] != 0x02U) { // Table ID 0x02 = PMT
        return;
    }

    const std::size_t sectionLength = ((static_cast<std::size_t>(payload[offset + 1] & 0x0FU)) << 8U)
        | static_cast<std::size_t>(payload[offset + 2]);
    offset += 3U;

    if (offset + sectionLength > size + 3U || sectionLength < 13U) {
        return;
    }

    offset += 7U; // Skip program_num, version, section_num, last_section, PCR_PID
    if (offset + 2U > size)
        return;

    const std::size_t progInfoLen
        = ((static_cast<std::size_t>(payload[offset] & 0x0FU)) << 8U) | static_cast<std::size_t>(payload[offset + 1]);
    offset += 2U + progInfoLen;

    const std::size_t esDataEnd = (offset - 9U - progInfoLen) + (sectionLength - 4U);
    while (offset + 5U <= esDataEnd && offset + 5U <= size) {
        const std::uint8_t streamType = payload[offset];
        const std::uint16_t elemPid
            = static_cast<std::uint16_t>(((payload[offset + 1] & 0x1FU) << 8U) | payload[offset + 2]);
        const std::size_t esInfoLen = ((static_cast<std::size_t>(payload[offset + 3] & 0x0FU)) << 8U)
            | static_cast<std::size_t>(payload[offset + 4]);

        // STANAG 4609 specifies stream_type 0x06 (PES private data) or 0x15 (metadata in PES)
        if (streamType == 0x06U || streamType == 0x15U) {
            addMetadataPid(elemPid);
        }

        offset += 5U + esInfoLen;
    }
}

std::size_t MpegTsKlvExtractor::handlePesPacket(std::uint16_t pid, const std::uint8_t* pesData, std::size_t pesSize)
{
    if (pesData == nullptr || pesSize < 6U) {
        return 0U;
    }

    const std::uint8_t* klvBytes = pesData;
    std::size_t klvSize = pesSize;

    // Check for PES start code prefix: 0x00 0x00 0x01
    if (pesData[0] == 0x00U && pesData[1] == 0x00U && pesData[2] == 0x01U) {
        const std::uint8_t streamId = pesData[3];
        std::size_t payloadOffset = 6U;
        if (streamId != 0xBCU && streamId != 0xBFU && streamId != 0xF0U && streamId != 0xF1U && streamId != 0xF2U
            && streamId != 0xF8U && streamId != 0xFFU) {
            if (pesSize >= 9U) {
                const std::size_t pesHeaderDataLen = static_cast<std::size_t>(pesData[8]);
                payloadOffset = 9U + pesHeaderDataLen;
            } else {
                return 0U;
            }
        }
        if (payloadOffset >= pesSize) {
            return 0U;
        }
        klvBytes = pesData + payloadOffset;
        klvSize = pesSize - payloadOffset;
    }

    // Multi-PID payload callback
    if (m_multiPayloadCallback) {
        m_multiPayloadCallback(pid, klvBytes, klvSize);
    }

    // Attempt multi-PID message decoding
    if (m_multiMessageCallback) {
        UasDatalinkMessage parsedMsg { };
        if (KlvParser::parse(klvBytes, klvSize, parsedMsg) == KlvStatus::Success) {
            m_multiMessageCallback(pid, parsedMsg);
        }
    }

    // Feed to scanner for legacy single-PID callbacks
    return m_scanner.processBytes(klvBytes, klvSize);
}

} // namespace Klv
