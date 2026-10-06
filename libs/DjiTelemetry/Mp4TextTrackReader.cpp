/// @file Mp4TextTrackReader.cpp
/// @brief Implementation of the hardened ISO-BMFF tx3g track reader.

#include "Mp4TextTrackReader.h"

#include <array>
#include <cstddef>
#include <limits>
#include <utility>

namespace Dji {

namespace {

    /// @brief Byte buffer alias for the in-memory moov body.
    using Buffer = std::vector<std::uint8_t>;

    constexpr std::uint64_t kMaxMoovBytes { 256ULL * 1024ULL * 1024ULL };
    constexpr std::uint32_t kMaxEntries { 10000000U };
    constexpr std::uint32_t kMaxSampleBytes { 65536U };
    constexpr std::uint64_t kEpoch1904To1970 { 2082844800ULL };
    constexpr std::uint64_t kUsPerSecond { 1000000ULL };
    constexpr std::size_t kBoxHeader { 8U };
    constexpr std::size_t kLargeHeader { 16U };
    constexpr std::size_t kFullBoxHeader { 4U };

    /// @brief Builds a big-endian four-character code.
    /// @param[in] a First byte.
    /// @param[in] b Second byte.
    /// @param[in] c Third byte.
    /// @param[in] d Fourth byte.
    /// @return Packed 32-bit code.
    [[nodiscard]] constexpr std::uint32_t fourcc(
        std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) noexcept
    {
        return (static_cast<std::uint32_t>(a) << 24U) | (static_cast<std::uint32_t>(b) << 16U)
            | (static_cast<std::uint32_t>(c) << 8U) | static_cast<std::uint32_t>(d);
    }

    /// @brief Builds a four-character code from ASCII characters.
    /// @param[in] s Exactly four ASCII characters.
    /// @return Packed 32-bit code.
    [[nodiscard]] constexpr std::uint32_t fourcc(const char (&s)[5]) noexcept
    {
        return fourcc(static_cast<std::uint8_t>(s[0]), static_cast<std::uint8_t>(s[1]), static_cast<std::uint8_t>(s[2]),
            static_cast<std::uint8_t>(s[3]));
    }

    constexpr std::uint32_t kFtyp { fourcc("ftyp") };
    constexpr std::uint32_t kMoov { fourcc("moov") };
    constexpr std::uint32_t kMvhd { fourcc("mvhd") };
    constexpr std::uint32_t kTrak { fourcc("trak") };
    constexpr std::uint32_t kTkhd { fourcc("tkhd") };
    constexpr std::uint32_t kEdts { fourcc("edts") };
    constexpr std::uint32_t kElst { fourcc("elst") };
    constexpr std::uint32_t kMdia { fourcc("mdia") };
    constexpr std::uint32_t kMdhd { fourcc("mdhd") };
    constexpr std::uint32_t kHdlr { fourcc("hdlr") };
    constexpr std::uint32_t kMinf { fourcc("minf") };
    constexpr std::uint32_t kStbl { fourcc("stbl") };
    constexpr std::uint32_t kStsd { fourcc("stsd") };
    constexpr std::uint32_t kStts { fourcc("stts") };
    constexpr std::uint32_t kStsc { fourcc("stsc") };
    constexpr std::uint32_t kStsz { fourcc("stsz") };
    constexpr std::uint32_t kStco { fourcc("stco") };
    constexpr std::uint32_t kCo64 { fourcc("co64") };
    constexpr std::uint32_t kUdta { fourcc("udta") };
    constexpr std::uint32_t kMeta { fourcc("meta") };
    constexpr std::uint32_t kIlst { fourcc("ilst") };
    constexpr std::uint32_t kData { fourcc("data") };
    constexpr std::uint32_t kTx3g { fourcc("tx3g") };
    constexpr std::uint32_t kVide { fourcc("vide") };
    constexpr std::uint32_t kSbtl { fourcc("sbtl") };
    constexpr std::uint32_t kText { fourcc("text") };
    constexpr std::uint32_t kSubt { fourcc("subt") };
    constexpr std::uint32_t kCopyTool { fourcc(
        0xA9U, static_cast<std::uint8_t>('t'), static_cast<std::uint8_t>('o'), static_cast<std::uint8_t>('o')) };

    /// @brief Decodes a big-endian 32-bit value from raw bytes.
    /// @param[in] p Pointer to at least 4 bytes.
    /// @return Decoded value.
    [[nodiscard]] std::uint32_t be32(const std::uint8_t* p) noexcept
    {
        return fourcc(p[0], p[1], p[2], p[3]);
    }

    /// @brief Reads a big-endian 32-bit value with bounds checking.
    /// @param[in] b Source buffer.
    /// @param[in] pos Byte offset.
    /// @param[out] out Decoded value.
    /// @return False if fewer than 4 bytes remain at @p pos.
    [[nodiscard]] bool readU32(const Buffer& b, std::size_t pos, std::uint32_t& out) noexcept
    {
        if ((pos > b.size()) || ((b.size() - pos) < 4U)) {
            return false;
        }
        out = be32(&b[pos]);
        return true;
    }

    /// @brief Reads a big-endian 64-bit value with bounds checking.
    /// @param[in] b Source buffer.
    /// @param[in] pos Byte offset.
    /// @param[out] out Decoded value.
    /// @return False if fewer than 8 bytes remain at @p pos.
    [[nodiscard]] bool readU64(const Buffer& b, std::size_t pos, std::uint64_t& out) noexcept
    {
        std::uint32_t hi { 0U };
        std::uint32_t lo { 0U };
        if (!readU32(b, pos, hi) || !readU32(b, pos + 4U, lo)) {
            return false;
        }
        out = (static_cast<std::uint64_t>(hi) << 32U) | static_cast<std::uint64_t>(lo);
        return true;
    }

    /// @brief Reads a full-box version byte.
    /// @param[in] b Source buffer.
    /// @param[in] pos Byte offset of the version/flags word.
    /// @param[out] version Version byte.
    /// @return False if out of bounds.
    [[nodiscard]] bool readVersion(const Buffer& b, std::size_t pos, std::uint8_t& version) noexcept
    {
        if (pos >= b.size()) {
            return false;
        }
        version = b[pos];
        return true;
    }

    /// @struct Box
    /// @brief A box located inside the moov buffer.
    struct Box {
        std::uint32_t type { 0U }; ///< Four-character code.
        std::size_t body { 0U }; ///< Offset of the first body byte.
        std::size_t size { 0U }; ///< Body size in bytes.
        std::size_t next { 0U }; ///< Offset of the following sibling.

        /// @brief Returns the offset one past the last body byte.
        /// @return End offset.
        [[nodiscard]] std::size_t end() const noexcept
        {
            return body + size;
        }
    };

    /// @brief Parses one box header within [pos, end).
    /// @param[in] b Source buffer.
    /// @param[in] pos Offset of the box header.
    /// @param[in] end Parent end offset.
    /// @param[out] out Parsed box.
    /// @return False if the header is truncated or the size escapes the parent.
    [[nodiscard]] bool parseBox(const Buffer& b, std::size_t pos, std::size_t end, Box& out) noexcept
    {
        if ((end > b.size()) || (pos > end) || ((end - pos) < kBoxHeader)) {
            return false;
        }
        const std::uint32_t size32 { be32(&b[pos]) };
        std::uint64_t size { size32 };
        std::size_t header { kBoxHeader };
        if (size32 == 1U) {
            if (!readU64(b, pos + kBoxHeader, size)) {
                return false;
            }
            header = kLargeHeader;
        } else if (size32 == 0U) {
            size = static_cast<std::uint64_t>(end - pos);
        } else {
            // 32-bit size already set.
        }
        if ((size < header) || (size > static_cast<std::uint64_t>(end - pos))) {
            return false;
        }
        out.type = be32(&b[pos + 4U]);
        out.body = pos + header;
        out.size = static_cast<std::size_t>(size) - header;
        out.next = pos + static_cast<std::size_t>(size);
        return true;
    }

    /// @brief Finds the first direct child box of a given type.
    /// @param[in] b Source buffer.
    /// @param[in] begin First child offset.
    /// @param[in] end Parent end offset.
    /// @param[in] type Wanted four-character code.
    /// @return The child box, or std::nullopt if absent or malformed.
    [[nodiscard]] std::optional<Box> findChild(const Buffer& b, std::size_t begin, std::size_t end, std::uint32_t type)
    {
        std::size_t pos { begin };
        Box box {};
        while ((pos < end) && parseBox(b, pos, end, box)) {
            if (box.type == type) {
                return box;
            }
            pos = box.next;
        }
        return std::nullopt;
    }

    /// @brief Finds a direct child of a parent box.
    /// @param[in] b Source buffer.
    /// @param[in] parent Parent box.
    /// @param[in] type Wanted four-character code.
    /// @return The child box, or std::nullopt.
    [[nodiscard]] std::optional<Box> child(const Buffer& b, const Box& parent, std::uint32_t type)
    {
        return findChild(b, parent.body, parent.end(), type);
    }

    /// @brief Follows a path of child box types from a parent.
    /// @param[in] b Source buffer.
    /// @param[in] parent Starting box.
    /// @param[in] path Child types to descend through, in order.
    /// @return The final box, or std::nullopt if any level is missing.
    template <std::size_t N>
    [[nodiscard]] std::optional<Box> descend(
        const Buffer& b, const Box& parent, const std::array<std::uint32_t, N>& path)
    {
        std::optional<Box> cur { parent };
        for (const std::uint32_t t : path) {
            cur = child(b, *cur, t);
            if (!cur.has_value()) {
                return std::nullopt;
            }
        }
        return cur;
    }

    /// @brief Converts a 32-bit two's-complement pattern to a signed value without UB.
    /// @param[in] u Raw bits.
    /// @return Signed value.
    [[nodiscard]] constexpr std::int64_t toSigned32(std::uint32_t u) noexcept
    {
        return (u >= 0x80000000U) ? -static_cast<std::int64_t>(0x100000000ULL - u) : static_cast<std::int64_t>(u);
    }

    /// @brief Converts a 64-bit two's-complement pattern to a signed value without UB.
    /// @param[in] u Raw bits.
    /// @return Signed value (INT64_MIN maps to INT64_MIN + 1).
    [[nodiscard]] constexpr std::int64_t toSigned64(std::uint64_t u) noexcept
    {
        constexpr std::uint64_t kSign { 0x8000000000000000ULL };
        if (u < kSign) {
            return static_cast<std::int64_t>(u);
        }
        const std::uint64_t mag { (~u) + 1U };
        return (mag >= kSign) ? (std::numeric_limits<std::int64_t>::min() + 1) : -static_cast<std::int64_t>(mag);
    }

    /// @struct EditInfo
    /// @brief Result of parsing an elst box.
    struct EditInfo {
        std::uint64_t emptyMovieUnits { 0U }; ///< Sum of leading empty edits (movie timescale).
        std::int64_t mediaTime { 0 }; ///< Media time of the first real edit (media timescale).
    };

    /// @brief Parses an elst box (leading empty edits + first media edit).
    /// @param[in] b Source buffer.
    /// @param[in] elst The elst box.
    /// @param[out] out Parsed edit information.
    /// @return False if the box is malformed.
    [[nodiscard]] bool parseElst(const Buffer& b, const Box& elst, EditInfo& out)
    {
        std::uint8_t version { 0U };
        std::uint32_t count { 0U };
        if (!readVersion(b, elst.body, version) || !readU32(b, elst.body + 4U, count) || (count > kMaxEntries)) {
            return false;
        }
        const std::size_t entrySize { (version == 1U) ? 20U : 12U };
        std::size_t pos { elst.body + 8U };
        for (std::uint32_t i { 0U }; i < count; ++i) {
            if ((pos > elst.end()) || ((elst.end() - pos) < entrySize)) {
                return false;
            }
            std::uint64_t duration { 0U };
            std::int64_t mediaTime { 0 };
            if (version == 1U) {
                std::uint64_t raw { 0U };
                if (!readU64(b, pos, duration) || !readU64(b, pos + 8U, raw)) {
                    return false;
                }
                mediaTime = toSigned64(raw);
            } else {
                std::uint32_t dur32 { 0U };
                std::uint32_t raw { 0U };
                if (!readU32(b, pos, dur32) || !readU32(b, pos + 4U, raw)) {
                    return false;
                }
                duration = dur32;
                mediaTime = toSigned32(raw);
            }
            if (mediaTime == -1) {
                out.emptyMovieUnits += duration;
            } else if (mediaTime >= 0) {
                out.mediaTime = mediaTime;
                return true;
            } else {
                return false;
            }
            pos += entrySize;
        }
        return true;
    }

    /// @struct StscEntry
    /// @brief One sample-to-chunk run.
    struct StscEntry {
        std::uint32_t firstChunk { 0U }; ///< 1-based first chunk of the run.
        std::uint32_t samplesPerChunk { 0U }; ///< Samples in each chunk of the run.
    };

    /// @brief Reads the entry count of a table full box and validates the payload length.
    /// @param[in] b Source buffer.
    /// @param[in] box Table box.
    /// @param[in] headerBytes Bytes between version/flags and the first entry (count included).
    /// @param[in] entryBytes Size of each entry.
    /// @param[out] count Entry count.
    /// @return False if the table does not fit inside the box.
    [[nodiscard]] bool tableCount(
        const Buffer& b, const Box& box, std::size_t headerBytes, std::size_t entryBytes, std::uint32_t& count)
    {
        if (!readU32(b, box.body + kFullBoxHeader + headerBytes - 4U, count) || (count > kMaxEntries)) {
            return false;
        }
        const std::size_t needed { kFullBoxHeader + headerBytes + (static_cast<std::size_t>(count) * entryBytes) };
        return needed <= box.size;
    }

    /// @class TableParser
    /// @brief Builds a Mp4Sample table from a text track's stbl.
    class TableParser {
    public:
        /// @brief Binds the parser to a moov buffer and file size.
        /// @param[in] b moov body.
        /// @param[in] fileSize Total file size for offset validation.
        TableParser(const Buffer& b, std::uint64_t fileSize) noexcept
            : m_b { b }
            , m_fileSize { fileSize }
        {
        }

        /// @brief Parses stsz/stco/co64/stsc/stts and produces timed samples.
        /// @param[in] stbl The stbl box.
        /// @param[in] timescale Media timescale (non-zero).
        /// @param[in] edit Edit-list information.
        /// @param[in] movieTimescale Movie timescale for empty edits.
        /// @param[out] out Produced samples.
        /// @return False if any table is malformed or a sample lies outside the file.
        [[nodiscard]] bool build(const Box& stbl, std::uint32_t timescale, const EditInfo& edit,
            std::uint32_t movieTimescale, std::vector<Mp4Sample>& out)
        {
            return readSizes(stbl) && readOffsets(stbl) && readStsc(stbl) && layout(out)
                && applyTimes(stbl, timescale, edit, movieTimescale, out);
        }

    private:
        [[nodiscard]] bool readSizes(const Box& stbl)
        {
            const auto stsz { child(m_b, stbl, kStsz) };
            if (!stsz.has_value() || !readU32(m_b, stsz->body + 4U, m_fixedSize)) {
                return false;
            }
            const std::size_t entryBytes { (m_fixedSize == 0U) ? 4U : 0U };
            if (!tableCount(m_b, *stsz, 8U, entryBytes, m_sampleCount)) {
                return false;
            }
            if (m_fixedSize == 0U) {
                m_sizes.resize(m_sampleCount);
                for (std::uint32_t i { 0U }; i < m_sampleCount; ++i) {
                    if (!readU32(m_b, stsz->body + 12U + (static_cast<std::size_t>(i) * 4U), m_sizes[i])) {
                        return false;
                    }
                }
            }
            return true;
        }

        [[nodiscard]] bool readOffsets(const Box& stbl)
        {
            auto co { child(m_b, stbl, kStco) };
            bool wide { false };
            if (!co.has_value()) {
                co = child(m_b, stbl, kCo64);
                wide = true;
            }
            std::uint32_t count { 0U };
            if (!co.has_value() || !tableCount(m_b, *co, 4U, wide ? 8U : 4U, count)) {
                return false;
            }
            m_offsets.resize(count);
            for (std::uint32_t i { 0U }; i < count; ++i) {
                const std::size_t pos { co->body + 8U + (static_cast<std::size_t>(i) * (wide ? 8U : 4U)) };
                if (wide) {
                    if (!readU64(m_b, pos, m_offsets[i])) {
                        return false;
                    }
                } else {
                    std::uint32_t off32 { 0U };
                    if (!readU32(m_b, pos, off32)) {
                        return false;
                    }
                    m_offsets[i] = off32;
                }
            }
            return true;
        }

        [[nodiscard]] bool readStsc(const Box& stbl)
        {
            const auto stsc { child(m_b, stbl, kStsc) };
            std::uint32_t count { 0U };
            if (!stsc.has_value() || !tableCount(m_b, *stsc, 4U, 12U, count) || (count == 0U)) {
                return false;
            }
            m_stsc.resize(count);
            std::uint32_t prevFirst { 0U };
            for (std::uint32_t i { 0U }; i < count; ++i) {
                const std::size_t pos { stsc->body + 8U + (static_cast<std::size_t>(i) * 12U) };
                StscEntry& e { m_stsc[i] };
                if (!readU32(m_b, pos, e.firstChunk) || !readU32(m_b, pos + 4U, e.samplesPerChunk)) {
                    return false;
                }
                if ((e.firstChunk <= prevFirst) || (e.samplesPerChunk == 0U)) {
                    return false;
                }
                prevFirst = e.firstChunk;
            }
            return true;
        }

        [[nodiscard]] bool layout(std::vector<Mp4Sample>& out) const
        {
            out.clear();
            out.reserve(m_sampleCount);
            const auto chunkCount { static_cast<std::uint32_t>(m_offsets.size()) };
            for (std::size_t e { 0U }; (e < m_stsc.size()) && (out.size() < m_sampleCount); ++e) {
                const std::uint32_t first { m_stsc[e].firstChunk };
                const std::uint32_t last { ((e + 1U) < m_stsc.size()) ? (m_stsc[e + 1U].firstChunk - 1U) : chunkCount };
                for (std::uint32_t chunk { first };
                     (chunk <= last) && (chunk <= chunkCount) && (out.size() < m_sampleCount); ++chunk) {
                    std::uint64_t offset { m_offsets[chunk - 1U] };
                    for (std::uint32_t k { 0U }; (k < m_stsc[e].samplesPerChunk) && (out.size() < m_sampleCount); ++k) {
                        const std::uint32_t size { (m_fixedSize != 0U) ? m_fixedSize : m_sizes[out.size()] };
                        if ((offset > m_fileSize) || (static_cast<std::uint64_t>(size) > (m_fileSize - offset))) {
                            return false;
                        }
                        out.push_back(Mp4Sample { offset, size, 0.0 });
                        offset += size;
                    }
                }
            }
            return !out.empty();
        }

        [[nodiscard]] bool applyTimes(const Box& stbl, std::uint32_t timescale, const EditInfo& edit,
            std::uint32_t movieTimescale, std::vector<Mp4Sample>& out) const
        {
            const auto stts { child(m_b, stbl, kStts) };
            std::uint32_t count { 0U };
            if (!stts.has_value() || !tableCount(m_b, *stts, 4U, 8U, count)) {
                return false;
            }
            const double offsetSec { (movieTimescale > 0U)
                    ? (static_cast<double>(edit.emptyMovieUnits) / static_cast<double>(movieTimescale))
                    : 0.0 };
            const double scale { static_cast<double>(timescale) };
            const double mediaStart { static_cast<double>(edit.mediaTime) };

            std::uint64_t dts { 0U };
            std::uint32_t delta { 0U };
            std::size_t idx { 0U };
            for (std::uint32_t i { 0U }; (i < count) && (idx < out.size()); ++i) {
                const std::size_t pos { stts->body + 8U + (static_cast<std::size_t>(i) * 8U) };
                std::uint32_t run { 0U };
                if (!readU32(m_b, pos, run) || !readU32(m_b, pos + 4U, delta)) {
                    return false;
                }
                for (std::uint32_t j { 0U }; (j < run) && (idx < out.size()); ++j) {
                    out[idx].timeSec = offsetSec + ((static_cast<double>(dts) - mediaStart) / scale);
                    dts += delta;
                    ++idx;
                }
            }
            for (; idx < out.size(); ++idx) { // stts shorter than stsz: extend with last delta
                out[idx].timeSec = offsetSec + ((static_cast<double>(dts) - mediaStart) / scale);
                dts += delta;
            }
            return true;
        }

        const Buffer& m_b;
        std::uint64_t m_fileSize { 0U };
        std::uint32_t m_fixedSize { 0U };
        std::uint32_t m_sampleCount { 0U };
        std::vector<std::uint32_t> m_sizes {};
        std::vector<std::uint64_t> m_offsets {};
        std::vector<StscEntry> m_stsc {};
    };

    /// @brief Reads the handler type of a trak.
    /// @param[in] b Source buffer.
    /// @param[in] mdia The trak's mdia box.
    /// @return Handler 4CC, or 0 if absent.
    [[nodiscard]] std::uint32_t handlerType(const Buffer& b, const Box& mdia)
    {
        const auto hdlr { child(b, mdia, kHdlr) };
        std::uint32_t type { 0U };
        if (!hdlr.has_value() || !readU32(b, hdlr->body + 8U, type)) {
            return 0U;
        }
        return type;
    }

    /// @brief Reads the timescale from an mvhd or mdhd box.
    /// @param[in] b Source buffer.
    /// @param[in] box mvhd or mdhd box (identical layout up to the timescale field).
    /// @param[out] timescale Timescale.
    /// @param[out] creation Creation time (seconds since 1904).
    /// @return False if malformed.
    [[nodiscard]] bool readHeaderTimes(
        const Buffer& b, const Box& box, std::uint32_t& timescale, std::uint64_t& creation)
    {
        std::uint8_t version { 0U };
        if (!readVersion(b, box.body, version)) {
            return false;
        }
        if (version == 1U) {
            return readU64(b, box.body + 4U, creation) && readU32(b, box.body + 20U, timescale);
        }
        std::uint32_t c32 { 0U };
        if (!readU32(b, box.body + 4U, c32) || !readU32(b, box.body + 12U, timescale)) {
            return false;
        }
        creation = c32;
        return true;
    }

    /// @brief Reads video dimensions from a tkhd box.
    /// @param[in] b Source buffer.
    /// @param[in] tkhd The tkhd box.
    /// @return Width/height ratio, or std::nullopt if zero or malformed.
    [[nodiscard]] std::optional<double> tkhdAspect(const Buffer& b, const Box& tkhd)
    {
        std::uint8_t version { 0U };
        if (!readVersion(b, tkhd.body, version)) {
            return std::nullopt;
        }
        const std::size_t pos { tkhd.body + ((version == 1U) ? 88U : 76U) };
        std::uint32_t w { 0U };
        std::uint32_t h { 0U };
        if ((pos + 8U > tkhd.end()) || !readU32(b, pos, w) || !readU32(b, pos + 4U, h) || (w == 0U) || (h == 0U)) {
            return std::nullopt;
        }
        return static_cast<double>(w) / static_cast<double>(h);
    }

    /// @brief Extracts moov/udta/meta/ilst/(c)too/data as a string.
    /// @param[in] b Source buffer.
    /// @param[in] moov Root box.
    /// @return Encoder string, empty if absent.
    [[nodiscard]] std::string readEncoder(const Buffer& b, const Box& moov)
    {
        const auto udta { child(b, moov, kUdta) };
        const auto meta { udta.has_value() ? child(b, *udta, kMeta) : std::nullopt };
        if (!meta.has_value()) {
            return {};
        }
        // ISO meta is a full box; QuickTime meta is not. Detect by peeking for a child header.
        std::uint32_t peek { 0U };
        const bool quickTime { readU32(b, meta->body + 4U, peek) && (peek == kHdlr) };
        const std::size_t first { quickTime ? meta->body : (meta->body + kFullBoxHeader) };
        const auto ilst { findChild(b, first, meta->end(), kIlst) };
        const auto tool { ilst.has_value() ? child(b, *ilst, kCopyTool) : std::nullopt };
        const auto data { tool.has_value() ? child(b, *tool, kData) : std::nullopt };
        if (!data.has_value() || (data->size < 8U)) {
            return {};
        }
        std::string out {};
        out.reserve(data->size - 8U);
        for (std::size_t i { data->body + 8U }; i < data->end(); ++i) {
            if (b[i] == 0U) {
                break;
            }
            out.push_back(static_cast<char>(b[i]));
        }
        return out;
    }

} // namespace

void Mp4TextTrackReader::reset() noexcept
{
    if (m_file.is_open()) {
        m_file.close();
    }
    m_file.clear();
    m_fileSize = 0U;
    m_timescale = 0U;
    m_samples.clear();
    m_creationUs.reset();
    m_encoder.clear();
    m_videoAspect.reset();
}

bool Mp4TextTrackReader::loadMoov(std::vector<std::uint8_t>& moov)
{
    std::uint64_t pos { 0U };
    bool first { true };
    while ((m_fileSize >= kBoxHeader) && (pos <= (m_fileSize - kBoxHeader))) {
        std::array<std::uint8_t, kLargeHeader> hdr {};
        m_file.seekg(static_cast<std::streamoff>(pos));
        m_file.read(reinterpret_cast<char*>(hdr.data()), static_cast<std::streamsize>(kBoxHeader));
        if (m_file.gcount() != static_cast<std::streamsize>(kBoxHeader)) {
            return false;
        }
        const std::uint32_t size32 { be32(hdr.data()) };
        const std::uint32_t type { be32(&hdr[4U]) };
        if (first && (type != kFtyp)) {
            return false;
        }
        first = false;

        std::uint64_t size { size32 };
        std::uint64_t header { kBoxHeader };
        if (size32 == 1U) {
            m_file.read(reinterpret_cast<char*>(&hdr[kBoxHeader]), static_cast<std::streamsize>(kBoxHeader));
            if (m_file.gcount() != static_cast<std::streamsize>(kBoxHeader)) {
                return false;
            }
            size = (static_cast<std::uint64_t>(be32(&hdr[8U])) << 32U) | static_cast<std::uint64_t>(be32(&hdr[12U]));
            header = kLargeHeader;
        } else if (size32 == 0U) {
            size = m_fileSize - pos;
        } else {
            // 32-bit size already set.
        }
        if ((size < header) || (size > (m_fileSize - pos))) {
            return false;
        }

        if (type == kMoov) {
            const std::uint64_t body { size - header };
            if (body > kMaxMoovBytes) {
                return false;
            }
            moov.resize(static_cast<std::size_t>(body));
            m_file.seekg(static_cast<std::streamoff>(pos + header));
            m_file.read(reinterpret_cast<char*>(moov.data()), static_cast<std::streamsize>(body));
            return m_file.gcount() == static_cast<std::streamsize>(body);
        }
        pos += size;
    }
    return false;
}

bool Mp4TextTrackReader::open(std::string_view path)
{
    reset();
    m_file.open(std::string(path), std::ios::binary | std::ios::ate);
    if (!m_file.is_open()) {
        return false;
    }
    const std::streamoff end { m_file.tellg() };
    if (end <= 0) {
        reset();
        return false;
    }
    m_fileSize = static_cast<std::uint64_t>(end);

    Buffer moovBody {};
    if (!loadMoov(moovBody)) {
        reset();
        return false;
    }
    const Box moov { kMoov, 0U, moovBody.size(), moovBody.size() };

    std::uint32_t movieTimescale { 0U };
    if (const auto mvhd { child(moovBody, moov, kMvhd) }) {
        std::uint64_t creation { 0U };
        if (readHeaderTimes(moovBody, *mvhd, movieTimescale, creation) && (creation > kEpoch1904To1970)) {
            m_creationUs = (creation - kEpoch1904To1970) * kUsPerSecond;
        }
    }
    m_encoder = readEncoder(moovBody, moov);

    std::size_t pos { 0U };
    Box trak {};
    while ((pos < moovBody.size()) && parseBox(moovBody, pos, moovBody.size(), trak)) {
        pos = trak.next;
        if (trak.type != kTrak) {
            continue;
        }
        const auto mdia { child(moovBody, trak, kMdia) };
        if (!mdia.has_value()) {
            continue;
        }
        const std::uint32_t handler { handlerType(moovBody, *mdia) };
        if ((handler == kVide) && !m_videoAspect.has_value()) {
            if (const auto tkhd { child(moovBody, trak, kTkhd) }) {
                m_videoAspect = tkhdAspect(moovBody, *tkhd);
            }
            continue;
        }
        if (!m_samples.empty() || ((handler != kSbtl) && (handler != kText) && (handler != kSubt))) {
            continue;
        }

        const auto stbl { descend(moovBody, *mdia, std::array<std::uint32_t, 2U> { kMinf, kStbl }) };
        const auto stsd { stbl.has_value() ? child(moovBody, *stbl, kStsd) : std::nullopt };
        const auto mdhd { child(moovBody, *mdia, kMdhd) };
        std::uint32_t entryType { 0U };
        if (!stsd.has_value() || !mdhd.has_value() || !readU32(moovBody, stsd->body + 12U, entryType)
            || (entryType != kTx3g)) {
            continue;
        }
        std::uint32_t timescale { 0U };
        std::uint64_t ignored { 0U };
        if (!readHeaderTimes(moovBody, *mdhd, timescale, ignored) || (timescale == 0U)) {
            continue;
        }
        EditInfo edit {};
        if (const auto elst { descend(moovBody, trak, std::array<std::uint32_t, 2U> { kEdts, kElst }) }) {
            if (!parseElst(moovBody, *elst, edit)) {
                continue;
            }
        }
        TableParser tables { moovBody, m_fileSize };
        std::vector<Mp4Sample> samples {};
        if (tables.build(*stbl, timescale, edit, movieTimescale, samples)) {
            m_samples = std::move(samples);
            m_timescale = timescale;
        }
    }

    if (m_samples.empty()) {
        reset();
        return false;
    }
    return true;
}

std::size_t Mp4TextTrackReader::sampleCount() const noexcept
{
    return m_samples.size();
}

const std::vector<Mp4Sample>& Mp4TextTrackReader::samples() const noexcept
{
    return m_samples;
}

std::optional<std::string> Mp4TextTrackReader::readText(std::size_t index)
{
    if ((index >= m_samples.size()) || !m_file.is_open()) {
        return std::nullopt;
    }
    const Mp4Sample& s { m_samples[index] };
    if ((s.size < 2U) || (s.size > kMaxSampleBytes)) {
        return std::nullopt;
    }
    std::string buf(static_cast<std::size_t>(s.size), '\0');
    m_file.clear();
    m_file.seekg(static_cast<std::streamoff>(s.offset));
    m_file.read(buf.data(), static_cast<std::streamsize>(s.size));
    if (m_file.gcount() != static_cast<std::streamsize>(s.size)) {
        return std::nullopt;
    }
    const std::size_t len { (static_cast<std::size_t>(static_cast<std::uint8_t>(buf[0])) << 8U)
        | static_cast<std::size_t>(static_cast<std::uint8_t>(buf[1])) };
    if (len > (buf.size() - 2U)) {
        return std::nullopt;
    }
    return buf.substr(2U, len);
}

std::uint32_t Mp4TextTrackReader::timescale() const noexcept
{
    return m_timescale;
}

std::optional<std::uint64_t> Mp4TextTrackReader::creationUtcUs() const noexcept
{
    return m_creationUs;
}

const std::string& Mp4TextTrackReader::encoderName() const noexcept
{
    return m_encoder;
}

std::optional<double> Mp4TextTrackReader::videoAspect() const noexcept
{
    return m_videoAspect;
}

} // namespace Dji
