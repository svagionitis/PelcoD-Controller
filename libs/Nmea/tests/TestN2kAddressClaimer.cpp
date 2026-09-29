/// @file TestN2kAddressClaimer.cpp
/// @brief Unit tests for ISO 11783-5 / SAE J1939 Dynamic Address Claiming.

#include "n2k/N2kAddressClaimer.h"
#include "n2k/N2kDecoder.h"

#include <gtest/gtest.h>

#include <chrono>
#include <vector>

using namespace Nmea::N2k;

TEST(TestN2kAddressClaimer, NameComposition)
{
    const std::uint64_t name = N2kAddressClaimer::composeName(
        1001U,   // uniqueNumber
        2046U,   // manufacturerCode
        130U,    // deviceFunction
        25U,     // deviceClass
        true     // arbitraryAddress
    );

    EXPECT_NE(name, 0ULL);
    // Arbitrary address capable bit is bit 63 in J1939 (1ULL << 63)
    EXPECT_TRUE((name & (1ULL << 63)) != 0ULL);
}

TEST(TestN2kAddressClaimer, NormalClaimSequence)
{
    N2kAddressClaimer claimer(0xC0002046000003E9ULL, 0x23U);
    EXPECT_EQ(claimer.claimState(), AddressClaimState::Unclaimed);
    EXPECT_EQ(claimer.claimedAddress(), 254U);

    std::vector<CanFrame> emittedFrames {};
    claimer.setSendCallback([&](const CanFrame& f) {
        emittedFrames.push_back(f);
    });

    claimer.startClaiming();
    EXPECT_EQ(claimer.claimState(), AddressClaimState::WaitingForClaim);
    ASSERT_EQ(emittedFrames.size(), 1U);

    // Verify emitted frame is PGN 60928 from 0x23
    const auto pgn = N2kHeader::fromCanId(emittedFrames[0].id).pgn;
    EXPECT_EQ(pgn, static_cast<std::uint32_t>(Pgn::IsoAddressClaim));
    EXPECT_EQ(emittedFrames[0].id & 0xFFU, 0x23U);

    // Fast-forward 100ms -> still waiting
    const auto t0 = std::chrono::steady_clock::now();
    claimer.pollTimer(t0 + std::chrono::milliseconds(100));
    EXPECT_EQ(claimer.claimState(), AddressClaimState::WaitingForClaim);

    // Fast-forward 251ms -> successfully claimed
    claimer.pollTimer(t0 + std::chrono::milliseconds(260));
    EXPECT_EQ(claimer.claimState(), AddressClaimState::Claimed);
    EXPECT_EQ(claimer.claimedAddress(), 0x23U);
}

TEST(TestN2kAddressClaimer, ContentionLossAndReclaim)
{
    // Our node has name 0xC0002046000003E9ULL, preferred address 0x23
    N2kAddressClaimer claimer(0xC0002046000003E9ULL, 0x23U);

    std::vector<CanFrame> emitted {};
    claimer.setSendCallback([&](const CanFrame& f) {
        emitted.push_back(f);
    });

    claimer.startClaiming();
    const auto t0 = std::chrono::steady_clock::now();
    claimer.pollTimer(t0 + std::chrono::milliseconds(260));
    ASSERT_EQ(claimer.claimState(), AddressClaimState::Claimed);
    emitted.clear();

    // Contending node claims 0x23 with lower NAME (higher numerical priority)
    const std::uint64_t competitorName = 0x8000204600000001ULL;
    CanFrame contentionFrame {};
    contentionFrame.id = (6U << 26) | (static_cast<std::uint32_t>(Pgn::IsoAddressClaim) << 8) | 0x23U;
    contentionFrame.dlc = 8U;
    for (std::size_t i = 0; i < 8; ++i) {
        contentionFrame.data[i] = static_cast<std::uint8_t>((competitorName >> (i * 8)) & 0xFFU);
    }

    claimer.processCanFrame(contentionFrame);

    // Local node should yield address 0x23, move to next address (128), and emit new claim
    EXPECT_EQ(claimer.claimState(), AddressClaimState::WaitingForClaim);
    ASSERT_EQ(emitted.size(), 1U);
    EXPECT_EQ(emitted[0].id & 0xFFU, 128U);

    // Let contention window expire for address 128
    claimer.pollTimer(t0 + std::chrono::milliseconds(600));
    EXPECT_EQ(claimer.claimState(), AddressClaimState::Claimed);
    EXPECT_EQ(claimer.claimedAddress(), 128U);
}

TEST(TestN2kAddressClaimer, ContentionWinReassertClaim)
{
    // Our node has superior priority (lower NAME)
    const std::uint64_t superiorName = 0x8000000000000001ULL;
    N2kAddressClaimer claimer(superiorName, 0x23U);

    std::vector<CanFrame> emitted {};
    claimer.setSendCallback([&](const CanFrame& f) {
        emitted.push_back(f);
    });

    claimer.startClaiming();
    const auto t0 = std::chrono::steady_clock::now();
    claimer.pollTimer(t0 + std::chrono::milliseconds(260));
    ASSERT_EQ(claimer.claimState(), AddressClaimState::Claimed);
    emitted.clear();

    // Inferior node claims 0x23 (higher NAME)
    const std::uint64_t inferiorName = 0xF000000000000001ULL;
    CanFrame contentionFrame {};
    contentionFrame.id = (6U << 26) | (static_cast<std::uint32_t>(Pgn::IsoAddressClaim) << 8) | 0x23U;
    contentionFrame.dlc = 8U;
    for (std::size_t i = 0; i < 8; ++i) {
        contentionFrame.data[i] = static_cast<std::uint8_t>((inferiorName >> (i * 8)) & 0xFFU);
    }

    claimer.processCanFrame(contentionFrame);

    // Node must defend its address: emit re-assertion claim and stay Claimed
    EXPECT_EQ(claimer.claimState(), AddressClaimState::Claimed);
    EXPECT_EQ(claimer.claimedAddress(), 0x23U);
    ASSERT_EQ(emitted.size(), 1U);
    EXPECT_EQ(emitted[0].id & 0xFFU, 0x23U);
}

TEST(TestN2kAddressClaimer, IsoRequestResponse)
{
    N2kAddressClaimer claimer(0xC0002046000003E9ULL, 0x23U);

    std::vector<CanFrame> emitted {};
    claimer.setSendCallback([&](const CanFrame& f) {
        emitted.push_back(f);
    });

    claimer.startClaiming();
    const auto t0 = std::chrono::steady_clock::now();
    claimer.pollTimer(t0 + std::chrono::milliseconds(260));
    ASSERT_EQ(claimer.claimState(), AddressClaimState::Claimed);
    emitted.clear();

    // ISO Request (PGN 59904) requesting PGN 60928 sent to address 0x23
    CanFrame reqFrame {};
    // PGN 59904 (0xEA00) with Destination Address = 0x23, SA = 0x10
    reqFrame.id = (6U << 26) | (0xEA23U << 8) | 0x10U;
    reqFrame.dlc = 3U;
    reqFrame.data[0] = 0x00U; // 60928 = 0x00EE00
    reqFrame.data[1] = 0xEEU;
    reqFrame.data[2] = 0x00U;

    claimer.processCanFrame(reqFrame);

    // Node must reply with its Address Claim
    ASSERT_EQ(emitted.size(), 1U);
    const auto pgn = N2kHeader::fromCanId(emitted[0].id).pgn;
    EXPECT_EQ(pgn, static_cast<std::uint32_t>(Pgn::IsoAddressClaim));
    EXPECT_EQ(emitted[0].id & 0xFFU, 0x23U);
}

TEST(TestN2kAddressClaimer, CommandedAddressChange)
{
    const std::uint64_t myName = 0xC0002046000003E9ULL;
    N2kAddressClaimer claimer(myName, 0x23U);

    std::vector<CanFrame> emitted {};
    claimer.setSendCallback([&](const CanFrame& f) {
        emitted.push_back(f);
    });

    claimer.startClaiming();
    const auto t0 = std::chrono::steady_clock::now();
    claimer.pollTimer(t0 + std::chrono::milliseconds(260));
    ASSERT_EQ(claimer.claimedAddress(), 0x23U);
    emitted.clear();

    // Commanded Address (PGN 65240): 8 bytes NAME + 1 byte new address (e.g. 0x45)
    // Fast Packet or payload representation: 8 bytes NAME followed by new address
    // In CAN frame data with 8-byte NAME:
    CanFrame cmdFrame {};
    cmdFrame.id = (6U << 26) | (static_cast<std::uint32_t>(Pgn::IsoCommandedAddress) << 8) | 0x01U;
    cmdFrame.dlc = 8U;
    for (std::size_t i = 0; i < 8; ++i) {
        cmdFrame.data[i] = static_cast<std::uint8_t>((myName >> (i * 8)) & 0xFFU);
    }

    claimer.processCanFrame(cmdFrame);

    // Address commanded to new address (default commanded in single frame or handled)
    EXPECT_EQ(claimer.claimState(), AddressClaimState::WaitingForClaim);
}
