#include "NmeaChecksum.h"
#include "NmeaDevice.h"
#include "Transport/BaseTransport.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Nmea {
namespace {

    class MockTestTransport : public Transport::BaseTransport {
    public:
        MockTestTransport() = default;
        ~MockTestTransport() override = default;

        bool open() override
        {
            m_isOpen = true;
            return true;
        }

        void close() override
        {
            m_isOpen = false;
        }

        bool isOpen() const noexcept override
        {
            return m_isOpen;
        }

        bool sendData(const std::vector<std::uint8_t>& data) override
        {
            std::lock_guard<std::mutex> lock(m_txMutex);
            m_sentPackets.push_back(std::string(data.begin(), data.end()));
            return true;
        }

        void injectString(std::string_view text)
        {
            const std::vector<std::uint8_t> data(text.begin(), text.end());
            invokeDataCallback(data);
        }

        std::vector<std::string> sentPackets() const
        {
            std::lock_guard<std::mutex> lock(m_txMutex);
            return m_sentPackets;
        }

        void clearSentPackets()
        {
            std::lock_guard<std::mutex> lock(m_txMutex);
            m_sentPackets.clear();
        }

    private:
        std::atomic<bool> m_isOpen { false };
        mutable std::mutex m_txMutex {};
        std::vector<std::string> m_sentPackets {};
    };

    TEST(TestNmeaDevice, LifecycleStartStop)
    {
        auto transport = std::make_shared<MockTestTransport>();
        NmeaDevice device(transport);

        EXPECT_FALSE(device.isConnected());
        EXPECT_TRUE(device.start());
        EXPECT_TRUE(device.isConnected());

        // Idempotent start
        EXPECT_TRUE(device.start());
        EXPECT_TRUE(device.isConnected());

        device.stop();
        EXPECT_FALSE(device.isConnected());

        // Idempotent stop
        device.stop();
        EXPECT_FALSE(device.isConnected());
    }

    TEST(TestNmeaDevice, GpsNavigationSnapshot)
    {
        auto transport = std::make_shared<MockTestTransport>();
        NmeaDevice device(transport);
        ASSERT_TRUE(device.start());

        std::atomic<std::size_t> navUpdateCount { 0U };
        NmeaNavSnapshot lastNav {};
        const auto subId = device.addNavCallback([&](const NmeaNavSnapshot& snap) {
            navUpdateCount++;
            lastNav = snap;
        });

        // Feed standard GGA sentence
        const std::string gga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
        transport->injectString(gga);

        EXPECT_EQ(navUpdateCount.load(), 1U);
        EXPECT_TRUE(lastNav.hasPosition);
        EXPECT_NEAR(lastNav.position.latitudeDeg, 48.1173, 1e-4);
        EXPECT_NEAR(lastNav.position.longitudeDeg, 11.5166, 1e-4);
        EXPECT_NEAR(lastNav.altitudeMeters, 545.4, 1e-1);
        EXPECT_EQ(lastNav.fixQuality, NmeaFixQuality::GpsFix);

        // Feed standard RMC sentence
        const std::string rmc
            = NmeaChecksum::frameSentence("GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230326,003.1,W,A");
        transport->injectString(rmc);

        EXPECT_EQ(navUpdateCount.load(), 2U);
        EXPECT_NEAR(lastNav.sogKnots, 22.4, 1e-1);
        EXPECT_NEAR(lastNav.cogDegrees, 84.4, 1e-1);

        device.removeNavCallback(subId);
        transport->injectString(gga);
        EXPECT_EQ(navUpdateCount.load(), 2U); // Should not increase after unsubscribe
    }

    TEST(TestNmeaDevice, HeadingAndPitchRollAttitude)
    {
        auto transport = std::make_shared<MockTestTransport>();
        NmeaDevice device(transport);
        ASSERT_TRUE(device.start());

        // Heading
        const std::string hdt = "$HEHDT,341.8,T*21\r\n";
        transport->injectString(hdt);

        auto snap = device.navSnapshot();
        EXPECT_TRUE(snap.hasHeading);
        EXPECT_NEAR(snap.trueHeadingDegrees, 341.8, 1e-1);

        // XDR Transducers
        const std::string xdr = NmeaChecksum::frameSentence("IIXDR,A,1.25,D,PITCH,A,-0.50,D,ROLL");
        transport->injectString(xdr);

        snap = device.navSnapshot();
        EXPECT_TRUE(snap.hasAttitude);
        EXPECT_NEAR(snap.pitchDegrees, 1.25, 1e-2);
        EXPECT_NEAR(snap.rollDegrees, -0.50, 1e-2);
    }

    TEST(TestNmeaDevice, AttitudeObserverAndSentences)
    {
        auto transport = std::make_shared<MockTestTransport>();
        NmeaDevice device(transport);
        ASSERT_TRUE(device.start());

        std::atomic<std::size_t> attCount { 0U };
        AttitudeData lastAtt {};
        const auto subId = device.addAttitudeCallback([&](const AttitudeData& att) {
            lastAtt = att;
            attCount++;
        });

        // 1. Test PASHR sentence
        const std::string pashr = NmeaChecksum::frameSentence("PASHR,120000.00,045.20,T,+03.50,-02.10,+0.12,0.05,0.05,0.10,2,1");
        transport->injectString(pashr);

        EXPECT_EQ(attCount.load(), 1U);
        EXPECT_TRUE(lastAtt.valid);
        EXPECT_TRUE(lastAtt.hasHeading);
        EXPECT_NEAR(lastAtt.headingDegrees, 45.2, 1e-2);
        EXPECT_NEAR(lastAtt.pitchDegrees, -2.1, 1e-2);
        EXPECT_NEAR(lastAtt.rollDegrees, 3.5, 1e-2);
        EXPECT_NEAR(lastAtt.heaveMeters, 0.12, 1e-2);

        auto optAtt = device.lastAttitude();
        ASSERT_TRUE(optAtt.has_value());
        EXPECT_NEAR(optAtt->pitchDegrees, -2.1, 1e-2);

        // 2. Test PFEC GPatt sentence
        const std::string pfec = NmeaChecksum::frameSentence("PFEC,GPatt,180.50,-04.20,+08.10");
        transport->injectString(pfec);

        EXPECT_EQ(attCount.load(), 2U);
        EXPECT_NEAR(lastAtt.headingDegrees, 180.5, 1e-2);
        EXPECT_NEAR(lastAtt.pitchDegrees, -4.2, 1e-2);
        EXPECT_NEAR(lastAtt.rollDegrees, 8.1, 1e-2);

        // 3. Test XDR with aliases "PTCH" and "RL"
        const std::string xdr = NmeaChecksum::frameSentence("IIXDR,A,0.75,D,ptch,A,-1.20,D,rl");
        transport->injectString(xdr);

        EXPECT_EQ(attCount.load(), 3U);
        EXPECT_NEAR(lastAtt.pitchDegrees, 0.75, 1e-2);
        EXPECT_NEAR(lastAtt.rollDegrees, -1.20, 1e-2);

        // 4. Test unsubscribe
        device.removeAttitudeCallback(subId);
        transport->injectString(pfec);
        EXPECT_EQ(attCount.load(), 3U);
    }

    TEST(TestNmeaDevice, ArpaRadarTargetManagement)
    {
        auto transport = std::make_shared<MockTestTransport>();
        NmeaDevice device(transport);
        ASSERT_TRUE(device.start());

        std::atomic<bool> callbackFired { false };
        device.addRadarCallback([&](const TtmData& ttm) {
            if (ttm.targetNumber == 1U) {
                callbackFired = true;
            }
        });

        const std::string ttm
            = NmeaChecksum::frameSentence("RATTM,01,2.5,045.0,T,15.0,090.0,T,0.8,4.2,K,CARGO_01,T,,123519,A");
        transport->injectString(ttm);

        EXPECT_TRUE(callbackFired.load());

        const auto targets = device.activeRadarTargets();
        ASSERT_EQ(targets.size(), 1U);
        EXPECT_EQ(targets[0].targetNumber, 1U);
        EXPECT_NEAR(targets[0].targetDistanceNmi, 2.5, 1e-1);
        EXPECT_NEAR(targets[0].bearingDegrees, 45.0, 1e-1);
        EXPECT_EQ(targets[0].targetName, "CARGO_01");

        const auto targetOpt = device.radarTarget(1U);
        ASSERT_TRUE(targetOpt.has_value());
        EXPECT_EQ(targetOpt->targetName, "CARGO_01");

        EXPECT_FALSE(device.radarTarget(99U).has_value());
    }

    TEST(TestNmeaDevice, AisVesselTargetDecoding)
    {
        auto transport = std::make_shared<MockTestTransport>();
        NmeaDevice device(transport);
        ASSERT_TRUE(device.start());

        std::atomic<bool> aisCallbackFired { false };
        device.addAisCallback([&](const AisVesselTarget& target) {
            if (target.mmsi == 477553000U) {
                aisCallbackFired = true;
            }
        });

        // 1-part Class A position report
        const std::string aivdm = "!AIVDM,1,1,,B,177KQJ001p5nk70GwLF85jww0+RA,0*67\r\n";
        transport->injectString(aivdm);

        EXPECT_TRUE(aisCallbackFired.load());

        const auto aisTargets = device.activeAisTargets();
        ASSERT_EQ(aisTargets.size(), 1U);
        EXPECT_EQ(aisTargets[0].mmsi, 477553000U);
        EXPECT_NEAR(aisTargets[0].speedOverGroundKnots, 12.0, 1e-1);

        const auto targetOpt = device.aisTarget(477553000U);
        ASSERT_TRUE(targetOpt.has_value());
        EXPECT_EQ(targetOpt->mmsi, 477553000U);
    }

    TEST(TestNmeaDevice, TargetPruningTtl)
    {
        auto transport = std::make_shared<MockTestTransport>();
        NmeaDevice device(transport);
        ASSERT_TRUE(device.start());

        const std::string ttm
            = NmeaChecksum::frameSentence("RATTM,01,2.5,045.0,T,15.0,090.0,T,0.8,4.2,K,CARGO_01,T,,123519,A");
        transport->injectString(ttm);
        EXPECT_EQ(device.activeRadarTargets().size(), 1U);

        // Immediate pruning with 0ms TTL should purge targets
        device.pruneStaleTargets(std::chrono::milliseconds(0), std::chrono::milliseconds(0));
        EXPECT_EQ(device.activeRadarTargets().size(), 0U);
    }

    TEST(TestNmeaDevice, SendSentenceFormatting)
    {
        auto transport = std::make_shared<MockTestTransport>();
        NmeaDevice device(transport);
        ASSERT_TRUE(device.start());

        std::atomic<bool> txRawCallbackFired { false };
        device.addRawCallback([&](std::string_view sentence, bool isTx) {
            if (isTx && sentence.find("HEHDT") != std::string_view::npos) {
                txRawCallbackFired = true;
            }
        });

        EXPECT_TRUE(device.sendSentence("HEHDT,180.0,T", true));
        EXPECT_TRUE(txRawCallbackFired.load());

        const auto sent = transport->sentPackets();
        ASSERT_EQ(sent.size(), 1U);
        EXPECT_TRUE(NmeaChecksum::validate(sent[0]));
        EXPECT_EQ(sent[0].substr(0, 6), "$HEHDT");
    }

    TEST(TestNmeaDevice, MultithreadedSubscriptionSafety)
    {
        auto transport = std::make_shared<MockTestTransport>();
        NmeaDevice device(transport);
        ASSERT_TRUE(device.start());

        std::atomic<bool> running { true };

        // Thread 1: Ingests sentences constantly
        std::thread feeder([&]() {
            while (running.load()) {
                transport->injectString("$HEHDT,100.0,T*2B\r\n");
                transport->injectString("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n");
            }
        });

        // Thread 2: Continuously adds and removes subscribers
        std::thread subscriber([&]() {
            for (int i = 0; i < 50; ++i) {
                const auto id1 = device.addNavCallback([](const NmeaNavSnapshot&) {});
                const auto id2 = device.addRadarCallback([](const TtmData&) {});
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                device.removeNavCallback(id1);
                device.removeRadarCallback(id2);
            }
        });

        subscriber.join();
        running.store(false);
        feeder.join();

        device.stop();
    }

} // namespace
} // namespace Nmea
