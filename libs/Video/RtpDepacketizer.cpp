/// @file RtpDepacketizer.cpp
/// @brief Zero-transcode RFC 6184 (H.264) and RFC 7798 (H.265) RTP depacketizer implementation.

#include "RtpDepacketizer.h"

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <string_view>
#include <vector>

#include "Klv/VideoNaluParser.h"
#include "Klv/VideoTypes.h"

namespace Video {

namespace {

    [[nodiscard]] int decodeBase64Char(char c) noexcept
    {
        if ((c >= 'A') && (c <= 'Z')) {
            return c - 'A';
        }
        if ((c >= 'a') && (c <= 'z')) {
            return (c - 'a') + 26;
        }
        if ((c >= '0') && (c <= '9')) {
            return (c - '0') + 52;
        }
        if (c == '+') {
            return 62;
        }
        if (c == '/') {
            return 63;
        }
        return -1;
    }

    [[nodiscard]] std::vector<std::uint8_t> decodeBase64(std::string_view base64Text)
    {
        std::vector<std::uint8_t> result { };
        result.reserve((base64Text.size() * 3U) / 4U);

        std::uint32_t buffer { 0U };
        int bitsCollected { 0 };

        for (const char c : base64Text) {
            if ((c == ' ') || (c == '\r') || (c == '\n') || (c == '\t')) {
                continue;
            }
            if (c == '=') {
                break;
            }

            const int val { decodeBase64Char(c) };
            if (val < 0) {
                continue;
            }

            buffer = (buffer << 6U) | static_cast<std::uint32_t>(val);
            bitsCollected += 6;

            if (bitsCollected >= 8) {
                bitsCollected -= 8;
                result.push_back(
                    static_cast<std::uint8_t>((buffer >> static_cast<unsigned int>(bitsCollected)) & 0xFFU));
            }
        }
        return result;
    }

} // namespace

RtpDepacketizer::RtpDepacketizer(Klv::VideoCodec codec) noexcept
    : m_codec(codec)
{
    m_auBuffer.reserve(262144U); // 256 KB initial AU reservation
    m_fuBuffer.reserve(131072U); // 128 KB initial FU reservation
}

void RtpDepacketizer::setUnitCallback(AccessUnitCallback callback)
{
    m_unitCallback = std::move(callback);
}

void RtpDepacketizer::setChunkCallback(RawChunkCallback callback)
{
    m_chunkCallback = std::move(callback);
}

void RtpDepacketizer::setCodec(Klv::VideoCodec codec) noexcept
{
    m_codec = codec;
}

[[nodiscard]] Klv::VideoCodec RtpDepacketizer::codec() const noexcept
{
    return m_codec;
}

void RtpDepacketizer::setParameterSets(const std::vector<std::vector<std::uint8_t>>& paramSets)
{
    m_paramSets = paramSets;
}

[[nodiscard]] std::vector<std::vector<std::uint8_t>> RtpDepacketizer::parseSprop(std::string_view sprop)
{
    std::vector<std::vector<std::uint8_t>> nalus { };
    std::size_t start { 0U };

    while (start < sprop.size()) {
        const std::size_t commaPos { sprop.find(',', start) };
        const std::string_view token { (commaPos == std::string_view::npos) ? sprop.substr(start)
                                                                            : sprop.substr(start, commaPos - start) };
        if (!token.empty()) {
            std::vector<std::uint8_t> decoded { decodeBase64(token) };
            if (!decoded.empty()) {
                nalus.push_back(std::move(decoded));
            }
        }
        if (commaPos == std::string_view::npos) {
            break;
        }
        start = commaPos + 1U;
    }

    return nalus;
}

bool RtpDepacketizer::processPacket(const std::uint8_t* packet, std::size_t size)
{
    if ((packet == nullptr) || (size < 12U)) {
        return false;
    }

    // RFC 3550 RTP Header Parsing
    const auto version = static_cast<std::uint8_t>((packet[0] >> 6U) & 0x03U);
    if (version != 2U) {
        return false;
    }

    const bool padding { (packet[0] & 0x20U) != 0U };
    const bool extension { (packet[0] & 0x10U) != 0U };
    const auto csrcCount = static_cast<std::size_t>(packet[0] & 0x0FU);
    const bool marker { (packet[1] & 0x80U) != 0U };

    const auto seq = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(packet[2]) << 8U) | static_cast<std::uint16_t>(packet[3]));

    const auto rtpTimestamp = static_cast<std::uint32_t>((static_cast<std::uint32_t>(packet[4]) << 24U)
        | (static_cast<std::uint32_t>(packet[5]) << 16U) | (static_cast<std::uint32_t>(packet[6]) << 8U)
        | static_cast<std::uint32_t>(packet[7]));

    std::size_t headerSize { 12U + (csrcCount * 4U) };
    if (size < headerSize) {
        return false;
    }

    if (extension) {
        if (size < (headerSize + 4U)) {
            return false;
        }
        const auto extWords = static_cast<std::size_t>((static_cast<std::size_t>(packet[headerSize + 2U]) << 8U)
            | static_cast<std::size_t>(packet[headerSize + 3U]));
        const std::size_t extBytes { 4U + (extWords * 4U) };
        headerSize += extBytes;
        if (size < headerSize) {
            return false;
        }
    }

    std::size_t paddingLen { 0U };
    if (padding) {
        paddingLen = static_cast<std::size_t>(packet[size - 1U]);
        if (size < (headerSize + paddingLen)) {
            return false;
        }
    }

    const std::size_t payloadLen { size - headerSize - paddingLen };
    if (payloadLen == 0U) {
        return true; // Keepalive or empty RTP packet
    }

    const std::uint8_t* payload { packet + headerSize };

    // Sequence continuity check & loss detection
    if (m_hasLastSequence) {
        const auto diff = static_cast<std::int16_t>(seq - m_lastSequence);
        if (diff > 1) {
            const auto lost = static_cast<std::size_t>(diff - 1);
            m_packetLossCount += lost;
            if (m_fuInProgress) {
                // Fragmented NAL unit corrupted by packet loss; discard partial fragment
                m_fuBuffer.clear();
                m_fuInProgress = false;
            }
        }
    }
    m_lastSequence = seq;
    m_hasLastSequence = true;
    m_packetsReceived += 1U;

    // Check if RTP timestamp changed -> previous AU has ended
    if (m_hasBaseTimestamp && (rtpTimestamp != m_lastRtpTimestamp) && !m_auBuffer.empty()) {
        emitAccessUnit(m_currentPtsUs);
    }

    m_currentPtsUs = unwrapTimestamp(rtpTimestamp);
    m_lastRtpTimestamp = rtpTimestamp;

    bool parseOk { false };
    if (m_codec == Klv::VideoCodec::H264) {
        parseOk = parseH264(payload, payloadLen, marker);
    } else if (m_codec == Klv::VideoCodec::H265) {
        parseOk = parseH265(payload, payloadLen, marker);
    }

    if (marker && !m_auBuffer.empty()) {
        emitAccessUnit(m_currentPtsUs);
    }

    return parseOk;
}

bool RtpDepacketizer::parseH264(const std::uint8_t* payload, std::size_t size, bool /*marker*/)
{
    if ((payload == nullptr) || (size < 1U)) {
        return false;
    }

    const auto nalType = static_cast<std::uint8_t>(payload[0] & 0x1FU);

    if ((nalType >= 1U) && (nalType <= 23U)) {
        // Single NAL unit packet
        appendNalu(payload, size);
        return true;
    }

    if (nalType == 24U) {
        // STAP-A aggregation packet
        std::size_t offset { 1U };
        while ((offset + 2U) <= size) {
            const auto naluLen = static_cast<std::size_t>(
                (static_cast<std::size_t>(payload[offset]) << 8U) | static_cast<std::size_t>(payload[offset + 1U]));
            offset += 2U;
            if ((offset + naluLen) > size) {
                return false; // Malformed STAP-A payload
            }
            appendNalu(payload + offset, naluLen);
            offset += naluLen;
        }
        return true;
    }

    if (nalType == 28U) {
        // FU-A fragmentation unit
        if (size < 2U) {
            return false;
        }
        const bool isStart { (payload[1] & 0x80U) != 0U };
        const bool isEnd { (payload[1] & 0x40U) != 0U };
        const auto origNalType = static_cast<std::uint8_t>(payload[1] & 0x1FU);
        const auto reconstructedHeader = static_cast<std::uint8_t>((payload[0] & 0xE0U) | origNalType);

        if (isStart) {
            m_fuBuffer.clear();
            m_fuBuffer.push_back(reconstructedHeader);
            if (size > 2U) {
                m_fuBuffer.insert(m_fuBuffer.end(), payload + 2U, payload + size);
            }
            m_fuInProgress = true;
        } else if (m_fuInProgress) {
            if (size > 2U) {
                m_fuBuffer.insert(m_fuBuffer.end(), payload + 2U, payload + size);
            }
            if (isEnd) {
                appendNalu(m_fuBuffer.data(), m_fuBuffer.size());
                m_fuBuffer.clear();
                m_fuInProgress = false;
            }
        }
        return true;
    }

    return false; // Unsupported or reserved NAL type
}

bool RtpDepacketizer::parseH265(const std::uint8_t* payload, std::size_t size, bool /*marker*/)
{
    if ((payload == nullptr) || (size < 2U)) {
        return false;
    }

    const auto nalType = static_cast<std::uint8_t>((payload[0] >> 1U) & 0x3FU);

    if (nalType <= 47U) {
        // Single NAL unit packet
        appendNalu(payload, size);
        return true;
    }

    if (nalType == 48U) {
        // AP aggregation packet
        std::size_t offset { 2U };
        while ((offset + 2U) <= size) {
            const auto naluLen = static_cast<std::size_t>(
                (static_cast<std::size_t>(payload[offset]) << 8U) | static_cast<std::size_t>(payload[offset + 1U]));
            offset += 2U;
            if ((offset + naluLen) > size) {
                return false; // Malformed AP payload
            }
            appendNalu(payload + offset, naluLen);
            offset += naluLen;
        }
        return true;
    }

    if (nalType == 49U) {
        // FU fragmentation unit
        if (size < 3U) {
            return false;
        }
        const bool isStart { (payload[2] & 0x80U) != 0U };
        const bool isEnd { (payload[2] & 0x40U) != 0U };
        const auto fuType = static_cast<std::uint8_t>(payload[2] & 0x3FU);

        const auto h0
            = static_cast<std::uint8_t>((payload[0] & 0x81U) | static_cast<std::uint8_t>((fuType & 0x3FU) << 1U));
        const std::uint8_t h1 { payload[1] };

        if (isStart) {
            m_fuBuffer.clear();
            m_fuBuffer.push_back(h0);
            m_fuBuffer.push_back(h1);
            if (size > 3U) {
                m_fuBuffer.insert(m_fuBuffer.end(), payload + 3U, payload + size);
            }
            m_fuInProgress = true;
        } else if (m_fuInProgress) {
            if (size > 3U) {
                m_fuBuffer.insert(m_fuBuffer.end(), payload + 3U, payload + size);
            }
            if (isEnd) {
                appendNalu(m_fuBuffer.data(), m_fuBuffer.size());
                m_fuBuffer.clear();
                m_fuInProgress = false;
            }
        }
        return true;
    }

    return false;
}

void RtpDepacketizer::appendNalu(const std::uint8_t* nalu, std::size_t size)
{
    if ((nalu == nullptr) || (size == 0U)) {
        return;
    }

    // Annex B 4-byte start code prefix: 00 00 00 01
    static constexpr std::uint8_t kStartCode[4] = { 0x00U, 0x00U, 0x00U, 0x01U };
    m_auBuffer.insert(m_auBuffer.end(), kStartCode, kStartCode + 4U);
    m_auBuffer.insert(m_auBuffer.end(), nalu, nalu + size);
}

void RtpDepacketizer::emitAccessUnit(std::uint64_t ptsUs)
{
    if (m_auBuffer.empty()) {
        return;
    }

    Klv::VideoAccessUnit au { Klv::VideoNaluParser::parseAccessUnit(
        m_auBuffer.data(), m_auBuffer.size(), m_codec, ptsUs) };

    // If this is a keyframe and parameter sets were supplied out-of-band via SDP,
    // inject them if not already present in the stream.
    if (au.isKeyframe && !m_paramSets.empty()) {
        bool hasParamSet { false };
        for (const auto& nalu : au.nalus) {
            if (m_codec == Klv::VideoCodec::H264) {
                if ((nalu.naluType == static_cast<std::uint8_t>(Klv::NaluTypeH264::Sps))
                    || (nalu.naluType == static_cast<std::uint8_t>(Klv::NaluTypeH264::Pps))) {
                    hasParamSet = true;
                    break;
                }
            } else if (m_codec == Klv::VideoCodec::H265) {
                if ((nalu.naluType == static_cast<std::uint8_t>(Klv::NaluTypeH265::VpsNut))
                    || (nalu.naluType == static_cast<std::uint8_t>(Klv::NaluTypeH265::SpsNut))
                    || (nalu.naluType == static_cast<std::uint8_t>(Klv::NaluTypeH265::PpsNut))) {
                    hasParamSet = true;
                    break;
                }
            }
        }

        if (!hasParamSet) {
            std::vector<std::uint8_t> prefixedBuffer { };
            static constexpr std::uint8_t kStartCode[4] = { 0x00U, 0x00U, 0x00U, 0x01U };
            for (const auto& ps : m_paramSets) {
                prefixedBuffer.insert(prefixedBuffer.end(), kStartCode, kStartCode + 4U);
                prefixedBuffer.insert(prefixedBuffer.end(), ps.begin(), ps.end());
            }
            prefixedBuffer.insert(prefixedBuffer.end(), m_auBuffer.begin(), m_auBuffer.end());
            m_auBuffer = std::move(prefixedBuffer);
            au = Klv::VideoNaluParser::parseAccessUnit(m_auBuffer.data(), m_auBuffer.size(), m_codec, ptsUs);
        }
    }

    if (m_chunkCallback) {
        m_chunkCallback(m_auBuffer.data(), m_auBuffer.size(), ptsUs);
    }

    if (m_unitCallback) {
        m_unitCallback(au);
    }

    m_auBuffer.clear();
}

void RtpDepacketizer::flush()
{
    if (!m_auBuffer.empty()) {
        emitAccessUnit(m_currentPtsUs);
    }
}

void RtpDepacketizer::reset() noexcept
{
    m_auBuffer.clear();
    m_fuBuffer.clear();
    m_fuInProgress = false;
    m_hasLastSequence = false;
    m_lastSequence = 0U;
    m_hasBaseTimestamp = false;
    m_lastRtpTimestamp = 0U;
    m_unwrappedTimestamp = 0U;
    m_baseRtpTimestamp = 0U;
    m_basePtsUs = 0U;
    m_currentPtsUs = 0U;
    m_packetLossCount = 0U;
    m_packetsReceived = 0U;
}

[[nodiscard]] std::size_t RtpDepacketizer::packetLossCount() const noexcept
{
    return m_packetLossCount;
}

[[nodiscard]] std::size_t RtpDepacketizer::packetsReceived() const noexcept
{
    return m_packetsReceived;
}

[[nodiscard]] std::uint64_t RtpDepacketizer::unwrapTimestamp(std::uint32_t rtpTs) noexcept
{
    if (!m_hasBaseTimestamp) {
        m_hasBaseTimestamp = true;
        m_baseRtpTimestamp = rtpTs;
        m_lastRtpTimestamp = rtpTs;
        m_unwrappedTimestamp = rtpTs;

        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        m_basePtsUs = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
        return m_basePtsUs;
    }

    const auto delta = static_cast<std::int32_t>(rtpTs - m_lastRtpTimestamp);
    m_unwrappedTimestamp = static_cast<std::uint64_t>(static_cast<std::int64_t>(m_unwrappedTimestamp) + delta);

    const auto elapsedTicks = static_cast<std::uint64_t>(m_unwrappedTimestamp - m_baseRtpTimestamp);
    const std::uint64_t elapsedUs { (elapsedTicks * 1000000ULL) / 90000ULL };
    return m_basePtsUs + elapsedUs;
}

} // namespace Video
