/// @file TestSightlineCrc8.cpp
/// @brief Unit tests verifying Sightline SLA CRC-8 checksum calculation against ICD reference vectors.

#include "SightlineCrc8.h"

#include <gtest/gtest.h>

#include <vector>

namespace Sightline {
namespace {

    /// @brief Verify checksum against official ICD example on page 3.
    /// @details Test vector: Message ID = 0x07 -> CRC-8 should equal 0xDD (221).
    TEST(TestSightlineCrc8, OfficialIcdExample1)
    {
        const std::vector<std::uint8_t> data { 0x07U };
        const std::uint8_t crc = SightlineCrc8::compute(data.data(), data.size());
        EXPECT_EQ(crc, 0xDDU);
        EXPECT_TRUE(SightlineCrc8::validate(data.data(), data.size(), 0xDDU));
    }

    /// @brief Verify checksum against official ICD example on page 4.
    /// @details Test vector: Length = 3, Message ID = 0x01, Mode = 0x02 -> Checksum = 0xBC.
    TEST(TestSightlineCrc8, OfficialIcdExample2)
    {
        const std::vector<std::uint8_t> data { 0x01U, 0x02U };
        const std::uint8_t crc = SightlineCrc8::compute(data.data(), data.size());
        EXPECT_EQ(crc, 0xBCU);
        EXPECT_TRUE(SightlineCrc8::validate(data.data(), data.size(), 0xBCU));
    }

    /// @brief Verify checksum for SLAGetParameters(0x00) from ICD section Command and Control.
    /// @details Full packet: 0x51, 0xAC, 0x03, 0x28, 0x00, 0x73 -> Checksum = 0x73.
    TEST(TestSightlineCrc8, GetParametersVersionQuery)
    {
        const std::vector<std::uint8_t> data { 0x28U, 0x00U };
        const std::uint8_t crc = SightlineCrc8::compute(data.data(), data.size());
        EXPECT_EQ(crc, 0x73U);
        EXPECT_TRUE(SightlineCrc8::validate(data.data(), data.size(), 0x73U));
    }

    /// @brief Verify empty buffer returns initial seed.
    TEST(TestSightlineCrc8, EmptyBuffer)
    {
        EXPECT_EQ(SightlineCrc8::compute(nullptr, 0U), SightlineCrc8::InitialSeed);
    }

} // namespace
} // namespace Sightline
