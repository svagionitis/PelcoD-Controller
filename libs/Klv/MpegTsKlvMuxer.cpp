#include "MpegTsKlvMuxer.h"
#include "KlvCrc.h"
#include "KlvEncoder.h"
#include <algorithm>
#include <cstring>

namespace Klv {

MpegTsKlvMuxer::MpegTsKlvMuxer(const MpegTsMuxerConfig& config)
    : m_config(config) {
}

void MpegTsKlvMuxer::setPacketCallback(TsPacketCallback callback) {
    m_callback = std::move(callback);
}

void MpegTsKlvMuxer::setConfig(const MpegTsMuxerConfig& config) {
    m_config = config;
}

const MpegTsMuxerConfig& MpegTsKlvMuxer::config() const noexcept {
    return m_config;
}

void MpegTsKlvMuxer::reset() noexcept {
    m_continuityCounters.clear();
    m_packetCounter = 0U;
    m_lastPcrTimestampUs = 0U;
}

std::uint8_t MpegTsKlvMuxer::nextCc(std::uint16_t pid) noexcept {
    auto it = m_continuityCounters.find(pid);
    if (it == m_continuityCounters.end()) {
        m_continuityCounters[pid] = 0U;
        return 0U;
    }
    const std::uint8_t cc = static_cast<std::uint8_t>((it->second + 1U) & 0x0FU);
    it->second = cc;
    return cc;
}

void MpegTsKlvMuxer::dispatchPacket(const std::vector<std::uint8_t>& packet) {
    if (m_callback && packet.size() == kTsPacketSize) {
        m_callback(packet.data(), packet.size());
    }
}

std::vector<std::uint8_t> MpegTsKlvMuxer::buildPatPacket() {
    std::vector<std::uint8_t> packet(kTsPacketSize, 0xFFU);
    packet[0] = kTsSyncByte; // 0x47
    packet[1] = 0x40U;       // PUSI=1, PID high 5 bits = 0
    packet[2] = 0x00U;       // PID low 8 bits = 0
    packet[3] = static_cast<std::uint8_t>(0x10U | (nextCc(0x0000U) & 0x0FU));

    packet[4] = 0x00U; // Pointer field

    // PAT section:
    // 5 bytes header + 4 bytes program 1 entry + 4 bytes CRC32 = 13 bytes
    constexpr std::uint16_t kSectionLength = 13U;
    std::vector<std::uint8_t> section;
    section.reserve(3U + kSectionLength);

    section.push_back(0x00U); // Table ID 0x00 = PAT
    section.push_back(static_cast<std::uint8_t>(0xB0U | ((kSectionLength >> 8U) & 0x0FU)));
    section.push_back(static_cast<std::uint8_t>(kSectionLength & 0xFFU));

    section.push_back(static_cast<std::uint8_t>((m_config.transportStreamId >> 8U) & 0xFFU));
    section.push_back(static_cast<std::uint8_t>(m_config.transportStreamId & 0xFFU));

    section.push_back(0xC1U); // version 0, current_next = 1
    section.push_back(0x00U); // section_number
    section.push_back(0x00U); // last_section_number

    // Program 1 entry
    section.push_back(static_cast<std::uint8_t>((m_config.programNumber >> 8U) & 0xFFU));
    section.push_back(static_cast<std::uint8_t>(m_config.programNumber & 0xFFU));
    section.push_back(static_cast<std::uint8_t>(0xE0U | ((m_config.pmtPid >> 8U) & 0x1FU)));
    section.push_back(static_cast<std::uint8_t>(m_config.pmtPid & 0xFFU));

    const std::uint32_t crc = KlvCrc::calculateCrc32Mpeg(section.data(), section.size());
    section.push_back(static_cast<std::uint8_t>((crc >> 24U) & 0xFFU));
    section.push_back(static_cast<std::uint8_t>((crc >> 16U) & 0xFFU));
    section.push_back(static_cast<std::uint8_t>((crc >> 8U) & 0xFFU));
    section.push_back(static_cast<std::uint8_t>(crc & 0xFFU));

    std::copy(section.begin(), section.end(), packet.begin() + 5);
    return packet;
}

std::vector<std::uint8_t> MpegTsKlvMuxer::buildPmtPacket() {
    std::vector<std::uint8_t> packet(kTsPacketSize, 0xFFU);
    packet[0] = kTsSyncByte; // 0x47
    packet[1] = static_cast<std::uint8_t>(0x40U | ((m_config.pmtPid >> 8U) & 0x1FU));
    packet[2] = static_cast<std::uint8_t>(m_config.pmtPid & 0xFFU);
    packet[3] = static_cast<std::uint8_t>(0x10U | (nextCc(m_config.pmtPid) & 0x0FU));

    packet[4] = 0x00U; // Pointer field

    // ES descriptors:
    // Registration descriptor (Tag 0x05, length 4, identifier "KLVA")
    std::vector<std::uint8_t> esDesc;
    if (m_config.formatIdentifier.size() >= 4U) {
        esDesc.push_back(0x05U); // Registration descriptor tag
        esDesc.push_back(0x04U); // Descriptor length
        esDesc.push_back(static_cast<std::uint8_t>(m_config.formatIdentifier[0]));
        esDesc.push_back(static_cast<std::uint8_t>(m_config.formatIdentifier[1]));
        esDesc.push_back(static_cast<std::uint8_t>(m_config.formatIdentifier[2]));
        esDesc.push_back(static_cast<std::uint8_t>(m_config.formatIdentifier[3]));
    }

    constexpr std::uint16_t progInfoLen = 0U;
    const auto esInfoLen = static_cast<std::uint16_t>(esDesc.size());

    // Section length = 9 (header after length) + progInfoLen + (5 + esInfoLen) + 4 (CRC)
    const std::uint16_t sectionLength = static_cast<std::uint16_t>(9U + progInfoLen + 5U + esInfoLen + 4U);

    std::vector<std::uint8_t> section;
    section.reserve(3U + sectionLength);
    section.push_back(0x02U); // Table ID 0x02 = PMT
    section.push_back(static_cast<std::uint8_t>(0xB0U | ((sectionLength >> 8U) & 0x0FU)));
    section.push_back(static_cast<std::uint8_t>(sectionLength & 0xFFU));

    section.push_back(static_cast<std::uint8_t>((m_config.programNumber >> 8U) & 0xFFU));
    section.push_back(static_cast<std::uint8_t>(m_config.programNumber & 0xFFU));

    section.push_back(0xC1U); // version 0, current_next = 1
    section.push_back(0x00U); // section_number
    section.push_back(0x00U); // last_section_number

    section.push_back(static_cast<std::uint8_t>(0xE0U | ((m_config.pcrPid >> 8U) & 0x1FU)));
    section.push_back(static_cast<std::uint8_t>(m_config.pcrPid & 0xFFU));

    section.push_back(static_cast<std::uint8_t>(0xF0U | ((progInfoLen >> 8U) & 0x0FU)));
    section.push_back(static_cast<std::uint8_t>(progInfoLen & 0xFFU));

    // Elementary stream entry
    section.push_back(static_cast<std::uint8_t>(m_config.streamType)); // 0x06 or 0x15
    section.push_back(static_cast<std::uint8_t>(0xE0U | ((m_config.metadataPid >> 8U) & 0x1FU)));
    section.push_back(static_cast<std::uint8_t>(m_config.metadataPid & 0xFFU));

    section.push_back(static_cast<std::uint8_t>(0xF0U | ((esInfoLen >> 8U) & 0x0FU)));
    section.push_back(static_cast<std::uint8_t>(esInfoLen & 0xFFU));
    section.insert(section.end(), esDesc.begin(), esDesc.end());

    const std::uint32_t crc = KlvCrc::calculateCrc32Mpeg(section.data(), section.size());
    section.push_back(static_cast<std::uint8_t>((crc >> 24U) & 0xFFU));
    section.push_back(static_cast<std::uint8_t>((crc >> 16U) & 0xFFU));
    section.push_back(static_cast<std::uint8_t>((crc >> 8U) & 0xFFU));
    section.push_back(static_cast<std::uint8_t>(crc & 0xFFU));

    std::copy(section.begin(), section.end(), packet.begin() + 5);
    return packet;
}

std::vector<std::uint8_t> MpegTsKlvMuxer::buildPesPacket(const std::uint8_t* klvData,
                                                         std::size_t size,
                                                         std::optional<std::uint64_t> timestampUs) {
    std::vector<std::uint8_t> pes;
    if (klvData == nullptr || size == 0U) {
        return pes;
    }

    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x01U);
    pes.push_back(static_cast<std::uint8_t>(m_config.streamId));

    const bool hasPts = m_config.emitPts && timestampUs.has_value();
    const std::size_t pesHeaderDataLen = hasPts ? 5U : 0U;
    const std::size_t pesPacketLen = 3U + pesHeaderDataLen + size;

    const std::uint16_t lengthField = (pesPacketLen <= 0xFFFFU)
                                          ? static_cast<std::uint16_t>(pesPacketLen)
                                          : 0x0000U;

    pes.push_back(static_cast<std::uint8_t>((lengthField >> 8U) & 0xFFU));
    pes.push_back(static_cast<std::uint8_t>(lengthField & 0xFFU));

    // Optional PES header extension
    // Byte 6: 10 (marker) | 00 (scrambling) | 0 (priority) | 1 (data alignment) | 0 (copyright) | 0 (orig) = 0x84
    pes.push_back(0x84U);
    // Byte 7: PTS_DTS flags (0x80 if PTS, 0x00 otherwise)
    pes.push_back(hasPts ? 0x80U : 0x00U);
    // Byte 8: PES_header_data_length
    pes.push_back(static_cast<std::uint8_t>(pesHeaderDataLen));

    if (hasPts) {
        // 33-bit PTS at 90 kHz
        const std::uint64_t pts = ((*timestampUs * 90ULL) / 1000ULL) & 0x1FFFFFFFFULL;
        pes.push_back(static_cast<std::uint8_t>(0x21U | (((pts >> 30U) & 0x07U) << 1U)));
        pes.push_back(static_cast<std::uint8_t>((pts >> 22U) & 0xFFU));
        pes.push_back(static_cast<std::uint8_t>((((pts >> 15U) & 0x7FU) << 1U) | 0x01U));
        pes.push_back(static_cast<std::uint8_t>((pts >> 7U) & 0xFFU));
        pes.push_back(static_cast<std::uint8_t>(((pts & 0x7FU) << 1U) | 0x01U));
    }

    pes.insert(pes.end(), klvData, klvData + size);
    return pes;
}

std::size_t MpegTsKlvMuxer::emitTsPacket(std::uint16_t pid,
                                         bool pusi,
                                         const std::uint8_t* payload,
                                         std::size_t payloadSize,
                                         std::optional<std::uint64_t> pcrUs) {
    std::vector<std::uint8_t> packet(kTsPacketSize, 0xFFU);
    packet[0] = kTsSyncByte;
    packet[1] = static_cast<std::uint8_t>((pusi ? 0x40U : 0x00U) | ((pid >> 8U) & 0x1FU));
    packet[2] = static_cast<std::uint8_t>(pid & 0xFFU);

    const bool hasPcr = pcrUs.has_value();
    const bool needsAdaptation = hasPcr || (payloadSize < 184U);

    if (!needsAdaptation) {
        // Payload only (AFC = 01)
        packet[3] = static_cast<std::uint8_t>(0x10U | (nextCc(pid) & 0x0FU));
        std::copy_n(payload, 184U, packet.begin() + 4);
    } else {
        // Adaptation field + payload (AFC = 11)
        packet[3] = static_cast<std::uint8_t>(0x30U | (nextCc(pid) & 0x0FU));

        const std::size_t afTotalSpace = 184U - payloadSize;
        if (afTotalSpace == 1U) {
            // Special case: 1 byte of adaptation field -> length byte = 0
            packet[4] = 0x00U;
            std::copy_n(payload, payloadSize, packet.begin() + 5);
        } else {
            const auto afLength = static_cast<std::uint8_t>(afTotalSpace - 1U);
            packet[4] = afLength;

            std::uint8_t flags = 0x00U;
            if (pusi) {
                flags |= 0x40U; // Random access indicator
            }
            if (hasPcr) {
                flags |= 0x10U; // PCR flag
            }
            packet[5] = flags;

            std::size_t afOffset = 6U;
            if (hasPcr) {
                const std::uint64_t pcrBase = ((*pcrUs * 90ULL) / 1000ULL) & 0x1FFFFFFFFULL;
                const auto pcrExt = static_cast<std::uint16_t>(((*pcrUs * 27ULL) % 300ULL) & 0x1FFULL);

                packet[afOffset]     = static_cast<std::uint8_t>((pcrBase >> 25U) & 0xFFU);
                packet[afOffset + 1] = static_cast<std::uint8_t>((pcrBase >> 17U) & 0xFFU);
                packet[afOffset + 2] = static_cast<std::uint8_t>((pcrBase >> 9U) & 0xFFU);
                packet[afOffset + 3] = static_cast<std::uint8_t>((pcrBase >> 1U) & 0xFFU);
                packet[afOffset + 4] = static_cast<std::uint8_t>(((pcrBase & 1U) << 7U) | 0x7EU | ((pcrExt >> 8U) & 1U));
                packet[afOffset + 5] = static_cast<std::uint8_t>(pcrExt & 0xFFU);
                afOffset += 6U;
            }

            // Remainder of adaptation field is filled with 0xFF stuffing
            const std::size_t payloadOffset = 5U + static_cast<std::size_t>(afLength);
            while (afOffset < payloadOffset) {
                packet[afOffset++] = 0xFFU;
            }

            if (payload != nullptr && payloadSize > 0U) {
                std::copy_n(payload, payloadSize, packet.begin() + static_cast<std::ptrdiff_t>(payloadOffset));
            }
        }
    }

    dispatchPacket(packet);
    return 1U;
}

std::size_t MpegTsKlvMuxer::emitPsiTables() {
    const auto pat = buildPatPacket();
    dispatchPacket(pat);

    const auto pmt = buildPmtPacket();
    dispatchPacket(pmt);

    return 2U;
}

std::size_t MpegTsKlvMuxer::muxKlvPacket(const std::uint8_t* klvData,
                                         std::size_t size,
                                         std::optional<std::uint64_t> timestampUs) {
    if (klvData == nullptr || size == 0U) {
        return 0U;
    }

    std::size_t emittedCount = 0U;

    // Periodic PAT / PMT emission
    if (m_packetCounter % m_config.patPmtPeriodPackets == 0U) {
        emittedCount += emitPsiTables();
    }
    m_packetCounter++;

    // Determine whether to generate PCR
    std::optional<std::uint64_t> pcrUs = std::nullopt;
    if (timestampUs.has_value()) {
        const std::uint64_t intervalUs = static_cast<std::uint64_t>(m_config.pcrIntervalMs) * 1000ULL;
        if (m_lastPcrTimestampUs == 0U || *timestampUs >= m_lastPcrTimestampUs + intervalUs) {
            pcrUs = timestampUs;
            m_lastPcrTimestampUs = *timestampUs;
        }
    }

    const auto pes = buildPesPacket(klvData, size, timestampUs);
    if (pes.empty()) {
        return emittedCount;
    }

    std::size_t pesOffset = 0U;
    bool isFirst = true;

    while (pesOffset < pes.size()) {
        const std::size_t remaining = pes.size() - pesOffset;
        const bool includePcr = isFirst && pcrUs.has_value();
        const std::size_t maxPayload = includePcr ? (184U - 8U) : 184U;
        const std::size_t chunkSize = std::min(remaining, maxPayload);

        emittedCount += emitTsPacket(m_config.metadataPid,
                                     isFirst,
                                     pes.data() + pesOffset,
                                     chunkSize,
                                     includePcr ? pcrUs : std::nullopt);

        pesOffset += chunkSize;
        isFirst = false;
    }

    return emittedCount;
}

std::size_t MpegTsKlvMuxer::muxMessage(const UasDatalinkMessage& message) {
    const auto klvBytes = KlvEncoder::encode(message);
    return muxKlvPacket(klvBytes.data(), klvBytes.size(), message.precisionTimeStampUs);
}

std::vector<std::uint8_t> MpegTsKlvMuxer::muxToBuffer(const std::uint8_t* klvData,
                                                      std::size_t size,
                                                      std::optional<std::uint64_t> timestampUs) {
    std::vector<std::uint8_t> output;
    const auto originalCallback = m_callback;

    setPacketCallback([&output](const std::uint8_t* packet, std::size_t sz) {
        output.insert(output.end(), packet, packet + sz);
    });

    static_cast<void>(muxKlvPacket(klvData, size, timestampUs));
    setPacketCallback(originalCallback);

    return output;
}

std::vector<std::uint8_t> MpegTsKlvMuxer::muxMessageToBuffer(const UasDatalinkMessage& message) {
    const auto klvBytes = KlvEncoder::encode(message);
    return muxToBuffer(klvBytes.data(), klvBytes.size(), message.precisionTimeStampUs);
}

} // namespace Klv
