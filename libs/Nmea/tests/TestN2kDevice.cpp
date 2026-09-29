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

} // namespace Nmea::N2k
