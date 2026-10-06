#pragma once

/// @file Mp4TestBuilder.h
/// @brief Header-only builder for synthetic ISO-BMFF (MP4) files with a tx3g text track.
/// @details Used by DjiTelemetry unit tests so that no binary fixtures need to be committed.
///          The produced layout mimics DJI recordings: ftyp, mdat, moov (or moov before mdat),
///          one video trak (tkhd dimensions only) and one 'sbtl'/'tx3g' text trak.

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace DjiTest {

/// @brief Byte buffer alias.
using Bytes = std::vector<std::uint8_t>;

/// @brief Appends a big-endian 16-bit value.
/// @param[in,out] b Destination buffer.
/// @param[in] v Value to append.
inline void putU16(Bytes& b, std::uint16_t v)
{
    b.push_back(static_cast<std::uint8_t>((v >> 8U) & 0xFFU));
    b.push_back(static_cast<std::uint8_t>(v & 0xFFU));
}

/// @brief Appends a big-endian 32-bit value.
/// @param[in,out] b Destination buffer.
/// @param[in] v Value to append.
inline void putU32(Bytes& b, std::uint32_t v)
{
    for (std::uint32_t shift { 24U };; shift -= 8U) {
        b.push_back(static_cast<std::uint8_t>((v >> shift) & 0xFFU));
        if (shift == 0U) {
            break;
        }
    }
}

/// @brief Appends a big-endian 64-bit value.
/// @param[in,out] b Destination buffer.
/// @param[in] v Value to append.
inline void putU64(Bytes& b, std::uint64_t v)
{
    putU32(b, static_cast<std::uint32_t>(v >> 32U));
    putU32(b, static_cast<std::uint32_t>(v & 0xFFFFFFFFU));
}

/// @brief Appends a four-character code given as a 4-char string literal.
/// @param[in,out] b Destination buffer.
/// @param[in] t Four-character code (only the first 4 bytes are used).
inline void putType(Bytes& b, const std::string& t)
{
    for (std::size_t i { 0U }; i < 4U; ++i) {
        b.push_back(static_cast<std::uint8_t>(t.at(i)));
    }
}

/// @brief Appends raw bytes.
/// @param[in,out] b Destination buffer.
/// @param[in] p Bytes to append.
inline void putBytes(Bytes& b, const Bytes& p)
{
    b.insert(b.end(), p.begin(), p.end());
}

/// @brief Wraps a payload into a box with a 32-bit size header.
/// @param[in] type Four-character box type.
/// @param[in] payload Box body.
/// @return Serialised box.
inline Bytes box(const std::string& type, const Bytes& payload)
{
    Bytes b {};
    putU32(b, static_cast<std::uint32_t>(payload.size() + 8U));
    putType(b, type);
    putBytes(b, payload);
    return b;
}

/// @brief Wraps a payload into a box with a 64-bit (largesize) header.
/// @param[in] type Four-character box type.
/// @param[in] payload Box body.
/// @return Serialised box.
inline Bytes box64(const std::string& type, const Bytes& payload)
{
    Bytes b {};
    putU32(b, 1U);
    putType(b, type);
    putU64(b, static_cast<std::uint64_t>(payload.size()) + 16U);
    putBytes(b, payload);
    return b;
}

/// @brief Wraps a payload into a full box (version + flags).
/// @param[in] type Four-character box type.
/// @param[in] version Full-box version.
/// @param[in] flags 24-bit flags.
/// @param[in] payload Box body following version/flags.
/// @return Serialised full box.
inline Bytes fullBox(const std::string& type, std::uint8_t version, std::uint32_t flags, const Bytes& payload)
{
    Bytes body {};
    putU32(body, (static_cast<std::uint32_t>(version) << 24U) | (flags & 0x00FFFFFFU));
    putBytes(body, payload);
    return box(type, body);
}

/// @struct Mp4BuildOptions
/// @brief Knobs controlling the synthetic MP4 layout.
struct Mp4BuildOptions {
    std::vector<std::string> texts {}; ///< One text sample per entry.
    bool moovFirst { false }; ///< Place moov before mdat.
    bool useCo64 { false }; ///< Use co64 instead of stco.
    bool largeMdat { false }; ///< Use a 64-bit mdat header.
    std::uint32_t samplesPerChunk { 1U }; ///< Samples per chunk (stsc).
    std::uint32_t timescale { 30000U }; ///< Text track media timescale.
    std::uint32_t sampleDelta { 1000U }; ///< stts delta.
    bool splitStts { false }; ///< First 2 samples delta, rest 2*delta.
    bool fixedSampleSize { false }; ///< Use stsz sample_size (all equal).
    bool addElst { true }; ///< Emit edts/elst.
    std::int32_t elstMediaTime { 0 }; ///< elst media_time (media units).
    std::uint32_t emptyEditMovieUnits { 0U }; ///< Initial empty edit (movie units, 1000/s).
    std::uint32_t creation1904 { 0xE6DAC627U }; ///< mvhd creation time (1904 epoch).
    bool includeTextTrack { true }; ///< Emit the tx3g trak.
    std::string encoder { "DJI DJI Matrice 4T" }; ///< udta/meta/ilst/(c)too string.
    std::uint16_t videoWidth { 1280U }; ///< Video tkhd width.
    std::uint16_t videoHeight { 1024U }; ///< Video tkhd height.
    std::uint64_t chunkOffsetBias { 0U }; ///< Added to every chunk offset (corruption).
};

/// @brief Serialises one text sample (2-byte BE length + UTF-8).
/// @param[in] text Sample text.
/// @return Sample bytes.
inline Bytes textSample(const std::string& text)
{
    Bytes b {};
    putU16(b, static_cast<std::uint16_t>(text.size()));
    for (const char c : text) {
        b.push_back(static_cast<std::uint8_t>(c));
    }
    return b;
}

/// @brief Builds an hdlr full box.
/// @param[in] handler Handler 4CC.
/// @return Serialised hdlr box.
inline Bytes hdlrBox(const std::string& handler)
{
    Bytes p {};
    putU32(p, 0U);
    putType(p, handler);
    putU32(p, 0U);
    putU32(p, 0U);
    putU32(p, 0U);
    p.push_back(0U);
    return fullBox("hdlr", 0U, 0U, p);
}

/// @brief Builds a version-0 tkhd box.
/// @param[in] width Track width (integer part of 16.16).
/// @param[in] height Track height (integer part of 16.16).
/// @return Serialised tkhd box.
inline Bytes tkhdBox(std::uint16_t width, std::uint16_t height)
{
    Bytes p {};
    putU32(p, 0U); // creation
    putU32(p, 0U); // modification
    putU32(p, 1U); // track id
    putU32(p, 0U); // reserved
    putU32(p, 0U); // duration
    putU64(p, 0U); // reserved
    putU16(p, 0U); // layer
    putU16(p, 0U); // alternate group
    putU16(p, 0U); // volume
    putU16(p, 0U); // reserved
    for (std::size_t i { 0U }; i < 9U; ++i) {
        putU32(p, 0U); // matrix
    }
    putU32(p, static_cast<std::uint32_t>(width) << 16U);
    putU32(p, static_cast<std::uint32_t>(height) << 16U);
    return fullBox("tkhd", 0U, 3U, p);
}

/// @brief Builds the moov box for the given options and mdat payload base offset.
/// @param[in] o Build options.
/// @param[in] samples Serialised samples (in mdat order).
/// @param[in] dataBase Absolute file offset of the first sample byte.
/// @return Serialised moov box.
inline Bytes buildMoov(const Mp4BuildOptions& o, const std::vector<Bytes>& samples, std::uint64_t dataBase)
{
    Bytes mvhdP {};
    putU32(mvhdP, o.creation1904);
    putU32(mvhdP, o.creation1904);
    putU32(mvhdP, 1000U); // movie timescale
    putU32(mvhdP, 0U); // duration
    putU32(mvhdP, 0x00010000U);
    putU16(mvhdP, 0x0100U);
    for (std::size_t i { 0U }; i < 10U; ++i) {
        mvhdP.push_back(0U);
    }
    for (std::size_t i { 0U }; i < 9U; ++i) {
        putU32(mvhdP, 0U);
    }
    for (std::size_t i { 0U }; i < 6U; ++i) {
        putU32(mvhdP, 0U);
    }
    putU32(mvhdP, 3U);
    Bytes moovP { fullBox("mvhd", 0U, 0U, mvhdP) };

    // Video trak: tkhd + mdia/hdlr('vide') only.
    {
        Bytes trakP { tkhdBox(o.videoWidth, o.videoHeight) };
        putBytes(trakP, box("mdia", hdlrBox("vide")));
        putBytes(moovP, box("trak", trakP));
    }

    if (o.includeTextTrack) {
        const auto n { static_cast<std::uint32_t>(samples.size()) };
        Bytes trakP { tkhdBox(0U, 0U) };

        if (o.addElst) {
            Bytes elstP {};
            const std::uint32_t entries { (o.emptyEditMovieUnits > 0U) ? 2U : 1U };
            putU32(elstP, entries);
            if (o.emptyEditMovieUnits > 0U) {
                putU32(elstP, o.emptyEditMovieUnits);
                putU32(elstP, 0xFFFFFFFFU); // media_time = -1
                putU32(elstP, 0x00010000U);
            }
            putU32(elstP, 1000U);
            putU32(elstP, static_cast<std::uint32_t>(o.elstMediaTime));
            putU32(elstP, 0x00010000U);
            putBytes(trakP, box("edts", fullBox("elst", 0U, 0U, elstP)));
        }

        Bytes mdhdP {};
        putU32(mdhdP, o.creation1904);
        putU32(mdhdP, o.creation1904);
        putU32(mdhdP, o.timescale);
        putU32(mdhdP, n * o.sampleDelta);
        putU16(mdhdP, 0x55C4U);
        putU16(mdhdP, 0U);

        Bytes stsdP {};
        putU32(stsdP, 1U);
        putU32(stsdP, 16U);
        putType(stsdP, "tx3g");
        putU32(stsdP, 0U);
        putU32(stsdP, 1U);

        Bytes sttsP {};
        if (o.splitStts && n > 2U) {
            putU32(sttsP, 2U);
            putU32(sttsP, 2U);
            putU32(sttsP, o.sampleDelta);
            putU32(sttsP, n - 2U);
            putU32(sttsP, o.sampleDelta * 2U);
        } else {
            putU32(sttsP, 1U);
            putU32(sttsP, n);
            putU32(sttsP, o.sampleDelta);
        }

        const std::uint32_t spc { (o.samplesPerChunk == 0U) ? 1U : o.samplesPerChunk };
        const std::uint32_t chunkCount { (n + spc - 1U) / spc };
        const std::uint32_t remainder { n % spc };
        Bytes stscP {};
        if (remainder != 0U && chunkCount > 1U) {
            putU32(stscP, 2U);
            putU32(stscP, 1U);
            putU32(stscP, spc);
            putU32(stscP, 1U);
            putU32(stscP, chunkCount);
            putU32(stscP, remainder);
            putU32(stscP, 1U);
        } else {
            putU32(stscP, 1U);
            putU32(stscP, 1U);
            putU32(stscP, (chunkCount == 1U && remainder != 0U) ? remainder : spc);
            putU32(stscP, 1U);
        }

        Bytes stszP {};
        if (o.fixedSampleSize && n > 0U) {
            putU32(stszP, static_cast<std::uint32_t>(samples.front().size()));
            putU32(stszP, n);
        } else {
            putU32(stszP, 0U);
            putU32(stszP, n);
            for (const auto& s : samples) {
                putU32(stszP, static_cast<std::uint32_t>(s.size()));
            }
        }

        Bytes coP {};
        putU32(coP, chunkCount);
        std::uint64_t cursor { dataBase };
        for (std::uint32_t c { 0U }; c < chunkCount; ++c) {
            const std::uint64_t off { cursor + o.chunkOffsetBias };
            if (o.useCo64) {
                putU64(coP, off);
            } else {
                putU32(coP, static_cast<std::uint32_t>(off));
            }
            for (std::uint32_t k { 0U }; k < spc; ++k) {
                const std::size_t idx { static_cast<std::size_t>(c) * spc + k };
                if (idx < samples.size()) {
                    cursor += samples[idx].size();
                }
            }
        }

        Bytes stblP { fullBox("stsd", 0U, 0U, stsdP) };
        putBytes(stblP, fullBox("stts", 0U, 0U, sttsP));
        putBytes(stblP, fullBox("stsc", 0U, 0U, stscP));
        putBytes(stblP, fullBox("stsz", 0U, 0U, stszP));
        putBytes(stblP, fullBox(o.useCo64 ? "co64" : "stco", 0U, 0U, coP));

        Bytes minfP { box("stbl", stblP) };
        Bytes mdiaP { fullBox("mdhd", 0U, 0U, mdhdP) };
        putBytes(mdiaP, hdlrBox("sbtl"));
        putBytes(mdiaP, box("minf", minfP));
        putBytes(trakP, box("mdia", mdiaP));
        putBytes(moovP, box("trak", trakP));
    }

    if (!o.encoder.empty()) {
        Bytes dataP {};
        putU32(dataP, 1U); // UTF-8 type
        putU32(dataP, 0U); // locale
        for (const char c : o.encoder) {
            dataP.push_back(static_cast<std::uint8_t>(c));
        }
        Bytes tooType {};
        tooType.push_back(0xA9U);
        tooType.push_back(static_cast<std::uint8_t>('t'));
        tooType.push_back(static_cast<std::uint8_t>('o'));
        tooType.push_back(static_cast<std::uint8_t>('o'));
        const Bytes dataBox { box("data", dataP) };
        Bytes tooBox {};
        putU32(tooBox, static_cast<std::uint32_t>(dataBox.size() + 8U));
        putBytes(tooBox, tooType);
        putBytes(tooBox, dataBox);

        Bytes metaP { hdlrBox("mdir") };
        putBytes(metaP, box("ilst", tooBox));
        putBytes(moovP, box("udta", fullBox("meta", 0U, 0U, metaP)));
    }

    return box("moov", moovP);
}

/// @brief Builds a complete synthetic MP4 file image.
/// @param[in] o Build options.
/// @return File bytes.
inline Bytes buildMp4(const Mp4BuildOptions& o)
{
    std::vector<Bytes> samples {};
    samples.reserve(o.texts.size());
    for (const auto& t : o.texts) {
        samples.push_back(textSample(t));
    }

    Bytes ftypP {};
    putType(ftypP, "isom");
    putU32(ftypP, 512U);
    putType(ftypP, "isom");
    putType(ftypP, "mp41");
    const Bytes ftyp { box("ftyp", ftypP) };

    Bytes mdatP {};
    for (const auto& s : samples) {
        putBytes(mdatP, s);
    }
    const std::uint64_t mdatHeader { o.largeMdat ? 16U : 8U };
    const Bytes mdat { o.largeMdat ? box64("mdat", mdatP) : box("mdat", mdatP) };

    Bytes out { ftyp };
    if (o.moovFirst) {
        const Bytes probe { buildMoov(o, samples, 0U) };
        const std::uint64_t base { ftyp.size() + probe.size() + mdatHeader };
        putBytes(out, buildMoov(o, samples, base));
        putBytes(out, mdat);
    } else {
        const std::uint64_t base { ftyp.size() + mdatHeader };
        putBytes(out, mdat);
        putBytes(out, buildMoov(o, samples, base));
    }
    return out;
}

/// @class TempFile
/// @brief RAII temporary file that is removed on destruction.
class TempFile {
public:
    /// @brief Writes bytes to a unique temporary file.
    /// @param[in] bytes File content.
    /// @param[in] tag Name fragment for uniqueness.
    TempFile(const Bytes& bytes, const std::string& tag)
        : m_path { (std::filesystem::temp_directory_path() / ("dji_test_" + tag + ".mp4")).string() }
    {
        std::ofstream f(m_path, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    /// @brief Removes the file.
    ~TempFile()
    {
        std::error_code ec {};
        static_cast<void>(std::filesystem::remove(m_path, ec));
    }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    TempFile(TempFile&&) = delete;
    TempFile& operator=(TempFile&&) = delete;

    /// @brief Returns the file path.
    /// @return Absolute path.
    [[nodiscard]] const std::string& path() const noexcept
    {
        return m_path;
    }

private:
    std::string m_path {};
};

} // namespace DjiTest
