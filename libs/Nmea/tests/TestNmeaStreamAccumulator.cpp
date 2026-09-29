#include "NmeaStreamAccumulator.h"

#include <future>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace Nmea {
namespace {

    TEST(TestNmeaStreamAccumulator, SingleCompleteSentence)
    {
        NmeaStreamAccumulator acc {};
        const std::string input = "$HEHDT,341.8,T*21\r\n";
        const auto result = acc.push(input, true);

        ASSERT_EQ(result.size(), 1U);
        EXPECT_EQ(result[0], "$HEHDT,341.8,T*21\r\n");
        EXPECT_EQ(acc.size(), 0U);
    }

    TEST(TestNmeaStreamAccumulator, FragmentedIngest)
    {
        NmeaStreamAccumulator acc {};
        EXPECT_TRUE(acc.push("$HEHD", true).empty());
        EXPECT_TRUE(acc.push("T,341.", true).empty());
        const auto res = acc.push("8,T*21\r\n", true);

        ASSERT_EQ(res.size(), 1U);
        EXPECT_EQ(res[0], "$HEHDT,341.8,T*21\r\n");
        EXPECT_EQ(acc.size(), 0U);
    }

    TEST(TestNmeaStreamAccumulator, MultipleSentencesInSingleChunk)
    {
        NmeaStreamAccumulator acc {};
        const std::string multi = "$HEHDT,341.8,T*21\r\n!AIVDM,1,1,,B,177KQJ001p5nk70GwLF85jww0+RA,0*67\r\n";
        const auto res = acc.push(multi, true);

        ASSERT_EQ(res.size(), 2U);
        EXPECT_EQ(res[0], "$HEHDT,341.8,T*21\r\n");
        EXPECT_EQ(res[1], "!AIVDM,1,1,,B,177KQJ001p5nk70GwLF85jww0+RA,0*67\r\n");
        EXPECT_EQ(acc.size(), 0U);
    }

    TEST(TestNmeaStreamAccumulator, DiscardGarbagePrefix)
    {
        NmeaStreamAccumulator acc {};
        const std::string noisy = "GARBAGE_NOISE_12345$HEHDT,341.8,T*21\r\n";
        const auto res = acc.push(noisy, true);

        ASSERT_EQ(res.size(), 1U);
        EXPECT_EQ(res[0], "$HEHDT,341.8,T*21\r\n");
        EXPECT_EQ(acc.size(), 0U);
    }

    TEST(TestNmeaStreamAccumulator, FilterInvalidChecksum)
    {
        NmeaStreamAccumulator acc {};
        const std::string invalid = "$HEHDT,341.8,T*99\r\n"; // 99 is wrong checksum
        const auto resFiltered = acc.push(invalid, true);
        EXPECT_TRUE(resFiltered.empty());

        // When validateChecksum is false, it accepts the frame
        const auto resUnfiltered = acc.push(invalid, false);
        ASSERT_EQ(resUnfiltered.size(), 1U);
        EXPECT_EQ(resUnfiltered[0], "$HEHDT,341.8,T*99\r\n");
    }

    TEST(TestNmeaStreamAccumulator, BufferOverflowProtection)
    {
        NmeaStreamAccumulator acc { 128U };
        std::string garbage(200U, 'X');
        const auto garbageRes = acc.push(garbage, true);
        EXPECT_TRUE(garbageRes.empty());
        EXPECT_LE(acc.size(), 128U);

        // After garbage, incoming valid sentence should still be successfully framed
        const auto res = acc.push("$HEHDT,341.8,T*21\r\n", true);
        ASSERT_EQ(res.size(), 1U);
        EXPECT_EQ(res[0], "$HEHDT,341.8,T*21\r\n");
    }

    TEST(TestNmeaStreamAccumulator, MultiThreadedIngest)
    {
        NmeaStreamAccumulator acc {};
        constexpr int kIterations = 50;

        auto producer = [&acc]() {
            for (int i = 0; i < kIterations; ++i) {
                static_cast<void>(acc.push("$HEHDT,341.8,T*21\r\n", true));
            }
        };

        std::thread t1(producer);
        std::thread t2(producer);

        t1.join();
        t2.join();

        // All sentences should have been extracted or processed safely without crashes
        EXPECT_EQ(acc.size(), 0U);
    }

} // namespace
} // namespace Nmea
