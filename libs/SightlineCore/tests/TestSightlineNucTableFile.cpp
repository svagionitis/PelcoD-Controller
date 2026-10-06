/// @file TestSightlineNucTableFile.cpp
/// @brief Tests for the on-card .nuc / .dead table file codec.
/// @details Layouts come from Appendix A2 (NUC table, version 1) and A3 (dead table,
///          versions 1..3) of docs/protocols/Sightline/EAN-NUC-and-DPR.pdf. The appendix does
///          not state byte order or packing; the codec assumes packed little-endian, matching
///          the SLA serial protocol.

#include "SightlineFraming.h"
#include "modules/SightlineNucTableFile.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Sightline {
namespace {

    /// @brief Builds a small, valid 2x3 NUC table with distinct per-pixel values.
    /// @return Populated table.
    NucTable makeNuc()
    {
        NucTable t {};
        t.height = 2;
        t.width = 3;
        for (std::uint16_t i { 0U }; i < 6U; ++i) {
            const NucGainOffset px { static_cast<std::uint16_t>(8192U + i),
                static_cast<std::int16_t>(-100 + static_cast<std::int16_t>(i)) };
            t.pixels.push_back(px);
        }
        return t;
    }

    /// @brief Builds a valid dead table of the given version with three sorted entries.
    /// @param[in] version Table version (1..3).
    /// @return Populated table.
    DeadTable makeDead(std::uint32_t version)
    {
        DeadTable t {};
        t.version = version;
        t.params.maxGain = 1.5F;
        t.params.minGain = 0.5F;
        t.params.maxOff = 1000;
        t.params.minOff = -1000;
        t.params.maxVal = 16000U;
        t.params.minVal = 10U;
        t.params.maxStdDev = 300U;
        t.params.maxNumDead = 5000;
        t.replaceMethod = 2;
        t.numReplace = 4U;
        const std::int8_t off { (version < 3U) ? static_cast<std::int8_t>(-1) : static_cast<std::int8_t>(0) };
        t.pixels.push_back(DeadPixelEntry { 1U, 5U, off, 0 });
        t.pixels.push_back(DeadPixelEntry { 1U, 9U, 0, off });
        t.pixels.push_back(DeadPixelEntry { 256U, 1U, 0, 0 });
        return t;
    }

    /// @brief Serialises a table and asserts success.
    /// @param[in] t Table to write.
    /// @return File bytes.
    std::vector<std::uint8_t> writeOk(const NucTable& t)
    {
        std::vector<std::uint8_t> out {};
        EXPECT_EQ(SightlineNucTableFile::writeNuc(t, out), TableFileError::Ok);
        return out;
    }

    /// @brief Serialises a table and asserts success.
    /// @param[in] t Table to write.
    /// @return File bytes.
    std::vector<std::uint8_t> writeOk(const DeadTable& t)
    {
        std::vector<std::uint8_t> out {};
        EXPECT_EQ(SightlineNucTableFile::writeDead(t, out), TableFileError::Ok);
        return out;
    }

} // namespace

// --- NUC table (A2) ---------------------------------------------------------

TEST(SightlineNucTableFile, NucLayoutIsLittleEndian)
{
    const std::vector<std::uint8_t> bytes { writeOk(makeNuc()) };
    ASSERT_EQ(bytes.size(), 12U + (6U * 4U));
    EXPECT_EQ(SightlineFraming::readU32Le(&bytes[0]), 1U);
    EXPECT_EQ(bytes[4], 0x0DU);
    EXPECT_EQ(bytes[5], 0xD0U);
    EXPECT_EQ(bytes[6], 0xACU);
    EXPECT_EQ(bytes[7], 0x51U);
    EXPECT_EQ(SightlineFraming::readS16Le(&bytes[8]), 2);
    EXPECT_EQ(SightlineFraming::readS16Le(&bytes[10]), 3);
    EXPECT_EQ(SightlineFraming::readU16Le(&bytes[12]), 8192U);
    EXPECT_EQ(SightlineFraming::readS16Le(&bytes[14]), -100);
}

TEST(SightlineNucTableFile, NucRoundTrip)
{
    const NucTable src { makeNuc() };
    const std::vector<std::uint8_t> bytes { writeOk(src) };
    NucTable dst {};
    ASSERT_EQ(SightlineNucTableFile::parseNuc(ByteView { bytes }, dst), TableFileError::Ok);
    EXPECT_EQ(dst.version, 1U);
    EXPECT_EQ(dst.height, 2);
    EXPECT_EQ(dst.width, 3);
    ASSERT_EQ(dst.pixels.size(), 6U);
    for (std::size_t i { 0U }; i < 6U; ++i) {
        EXPECT_EQ(dst.pixels[i].gain, src.pixels[i].gain);
        EXPECT_EQ(dst.pixels[i].offset, src.pixels[i].offset);
    }
}

TEST(SightlineNucTableFile, NucRejectsBadMagic)
{
    std::vector<std::uint8_t> bytes { writeOk(makeNuc()) };
    bytes[4] = 0x00U;
    NucTable dst {};
    EXPECT_EQ(SightlineNucTableFile::parseNuc(ByteView { bytes }, dst), TableFileError::BadMagic);
}

TEST(SightlineNucTableFile, NucRejectsUndocumentedVersion)
{
    std::vector<std::uint8_t> bytes { writeOk(makeNuc()) };
    bytes[0] = 2U;
    NucTable dst {};
    EXPECT_EQ(SightlineNucTableFile::parseNuc(ByteView { bytes }, dst), TableFileError::BadVersion);

    NucTable src { makeNuc() };
    src.version = 0U;
    std::vector<std::uint8_t> out {};
    EXPECT_EQ(SightlineNucTableFile::writeNuc(src, out), TableFileError::BadVersion);
}

TEST(SightlineNucTableFile, NucRejectsTruncatedAndTrailing)
{
    const std::vector<std::uint8_t> good { writeOk(makeNuc()) };
    NucTable dst {};

    const std::vector<std::uint8_t> shortHdr(good.begin(), good.begin() + 11);
    EXPECT_EQ(SightlineNucTableFile::parseNuc(ByteView { shortHdr }, dst), TableFileError::Truncated);

    const std::vector<std::uint8_t> shortBody(good.begin(), good.end() - 1);
    EXPECT_EQ(SightlineNucTableFile::parseNuc(ByteView { shortBody }, dst), TableFileError::Truncated);

    std::vector<std::uint8_t> trailing { good };
    trailing.push_back(0U);
    EXPECT_EQ(SightlineNucTableFile::parseNuc(ByteView { trailing }, dst), TableFileError::TrailingData);

    EXPECT_EQ(SightlineNucTableFile::parseNuc(ByteView {}, dst), TableFileError::Truncated);
}

TEST(SightlineNucTableFile, NucRejectsBadDimensions)
{
    std::vector<std::uint8_t> bytes { writeOk(makeNuc()) };
    bytes[8] = 0xFFU; // height = -1 (0xFFFF)
    bytes[9] = 0xFFU;
    NucTable dst {};
    EXPECT_EQ(SightlineNucTableFile::parseNuc(ByteView { bytes }, dst), TableFileError::BadSize);

    NucTable src { makeNuc() };
    src.width = 0;
    std::vector<std::uint8_t> out {};
    EXPECT_EQ(SightlineNucTableFile::writeNuc(src, out), TableFileError::BadSize);

    src = makeNuc();
    src.pixels.pop_back();
    EXPECT_EQ(SightlineNucTableFile::writeNuc(src, out), TableFileError::BadSize);
}

TEST(SightlineNucTableFile, NucRejectsOversizedHeaderBeforeAllocating)
{
    std::vector<std::uint8_t> bytes {};
    SightlineFraming::appendU32Le(bytes, 1U);
    SightlineFraming::appendU32Le(bytes, kNucFileMagic);
    SightlineFraming::appendS16Le(bytes, 32767);
    SightlineFraming::appendS16Le(bytes, 32767);
    NucTable dst {};
    EXPECT_EQ(SightlineNucTableFile::parseNuc(ByteView { bytes }, dst), TableFileError::TooLarge);
    EXPECT_TRUE(dst.pixels.empty());
}

TEST(SightlineNucTableFile, FailedParseLeavesOutputUntouched)
{
    NucTable dst { makeNuc() };
    const std::vector<std::uint8_t> junk { 1U, 2U, 3U };
    EXPECT_NE(SightlineNucTableFile::parseNuc(ByteView { junk }, dst), TableFileError::Ok);
    EXPECT_EQ(dst.pixels.size(), 6U);
    EXPECT_EQ(dst.width, 3);
}

TEST(SightlineNucTableFile, GainFixedPointConversion)
{
    EXPECT_DOUBLE_EQ(SightlineNucTableFile::gainToReal(8192U), 1.0);
    EXPECT_DOUBLE_EQ(SightlineNucTableFile::gainToReal(4096U), 0.5);

    std::uint16_t raw { 0U };
    EXPECT_TRUE(SightlineNucTableFile::gainFromReal(1.25, raw));
    EXPECT_EQ(raw, 10240U);
    EXPECT_FALSE(SightlineNucTableFile::gainFromReal(8.0, raw));
    EXPECT_FALSE(SightlineNucTableFile::gainFromReal(-0.1, raw));
    EXPECT_EQ(raw, 10240U);
}

// --- Dead table (A3) --------------------------------------------------------

TEST(SightlineNucTableFile, DeadV3LayoutIsLittleEndian)
{
    const std::vector<std::uint8_t> bytes { writeOk(makeDead(3U)) };
    ASSERT_EQ(bytes.size(), 45U + (3U * 4U));
    EXPECT_EQ(SightlineFraming::readU32Le(&bytes[0]), 3U);
    EXPECT_EQ(SightlineFraming::readU32Le(&bytes[4]), kNucFileMagic);
    EXPECT_EQ(SightlineFraming::readS32Le(&bytes[8]), 3);
    EXPECT_FLOAT_EQ(SightlineFraming::readFloat32Le(&bytes[12]), 1.5F);
    EXPECT_FLOAT_EQ(SightlineFraming::readFloat32Le(&bytes[16]), 0.5F);
    EXPECT_EQ(SightlineFraming::readS32Le(&bytes[20]), 1000);
    EXPECT_EQ(SightlineFraming::readS32Le(&bytes[24]), -1000);
    EXPECT_EQ(SightlineFraming::readU16Le(&bytes[28]), 16000U);
    EXPECT_EQ(SightlineFraming::readU16Le(&bytes[30]), 10U);
    EXPECT_EQ(SightlineFraming::readU32Le(&bytes[32]), 300U);
    EXPECT_EQ(SightlineFraming::readS32Le(&bytes[36]), 5000);
    EXPECT_EQ(SightlineFraming::readS32Le(&bytes[40]), 2);
    EXPECT_EQ(bytes[44], 4U);
    EXPECT_EQ(SightlineFraming::readU16Le(&bytes[45]), 1U);  // row (Y)
    EXPECT_EQ(SightlineFraming::readU16Le(&bytes[47]), 5U);  // col (X)
    EXPECT_EQ(SightlineFraming::readU16Le(&bytes[53]), 256U);
    EXPECT_EQ(SightlineFraming::readU16Le(&bytes[55]), 1U);
}

TEST(SightlineNucTableFile, DeadV3RoundTrip)
{
    const DeadTable src { makeDead(3U) };
    const std::vector<std::uint8_t> bytes { writeOk(src) };
    DeadTable dst {};
    ASSERT_EQ(SightlineNucTableFile::parseDead(ByteView { bytes }, dst), TableFileError::Ok);
    EXPECT_EQ(dst.version, 3U);
    EXPECT_FLOAT_EQ(dst.params.maxGain, 1.5F);
    EXPECT_EQ(dst.params.maxNumDead, 5000);
    EXPECT_EQ(dst.replaceMethod, 2);
    EXPECT_EQ(dst.numReplace, 4U);
    ASSERT_EQ(dst.pixels.size(), 3U);
    EXPECT_EQ(dst.pixels[2].row, 256U);
    EXPECT_EQ(dst.pixels[2].col, 1U);
    EXPECT_EQ(dst.pixels[0].rowOff, 0);
}

TEST(SightlineNucTableFile, DeadV2HasParamsAndOffsets)
{
    const DeadTable src { makeDead(2U) };
    const std::vector<std::uint8_t> bytes { writeOk(src) };
    ASSERT_EQ(bytes.size(), 40U + (3U * 6U));
    EXPECT_EQ(static_cast<std::int8_t>(bytes[44]), -1); // first entry rowOff

    DeadTable dst {};
    ASSERT_EQ(SightlineNucTableFile::parseDead(ByteView { bytes }, dst), TableFileError::Ok);
    EXPECT_EQ(dst.version, 2U);
    EXPECT_EQ(dst.params.maxStdDev, 300U);
    EXPECT_EQ(dst.pixels[0].rowOff, -1);
    EXPECT_EQ(dst.pixels[1].colOff, -1);
}

TEST(SightlineNucTableFile, DeadV1HasNoParams)
{
    const DeadTable src { makeDead(1U) };
    const std::vector<std::uint8_t> bytes { writeOk(src) };
    ASSERT_EQ(bytes.size(), 12U + (3U * 6U));
    EXPECT_EQ(SightlineFraming::readU16Le(&bytes[12]), 1U);
    EXPECT_EQ(SightlineFraming::readU16Le(&bytes[14]), 5U);

    DeadTable dst {};
    ASSERT_EQ(SightlineNucTableFile::parseDead(ByteView { bytes }, dst), TableFileError::Ok);
    EXPECT_EQ(dst.version, 1U);
    EXPECT_EQ(dst.params.maxNumDead, 0);
    EXPECT_EQ(dst.pixels[0].rowOff, -1);
}

TEST(SightlineNucTableFile, DeadEmptyListIsValid)
{
    DeadTable src { makeDead(3U) };
    src.pixels.clear();
    const std::vector<std::uint8_t> bytes { writeOk(src) };
    EXPECT_EQ(bytes.size(), 45U);
    DeadTable dst {};
    EXPECT_EQ(SightlineNucTableFile::parseDead(ByteView { bytes }, dst), TableFileError::Ok);
    EXPECT_TRUE(dst.pixels.empty());
}

TEST(SightlineNucTableFile, DeadRejectsBadHeader)
{
    std::vector<std::uint8_t> bytes { writeOk(makeDead(3U)) };
    DeadTable dst {};

    std::vector<std::uint8_t> badMagic { bytes };
    badMagic[7] = 0x00U;
    EXPECT_EQ(SightlineNucTableFile::parseDead(ByteView { badMagic }, dst), TableFileError::BadMagic);

    std::vector<std::uint8_t> badVer { bytes };
    badVer[0] = 4U;
    EXPECT_EQ(SightlineNucTableFile::parseDead(ByteView { badVer }, dst), TableFileError::BadVersion);
    badVer[0] = 0U;
    EXPECT_EQ(SightlineNucTableFile::parseDead(ByteView { badVer }, dst), TableFileError::BadVersion);

    std::vector<std::uint8_t> negCount { bytes };
    negCount[11] = 0x80U;
    EXPECT_EQ(SightlineNucTableFile::parseDead(ByteView { negCount }, dst), TableFileError::BadSize);
}

TEST(SightlineNucTableFile, DeadRejectsTruncatedAndTrailing)
{
    const std::vector<std::uint8_t> good { writeOk(makeDead(3U)) };
    DeadTable dst {};

    const std::vector<std::uint8_t> shortParams(good.begin(), good.begin() + 44);
    EXPECT_EQ(SightlineNucTableFile::parseDead(ByteView { shortParams }, dst), TableFileError::Truncated);

    const std::vector<std::uint8_t> shortBody(good.begin(), good.end() - 1);
    EXPECT_EQ(SightlineNucTableFile::parseDead(ByteView { shortBody }, dst), TableFileError::Truncated);

    std::vector<std::uint8_t> trailing { good };
    trailing.push_back(0U);
    EXPECT_EQ(SightlineNucTableFile::parseDead(ByteView { trailing }, dst), TableFileError::TrailingData);
}

TEST(SightlineNucTableFile, DeadRejectsHugeCountBeforeAllocating)
{
    std::vector<std::uint8_t> bytes {};
    SightlineFraming::appendU32Le(bytes, 1U);
    SightlineFraming::appendU32Le(bytes, kNucFileMagic);
    SightlineFraming::appendS32Le(bytes, 0x7FFFFFFF);
    DeadTable dst {};
    EXPECT_EQ(SightlineNucTableFile::parseDead(ByteView { bytes }, dst), TableFileError::TooLarge);
}

TEST(SightlineNucTableFile, DeadRejectsUnsortedAndDuplicates)
{
    DeadTable unsorted { makeDead(3U) };
    std::swap(unsorted.pixels[0], unsorted.pixels[2]);
    std::vector<std::uint8_t> out {};
    EXPECT_EQ(SightlineNucTableFile::writeDead(unsorted, out), TableFileError::NotSorted);

    DeadTable dup { makeDead(3U) };
    dup.pixels[1] = dup.pixels[0];
    EXPECT_EQ(SightlineNucTableFile::writeDead(dup, out), TableFileError::Duplicate);

    // A file with an out-of-order list is rejected on parse as well.
    std::vector<std::uint8_t> bytes { writeOk(makeDead(3U)) };
    bytes[45] = 0xFFU; // first row -> 0x00FF > 1, so entry 0 > entry 1
    DeadTable dst {};
    EXPECT_EQ(SightlineNucTableFile::parseDead(ByteView { bytes }, dst), TableFileError::NotSorted);
}

TEST(SightlineNucTableFile, DeadRejectsBadReplaceSettings)
{
    std::vector<std::uint8_t> out {};
    DeadTable t { makeDead(3U) };
    t.replaceMethod = 3;
    EXPECT_EQ(SightlineNucTableFile::writeDead(t, out), TableFileError::BadValue);

    t = makeDead(3U);
    t.numReplace = 0U;
    EXPECT_EQ(SightlineNucTableFile::writeDead(t, out), TableFileError::BadValue);
    t.numReplace = 9U;
    EXPECT_EQ(SightlineNucTableFile::writeDead(t, out), TableFileError::BadValue);

    std::vector<std::uint8_t> bytes { writeOk(makeDead(3U)) };
    bytes[40] = 7U; // replaceMethod
    DeadTable dst {};
    EXPECT_EQ(SightlineNucTableFile::parseDead(ByteView { bytes }, dst), TableFileError::BadValue);
}

TEST(SightlineNucTableFile, V3RejectsReplacementOffsets)
{
    DeadTable t { makeDead(3U) };
    t.pixels[0].rowOff = 3;
    std::vector<std::uint8_t> out {};
    EXPECT_EQ(SightlineNucTableFile::writeDead(t, out), TableFileError::BadValue);
}

TEST(SightlineNucTableFile, SortDeadOrdersAndDedupes)
{
    std::vector<DeadPixelEntry> px {
        DeadPixelEntry { 5U, 2U, 0, 0 },
        DeadPixelEntry { 1U, 9U, 0, 0 },
        DeadPixelEntry { 5U, 1U, 0, 0 },
        DeadPixelEntry { 1U, 9U, 0, 0 },
        DeadPixelEntry { 1U, 3U, 0, 0 },
    };
    EXPECT_FALSE(SightlineNucTableFile::isSorted(px));
    EXPECT_EQ(SightlineNucTableFile::sortDead(px), 1U);
    ASSERT_EQ(px.size(), 4U);
    EXPECT_TRUE(SightlineNucTableFile::isSorted(px));
    EXPECT_EQ(px[0].row, 1U);
    EXPECT_EQ(px[0].col, 3U);
    EXPECT_EQ(px[1].col, 9U);
    EXPECT_EQ(px[2].row, 5U);
    EXPECT_EQ(px[2].col, 1U);
    EXPECT_EQ(px[3].col, 2U);
}

TEST(SightlineNucTableFile, ErrorTextIsNeverNull)
{
    for (std::uint8_t i { 0U }; i <= static_cast<std::uint8_t>(TableFileError::TooLarge); ++i) {
        const char* txt { SightlineNucTableFile::errorText(static_cast<TableFileError>(i)) };
        ASSERT_NE(txt, nullptr);
        EXPECT_FALSE(std::string { txt }.empty());
    }
}

} // namespace Sightline
