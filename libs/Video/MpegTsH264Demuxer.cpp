/// @file MpegTsH264Demuxer.cpp
/// @brief Zero-allocation streaming MPEG-2 Transport Stream demuxer implementation.

#include "MpegTsH264Demuxer.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <glog/logging.h>

namespace Video {

namespace {

    constexpr std::size_t kMaxStreamBufferSize { 65536U };
    constexpr std::size_t kMaxPesBufferSize { 4U * 1024U * 1024U };

    /// @brief Helper to find the start of Annex-B start codes.
    /// @return Pair of {offset_to_prefix, prefix_length (3 or 4)}, or nullopt if none.
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> findStartCode(
        const std::uint8_t* data, std::size_t size, std::size_t startOffset)
    {
        if ((data == nullptr) || (size < 3U) || (startOffset >= size)) {
            return std::nullopt;
        }

        for (std::size_t i { startOffset }; i + 2U < size; ++i) {
            if ((data[i] == 0x00U) && (data[i + 1U] == 0x00U)) {
                if (data[i + 2U] == 0x01U) {
                    if ((i > 0U) && (data[i - 1U] == 0x00U)) {
                        return std::make_pair(i - 1U, 4U);
                    }
                    return std::make_pair(i, 3U);
                }
            }
        }
        return std::nullopt;
    }

} // namespace

MpegTsH264Demuxer::MpegTsH264Demuxer(NaluCallback callback)
    : m_naluCallback(std::move(callback))
{
    m_pesBuffer.reserve(131072U);
}

std::size_t MpegTsH264Demuxer::processData(const std::uint8_t* data, std::size_t size)
{
    if ((data == nullptr) || (size == 0U)) {
        return 0U;
    }

    std::size_t dispatched { 0U };

    if (m_streamBuffer.empty()) {
        std::size_t offset { 0U };
        while (offset < size) {
            if (data[offset] != kTsSyncByte) {
                // Scan forward for next sync byte
                const auto* pNext
                    = static_cast<const std::uint8_t*>(std::memchr(data + offset, kTsSyncByte, size - offset));
                if (pNext == nullptr) {
                    break;
                }
                offset = static_cast<std::size_t>(pNext - data);
            }

            if (offset + kTsPacketSize <= size) {
                dispatched += processPacket(data + offset);
                offset += kTsPacketSize;
            } else {
                // Less than a full packet left; buffer it
                m_streamBuffer.assign(data + offset, data + size);
                break;
            }
        }
    } else {
        m_streamBuffer.insert(m_streamBuffer.end(), data, data + size);
        std::size_t offset { 0U };
        while (offset + kTsPacketSize <= m_streamBuffer.size()) {
            if (m_streamBuffer[offset] != kTsSyncByte) {
                ++offset;
                continue;
            }

            dispatched += processPacket(m_streamBuffer.data() + offset);
            offset += kTsPacketSize;
        }

        if (offset > 0U) {
            m_streamBuffer.erase(m_streamBuffer.begin(), m_streamBuffer.begin() + static_cast<std::ptrdiff_t>(offset));
        }

        if (m_streamBuffer.size() > kMaxStreamBufferSize) {
            LOG(WARNING) << "MpegTsH264Demuxer: Stream buffer exceeded " << kMaxStreamBufferSize
                         << " bytes without sync; clearing buffer.";
            m_streamBuffer.clear();
        }
    }

    return dispatched;
}

std::size_t MpegTsH264Demuxer::processPacket(const std::uint8_t* packet)
{
    if (packet == nullptr) {
        return 0U;
    }

    if (packet[0] != kTsSyncByte) {
        return 0U;
    }

    // Byte 1: TEI (bit 7), PUSI (bit 6), Transport Priority (bit 5), PID upper 5 bits
    const bool tei { (packet[1] & 0x80U) != 0U };
    if (tei) {
        return 0U; // Corrupt packet indicated by demodulator/network
    }

    const bool pusi { (packet[1] & 0x40U) != 0U };
    const std::uint16_t pid { static_cast<std::uint16_t>(
        ((static_cast<std::uint16_t>(packet[1] & 0x1FU)) << 8) | packet[2]) };

    // Byte 3: TSC (bits 7-6), AFC (bits 5-4), Continuity Counter (bits 3-0)
    const std::uint8_t tsc { static_cast<std::uint8_t>((packet[3] & 0xC0U) >> 6) };
    if (tsc != 0U) {
        return 0U; // Scrambled packets are not supported
    }

    const std::uint8_t afc { static_cast<std::uint8_t>((packet[3] & 0x30U) >> 4) };
    std::size_t headerLength { 4U };

    if (afc == 0x00U) {
        return 0U; // Reserved
    }
    if ((afc == 0x02U) || (afc == 0x03U)) {
        const std::size_t adaptationLength { static_cast<std::size_t>(packet[4]) };
        headerLength += 1U + adaptationLength;
        if (headerLength > kTsPacketSize) {
            return 0U; // Invalid adaptation field length
        }
        if (afc == 0x02U) {
            return 0U; // Adaptation field only, no payload
        }
    }

    if (headerLength >= kTsPacketSize) {
        return 0U;
    }

    const std::uint8_t* payload { packet + headerLength };
    const std::size_t payloadSize { kTsPacketSize - headerLength };

    if (pid == 0x0000U) {
        parsePat(payload, payloadSize);
        return 0U;
    }

    if (m_pmtPid.has_value() && (pid == m_pmtPid.value())) {
        parsePmt(payload, payloadSize);
        return 0U;
    }

    if (m_videoPid.has_value() && (pid == m_videoPid.value())) {
        return handlePesData(payload, payloadSize, pusi);
    }

    return 0U;
}

void MpegTsH264Demuxer::parsePat(const std::uint8_t* payload, std::size_t size)
{
    if ((payload == nullptr) || (size < 4U)) {
        return;
    }

    // Pointer field skip if present (pushes into section start)
    const std::size_t pointerField { static_cast<std::size_t>(payload[0]) };
    const std::size_t sectionOffset { 1U + pointerField };
    if (sectionOffset + 8U > size) {
        return;
    }

    const std::uint8_t* sec { payload + sectionOffset };
    const std::size_t secSize { size - sectionOffset };

    const std::uint8_t tableId { sec[0] };
    if (tableId != 0x00U) {
        return; // Not a PAT table
    }

    const std::size_t sectionLength { static_cast<std::size_t>(
        ((static_cast<std::size_t>(sec[1] & 0x0FU)) << 8) | sec[2]) };
    if (sectionLength + 3U > secSize) {
        return;
    }

    const std::uint8_t currentNextIndicator { static_cast<std::uint8_t>(sec[5] & 0x01U) };
    if (currentNextIndicator == 0U) {
        return; // Next PAT, not applicable yet
    }

    // Program loop starts at byte 8 and ends before 4-byte CRC32
    if (sectionLength < 9U) {
        return;
    }
    const std::size_t loopEnd { std::min(secSize, 3U + sectionLength - 4U) };

    for (std::size_t i { 8U }; i + 4U <= loopEnd; i += 4U) {
        const std::uint16_t programNumber { static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(sec[i]) << 8) | sec[i + 1U]) };
        const std::uint16_t pmtPid { static_cast<std::uint16_t>(
            ((static_cast<std::uint16_t>(sec[i + 2U] & 0x1FU)) << 8) | sec[i + 3U]) };

        if (programNumber != 0U) { // Skip NIT (program 0)
            m_pmtPid = pmtPid;
            break;
        }
    }
}

void MpegTsH264Demuxer::parsePmt(const std::uint8_t* payload, std::size_t size)
{
    if ((payload == nullptr) || (size < 4U)) {
        return;
    }

    const std::size_t pointerField { static_cast<std::size_t>(payload[0]) };
    const std::size_t sectionOffset { 1U + pointerField };
    if (sectionOffset + 12U > size) {
        return;
    }

    const std::uint8_t* sec { payload + sectionOffset };
    const std::size_t secSize { size - sectionOffset };

    const std::uint8_t tableId { sec[0] };
    if (tableId != 0x02U) {
        return; // Not a PMT table
    }

    const std::size_t sectionLength { static_cast<std::size_t>(
        ((static_cast<std::size_t>(sec[1] & 0x0FU)) << 8) | sec[2]) };
    if (sectionLength + 3U > secSize) {
        return;
    }

    const std::uint8_t currentNextIndicator { static_cast<std::uint8_t>(sec[5] & 0x01U) };
    if (currentNextIndicator == 0U) {
        return;
    }

    const std::size_t programInfoLength { static_cast<std::size_t>(
        ((static_cast<std::size_t>(sec[10] & 0x0FU)) << 8) | sec[11]) };
    std::size_t esOffset { 12U + programInfoLength };
    const std::size_t loopEnd { std::min(secSize, 3U + sectionLength - 4U) };

    while (esOffset + 5U <= loopEnd) {
        const std::uint8_t streamType { sec[esOffset] };
        const std::uint16_t elementaryPid { static_cast<std::uint16_t>(
            ((static_cast<std::uint16_t>(sec[esOffset + 1U] & 0x1FU)) << 8) | sec[esOffset + 2U]) };
        const std::size_t esInfoLength { static_cast<std::size_t>(
            ((static_cast<std::size_t>(sec[esOffset + 3U] & 0x0FU)) << 8) | sec[esOffset + 4U]) };

        if (streamType == kStreamTypeH264) {
            m_videoPid = elementaryPid;
            break;
        }

        esOffset += 5U + esInfoLength;
    }
}

std::size_t MpegTsH264Demuxer::handlePesData(const std::uint8_t* payload, std::size_t size, bool pusi)
{
    if ((payload == nullptr) || (size == 0U)) {
        return 0U;
    }

    std::size_t dispatched { 0U };

    if (pusi) {
        if (!m_pesBuffer.empty()) {
            dispatched += dispatchNalus();
            m_pesBuffer.clear();
        }

        // Validate PES start code prefix (0x000001)
        if ((size >= 9U) && (payload[0] == 0x00U) && (payload[1] == 0x00U) && (payload[2] == 0x01U)) {
            const std::uint8_t ptsDtsFlags { static_cast<std::uint8_t>((payload[7] & 0xC0U) >> 6) };
            const std::size_t headerDataLength { static_cast<std::size_t>(payload[8]) };

            if (((ptsDtsFlags & 0x02U) != 0U) && (size >= 14U)) {
                const std::uint64_t pts { ((static_cast<std::uint64_t>(payload[9] & 0x0EU)) << 29)
                    | ((static_cast<std::uint64_t>(payload[10])) << 22)
                    | ((static_cast<std::uint64_t>(payload[11] & 0xFEU)) << 14)
                    | ((static_cast<std::uint64_t>(payload[12])) << 7)
                    | ((static_cast<std::uint64_t>(payload[13] & 0xFEU)) >> 1) };
                m_currentPts = static_cast<double>(pts) / 90000.0;
            }

            const std::size_t pesHeaderSize { 9U + headerDataLength };
            if (size > pesHeaderSize) {
                m_pesBuffer.assign(payload + pesHeaderSize, payload + size);
            }
        }
    } else {
        if (!m_pesBuffer.empty()) {
            if (m_pesBuffer.size() + size <= kMaxPesBufferSize) {
                m_pesBuffer.insert(m_pesBuffer.end(), payload, payload + size);
            } else {
                LOG(WARNING) << "MpegTsH264Demuxer: PES buffer exceeded " << kMaxPesBufferSize
                             << " bytes without PUSI boundary; resetting.";
                m_pesBuffer.clear();
            }
        }
    }

    return dispatched;
}

std::size_t MpegTsH264Demuxer::dispatchNalus()
{
    if (m_pesBuffer.empty() || !m_naluCallback) {
        return 0U;
    }

    std::size_t count { 0U };
    std::size_t searchPos { 0U };

    auto firstCode = findStartCode(m_pesBuffer.data(), m_pesBuffer.size(), searchPos);
    if (!firstCode.has_value()) {
        // No Annex-B start code found; prepend standard 4-byte 0x00000001 start code
        std::vector<std::uint8_t> framedNalu;
        framedNalu.reserve(m_pesBuffer.size() + 4U);
        framedNalu.push_back(0x00U);
        framedNalu.push_back(0x00U);
        framedNalu.push_back(0x00U);
        framedNalu.push_back(0x01U);
        framedNalu.insert(framedNalu.end(), m_pesBuffer.begin(), m_pesBuffer.end());
        m_naluCallback(framedNalu.data(), framedNalu.size(), m_currentPts);
        return 1U;
    }

    std::size_t currentOffset { firstCode->first };
    searchPos = currentOffset + firstCode->second;

    while (searchPos < m_pesBuffer.size()) {
        auto nextCode = findStartCode(m_pesBuffer.data(), m_pesBuffer.size(), searchPos);
        if (!nextCode.has_value()) {
            break;
        }

        const std::size_t nextOffset { nextCode->first };
        if (nextOffset > currentOffset) {
            const std::size_t naluSize { nextOffset - currentOffset };
            m_naluCallback(m_pesBuffer.data() + currentOffset, naluSize, m_currentPts);
            ++count;
        }

        currentOffset = nextOffset;
        searchPos = currentOffset + nextCode->second;
    }

    if (currentOffset < m_pesBuffer.size()) {
        const std::size_t naluSize { m_pesBuffer.size() - currentOffset };
        m_naluCallback(m_pesBuffer.data() + currentOffset, naluSize, m_currentPts);
        ++count;
    }

    return count;
}

void MpegTsH264Demuxer::flush()
{
    if (!m_pesBuffer.empty()) {
        dispatchNalus();
        m_pesBuffer.clear();
    }
}

void MpegTsH264Demuxer::reset() noexcept
{
    m_streamBuffer.clear();
    m_pesBuffer.clear();
    m_pmtPid = std::nullopt;
    m_videoPid = std::nullopt;
    m_currentPts = 0.0;
}

void MpegTsH264Demuxer::setVideoPid(std::uint16_t pid) noexcept
{
    m_videoPid = pid;
}

std::optional<std::uint16_t> MpegTsH264Demuxer::videoPid() const noexcept
{
    return m_videoPid;
}

std::optional<std::uint16_t> MpegTsH264Demuxer::pmtPid() const noexcept
{
    return m_pmtPid;
}

} // namespace Video
