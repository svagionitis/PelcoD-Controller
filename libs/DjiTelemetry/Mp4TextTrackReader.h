#pragma once

/// @file Mp4TextTrackReader.h
/// @brief Minimal, hardened ISO-BMFF (MP4/MOV) reader for timed-text (tx3g) tracks.

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Dji {

/// @struct Mp4Sample
/// @brief Location and presentation time of one text sample.
struct Mp4Sample {
    std::uint64_t offset { 0U }; ///< Absolute file offset of the sample.
    std::uint32_t size { 0U }; ///< Sample size in bytes (including the 2-byte length prefix).
    double timeSec { 0.0 }; ///< Presentation time in seconds (edit list applied).
};

/// @class Mp4TextTrackReader
/// @brief Locates the first tx3g text track in an MP4 file and reads its samples.
/// @details Only the 'moov' box is loaded into memory (capped at 256 MiB); sample payloads
///          are read on demand with seeks, so multi-GB recordings are never loaded whole.
///          'moov' may appear before or after 'mdat'. Supports 32/64-bit box sizes,
///          stco/co64, multi-entry stts/stsc, fixed or per-sample stsz and a single
///          edit-list segment (optionally preceded by an empty edit).
///
///          Every size, count and offset read from the file is bounds-checked against its
///          parent box and the file size (SEI CERT INT30-C/INT32-C/ARR30-C). Malformed input
///          makes open() return false; nothing throws.
/// @note Not thread-safe: readText() moves the underlying file position.
class Mp4TextTrackReader {
public:
    /// @brief Opens a file and indexes its first tx3g track.
    /// @details Any previous state is discarded first, so a failed open leaves the reader empty.
    /// @param[in] path File system path.
    /// @return True if a tx3g track with at least one valid sample was found.
    [[nodiscard]] bool open(std::string_view path);

    /// @brief Returns the number of indexed samples.
    /// @return Sample count (0 if not open).
    [[nodiscard]] std::size_t sampleCount() const noexcept;

    /// @brief Returns the sample table.
    /// @return Samples in decode order.
    [[nodiscard]] const std::vector<Mp4Sample>& samples() const noexcept;

    /// @brief Reads the text of one sample.
    /// @details Strips the 2-byte big-endian length prefix and ignores trailing modifier boxes.
    /// @param[in] index Sample index.
    /// @return Sample text.
    /// @retval std::nullopt Index out of range, read failure or inconsistent length prefix.
    [[nodiscard]] std::optional<std::string> readText(std::size_t index);

    /// @brief Returns the text track media timescale.
    /// @return Ticks per second (0 if not open).
    [[nodiscard]] std::uint32_t timescale() const noexcept;

    /// @brief Returns the movie creation time from 'mvhd'.
    /// @return Microseconds since 1970-01-01 UTC.
    /// @retval std::nullopt Not open, or creation time missing/before 1970.
    [[nodiscard]] std::optional<std::uint64_t> creationUtcUs() const noexcept;

    /// @brief Returns the encoder / device string from moov/udta/meta/ilst/(c)too.
    /// @return Encoder name, empty if absent.
    [[nodiscard]] const std::string& encoderName() const noexcept;

    /// @brief Returns the width/height ratio of the first video track ('tkhd').
    /// @return Aspect ratio.
    /// @retval std::nullopt No video track with non-zero dimensions.
    [[nodiscard]] std::optional<double> videoAspect() const noexcept;

private:
    /// @brief Clears all state and closes the file.
    void reset() noexcept;

    /// @brief Locates 'moov' among the top-level boxes and loads its body.
    /// @param[out] moov Receives the moov body bytes.
    /// @return True on success.
    [[nodiscard]] bool loadMoov(std::vector<std::uint8_t>& moov);

    std::ifstream m_file {};
    std::uint64_t m_fileSize { 0U };
    std::uint32_t m_timescale { 0U };
    std::vector<Mp4Sample> m_samples {};
    std::optional<std::uint64_t> m_creationUs {};
    std::string m_encoder {};
    std::optional<double> m_videoAspect {};
};

} // namespace Dji
