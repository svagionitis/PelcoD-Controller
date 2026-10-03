#include "Misb1201.h"
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <vector>

using namespace Klv;

// =============================================================================
// MISB ST 1201.5 Appendix A - Test 1: IMAPA(0.0, 100.0, 1E-5)
// =============================================================================
TEST(TestMisb1201, AppendixATest1StartingPointA) {
    const double minVal = 0.0;
    const double maxVal = 100.0;
    const double precision = 1.0e-5;

    const std::size_t len = Misb1201::computeLength(minVal, maxVal, precision);
    EXPECT_EQ(len, 4U);

    const Misb1201Params params = Misb1201::computeParams(minVal, maxVal, len);
    EXPECT_DOUBLE_EQ(params.forwardScale, 1.6777216e7);
    EXPECT_DOUBLE_EQ(params.reverseScale, 5.9604644775390625e-8);
    EXPECT_DOUBLE_EQ(params.zeroOffset, 0.0);

    // Test specific values from ST 1201.5 Table Test 1
    EXPECT_EQ(Misb1201::encode(0.0, params), 0x00000000ULL);
    EXPECT_EQ(Misb1201::encode(10.1, params), 0x0A199999ULL);
    EXPECT_EQ(Misb1201::encode(20.2, params), 0x14333333ULL);
    EXPECT_EQ(Misb1201::encode(50.5, params), 0x32800000ULL);
    EXPECT_EQ(Misb1201::encode(100.0, params), 0x64000000ULL);

    // Special values
    EXPECT_EQ(Misb1201::encode(std::numeric_limits<double>::infinity(), params), 0xC8000000ULL);
    EXPECT_EQ(Misb1201::encode(-std::numeric_limits<double>::infinity(), params), 0xE8000000ULL);
    EXPECT_EQ(Misb1201::encode(std::numeric_limits<double>::quiet_NaN(), params), 0xD0000000ULL);
    EXPECT_EQ(Misb1201::encode(-1.0, params), 0xE0000000ULL);    // IMAP_BELOW_MINIMUM
    EXPECT_EQ(Misb1201::encode(101.0, params), 0xE1000000ULL);   // IMAP_ABOVE_MAXIMUM

    // Reverse mapping validations
    const auto res0 = Misb1201::decode(0x00000000ULL, params);
    EXPECT_TRUE(res0.isValid());
    EXPECT_NEAR(res0.value, 0.0, 1.0e-6);

    const auto res101 = Misb1201::decode(0x0A199999ULL, params);
    EXPECT_TRUE(res101.isValid());
    EXPECT_NEAR(res101.value, 10.1, 1.0e-6);

    const auto resNan = Misb1201::decode(0xD0000000ULL, params);
    EXPECT_EQ(resNan.status, Misb1201SpecialValue::PositiveQuietNan);
    EXPECT_TRUE(std::isnan(resNan.value));

    const auto resPosInf = Misb1201::decode(0xC8000000ULL, params);
    EXPECT_EQ(resPosInf.status, Misb1201SpecialValue::PositiveInfinity);
    EXPECT_TRUE(std::isinf(resPosInf.value));
    EXPECT_GT(resPosInf.value, 0.0);

    const auto resNegInf = Misb1201::decode(0xE8000000ULL, params);
    EXPECT_EQ(resNegInf.status, Misb1201SpecialValue::NegativeInfinity);
    EXPECT_TRUE(std::isinf(resNegInf.value));
    EXPECT_LT(resNegInf.value, 0.0);

    const auto resBelow = Misb1201::decode(0xE0000000ULL, params);
    EXPECT_EQ(resBelow.status, Misb1201SpecialValue::BelowMinimum);
    EXPECT_DOUBLE_EQ(resBelow.value, 0.0);

    const auto resAbove = Misb1201::decode(0xE1000000ULL, params);
    EXPECT_EQ(resAbove.status, Misb1201SpecialValue::AboveMaximum);
    EXPECT_DOUBLE_EQ(resAbove.value, 100.0);
}

// =============================================================================
// MISB ST 1201.5 Appendix A - Test 2: IMAPB(0.0, 100.0, 3)
// =============================================================================
TEST(TestMisb1201, AppendixATest2StartingPointB) {
    const double minVal = 0.0;
    const double maxVal = 100.0;
    const std::size_t len = 3U;

    const Misb1201Params params = Misb1201::computeParams(minVal, maxVal, len);
    EXPECT_DOUBLE_EQ(params.forwardScale, 65536.0);
    EXPECT_DOUBLE_EQ(params.reverseScale, 1.52587890625e-5);
    EXPECT_DOUBLE_EQ(params.zeroOffset, 0.0);

    EXPECT_EQ(Misb1201::encode(0.0, params), 0x000000ULL);
    EXPECT_EQ(Misb1201::encode(10.1, params), 0x0A1999ULL);
    EXPECT_EQ(Misb1201::encode(20.2, params), 0x143333ULL);
    EXPECT_EQ(Misb1201::encode(30.3, params), 0x1E4CCCULL);
    EXPECT_EQ(Misb1201::encode(100.0, params), 0x640000ULL);

    EXPECT_EQ(Misb1201::encode(std::numeric_limits<double>::infinity(), params), 0xC80000ULL);
    EXPECT_EQ(Misb1201::encode(-std::numeric_limits<double>::infinity(), params), 0xE80000ULL);
    EXPECT_EQ(Misb1201::encode(std::numeric_limits<double>::quiet_NaN(), params), 0xD00000ULL);
    EXPECT_EQ(Misb1201::encode(-1.0, params), 0xE00000ULL);
    EXPECT_EQ(Misb1201::encode(101.0, params), 0xE10000ULL);

    const auto res101 = Misb1201::decode(0x0A1999ULL, params);
    EXPECT_TRUE(res101.isValid());
    EXPECT_NEAR(res101.value, 10.09999, 1.0e-4);
}

// =============================================================================
// MISB ST 1201.5 Appendix A - Test 3: IMAPB(-9.9, 110.0, 3)
// =============================================================================
TEST(TestMisb1201, AppendixATest3ZeroOffset) {
    const double minVal = -9.9;
    const double maxVal = 110.0;
    const std::size_t len = 3U;

    const Misb1201Params params = Misb1201::computeParams(minVal, maxVal, len);
    EXPECT_DOUBLE_EQ(params.forwardScale, 65536.0);
    EXPECT_DOUBLE_EQ(params.reverseScale, 1.52587890625e-5);
    EXPECT_NEAR(params.zeroOffset, 0.6, 1.0e-9);

    EXPECT_EQ(Misb1201::encode(-9.9, params), 0x000000ULL);
    EXPECT_EQ(Misb1201::encode(0.225, params), 0x0A2000ULL);
    EXPECT_EQ(Misb1201::encode(110.0, params), 0x77E667ULL);
    EXPECT_EQ(Misb1201::encode(0.0, params), 0x09E667ULL); // Zero mapping!

    const auto resZero = Misb1201::decode(0x09E667ULL, params);
    EXPECT_TRUE(resZero.isValid());
    EXPECT_NEAR(resZero.value, 0.0, 1.0e-5);
}

// =============================================================================
// Byte serialization and deserialization
// =============================================================================
TEST(TestMisb1201, ByteSerializationRoundTrip) {
    const double minVal = -900.0;
    const double maxVal = 19000.0;
    const std::size_t len = 2U;

    std::vector<std::uint8_t> buffer;
    Misb1201::encodeBytes(1250.0, minVal, maxVal, len, buffer);
    ASSERT_EQ(buffer.size(), 2U);

    const auto decoded = Misb1201::decodeBytes(buffer.data(), buffer.size(), minVal, maxVal);
    EXPECT_TRUE(decoded.isValid());
    EXPECT_NEAR(decoded.value, 1250.0, 1.0);
}

// =============================================================================
// Edge Cases & Parameter Validation
// =============================================================================
TEST(TestMisb1201, EdgeCases) {
    // Negative zero handling
    const auto encNegZero = Misb1201::encode(-0.0, -100.0, 100.0, 2U);
    const auto encPosZero = Misb1201::encode(0.0, -100.0, 100.0, 2U);
    EXPECT_EQ(encNegZero, encPosZero);

    // Invalid parameters
    EXPECT_EQ(Misb1201::computeLength(10.0, 5.0, 0.1), 0U);
    EXPECT_EQ(Misb1201::computeParams(10.0, 5.0, 2U).lengthBytes, 0U);

    const auto decodedNull = Misb1201::decodeBytes(nullptr, 0U, 0.0, 10.0);
    EXPECT_EQ(decodedNull.status, Misb1201SpecialValue::Reserved);
}
