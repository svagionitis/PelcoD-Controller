#include "KlvEncoder.h"
#include "KlvStreamScanner.h"
#include <gtest/gtest.h>
#include <vector>

using namespace Klv;

TEST(KlvStreamScannerTest, ExtractsPacketWithSurroundingGarbage) {
    UasDatalinkMessage msg;
    msg.missionId = "TEST_SCANNER";
    msg.platformHeadingDeg = 270.0;
    const auto validPacket = KlvEncoder::encode(msg);

    // Surround with garbage bytes
    std::vector<std::uint8_t> stream;
    stream.insert(stream.end(), { 0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34 });
    stream.insert(stream.end(), validPacket.begin(), validPacket.end());
    stream.insert(stream.end(), { 0xAA, 0xBB, 0xCC });

    KlvStreamScanner scanner;
    int packetCount = 0;
    std::string receivedMission;

    scanner.setMessageCallback([&](const UasDatalinkMessage& received) {
        packetCount++;
        if (received.missionId) {
            receivedMission = *received.missionId;
        }
    });

    const std::size_t dispatched = scanner.processBytes(stream.data(), stream.size());
    EXPECT_EQ(dispatched, 1U);
    EXPECT_EQ(packetCount, 1);
    EXPECT_EQ(receivedMission, "TEST_SCANNER");
}

TEST(KlvStreamScannerTest, FragmentedPacketReassembly) {
    UasDatalinkMessage msg;
    msg.missionId = "FRAGMENTED_STREAM";
    msg.sensorTrueAltitudeM = 500.0;
    const auto validPacket = KlvEncoder::encode(msg);

    KlvStreamScanner scanner;
    int packetCount = 0;
    scanner.setMessageCallback([&](const UasDatalinkMessage&) {
        packetCount++;
    });

    // Split packet across 3 separate network chunks
    const std::size_t chunk1Size = 10U;
    const std::size_t chunk2Size = 25U;

    EXPECT_EQ(scanner.processBytes(validPacket.data(), chunk1Size), 0U);
    EXPECT_EQ(packetCount, 0);

    EXPECT_EQ(scanner.processBytes(validPacket.data() + chunk1Size, chunk2Size), 0U);
    EXPECT_EQ(packetCount, 0);

    const std::size_t chunk3Size = validPacket.size() - chunk1Size - chunk2Size;
    EXPECT_EQ(scanner.processBytes(validPacket.data() + chunk1Size + chunk2Size, chunk3Size), 1U);
    EXPECT_EQ(packetCount, 1);
}

TEST(KlvStreamScannerTest, MultipleConsecutivePackets) {
    UasDatalinkMessage msg1;
    msg1.platformHeadingDeg = 10.0;
    const auto pkt1 = KlvEncoder::encode(msg1);

    UasDatalinkMessage msg2;
    msg2.platformHeadingDeg = 20.0;
    const auto pkt2 = KlvEncoder::encode(msg2);

    std::vector<std::uint8_t> stream;
    stream.insert(stream.end(), pkt1.begin(), pkt1.end());
    stream.insert(stream.end(), pkt2.begin(), pkt2.end());

    KlvStreamScanner scanner;
    std::vector<double> headings;
    scanner.setMessageCallback([&](const UasDatalinkMessage& m) {
        if (m.platformHeadingDeg) {
            headings.push_back(*m.platformHeadingDeg);
        }
    });

    EXPECT_EQ(scanner.processBytes(stream.data(), stream.size()), 2U);
    ASSERT_EQ(headings.size(), 2U);
    EXPECT_NEAR(headings[0], 10.0, 0.01);
    EXPECT_NEAR(headings[1], 20.0, 0.01);
}
