#include "VideoNaluParser.h"
#include <algorithm>
#include <cstring>

namespace Klv {

namespace {

class BitReader {
public:
    BitReader(const std::uint8_t* data, std::size_t size) noexcept
        : m_data(data)
        , m_size(size) {
    }

    [[nodiscard]] bool readBit(std::uint8_t& outBit) noexcept {
        if (m_byteOffset >= m_size) {
            return false;
        }
        outBit = static_cast<std::uint8_t>((m_data[m_byteOffset] >> (7U - m_bitOffset)) & 0x01U);
        m_bitOffset++;
        if (m_bitOffset == 8U) {
            m_bitOffset = 0U;
            m_byteOffset++;
            // Skip emulation prevention byte (0x000003)
            if (m_byteOffset >= 2U && m_byteOffset < m_size &&
                m_data[m_byteOffset - 2U] == 0x00U &&
                m_data[m_byteOffset - 1U] == 0x00U &&
                m_data[m_byteOffset] == 0x03U) {
                m_byteOffset++;
            }
        }
        return true;
    }

    [[nodiscard]] bool readBits(std::size_t numBits, std::uint32_t& outVal) noexcept {
        if (numBits > 32U) {
            return false;
        }
        outVal = 0U;
        for (std::size_t i = 0U; i < numBits; ++i) {
            std::uint8_t b = 0U;
            if (!readBit(b)) {
                return false;
            }
            outVal = (outVal << 1U) | static_cast<std::uint32_t>(b);
        }
        return true;
    }

    [[nodiscard]] bool readUe(std::uint32_t& outVal) noexcept {
        std::size_t leadingZeroBits = 0U;
        std::uint8_t bit = 0U;
        while (readBit(bit) && bit == 0U) {
            leadingZeroBits++;
            if (leadingZeroBits > 31U) {
                return false;
            }
        }
        if (bit == 0U) {
            return false;
        }
        if (leadingZeroBits == 0U) {
            outVal = 0U;
            return true;
        }
        std::uint32_t suffix = 0U;
        if (!readBits(leadingZeroBits, suffix)) {
            return false;
        }
        outVal = (1U << leadingZeroBits) - 1U + suffix;
        return true;
    }

private:
    const std::uint8_t* m_data;
    std::size_t m_size;
    std::size_t m_byteOffset { 0U };
    std::size_t m_bitOffset { 0U };
};

} // namespace

VideoNaluParser::VideoNaluParser(VideoCodec codec) noexcept
    : m_codec(codec) {
}

void VideoNaluParser::setUnitCallback(AccessUnitCallback callback) {
    m_callback = std::move(callback);
}

VideoCodec VideoNaluParser::codec() const noexcept {
    return m_codec;
}

void VideoNaluParser::setCodec(VideoCodec codec) noexcept {
    m_codec = codec;
}

std::size_t VideoNaluParser::findStartCode(const std::uint8_t* data,
                                           std::size_t size,
                                           std::size_t offset,
                                           std::size_t& prefixLen) noexcept {
    prefixLen = 0U;
    if (data == nullptr || size < 3U || offset > size - 3U) {
        return size;
    }

    for (std::size_t i = offset; i <= size - 3U; ++i) {
        if (data[i] == 0x00U && data[i + 1U] == 0x00U && data[i + 2U] == 0x01U) {
            if (i > 0U && data[i - 1U] == 0x00U) {
                prefixLen = 4U;
                return i - 1U;
            }
            prefixLen = 3U;
            return i;
        }
    }
    return size;
}

std::uint8_t VideoNaluParser::extractNaluType(std::uint8_t headerByte,
                                              VideoCodec codec) noexcept {
    if (codec == VideoCodec::H264) {
        return headerByte & 0x1FU;
    }
    if (codec == VideoCodec::H265) {
        return (headerByte >> 1U) & 0x3FU;
    }
    return 0U;
}

bool VideoNaluParser::isKeyframeType(std::uint8_t naluType,
                                     VideoCodec codec) noexcept {
    if (codec == VideoCodec::H264) {
        return naluType == static_cast<std::uint8_t>(NaluTypeH264::IdrSlice);
    }
    if (codec == VideoCodec::H265) {
        return naluType == static_cast<std::uint8_t>(NaluTypeH265::IdrWRadl) ||
               naluType == static_cast<std::uint8_t>(NaluTypeH265::IdrNLp) ||
               naluType == static_cast<std::uint8_t>(NaluTypeH265::CraNut);
    }
    return false;
}

VideoSliceType VideoNaluParser::parseH264Slice(const std::uint8_t* payload,
                                               std::size_t size) noexcept {
    if (payload == nullptr || size < 2U) {
        return VideoSliceType::Unknown;
    }
    // Skip 1-byte NAL unit header
    BitReader reader(payload + 1U, size - 1U);
    std::uint32_t firstMb = 0U;
    if (!reader.readUe(firstMb)) {
        return VideoSliceType::Unknown;
    }
    std::uint32_t sliceTypeUe = 0U;
    if (!reader.readUe(sliceTypeUe)) {
        return VideoSliceType::Unknown;
    }
    const std::uint32_t modType = sliceTypeUe % 5U;
    switch (modType) {
    case 0U: return VideoSliceType::P;
    case 1U: return VideoSliceType::B;
    case 2U: return VideoSliceType::I;
    case 3U: return VideoSliceType::P;
    case 4U: return VideoSliceType::I;
    default: break;
    }
    return VideoSliceType::Unknown;
}

std::vector<NaluDescriptor> VideoNaluParser::extractNalus(const std::uint8_t* data,
                                                         std::size_t size,
                                                         VideoCodec codec) {
    std::vector<NaluDescriptor> result;
    if (data == nullptr || size < 4U) {
        return result;
    }

    std::size_t offset = 0U;
    std::size_t prefixLen = 0U;
    std::size_t currentStart = findStartCode(data, size, offset, prefixLen);

    while (currentStart < size) {
        const std::size_t naluDataOffset = currentStart + prefixLen;
        if (naluDataOffset >= size) {
            break;
        }

        std::size_t nextPrefixLen = 0U;
        const std::size_t nextStart = findStartCode(data, size, naluDataOffset, nextPrefixLen);
        const std::size_t naluSize = nextStart - naluDataOffset;

        if (naluSize > 0U) {
            const std::uint8_t type = extractNaluType(data[naluDataOffset], codec);
            const bool key = isKeyframeType(type, codec);
            result.push_back({ naluDataOffset, naluSize, type, key });
        }

        currentStart = nextStart;
        prefixLen = nextPrefixLen;
    }

    return result;
}

VideoAccessUnit VideoNaluParser::parseAccessUnit(const std::uint8_t* data,
                                                 std::size_t size,
                                                 VideoCodec codec,
                                                 std::uint64_t ptsUs,
                                                 std::optional<std::uint64_t> dtsUs) {
    VideoAccessUnit au;
    au.codec = codec;
    au.ptsUs = ptsUs;
    au.dtsUs = dtsUs.value_or(ptsUs);
    au.hasDts = dtsUs.has_value();

    if (data == nullptr || size == 0U) {
        return au;
    }

    au.data.assign(data, data + size);
    au.nalus = extractNalus(au.data.data(), au.data.size(), codec);

    for (const auto& nalu : au.nalus) {
        if (nalu.isKeyframe) {
            au.isKeyframe = true;
            au.primarySliceType = VideoSliceType::I;
            break;
        }
        if (codec == VideoCodec::H264 &&
            nalu.naluType == static_cast<std::uint8_t>(NaluTypeH264::NonIdrSlice)) {
            const auto st = parseH264Slice(au.data.data() + nalu.offset, nalu.size);
            if (st != VideoSliceType::Unknown && au.primarySliceType == VideoSliceType::Unknown) {
                au.primarySliceType = st;
            }
        }
    }

    if (au.primarySliceType == VideoSliceType::Unknown && !au.nalus.empty()) {
        au.primarySliceType = au.isKeyframe ? VideoSliceType::I : VideoSliceType::P;
    }

    return au;
}

bool VideoNaluParser::isAuBoundary(std::uint8_t naluType,
                                   VideoCodec codec,
                                   const std::uint8_t* payload,
                                   std::size_t size) noexcept {
    if (codec == VideoCodec::H264) {
        if (naluType == static_cast<std::uint8_t>(NaluTypeH264::Aud)) {
            return true;
        }
        if (naluType == static_cast<std::uint8_t>(NaluTypeH264::Sps) ||
            naluType == static_cast<std::uint8_t>(NaluTypeH264::Pps)) {
            return true;
        }
        if (naluType == static_cast<std::uint8_t>(NaluTypeH264::IdrSlice) ||
            naluType == static_cast<std::uint8_t>(NaluTypeH264::NonIdrSlice)) {
            if (payload != nullptr && size >= 2U) {
                BitReader reader(payload + 1U, size - 1U);
                std::uint32_t firstMb = 0U;
                if (reader.readUe(firstMb) && firstMb == 0U) {
                    return true;
                }
            }
        }
    } else if (codec == VideoCodec::H265) {
        if (naluType == static_cast<std::uint8_t>(NaluTypeH265::AudNut)) {
            return true;
        }
        if (naluType == static_cast<std::uint8_t>(NaluTypeH265::VpsNut) ||
            naluType == static_cast<std::uint8_t>(NaluTypeH265::SpsNut) ||
            naluType == static_cast<std::uint8_t>(NaluTypeH265::PpsNut)) {
            return true;
        }
        if ((naluType <= 9U || (naluType >= 16U && naluType <= 21U)) && payload != nullptr && size >= 3U) {
            // First bit of slice header after 2-byte NALU header
            BitReader reader(payload + 2U, size - 2U);
            std::uint8_t firstSlice = 0U;
            if (reader.readBit(firstSlice) && firstSlice == 1U) {
                return true;
            }
        }
    }
    return false;
}

std::size_t VideoNaluParser::pushChunk(const std::uint8_t* data,
                                       std::size_t size,
                                       std::uint64_t ptsUs) {
    if (data == nullptr || size == 0U) {
        return 0U;
    }

    if (m_buffer.empty()) {
        m_currentPtsUs = ptsUs;
    }

    m_buffer.insert(m_buffer.end(), data, data + size);
    std::size_t emittedCount = 0U;

    // Scan for Access Unit boundaries within m_buffer
    std::size_t offset = 0U;
    std::size_t prefixLen = 0U;
    std::size_t firstStart = findStartCode(m_buffer.data(), m_buffer.size(), offset, prefixLen);

    if (firstStart == m_buffer.size()) {
        return 0U;
    }

    std::size_t prevStart = firstStart;
    std::size_t prevPrefix = prefixLen;
    std::size_t scanPos = firstStart + prefixLen;
    bool hasSliceInCurrentAu = false;

    while (scanPos < m_buffer.size()) {
        std::size_t curPrefix = 0U;
        const std::size_t curStart = findStartCode(m_buffer.data(), m_buffer.size(), scanPos, curPrefix);
        if (curStart == m_buffer.size()) {
            break;
        }

        const std::size_t naluDataOffset = curStart + curPrefix;
        if (naluDataOffset < m_buffer.size()) {
            const std::uint8_t naluType = extractNaluType(m_buffer[naluDataOffset], m_codec);
            const std::uint8_t* payload = m_buffer.data() + naluDataOffset;
            const std::size_t remSize = m_buffer.size() - naluDataOffset;

            const bool isBoundary = isAuBoundary(naluType, m_codec, payload, remSize);
            const bool isSlice = (m_codec == VideoCodec::H264 && (naluType == 1U || naluType == 5U)) ||
                                 (m_codec == VideoCodec::H265 && (naluType <= 9U || (naluType >= 16U && naluType <= 21U)));

            if (isBoundary && hasSliceInCurrentAu) {
                // We found the start of the next AU at curStart!
                const auto au = parseAccessUnit(m_buffer.data(), curStart, m_codec, m_currentPtsUs);
                if (m_callback) {
                    m_callback(au);
                }
                emittedCount++;

                // Shift buffer
                m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(curStart));
                m_currentPtsUs = ptsUs;
                hasSliceInCurrentAu = isSlice;
                scanPos = curPrefix;
                prevStart = 0U;
                prevPrefix = curPrefix;
                continue;
            }

            if (isSlice) {
                hasSliceInCurrentAu = true;
            }
        }

        prevStart = curStart;
        prevPrefix = curPrefix;
        scanPos = curStart + curPrefix;
    }

    return emittedCount;
}

std::size_t VideoNaluParser::flush() {
    if (m_buffer.empty()) {
        return 0U;
    }
    const auto au = parseAccessUnit(m_buffer.data(), m_buffer.size(), m_codec, m_currentPtsUs);
    m_buffer.clear();
    if (!au.nalus.empty()) {
        if (m_callback) {
            m_callback(au);
        }
        return 1U;
    }
    return 0U;
}

void VideoNaluParser::reset() noexcept {
    m_buffer.clear();
    m_currentPtsUs = 0U;
}

} // namespace Klv
