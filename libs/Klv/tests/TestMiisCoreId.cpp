#include "MiisCoreId.h"
#include "KlvEncoder.h"
#include "KlvParser.h"
#include "KlvTypes.h"
#include "St1607Encoder.h"
#include "St1607Parser.h"
#include "St1607Types.h"
#include "VmtiEncoder.h"
#include "VmtiParser.h"
#include "VmtiTypes.h"

#include <gtest/gtest.h>
#include <array>
#include <string>
#include <vector>

namespace Klv {
namespace {

TEST(TestMiisCoreId, UuidStringParsingAndFormatting) {
    const std::string canonical = "f592f023-7336-4af8-aa91-62c00f2eb2da";
    Uuid u {};
    EXPECT_TRUE(Uuid::fromString(canonical, u));
    EXPECT_FALSE(u.isNull());
    EXPECT_TRUE(u.isValidVariant());
    EXPECT_EQ(u.version(), 4U);
    EXPECT_EQ(u.toString(false), canonical);
    EXPECT_EQ(u.toUrn(), "urn:uuid:" + canonical);

    // Test with "urn:uuid:" prefix
    Uuid u2 {};
    EXPECT_TRUE(Uuid::fromString("urn:uuid:F592F023-7336-4AF8-AA91-62C00F2EB2DA", u2));
    EXPECT_EQ(u, u2);

    // Test with 32 continuous hex chars
    Uuid u3 {};
    EXPECT_TRUE(Uuid::fromString("f592f02373364af8aa9162c00f2eb2da", u3));
    EXPECT_EQ(u, u3);

    // Invalid strings
    Uuid bad {};
    EXPECT_FALSE(Uuid::fromString("invalid-uuid-string", bad));
    EXPECT_FALSE(Uuid::fromString("f592f023-7336-4af8-aa91", bad));
}

TEST(TestMiisCoreId, UuidV4Generation) {
    const Uuid u1 = Uuid::generateV4();
    const Uuid u2 = Uuid::generateV4();

    EXPECT_FALSE(u1.isNull());
    EXPECT_FALSE(u2.isNull());
    EXPECT_NE(u1, u2);
    EXPECT_TRUE(u1.isValidVariant());
    EXPECT_TRUE(u2.isValidVariant());
    EXPECT_EQ(u1.version(), 4U);
    EXPECT_EQ(u2.version(), 4U);
}

TEST(TestMiisCoreId, UuidV5DeterministicGeneration) {
    // RFC 4122 DNS namespace UUID: 6ba7b810-9dad-11d1-80b4-00c04fd430c8
    Uuid dnsNs {};
    ASSERT_TRUE(Uuid::fromString("6ba7b810-9dad-11d1-80b4-00c04fd430c8", dnsNs));

    // RFC 4122 test vector for name "python.org":
    // 886313e1-3b8a-5372-9b90-0c9aee199e5d
    const Uuid u = Uuid::generateV5(dnsNs, "python.org");
    EXPECT_TRUE(u.isValidVariant());
    EXPECT_EQ(u.version(), 5U);
    EXPECT_EQ(u.toString(false), "886313e1-3b8a-5372-9b90-0c9aee199e5d");
}

TEST(TestMiisCoreId, UuidV1Generation) {
    const std::array<std::uint8_t, 6> mac = { 0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E };
    const std::uint64_t timestamp = 0x1ED58A00B7F83210ULL;
    const std::uint16_t clockSeq = 0x1234U;

    const Uuid u = Uuid::generateV1(mac, timestamp, clockSeq);
    EXPECT_TRUE(u.isValidVariant());
    EXPECT_EQ(u.version(), 1U);

    // Verify MAC node in bytes 10..15
    for (std::size_t i = 0U; i < 6U; ++i) {
        EXPECT_EQ(u.bytes[10U + i], mac[i]);
    }
}

TEST(TestMiisCoreId, SingleUuidRoundTrip) {
    Uuid sensorUuid {};
    ASSERT_TRUE(Uuid::fromString("f592f023-7336-4af8-aa91-62c00f2eb2da", sensorUuid));

    MiisCoreId original(sensorUuid);
    EXPECT_TRUE(original.validate());
    EXPECT_EQ(original.primaryUuid(), sensorUuid);

    const std::vector<std::uint8_t> encoded = original.encode();
    EXPECT_EQ(encoded.size(), 16U);

    MiisCoreId decoded {};
    EXPECT_EQ(decoded.decode(encoded.data(), encoded.size()), KlvStatus::Success);
    EXPECT_EQ(decoded.primaryUuid(), sensorUuid);
    EXPECT_EQ(original, decoded);
}

TEST(TestMiisCoreId, Composite33ByteRoundTrip) {
    Uuid sensorUuid {};
    Uuid platformUuid {};
    ASSERT_TRUE(Uuid::fromString("f592f023-7336-4af8-aa91-62c00f2eb2da", sensorUuid));
    ASSERT_TRUE(Uuid::fromString("16b74341-0008-41a0-be36-5b5ab96a3645", platformUuid));

    MiisCoreId original(sensorUuid, platformUuid);
    EXPECT_TRUE(original.validate());

    const std::vector<std::uint8_t> encoded = original.encode(false);
    EXPECT_EQ(encoded.size(), 33U);
    EXPECT_EQ(encoded[0], 1U); // Version

    MiisCoreId decoded {};
    EXPECT_EQ(decoded.decode(encoded.data(), encoded.size()), KlvStatus::Success);
    ASSERT_TRUE(decoded.sensorId.has_value());
    ASSERT_TRUE(decoded.platformId.has_value());
    EXPECT_EQ(decoded.sensorId->uuid, sensorUuid);
    EXPECT_EQ(decoded.platformId->uuid, platformUuid);
    EXPECT_EQ(original, decoded);
}

TEST(TestMiisCoreId, Composite35ByteQualityRoundTrip) {
    Uuid sensorUuid {};
    Uuid platformUuid {};
    ASSERT_TRUE(Uuid::fromString("f592f023-7336-4af8-aa91-62c00f2eb2da", sensorUuid));
    ASSERT_TRUE(Uuid::fromString("16b74341-0008-41a0-be36-5b5ab96a3645", platformUuid));

    MiisCoreId original(sensorUuid, platformUuid, MiisIdQuality::Physical, MiisIdQuality::Virtual);
    EXPECT_TRUE(original.validate());

    const std::vector<std::uint8_t> encoded = original.encode(true);
    EXPECT_EQ(encoded.size(), 35U);
    EXPECT_EQ(encoded[0], 1U);
    EXPECT_EQ(encoded[1], static_cast<std::uint8_t>(MiisIdQuality::Physical));
    EXPECT_EQ(encoded[18], static_cast<std::uint8_t>(MiisIdQuality::Virtual));

    MiisCoreId decoded {};
    EXPECT_EQ(decoded.decode(encoded.data(), encoded.size()), KlvStatus::Success);
    ASSERT_TRUE(decoded.sensorId.has_value());
    ASSERT_TRUE(decoded.platformId.has_value());
    EXPECT_EQ(decoded.sensorId->quality, MiisIdQuality::Physical);
    EXPECT_EQ(decoded.platformId->quality, MiisIdQuality::Virtual);
    EXPECT_EQ(decoded.sensorId->uuid, sensorUuid);
    EXPECT_EQ(decoded.platformId->uuid, platformUuid);
    EXPECT_EQ(original, decoded);
}

TEST(TestMiisCoreId, St0601Tag94Integration) {
    Uuid sensorUuid {};
    Uuid platformUuid {};
    ASSERT_TRUE(Uuid::fromString("f592f023-7336-4af8-aa91-62c00f2eb2da", sensorUuid));
    ASSERT_TRUE(Uuid::fromString("16b74341-0008-41a0-be36-5b5ab96a3645", platformUuid));

    UasDatalinkMessage originalMsg {};
    originalMsg.precisionTimeStampUs = 1690000000000ULL;
    originalMsg.missionId = "TEST_MISSION";
    originalMsg.miisCoreId = MiisCoreId(sensorUuid, platformUuid);

    const std::vector<std::uint8_t> packet = KlvEncoder::encode(originalMsg);
    EXPECT_GT(packet.size(), 50U);

    UasDatalinkMessage decodedMsg {};
    EXPECT_EQ(KlvParser::parse(packet.data(), packet.size(), decodedMsg), KlvStatus::Success);

    ASSERT_TRUE(decodedMsg.miisCoreId.has_value());
    EXPECT_EQ(*decodedMsg.miisCoreId, *originalMsg.miisCoreId);
    ASSERT_TRUE(decodedMsg.miisCoreId->sensorId.has_value());
    ASSERT_TRUE(decodedMsg.miisCoreId->platformId.has_value());
    EXPECT_EQ(decodedMsg.miisCoreId->sensorId->uuid, sensorUuid);
    EXPECT_EQ(decodedMsg.miisCoreId->platformId->uuid, platformUuid);
}

TEST(TestMiisCoreId, St1607Tag94Integration) {
    Uuid sensorUuid {};
    ASSERT_TRUE(Uuid::fromString("f592f023-7336-4af8-aa91-62c00f2eb2da", sensorUuid));

    SegmentLocalSet seg {};
    seg.miisCoreId = MiisCoreId(sensorUuid);

    std::vector<std::uint8_t> segBytes {};
    EXPECT_EQ(St1607Encoder::encodeSegment(seg, segBytes), KlvStatus::Success);

    SegmentLocalSet decSeg {};
    EXPECT_EQ(St1607Parser::parseSegment(segBytes.data(), segBytes.size(), decSeg), KlvStatus::Success);
    ASSERT_TRUE(decSeg.miisCoreId.has_value());
    EXPECT_EQ(*decSeg.miisCoreId, *seg.miisCoreId);

    // Test applyTo
    UasDatalinkMessage targetMsg {};
    decSeg.applyTo(targetMsg);
    ASSERT_TRUE(targetMsg.miisCoreId.has_value());
    EXPECT_EQ(targetMsg.miisCoreId->primaryUuid(), sensorUuid);
}

TEST(TestMiisCoreId, VmtiTag13Integration) {
    Uuid streamUuid = Uuid::generateV4();

    VmtiLocalSet vmti {};
    vmti.precisionTimeStampUs = 1700000000000ULL;
    vmti.systemName = "TRACKER_V1";
    vmti.miisId = MiisCoreId(streamUuid);

    const std::vector<std::uint8_t> packet = VmtiEncoder::encode(vmti, true);

    VmtiLocalSet decVmti {};
    EXPECT_EQ(VmtiParser::parse(packet.data(), packet.size(), decVmti, true), KlvStatus::Success);
    ASSERT_TRUE(decVmti.miisId.has_value());
    EXPECT_EQ(decVmti.miisId->primaryUuid(), streamUuid);
}

} // namespace
} // namespace Klv
