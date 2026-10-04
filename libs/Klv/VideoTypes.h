#pragma once

/// @file VideoTypes.h
/// @brief Video codec, NAL unit, and Access Unit structures for STANAG 4609 multiplexing.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Klv {

/// @enum VideoCodec
/// @brief Elementary stream video compression standard.
enum class VideoCodec : std::uint8_t {
    None = 0U,
    H264 = 1U, ///< ITU-T H.264 / ISO/IEC 14496-10 (AVC) - Stream Type 0x1B
    H265 = 2U  ///< ITU-T H.265 / ISO/IEC 23008-2 (HEVC) - Stream Type 0x24
};

/// @enum VideoSliceType
/// @brief High-level video frame coding slice type.
enum class VideoSliceType : std::uint8_t {
    Unknown = 0U,
    I = 1U,  ///< Intra-coded (keyframe)
    P = 2U,  ///< Predicted
    B = 3U   ///< Bi-directionally predicted
};

/// @enum NaluTypeH264
/// @brief Common H.264 / AVC NAL unit types per ITU-T H.264 Table 7-1.
enum class NaluTypeH264 : std::uint8_t {
    Unspecified     = 0U,
    NonIdrSlice     = 1U,
    DataPartitionA  = 2U,
    DataPartitionB  = 3U,
    DataPartitionC  = 4U,
    IdrSlice        = 5U,
    Sei             = 6U,
    Sps             = 7U,
    Pps             = 8U,
    Aud             = 9U,
    EndOfSequence   = 10U,
    EndOfStream     = 11U,
    FillerData      = 12U,
    SpsExtension    = 13U,
    PrefixNalUnit   = 14U,
    SubsetSps       = 15U
};

/// @enum NaluTypeH265
/// @brief Common H.265 / HEVC NAL unit types per ITU-T H.265 Table 7-1.
enum class NaluTypeH265 : std::uint8_t {
    TrailN          = 0U,
    TrailR          = 1U,
    TsaN            = 2U,
    TsaR            = 3U,
    StsaN           = 4U,
    StsaR           = 5U,
    RadlN           = 6U,
    RadlR           = 7U,
    RaslN           = 8U,
    RaslR           = 9U,
    IdrWRadl        = 19U,
    IdrNLp          = 20U,
    CraNut          = 21U,
    VpsNut          = 32U,
    SpsNut          = 33U,
    PpsNut          = 34U,
    AudNut          = 35U,
    EosNut          = 36U,
    EobNut          = 37U,
    FdNut           = 38U,
    PrefixSeiNut    = 39U,
    SuffixSeiNut    = 40U
};

/// @struct NaluDescriptor
/// @brief Non-owning descriptor of an extracted NAL unit within a buffer.
struct NaluDescriptor {
    std::size_t offset { 0U };     ///< Byte offset of NAL header within AU data
    std::size_t size { 0U };       ///< Size of NAL unit excluding start code
    std::uint8_t naluType { 0U };  ///< Raw NAL unit type
    bool isKeyframe { false };     ///< True if IDR/IRAP
};

/// @struct VideoAccessUnit
/// @brief A complete video access unit (one frame/field) containing one or more NALUs.
struct VideoAccessUnit {
    VideoCodec codec { VideoCodec::H264 };
    std::vector<std::uint8_t> data {};
    std::vector<NaluDescriptor> nalus {};
    std::uint64_t ptsUs { 0U };
    std::uint64_t dtsUs { 0U };
    bool hasDts { false };
    bool isKeyframe { false };
    VideoSliceType primarySliceType { VideoSliceType::Unknown };
};

} // namespace Klv
