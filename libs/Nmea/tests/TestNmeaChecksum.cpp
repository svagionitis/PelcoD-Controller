#include "NmeaChecksum.h"

#include <gtest/gtest.h>

namespace Nmea {
namespace {

    TEST(TestNmeaChecksum, CalculateStandardSentences)
    {
        // Real-world sample sentences
        EXPECT_EQ(NmeaChecksum::calculate("$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,"), 0x76);
        EXPECT_EQ(
            NmeaChecksum::calculate("$GPRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091218,,,A"), 0x5C);
        EXPECT_EQ(NmeaChecksum::calculate("$HEHDT,341.8,T"), 0x21);
        EXPECT_EQ(NmeaChecksum::calculate("!AIVDM,1,1,,B,177KQJ001p5nk70GwLF85jww0+RA,0*67"), 0x67);
    }

    TEST(TestNmeaChecksum, ValidateValidSentences)
    {
        EXPECT_TRUE(
            NmeaChecksum::validate("$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76\r\n"));
        EXPECT_TRUE(
            NmeaChecksum::validate("$GPRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091218,,,A*5C\n"));
        EXPECT_TRUE(NmeaChecksum::validate("$HEHDT,341.8,T*21"));
        EXPECT_TRUE(NmeaChecksum::validate("!AIVDM,1,1,,B,177KQJ001p5nk70GwLF85jww0+RA,0*67"));
    }

    TEST(TestNmeaChecksum, ValidateInvalidSentences)
    {
        // Corrupted checksum
        EXPECT_FALSE(NmeaChecksum::validate("$HEHDT,341.8,T*99"));
        // Missing asterisk
        EXPECT_FALSE(NmeaChecksum::validate("$HEHDT,341.8,T"));
        // Truncated checksum
        EXPECT_FALSE(NmeaChecksum::validate("$HEHDT,341.8,T*1"));
        // Non-hex checksum character
        EXPECT_FALSE(NmeaChecksum::validate("$HEHDT,341.8,T*1Z"));
        // Empty
        EXPECT_FALSE(NmeaChecksum::validate(""));
    }

    TEST(TestNmeaChecksum, FrameSentence)
    {
        const std::string framed = NmeaChecksum::frameSentence("HEHDT,341.8,T");
        EXPECT_EQ(framed, "$HEHDT,341.8,T*21\r\n");
        EXPECT_TRUE(NmeaChecksum::validate(framed));

        // Framing sentence with leading !
        const std::string framedAis = NmeaChecksum::frameSentence("AIVDM,1,1,,B,177KQJ001p5nk70GwLF85jww0+RA,0", '!');
        EXPECT_EQ(framedAis, "!AIVDM,1,1,,B,177KQJ001p5nk70GwLF85jww0+RA,0*67\r\n");
        EXPECT_TRUE(NmeaChecksum::validate(framedAis));
    }

} // namespace
} // namespace Nmea
