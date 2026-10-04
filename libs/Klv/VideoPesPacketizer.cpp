#include "VideoPesPacketizer.h"
#include <algorithm>

namespace Klv {

VideoPesPacketizer::VideoPesPacketizer(bool prependAud) noexcept
    : m_prependAud(prependAud) {
}

bool VideoPesPacketizer::hasLeadingAud(const std::uint8_t* data,
                                       std::size_t size,
                                       VideoCodec codec) noexcept {
    if (data == nullptr || size < 5U) {
        return false;
    }

    std::size_t naluOffset = 0U;
    if (data[0] == 0x00U && data[1] == 0x00U && data[2] == 0x01U) {
        naluOffset = 3U;
    } else if (size >= 4U && data[0] == 0x00U && data[1] == 0x00U && data[2] == 0x00U && data[3] == 0x01U) {
        naluOffset = 4U;
    } else {
        return false;
    }

    if (codec == VideoCodec::H264) {
        const std::uint8_t type = data[naluOffset] & 0x1FU;
        return type == static_cast<std::uint8_t>(NaluTypeH264::Aud);
    }
    if (codec == VideoCodec::H265) {
        const std::uint8_t type = (data[naluOffset] >> 1U) & 0x3FU;
        return type == static_cast<std::uint8_t>(NaluTypeH265::AudNut);
    }
    return false;
}

std::vector<std::uint8_t> VideoPesPacketizer::generateAud(VideoCodec codec,
                                                          VideoSliceType sliceType) {
    std::vector<std::uint8_t> aud;
    if (codec == VideoCodec::H264) {
        // Start code: 00 00 00 01, NAL 0x09
        aud = { 0x00U, 0x00U, 0x00U, 0x01U, 0x09U };
        std::uint8_t picType = 7U; // Any slice type default
        if (sliceType == VideoSliceType::I) {
            picType = 0U; // I slices only
        } else if (sliceType == VideoSliceType::P) {
            picType = 1U; // I and P slices
        } else if (sliceType == VideoSliceType::B) {
            picType = 2U; // I, P, B slices
        }
        aud.push_back(static_cast<std::uint8_t>((picType << 5U) | 0x10U));
    } else if (codec == VideoCodec::H265) {
        // Start code: 00 00 00 01, NAL 0x46 (35 << 1), temporal_id_plus1 = 1 (0x01)
        aud = { 0x00U, 0x00U, 0x00U, 0x01U, 0x46U, 0x01U };
        std::uint8_t picType = 2U; // I, P, B
        if (sliceType == VideoSliceType::I) {
            picType = 0U; // I only
        } else if (sliceType == VideoSliceType::P) {
            picType = 1U; // I and P
        }
        aud.push_back(static_cast<std::uint8_t>((picType << 5U) | 0x10U));
    }
    return aud;
}

std::vector<std::uint8_t> VideoPesPacketizer::buildPesPacket(const std::uint8_t* data,
                                                             std::size_t size,
                                                             std::uint64_t ptsUs,
                                                             std::optional<std::uint64_t> dtsUs,
                                                             bool /*isKeyframe*/) const {
    std::vector<std::uint8_t> pes;
    if (data == nullptr || size == 0U) {
        return pes;
    }

    const bool hasDts = dtsUs.has_value();
    const std::size_t pesHeaderDataLen = hasDts ? 10U : 5U;
    const std::size_t pesPayloadLen = 3U + pesHeaderDataLen + size;

    // Unbounded PES length (0x0000) for video streams if exceeds 65535 bytes per ISO/IEC 13818-1
    const std::uint16_t lengthField = (pesPayloadLen <= 0xFFFFU)
                                          ? static_cast<std::uint16_t>(pesPayloadLen)
                                          : 0x0000U;

    pes.reserve(9U + pesHeaderDataLen + size);

    // 1. Packet start code prefix (00 00 01)
    pes.push_back(0x00U);
    pes.push_back(0x00U);
    pes.push_back(0x01U);

    // 2. Stream ID (0xE0 = Video stream 0)
    pes.push_back(kVideoStreamId);

    // 3. PES packet length
    pes.push_back(static_cast<std::uint8_t>((lengthField >> 8U) & 0xFFU));
    pes.push_back(static_cast<std::uint8_t>(lengthField & 0xFFU));

    // 4. PES header flags:
    // Byte 6: 10 (marker) | 00 (scrambling) | 0 (priority) | 1 (data_alignment) | 0 (orig) = 0x84
    pes.push_back(0x84U);

    // Byte 7: PTS_DTS flags (0xC0 if PTS+DTS, 0x80 if PTS only)
    pes.push_back(hasDts ? 0xC0U : 0x80U);

    // Byte 8: PES_header_data_length
    pes.push_back(static_cast<std::uint8_t>(pesHeaderDataLen));

    // 5. Presentation Time Stamp (PTS)
    const std::uint64_t pts = toPts90kHz(ptsUs);
    const std::uint8_t ptsPrefix = hasDts ? 0x31U : 0x21U;

    pes.push_back(static_cast<std::uint8_t>(ptsPrefix | (((pts >> 30U) & 0x07U) << 1U)));
    pes.push_back(static_cast<std::uint8_t>((pts >> 22U) & 0xFFU));
    pes.push_back(static_cast<std::uint8_t>((((pts >> 15U) & 0x7FU) << 1U) | 0x01U));
    pes.push_back(static_cast<std::uint8_t>((pts >> 7U) & 0xFFU));
    pes.push_back(static_cast<std::uint8_t>(((pts & 0x7FU) << 1U) | 0x01U));

    // 6. Decode Time Stamp (DTS) if present
    if (hasDts) {
        const std::uint64_t dts = toPts90kHz(*dtsUs);
        pes.push_back(static_cast<std::uint8_t>(0x11U | (((dts >> 30U) & 0x07U) << 1U)));
        pes.push_back(static_cast<std::uint8_t>((dts >> 22U) & 0xFFU));
        pes.push_back(static_cast<std::uint8_t>((((dts >> 15U) & 0x7FU) << 1U) | 0x01U));
        pes.push_back(static_cast<std::uint8_t>((dts >> 7U) & 0xFFU));
        pes.push_back(static_cast<std::uint8_t>(((dts & 0x7FU) << 1U) | 0x01U));
    }

    // 7. Video payload
    pes.insert(pes.end(), data, data + size);
    return pes;
}

std::vector<std::uint8_t> VideoPesPacketizer::packetize(const VideoAccessUnit& au) const {
    if (au.data.empty()) {
        return {};
    }

    const bool needsAud = m_prependAud && !hasLeadingAud(au.data.data(), au.data.size(), au.codec);
    if (!needsAud) {
        return buildPesPacket(au.data.data(),
                              au.data.size(),
                              au.ptsUs,
                              au.hasDts ? std::make_optional(au.dtsUs) : std::nullopt,
                              au.isKeyframe);
    }

    const auto aud = generateAud(au.codec, au.primarySliceType);
    std::vector<std::uint8_t> merged;
    merged.reserve(aud.size() + au.data.size());
    merged.insert(merged.end(), aud.begin(), aud.end());
    merged.insert(merged.end(), au.data.begin(), au.data.end());

    return buildPesPacket(merged.data(),
                          merged.size(),
                          au.ptsUs,
                          au.hasDts ? std::make_optional(au.dtsUs) : std::nullopt,
                          au.isKeyframe);
}

} // namespace Klv
