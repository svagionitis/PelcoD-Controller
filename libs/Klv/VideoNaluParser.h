#pragma once

/// @file VideoNaluParser.h
/// @brief Zero-copy H.264 / H.265 Annex B NAL unit and Access Unit parser.

#include "VideoTypes.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace Klv {

/// @class VideoNaluParser
/// @brief Parses Annex B H.264 (AVC) and H.265 (HEVC) byte streams into Access Units.
class VideoNaluParser {
public:
    /// @brief Callback invoked when a complete Access Unit is assembled.
    using AccessUnitCallback = std::function<void(const VideoAccessUnit& au)>;

    /// @brief Constructs parser for designated video codec.
    /// @param[in] codec Video codec format.
    explicit VideoNaluParser(VideoCodec codec = VideoCodec::H264) noexcept;

    /// @brief Configures output Access Unit callback.
    /// @param[in] callback Function invoked when an AU is complete.
    void setUnitCallback(AccessUnitCallback callback);

    /// @brief Gets active codec.
    /// @return Active VideoCodec.
    [[nodiscard]] VideoCodec codec() const noexcept;

    /// @brief Sets active codec.
    /// @param[in] codec New VideoCodec.
    void setCodec(VideoCodec codec) noexcept;

    /// @brief Parses a standalone buffer containing a single complete frame Access Unit.
    /// @param[in] data Pointer to raw Annex B byte buffer.
    /// @param[in] size Size of buffer in bytes.
    /// @param[in] codec Codec format.
    /// @param[in] ptsUs Presentation timestamp in microseconds.
    /// @param[in] dtsUs Optional decode timestamp in microseconds.
    /// @return Assembled VideoAccessUnit.
    [[nodiscard]] static VideoAccessUnit parseAccessUnit(const std::uint8_t* data,
                                                         std::size_t size,
                                                         VideoCodec codec,
                                                         std::uint64_t ptsUs,
                                                         std::optional<std::uint64_t> dtsUs = std::nullopt);

    /// @brief Pushes a streaming chunk of Annex B bytes into internal parser buffer.
    /// @param[in] data Pointer to byte stream.
    /// @param[in] size Number of bytes.
    /// @param[in] ptsUs Timestamp to assign to completed AUs.
    /// @return Number of complete Access Units emitted.
    [[nodiscard]] std::size_t pushChunk(const std::uint8_t* data,
                                        std::size_t size,
                                        std::uint64_t ptsUs);

    /// @brief Flushes pending bytes in buffer as an Access Unit if available.
    /// @return Number of Access Units emitted (0 or 1).
    [[nodiscard]] std::size_t flush();

    /// @brief Resets parser state and clears accumulation buffer.
    void reset() noexcept;

    /// @brief Scans for the next 3-byte (0x000001) or 4-byte (0x00000001) start code.
    /// @param[in] data Buffer pointer.
    /// @param[in] size Buffer size.
    /// @param[in] offset Start scan offset.
    /// @param[out] prefixLen Length of detected start code (3 or 4).
    /// @return Offset of start code prefix, or size if not found.
    [[nodiscard]] static std::size_t findStartCode(const std::uint8_t* data,
                                                   std::size_t size,
                                                   std::size_t offset,
                                                   std::size_t& prefixLen) noexcept;

    /// @brief Extracts raw NAL unit type from NAL header byte(s).
    /// @param[in] headerByte First byte of NAL unit.
    /// @param[in] codec Video codec.
    /// @return NAL unit type integer.
    [[nodiscard]] static std::uint8_t extractNaluType(std::uint8_t headerByte,
                                                      VideoCodec codec) noexcept;

    /// @brief Determines whether a given NAL unit type is a Keyframe / IDR.
    /// @param[in] naluType NAL unit type.
    /// @param[in] codec Video codec.
    /// @return True if keyframe.
    [[nodiscard]] static bool isKeyframeType(std::uint8_t naluType,
                                             VideoCodec codec) noexcept;

private:
    VideoCodec m_codec { VideoCodec::H264 };
    AccessUnitCallback m_callback {};
    std::vector<std::uint8_t> m_buffer {};
    std::uint64_t m_currentPtsUs { 0U };

    [[nodiscard]] static VideoSliceType parseH264Slice(const std::uint8_t* payload,
                                                       std::size_t size) noexcept;

    [[nodiscard]] static std::vector<NaluDescriptor> extractNalus(const std::uint8_t* data,
                                                                 std::size_t size,
                                                                 VideoCodec codec);

    [[nodiscard]] static bool isAuBoundary(std::uint8_t naluType,
                                           VideoCodec codec,
                                           const std::uint8_t* payload,
                                           std::size_t size) noexcept;
};

} // namespace Klv
