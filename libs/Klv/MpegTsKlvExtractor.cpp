#include "MpegTsKlvExtractor.h"
#include <algorithm>
#include <cstring>

namespace Klv {

MpegTsKlvExtractor::MpegTsKlvExtractor() {
    m_scanner.setRawPacketCallback([this](const std::uint8_t* packet, std::size_t size) {
        if (m_payloadCallback) {
            m_payloadCallback(packet, size);
        }
    });
}

void MpegTsKlvExtractor::setMetadataPid(std::uint16_t pid) noexcept {
    m_metadataPid = pid;
}

void MpegTsKlvExtractor::setKlvPayloadCallback(KlvPayloadCallback callback) {
    m_payloadCallback = std::move(callback);
}

void MpegTsKlvExtractor::setMessageCallback(KlvStreamScanner::MessageCallback callback) {
    m_scanner.setMessageCallback(std::move(callback));
}

std::optional<std::uint16_t> MpegTsKlvExtractor::metadataPid() const noexcept {
    return m_metadataPid;
}

void MpegTsKlvExtractor::reset() {
    m_streamBuffer.clear();
    m_pesReassemblyBuffer.clear();
    m_lastContinuityCounter = -1;
    m_scanner.reset();
}

std::size_t MpegTsKlvExtractor::processStream(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0U) {
        return 0U;
    }

    m_streamBuffer.insert(m_streamBuffer.end(), data, data + size);
    std::size_t dispatched = 0U;

    while (m_streamBuffer.size() >= kTsPacketSize) {
        // Find 0x47 sync byte
        auto it = std::find(m_streamBuffer.begin(), m_streamBuffer.end(), kTsSyncByte);
        if (it == m_streamBuffer.end()) {
            m_streamBuffer.clear();
            break;
        }

        if (it != m_streamBuffer.begin()) {
            m_streamBuffer.erase(m_streamBuffer.begin(), it);
        }

        if (m_streamBuffer.size() < kTsPacketSize) {
            break;
        }

        // Verify next packet sync if enough bytes are available
        if (m_streamBuffer.size() >= 2U * kTsPacketSize && m_streamBuffer[kTsPacketSize] != kTsSyncByte) {
            // False sync byte, drop 1 byte and re-align
            m_streamBuffer.erase(m_streamBuffer.begin());
            continue;
        }

        dispatched += processTsPacket(m_streamBuffer.data());
        m_streamBuffer.erase(m_streamBuffer.begin(), m_streamBuffer.begin() + kTsPacketSize);
    }

    return dispatched;
}

std::size_t MpegTsKlvExtractor::processTsPacket(const std::uint8_t* packet) {
    if (packet == nullptr || packet[0] != kTsSyncByte) {
        return 0U;
    }

    // Transport Error Indicator
    if ((packet[1] & 0x80U) != 0U) {
        return 0U;
    }

    const bool pusi = (packet[1] & 0x40U) != 0U;
    const std::uint16_t pid = static_cast<std::uint16_t>(((static_cast<std::uint16_t>(packet[1] & 0x1FU)) << 8U) |
                                                         static_cast<std::uint16_t>(packet[2]));
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

    // If metadata PID not discovered yet, check if this packet contains MISB UL
    if (!m_metadataPid.has_value()) {
        if (payloadSize >= kUniversalLabelSize &&
            std::memcmp(payload, kMisb0601UniversalLabel.data(), kMisb0601PrefixSize) == 0) {
            m_metadataPid = pid;
        }
    }

    std::size_t dispatched = 0U;

    // Process metadata packet if matched
    if (m_metadataPid.has_value() && pid == *m_metadataPid) {
        if (pusi) {
            if (!m_pesReassemblyBuffer.empty()) {
                handlePesPacket(m_pesReassemblyBuffer.data(), m_pesReassemblyBuffer.size());
                m_pesReassemblyBuffer.clear();
            }
        }
        m_pesReassemblyBuffer.insert(m_pesReassemblyBuffer.end(), payload, payload + payloadSize);

        // Feed directly to scanner in case of unfragmented or streaming KLV
        dispatched += m_scanner.processBytes(payload, payloadSize);
    }

    return dispatched;
}

void MpegTsKlvExtractor::parsePat(const std::uint8_t* payload, std::size_t size) {
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

    const std::size_t sectionLength = ((static_cast<std::size_t>(payload[offset + 1] & 0x0FU)) << 8U) |
                                      static_cast<std::size_t>(payload[offset + 2]);
    offset += 3U; // Past table_id and section_length

    if (offset + sectionLength > size + 3U || sectionLength < 9U) {
        return;
    }

    offset += 5U; // Skip transport_stream_id, version/current_next, section_number, last_section_number

    const std::size_t programDataEnd = (offset - 5U) + (sectionLength - 4U); // Exclude 4-byte CRC32
    while (offset + 4U <= programDataEnd && offset + 4U <= size) {
        const std::uint16_t programNum = static_cast<std::uint16_t>((payload[offset] << 8U) | payload[offset + 1]);
        const std::uint16_t pmtPid = static_cast<std::uint16_t>(((payload[offset + 2] & 0x1FU) << 8U) | payload[offset + 3]);
        if (programNum != 0U) {
            m_pmtPid = pmtPid;
            break;
        }
        offset += 4U;
    }
}

void MpegTsKlvExtractor::parsePmt(const std::uint8_t* payload, std::size_t size) {
    if (payload == nullptr || size < 12U) {
        return;
    }

    std::size_t offset = 0U;
    const std::size_t pointerField = static_cast<std::size_t>(payload[0]);
    offset += 1U + pointerField;

    if (offset + 11U > size || payload[offset] != 0x02U) { // Table ID 0x02 = PMT
        return;
    }

    const std::size_t sectionLength = ((static_cast<std::size_t>(payload[offset + 1] & 0x0FU)) << 8U) |
                                      static_cast<std::size_t>(payload[offset + 2]);
    offset += 3U;

    if (offset + sectionLength > size + 3U || sectionLength < 13U) {
        return;
    }

    offset += 7U; // Skip program_num, version, section_num, last_section, PCR_PID
    if (offset + 2U > size) return;

    const std::size_t progInfoLen = ((static_cast<std::size_t>(payload[offset] & 0x0FU)) << 8U) |
                                    static_cast<std::size_t>(payload[offset + 1]);
    offset += 2U + progInfoLen;

    const std::size_t esDataEnd = (offset - 9U - progInfoLen) + (sectionLength - 4U);
    while (offset + 5U <= esDataEnd && offset + 5U <= size) {
        const std::uint8_t streamType = payload[offset];
        const std::uint16_t elemPid = static_cast<std::uint16_t>(((payload[offset + 1] & 0x1FU) << 8U) | payload[offset + 2]);
        const std::size_t esInfoLen = ((static_cast<std::size_t>(payload[offset + 3] & 0x0FU)) << 8U) |
                                      static_cast<std::size_t>(payload[offset + 4]);

        // STANAG 4609 specifies stream_type 0x06 (PES private data) or 0x15 (metadata in PES)
        if (streamType == 0x06U || streamType == 0x15U) {
            m_metadataPid = elemPid;
            break;
        }

        offset += 5U + esInfoLen;
    }
}

void MpegTsKlvExtractor::handlePesPacket(const std::uint8_t* pesData, std::size_t pesSize) {
    if (pesData == nullptr || pesSize < 6U) {
        return;
    }

    // Check for PES start code prefix: 0x00 0x00 0x01
    if (pesData[0] == 0x00U && pesData[1] == 0x00U && pesData[2] == 0x01U) {
        // Stream ID at pesData[3]
        // PES packet length at pesData[4..5]
        if (pesSize >= 9U) {
            // Optional PES header flags at pesData[6..7]
            const std::size_t pesHeaderDataLen = static_cast<std::size_t>(pesData[8]);
            const std::size_t payloadOffset = 9U + pesHeaderDataLen;
            if (payloadOffset < pesSize) {
                m_scanner.processBytes(pesData + payloadOffset, pesSize - payloadOffset);
                return;
            }
        }
    }

    // If not standard PES header or payloadOffset past end, feed entire buffer to scanner
    m_scanner.processBytes(pesData, pesSize);
}

} // namespace Klv
