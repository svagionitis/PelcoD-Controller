#include "n2k/N2kDecoder.h"
#include "n2k/N2kDevice.h"

#include <atomic>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace Nmea::N2k {

TEST(TestN2kDevice, PositionAndHeadingDispatch)
{
    N2kDevice device;

    std::atomic<bool> posCalled { false };
    PositionRapid rxPos {};
    const auto posSubId = device.addPositionCallback([&](const PositionRapid& p) {
        rxPos = p;
        posCalled = true;
    });

    std::atomic<bool> hdgCalled { false };
    VesselHeading rxHdg {};
    const auto hdgSubId = device.addHeadingCallback([&](const VesselHeading& h) {
        rxHdg = h;
        hdgCalled = true;
    });

    // 1. Send Position Rapid Update
    PositionRapid pIn {};
    pIn.latitudeDeg = 25.1234567;
    pIn.longitudeDeg = 55.7654321;
    pIn.isValid = true;

    N2kHeader posHdr {};
    posHdr.pgn = static_cast<std::uint32_t>(Pgn::PositionRapidUpdate);
    posHdr.sourceAddress = 22U;

    CanFrame posFrame {};
    posFrame.id = posHdr.toCanId();
    posFrame.dlc = 8U;
    const auto posBytes = N2kDecoder::encodePgn129025(pIn);
    std::memcpy(posFrame.data.data(), posBytes.data(), 8U);

    device.onCanFrame(posFrame);

    EXPECT_TRUE(posCalled.load());
    EXPECT_NEAR(rxPos.latitudeDeg, 25.1234567, 1e-6);
    EXPECT_NEAR(rxPos.longitudeDeg, 55.7654321, 1e-6);
    ASSERT_TRUE(device.position().has_value());
    EXPECT_NEAR(device.position()->latitudeDeg, 25.1234567, 1e-6);

    // 2. Send Heading
    VesselHeading hIn {};
    hIn.sid = 1U;
    hIn.headingDegrees = 89.5;
    hIn.reference = HeadingReference::True;
    hIn.hasHeading = true;

    N2kHeader hdgHdr {};
    hdgHdr.pgn = static_cast<std::uint32_t>(Pgn::VesselHeading);
    hdgHdr.sourceAddress = 22U;

    CanFrame hdgFrame {};
    hdgFrame.id = hdgHdr.toCanId();
    hdgFrame.dlc = 8U;
    const auto hdgBytes = N2kDecoder::encodePgn127250(hIn);
    std::memcpy(hdgFrame.data.data(), hdgBytes.data(), 8U);

    device.onCanFrame(hdgFrame);

    EXPECT_TRUE(hdgCalled.load());
    EXPECT_NEAR(rxHdg.headingDegrees, 89.5, 0.1);
    ASSERT_TRUE(device.heading().has_value());
    EXPECT_NEAR(device.heading()->headingDegrees, 89.5, 0.1);

    // Unsubscribe
    device.removePositionCallback(posSubId);
    device.removeHeadingCallback(hdgSubId);

    posCalled = false;
    device.onCanFrame(posFrame);
    EXPECT_FALSE(posCalled.load());
}

TEST(TestN2kDevice, AttitudeAndWindDispatch)
{
    N2kDevice device;

    std::atomic<bool> attCalled { false };
    Attitude rxAtt {};
    const auto attSub = device.addAttitudeCallback([&](const Attitude& a) {
        rxAtt = a;
        attCalled = true;
    });
    (void)attSub;

    std::atomic<bool> windCalled { false };
    WindData rxWind {};
    const auto windSub = device.addWindCallback([&](const WindData& w) {
        rxWind = w;
        windCalled = true;
    });
    (void)windSub;

    // 1. Send Attitude
    Attitude attIn {};
    attIn.yawDegrees = 180.0;
    attIn.pitchDegrees = 4.2;
    attIn.rollDegrees = -1.8;
    attIn.hasYaw = true;
    attIn.hasPitch = true;
    attIn.hasRoll = true;

    N2kHeader attHdr {};
    attHdr.pgn = static_cast<std::uint32_t>(Pgn::Attitude);
    attHdr.sourceAddress = 30U;

    CanFrame attFrame {};
    attFrame.id = attHdr.toCanId();
    attFrame.dlc = 8U;
    const auto attBytes = N2kDecoder::encodePgn127257(attIn);
    std::memcpy(attFrame.data.data(), attBytes.data(), 8U);

    device.onCanFrame(attFrame);

    EXPECT_TRUE(attCalled.load());
    EXPECT_NEAR(rxAtt.pitchDegrees, 4.2, 0.1);
    EXPECT_NEAR(rxAtt.rollDegrees, -1.8, 0.1);
    ASSERT_TRUE(device.attitude().has_value());
    EXPECT_NEAR(device.attitude()->pitchDegrees, 4.2, 0.1);

    // 2. Send Wind Data
    WindData windIn {};
    windIn.windSpeedMps = 15.0;
    windIn.windAngleDegrees = 45.0;
    windIn.reference = WindReference::Apparent;
    windIn.hasWindSpeed = true;
    windIn.hasWindAngle = true;

    N2kHeader windHdr {};
    windHdr.pgn = static_cast<std::uint32_t>(Pgn::WindData);
    windHdr.sourceAddress = 30U;

    CanFrame windFrame {};
    windFrame.id = windHdr.toCanId();
    windFrame.dlc = 6U;
    const auto windBytes = N2kDecoder::encodePgn130306(windIn);
    std::memcpy(windFrame.data.data(), windBytes.data(), 6U);

    device.onCanFrame(windFrame);

    EXPECT_TRUE(windCalled.load());
    EXPECT_NEAR(rxWind.windSpeedMps, 15.0, 0.1);
    EXPECT_NEAR(rxWind.windAngleDegrees, 45.0, 0.1);
    ASSERT_TRUE(device.wind().has_value());
    EXPECT_NEAR(device.wind()->windSpeedMps, 15.0, 0.1);
}

TEST(TestN2kDevice, SocketCanRawIngestion)
{
    N2kDevice device;

    std::atomic<bool> received { false };
    const auto posSub = device.addPositionCallback([&](const PositionRapid& p) {
        if (p.isValid) {
            received = true;
        }
    });
    (void)posSub;

    PositionRapid pIn {};
    pIn.latitudeDeg = 10.0;
    pIn.longitudeDeg = 20.0;
    pIn.isValid = true;

    N2kHeader posHdr {};
    posHdr.pgn = static_cast<std::uint32_t>(Pgn::PositionRapidUpdate);
    posHdr.sourceAddress = 50U;

    const auto payload = N2kDecoder::encodePgn129025(pIn);

    // Build 16-byte SocketCAN frame
    std::array<std::uint8_t, 16> socketCanFrame {};
    const std::uint32_t canId = posHdr.toCanId();
    std::memcpy(socketCanFrame.data(), &canId, sizeof(canId));
    socketCanFrame[4] = 8U; // DLC
    std::memcpy(socketCanFrame.data() + 8, payload.data(), 8U);

    device.onRawSocketCanData(socketCanFrame.data(), socketCanFrame.size());
    EXPECT_TRUE(received.load());
}

TEST(TestN2kDevice, AisTargetManagementAndPruning)
{
    N2kDevice device;

    std::atomic<bool> aisCalled { false };
    const auto aisSub = device.addAisClassACallback([&](const AisClassAPosition& a) {
        if (a.mmsi == 123456789U) {
            aisCalled = true;
        }
    });
    (void)aisSub;

    // Construct 28-byte AIS Class A message
    std::vector<std::uint8_t> payload(28U, 0xFFU);
    payload[0] = 1U;
    const std::uint32_t mmsi = 123456789U;
    std::memcpy(&payload[2], &mmsi, sizeof(mmsi));
    const std::int32_t latRaw = 300000000;
    const std::int32_t lonRaw = 400000000;
    std::memcpy(&payload[6], &lonRaw, sizeof(lonRaw));
    std::memcpy(&payload[10], &latRaw, sizeof(latRaw));

    N2kHeader hdr {};
    hdr.pgn = static_cast<std::uint32_t>(Pgn::AisClassAPositionReport);
    hdr.sourceAddress = 88U;

    const auto frames = N2kDecoder::splitFastPacket(hdr, payload.data(), payload.size(), 1U);
    for (const auto& f : frames) {
        device.onCanFrame(f);
    }

    EXPECT_TRUE(aisCalled.load());
    EXPECT_EQ(device.aisTargetCount(), 1U);
    const auto target = device.aisClassATarget(123456789U);
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(target->mmsi, 123456789U);
    EXPECT_NEAR(target->latitudeDeg, 30.0, 1e-6);

    // Pruning with 0 second TTL should remove it
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    device.pruneAisTargets(std::chrono::seconds(0));
    EXPECT_EQ(device.aisTargetCount(), 0U);
    EXPECT_FALSE(device.aisClassATarget(123456789U).has_value());
}

TEST(TestN2kDevice, MultithreadedSubscriptionSafety)
{
    N2kDevice device;

    std::atomic<bool> stop { false };
    std::atomic<int> counter { 0 };

    auto subId = device.addPositionCallback([&](const PositionRapid&) { counter.fetch_add(1); });

    // Thread 1: Ingest CAN frames rapidly
    std::thread producer([&]() {
        PositionRapid p {};
        p.latitudeDeg = 1.0;
        p.longitudeDeg = 1.0;
        p.isValid = true;
        N2kHeader hdr {};
        hdr.pgn = static_cast<std::uint32_t>(Pgn::PositionRapidUpdate);

        CanFrame cf {};
        cf.id = hdr.toCanId();
        cf.dlc = 8U;
        const auto bytes = N2kDecoder::encodePgn129025(p);
        std::memcpy(cf.data.data(), bytes.data(), 8U);

        while (!stop.load()) {
            device.onCanFrame(cf);
        }
    });

    // Thread 2: Continuously register and unregister callbacks
    std::thread subscriber([&]() {
        for (int i = 0; i < 50; ++i) {
            auto id = device.addPositionCallback([](const PositionRapid&) {});
            std::this_thread::yield();
            device.removePositionCallback(id);
        }
    });

    subscriber.join();
    stop.store(true);
    producer.join();

    device.removePositionCallback(subId);
    EXPECT_GT(counter.load(), 0);
}

TEST(TestN2kDevice, RudderAndMagVarDispatch)
{
    N2kDevice device;

    std::atomic<bool> rudderCalled { false };
    RudderData rxRudder {};
    const auto rSub = device.addRudderCallback([&](const RudderData& r) {
        rxRudder = r;
        rudderCalled = true;
    });
    (void)rSub;

    std::atomic<bool> magVarCalled { false };
    MagneticVariation rxMagVar {};
    const auto mSub = device.addMagVariationCallback([&](const MagneticVariation& m) {
        rxMagVar = m;
        magVarCalled = true;
    });
    (void)mSub;

    // 1. Send Rudder
    RudderData rIn {};
    rIn.instance = 0U;
    rIn.positionDegrees = -12.5;
    rIn.hasPosition = true;

    N2kHeader rHdr {};
    rHdr.pgn = static_cast<std::uint32_t>(Pgn::Rudder);
    rHdr.sourceAddress = 40U;

    CanFrame rFrame {};
    rFrame.id = rHdr.toCanId();
    rFrame.dlc = 8U;
    const auto rBytes = N2kDecoder::encodePgn127245(rIn);
    std::memcpy(rFrame.data.data(), rBytes.data(), 8U);

    device.onCanFrame(rFrame);

    EXPECT_TRUE(rudderCalled.load());
    EXPECT_NEAR(rxRudder.positionDegrees, -12.5, 0.05);
    ASSERT_TRUE(device.rudder().has_value());
    EXPECT_NEAR(device.rudder()->positionDegrees, -12.5, 0.05);

    // 2. Send MagVar
    MagneticVariation mIn {};
    mIn.variationDegrees = 3.8;
    mIn.hasVariation = true;

    N2kHeader mHdr {};
    mHdr.pgn = static_cast<std::uint32_t>(Pgn::MagneticVariation);
    mHdr.sourceAddress = 40U;

    CanFrame mFrame {};
    mFrame.id = mHdr.toCanId();
    mFrame.dlc = 8U;
    const auto mBytes = N2kDecoder::encodePgn127258(mIn);
    std::memcpy(mFrame.data.data(), mBytes.data(), 8U);

    device.onCanFrame(mFrame);

    EXPECT_TRUE(magVarCalled.load());
    EXPECT_NEAR(rxMagVar.variationDegrees, 3.8, 0.05);
    ASSERT_TRUE(device.magneticVariation().has_value());
    EXPECT_NEAR(device.magneticVariation()->variationDegrees, 3.8, 0.05);
}

TEST(TestN2kDevice, SystemTimeAndHeartbeatDispatch)
{
    N2kDevice device;

    std::atomic<bool> timeCalled { false };
    SystemTimeData rxTime {};
    const auto tSub = device.addSystemTimeCallback([&](const SystemTimeData& t) {
        rxTime = t;
        timeCalled = true;
    });
    (void)tSub;

    std::atomic<bool> hbCalled { false };
    HeartbeatData rxHb {};
    const auto hbSub = device.addHeartbeatCallback([&](const HeartbeatData& h) {
        rxHb = h;
        hbCalled = true;
    });
    (void)hbSub;

    // 1. Send System Time
    SystemTimeData tIn {};
    tIn.systemDateDays = 20000U;
    tIn.secondsSinceMidnight = 36000.0;
    tIn.hasTime = true;
    tIn.hasDate = true;

    N2kHeader tHdr {};
    tHdr.pgn = static_cast<std::uint32_t>(Pgn::SystemTime);
    tHdr.sourceAddress = 50U;

    CanFrame tFrame {};
    tFrame.id = tHdr.toCanId();
    tFrame.dlc = 8U;
    const auto tBytes = N2kDecoder::encodePgn126992(tIn);
    std::memcpy(tFrame.data.data(), tBytes.data(), 8U);

    device.onCanFrame(tFrame);

    EXPECT_TRUE(timeCalled.load());
    EXPECT_EQ(rxTime.systemDateDays, 20000U);
    ASSERT_TRUE(device.systemTime().has_value());
    EXPECT_EQ(device.systemTime()->systemDateDays, 20000U);

    // 2. Send Heartbeat
    HeartbeatData hIn {};
    hIn.transmitIntervalMs = 500U;
    hIn.sequenceCounter = 12U;
    hIn.valid = true;

    N2kHeader hHdr {};
    hHdr.pgn = static_cast<std::uint32_t>(Pgn::Heartbeat);
    hHdr.sourceAddress = 50U;

    CanFrame hFrame {};
    hFrame.id = hHdr.toCanId();
    hFrame.dlc = 8U;
    const auto hBytes = N2kDecoder::encodePgn126993(hIn);
    std::memcpy(hFrame.data.data(), hBytes.data(), 8U);

    device.onCanFrame(hFrame);

    EXPECT_TRUE(hbCalled.load());
    EXPECT_EQ(rxHb.transmitIntervalMs, 500U);
    ASSERT_TRUE(device.heartbeat().has_value());
    EXPECT_EQ(device.heartbeat()->transmitIntervalMs, 500U);
}

TEST(TestN2kDevice, AddressClaimerIntegration)
{
    N2kDevice device(0xC0002046000003E9ULL, 0x28U);
    EXPECT_EQ(device.claimedAddress(), 254U); // Unclaimed initially

    device.addressClaimer().startClaiming();
    EXPECT_EQ(device.addressClaimer().claimState(), AddressClaimState::WaitingForClaim);

    // Poll timer to claim address
    const auto t0 = std::chrono::steady_clock::now();
    device.addressClaimer().pollTimer(t0 + std::chrono::milliseconds(260));
    EXPECT_EQ(device.claimedAddress(), 0x28U);
}
} // namespace Nmea::N2k
