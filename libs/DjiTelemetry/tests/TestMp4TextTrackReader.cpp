/// @file TestMp4TextTrackReader.cpp
/// @brief Unit tests for the ISO-BMFF tx3g text track reader.

#include "Mp4TestBuilder.h"
#include "Mp4TextTrackReader.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

using Dji::Mp4TextTrackReader;
using DjiTest::buildMp4;
using DjiTest::Mp4BuildOptions;
using DjiTest::TempFile;

namespace {

/// @brief Returns N distinct sample strings.
/// @param[in] n Number of samples.
/// @return Sample texts.
std::vector<std::string> makeTexts(std::size_t n)
{
    std::vector<std::string> texts {};
    for (std::size_t i { 0U }; i < n; ++i) {
        texts.push_back("FrameCnt: " + std::to_string(i) + " [latitude: 1.0] [longitude: 2.0]");
    }
    return texts;
}

/// @brief Asserts that every sample can be read back and is timed at i * delta / timescale.
/// @param[in] reader Opened reader.
/// @param[in] texts Expected texts.
void expectRoundTrip(Mp4TextTrackReader& reader, const std::vector<std::string>& texts)
{
    ASSERT_EQ(reader.sampleCount(), texts.size());
    for (std::size_t i { 0U }; i < texts.size(); ++i) {
        const auto text { reader.readText(i) };
        ASSERT_TRUE(text.has_value()) << "sample " << i;
        EXPECT_EQ(*text, texts[i]);
        EXPECT_NEAR(reader.samples()[i].timeSec, static_cast<double>(i) / 30.0, 1e-9);
    }
}

} // namespace

TEST(Mp4TextTrackReaderTest, MoovAtEndLikeDji)
{
    Mp4BuildOptions o {};
    o.texts = makeTexts(5U);
    const TempFile file { buildMp4(o), "reader_moov_end" };

    Mp4TextTrackReader reader {};
    ASSERT_TRUE(reader.open(file.path()));
    expectRoundTrip(reader, o.texts);
    EXPECT_EQ(reader.timescale(), 30000U);

    ASSERT_TRUE(reader.creationUtcUs().has_value());
    EXPECT_EQ(*reader.creationUtcUs(), 1790252455ULL * 1000000ULL);
    EXPECT_EQ(reader.encoderName(), "DJI DJI Matrice 4T");
    ASSERT_TRUE(reader.videoAspect().has_value());
    EXPECT_DOUBLE_EQ(*reader.videoAspect(), 1.25);
}

TEST(Mp4TextTrackReaderTest, MoovFirst)
{
    Mp4BuildOptions o {};
    o.texts = makeTexts(4U);
    o.moovFirst = true;
    const TempFile file { buildMp4(o), "reader_moov_first" };

    Mp4TextTrackReader reader {};
    ASSERT_TRUE(reader.open(file.path()));
    expectRoundTrip(reader, o.texts);
}

TEST(Mp4TextTrackReaderTest, Co64AndLargeMdat)
{
    Mp4BuildOptions o {};
    o.texts = makeTexts(3U);
    o.useCo64 = true;
    o.largeMdat = true;
    const TempFile file { buildMp4(o), "reader_co64" };

    Mp4TextTrackReader reader {};
    ASSERT_TRUE(reader.open(file.path()));
    expectRoundTrip(reader, o.texts);
}

TEST(Mp4TextTrackReaderTest, MultiSampleChunksWithRemainder)
{
    Mp4BuildOptions o {};
    o.texts = makeTexts(7U);
    o.samplesPerChunk = 3U; // chunks: 3, 3, 1 -> two stsc entries
    const TempFile file { buildMp4(o), "reader_chunks" };

    Mp4TextTrackReader reader {};
    ASSERT_TRUE(reader.open(file.path()));
    expectRoundTrip(reader, o.texts);
}

TEST(Mp4TextTrackReaderTest, FixedSampleSize)
{
    Mp4BuildOptions o {};
    o.texts = { "AAAA", "BBBB", "CCCC" };
    o.fixedSampleSize = true;
    const TempFile file { buildMp4(o), "reader_fixed" };

    Mp4TextTrackReader reader {};
    ASSERT_TRUE(reader.open(file.path()));
    expectRoundTrip(reader, o.texts);
}

TEST(Mp4TextTrackReaderTest, MultiEntryStts)
{
    Mp4BuildOptions o {};
    o.texts = makeTexts(5U);
    o.splitStts = true; // deltas: 1000, 1000, 2000, 2000, 2000
    const TempFile file { buildMp4(o), "reader_stts" };

    Mp4TextTrackReader reader {};
    ASSERT_TRUE(reader.open(file.path()));
    ASSERT_EQ(reader.sampleCount(), 5U);
    const std::vector<double> expected { 0.0, 1.0 / 30.0, 2.0 / 30.0, 4.0 / 30.0, 6.0 / 30.0 };
    for (std::size_t i { 0U }; i < expected.size(); ++i) {
        EXPECT_NEAR(reader.samples()[i].timeSec, expected[i], 1e-9);
    }
}

TEST(Mp4TextTrackReaderTest, EditListShiftsTimes)
{
    Mp4BuildOptions o {};
    o.texts = makeTexts(3U);
    o.elstMediaTime = 2000; // skip 2000/30000 s of media
    o.emptyEditMovieUnits = 500U; // 0.5 s initial delay (movie timescale 1000)
    const TempFile file { buildMp4(o), "reader_elst" };

    Mp4TextTrackReader reader {};
    ASSERT_TRUE(reader.open(file.path()));
    ASSERT_EQ(reader.sampleCount(), 3U);
    // timeSec = emptyEdit + (dts - mediaTime) / timescale
    EXPECT_NEAR(reader.samples()[0].timeSec, 0.5 + (0.0 - 2000.0) / 30000.0, 1e-9);
    EXPECT_NEAR(reader.samples()[1].timeSec, 0.5 + (1000.0 - 2000.0) / 30000.0, 1e-9);
    EXPECT_NEAR(reader.samples()[2].timeSec, 0.5, 1e-9);
}

TEST(Mp4TextTrackReaderTest, NoTextTrackFails)
{
    Mp4BuildOptions o {};
    o.texts = makeTexts(2U);
    o.includeTextTrack = false;
    const TempFile file { buildMp4(o), "reader_no_text" };

    Mp4TextTrackReader reader {};
    EXPECT_FALSE(reader.open(file.path()));
    EXPECT_EQ(reader.sampleCount(), 0U);
}

TEST(Mp4TextTrackReaderTest, MissingAndNonMp4FilesFail)
{
    Mp4TextTrackReader reader {};
    EXPECT_FALSE(reader.open("/nonexistent/dji_missing.mp4"));

    const DjiTest::Bytes junk(512U, 0x47U); // looks like TS sync bytes, not MP4
    const TempFile file { junk, "reader_junk" };
    EXPECT_FALSE(reader.open(file.path()));
}

TEST(Mp4TextTrackReaderTest, TruncatedMoovFails)
{
    Mp4BuildOptions o {};
    o.texts = makeTexts(4U);
    DjiTest::Bytes bytes { buildMp4(o) };
    bytes.resize(bytes.size() - 40U);
    const TempFile file { bytes, "reader_truncated" };

    Mp4TextTrackReader reader {};
    EXPECT_FALSE(reader.open(file.path()));
}

TEST(Mp4TextTrackReaderTest, ChunkOffsetsOutsideFileFail)
{
    Mp4BuildOptions o {};
    o.texts = makeTexts(2U);
    o.chunkOffsetBias = 0x7FFF0000U;
    const TempFile file { buildMp4(o), "reader_bad_offset" };

    Mp4TextTrackReader reader {};
    EXPECT_FALSE(reader.open(file.path()));
}

TEST(Mp4TextTrackReaderTest, ReadOutOfRangeReturnsNullopt)
{
    Mp4BuildOptions o {};
    o.texts = makeTexts(1U);
    const TempFile file { buildMp4(o), "reader_oob" };

    Mp4TextTrackReader reader {};
    ASSERT_TRUE(reader.open(file.path()));
    EXPECT_FALSE(reader.readText(1U).has_value());
    EXPECT_FALSE(reader.readText(1000000U).has_value());
}

TEST(Mp4TextTrackReaderTest, ReopenResetsState)
{
    Mp4BuildOptions o {};
    o.texts = makeTexts(3U);
    const TempFile good { buildMp4(o), "reader_reopen_good" };
    const DjiTest::Bytes junk(64U, 0x00U);
    const TempFile bad { junk, "reader_reopen_bad" };

    Mp4TextTrackReader reader {};
    ASSERT_TRUE(reader.open(good.path()));
    EXPECT_EQ(reader.sampleCount(), 3U);
    EXPECT_FALSE(reader.open(bad.path()));
    EXPECT_EQ(reader.sampleCount(), 0U);
    EXPECT_FALSE(reader.creationUtcUs().has_value());
}
