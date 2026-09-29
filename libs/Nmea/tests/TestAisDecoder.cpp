#include "AisDecoder.h"

#include <gtest/gtest.h>

namespace Nmea {
namespace {

    TEST(TestAisDecoder, SinglePartClassAPositionReport)
    {
        AisDecoder decoder {};
        const std::string sentence = "!AIVDM,1,1,,B,177KQJ001p5nk70GwLF85jww0+RA,0*67\r\n";

        AisVesselTarget target {};
        ASSERT_TRUE(decoder.decodeSentence(sentence, target, true));

        EXPECT_EQ(target.messageType, AisMessageType::ClassAPosition1);
        EXPECT_EQ(target.mmsi, 477553000U);
        EXPECT_EQ(target.navStatus, AisNavStatus::UnderWayUsingEngine);
        EXPECT_NEAR(target.speedOverGroundKnots, 12.0, 1e-1);
        EXPECT_TRUE(target.positionValid);
        EXPECT_NEAR(target.coordinates.longitudeDeg, 81.876, 1e-4);
        EXPECT_NEAR(target.coordinates.latitudeDeg, 41.9278, 1e-4);
        EXPECT_NEAR(target.courseOverGroundDegrees, 207.1, 1e-1);
        EXPECT_NEAR(target.trueHeadingDegrees, 95.0, 1e-1);
    }

    TEST(TestAisDecoder, MultiPartClassAStaticVoyageData)
    {
        AisDecoder decoder {};

        // 2-part message for Type 5
        const std::string part1
            = "!AIVDM,2,1,3,A,539Lg1h2;=`048?7;?@t<D4r0EQ0hu8E80000016BhN<=7<jNEDSm51DQ0C@,0*19\r\n";
        const std::string part2 = "!AIVDM,2,2,3,A,00000000000,2*27\r\n";

        AisVesselTarget target {};

        // Part 1 should buffer and return false
        EXPECT_FALSE(decoder.decodeSentence(part1, target, true));
        EXPECT_FALSE(target.staticDataValid);

        // Part 2 should complete reassembly and return true
        ASSERT_TRUE(decoder.decodeSentence(part2, target, true));

        EXPECT_EQ(target.messageType, AisMessageType::ClassAStaticVoyage5);
        EXPECT_EQ(target.mmsi, 211234567U);
        EXPECT_EQ(target.imoNumber, 9123456U);
        EXPECT_EQ(target.callSign, "ABC1234");
        EXPECT_EQ(target.vesselName, "OCEAN EXPLORER");
        EXPECT_EQ(target.shipType, 70U);

        EXPECT_EQ(target.dimensions.toBow, 150U);
        EXPECT_EQ(target.dimensions.toStern, 30U);
        EXPECT_EQ(target.dimensions.toPort, 12U);
        EXPECT_EQ(target.dimensions.toStarboard, 13U);
        EXPECT_EQ(target.dimensions.lengthMeters(), 180U);
        EXPECT_EQ(target.dimensions.beamMeters(), 25U);

        EXPECT_NEAR(target.draughtMeters, 8.5, 1e-1);
        EXPECT_EQ(target.destination, "ROTTERDAM");
        EXPECT_TRUE(target.staticDataValid);
    }

    TEST(TestAisDecoder, SinglePartClassBPositionReport)
    {
        AisDecoder decoder {};
        const std::string sentence = "!AIVDM,1,1,,B,B39i700000?4;7Wd:d403w<00000,0*25\r\n";

        AisVesselTarget target {};
        ASSERT_TRUE(decoder.decodeSentence(sentence, target, true));

        EXPECT_EQ(target.messageType, AisMessageType::ClassBPosition18);
        EXPECT_EQ(target.mmsi, 211568384U);
        EXPECT_TRUE(target.positionValid);
        EXPECT_NEAR(target.coordinates.longitudeDeg, 13.164185, 1e-4);
        EXPECT_NEAR(target.coordinates.latitudeDeg, 53.757762, 1e-4);
        EXPECT_NEAR(target.speedOverGroundKnots, 0.0, 1e-1);
        EXPECT_NEAR(target.trueHeadingDegrees, 510.0, 1e-1);
    }

    TEST(TestAisDecoder, RejectInvalidChecksum)
    {
        AisDecoder decoder {};
        // Corrupt checksum from *67 to *99
        const std::string badSentence = "!AIVDM,1,1,,B,177KQJ001p5nk70GwLF85jww0+RA,0*99\r\n";

        AisVesselTarget target {};
        EXPECT_FALSE(decoder.decodeSentence(badSentence, target, true));
    }

    TEST(TestAisDecoder, MultiPartFragmentTimeoutAndReset)
    {
        AisDecoder decoder {};
        decoder.setFragmentTtl(std::chrono::milliseconds(50));

        const std::string part1
            = "!AIVDM,2,1,3,A,539Lg1h2;=`048?7;?@t<D4r0EQ0hu8E80000016BhN<=7<jNEDSm51DQ0C@,0*19\r\n";
        AisVesselTarget target {};
        EXPECT_FALSE(decoder.decodeSentence(part1, target, true));

        // Reset should clear cache
        decoder.reset();

        // Trying to send part 2 directly should fail since part 1 was cleared
        const std::string part2 = "!AIVDM,2,2,3,A,00000000000,2*27\r\n";
        EXPECT_FALSE(decoder.decodeSentence(part2, target, true));
    }

} // namespace
} // namespace Nmea
