#include "GeoLockController.h"
#include "Nmea/NmeaChecksum.h"
#include "Nmea/NmeaDevice.h"
#include "NmeaSlavingBridge.h"
#include "Transport/BaseTransport.h"
#include "sim/SimulatedPayload.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace PayloadHal {
namespace {

    class MockNmeaTransport : public Transport::BaseTransport {
    public:
        MockNmeaTransport() = default;
        ~MockNmeaTransport() override = default;

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

    private:
        std::atomic<bool> m_isOpen { false };
        mutable std::mutex m_txMutex {};
        std::vector<std::string> m_sentPackets {};
    };

    TEST(TestNmeaSlavingBridge, RadarTargetForwardProjection)
    {
        // Platform at (37.0, -122.0, 0.0), target 1.0 NMi due North (0 deg bearing)
        const Klv::GeoPoint3D platform { 37.0, -122.0, 0.0 };
        const double rangeMeters = 1852.0; // 1 NMi
        const double bearingDeg = 0.0;

        const auto projected = NmeaSlavingBridge::projectTargetFromRadar(platform, rangeMeters, bearingDeg);

        // Latitude should increase by approx 1852 / 111111 = 0.016668 deg
        EXPECT_NEAR(projected.latitudeDeg, 37.01666, 1e-4);
        EXPECT_NEAR(projected.longitudeDeg, -122.0, 1e-4);

        // Target due East (90 deg bearing)
        const auto projectedEast = NmeaSlavingBridge::projectTargetFromRadar(platform, rangeMeters, 90.0);
        EXPECT_NEAR(projectedEast.latitudeDeg, 37.0, 1e-4);
        EXPECT_GT(projectedEast.longitudeDeg, -122.0);
    }

    TEST(TestNmeaSlavingBridge, ArpaRadarSlavingLifecycle)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        auto geoLock = std::make_shared<GeoLockController>(payload);
        auto transport = std::make_shared<MockNmeaTransport>();
        auto nmeaDevice = std::make_shared<Nmea::NmeaDevice>(transport);
        ASSERT_TRUE(nmeaDevice->start());

        NmeaSlavingBridge bridge(nmeaDevice, geoLock);

        // Feed own-ship navigation: (37.0, -122.0) and heading 0.0 deg
        const std::string gga
            = Nmea::NmeaChecksum::frameSentence("GPGGA,123519,3700.000,N,12200.000,W,1,08,0.9,10.0,M,0.0,M,,");
        const std::string hdt = Nmea::NmeaChecksum::frameSentence("HEHDT,000.0,T");
        transport->injectString(gga);
        transport->injectString(hdt);

        EXPECT_FALSE(bridge.isSlaving());
        EXPECT_TRUE(bridge.slaveToRadarTarget(5U));
        EXPECT_TRUE(bridge.isSlaving());

        // Feed radar target 5: distance 2.0 NMi, bearing 045.0 True, speed 15.0 kt
        const std::string ttm
            = Nmea::NmeaChecksum::frameSentence("RATTM,05,2.0,045.0,T,15.0,090.0,T,0.8,4.2,K,CARGO_ALPHA,T,,123519,A");
        transport->injectString(ttm);

        const auto st = bridge.status();
        EXPECT_TRUE(st.active);
        EXPECT_EQ(st.targetType, MarineTargetType::RadarArpa);
        EXPECT_EQ(st.targetId, 5U);
        EXPECT_EQ(st.targetName, "CARGO_ALPHA");
        ASSERT_TRUE(st.targetPosition.has_value());
        EXPECT_GT(st.targetPosition->latitudeDeg, 37.0);
        EXPECT_GT(st.targetPosition->longitudeDeg, -122.0);

        EXPECT_TRUE(geoLock->isEngaged());
        ASSERT_TRUE(geoLock->currentTarget().has_value());
        EXPECT_NEAR(geoLock->currentTarget()->latitudeDeg, st.targetPosition->latitudeDeg, 1e-5);

        bridge.disengage();
        EXPECT_FALSE(bridge.isSlaving());
        EXPECT_FALSE(geoLock->isEngaged());
    }

    TEST(TestNmeaSlavingBridge, AisVesselSlaving)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        auto geoLock = std::make_shared<GeoLockController>(payload);
        auto transport = std::make_shared<MockNmeaTransport>();
        auto nmeaDevice = std::make_shared<Nmea::NmeaDevice>(transport);
        ASSERT_TRUE(nmeaDevice->start());

        NmeaSlavingBridge bridge(nmeaDevice, geoLock);

        // Feed own-ship navigation
        const std::string gga
            = Nmea::NmeaChecksum::frameSentence("GPGGA,123519,4155.668,N,08152.560,E,1,08,0.9,10.0,M,0.0,M,,");
        const std::string hdt = Nmea::NmeaChecksum::frameSentence("HEHDT,090.0,T");
        transport->injectString(gga);
        transport->injectString(hdt);

        EXPECT_TRUE(bridge.slaveToAisVessel(477553000U));
        EXPECT_TRUE(bridge.isSlaving());

        // Feed 1-part Class A position report for MMSI 477553000
        const std::string aivdm = "!AIVDM,1,1,,B,177KQJ001p5nk70GwLF85jww0+RA,0*67\r\n";
        transport->injectString(aivdm);

        const auto st = bridge.status();
        EXPECT_TRUE(st.active);
        EXPECT_EQ(st.targetType, MarineTargetType::AisVessel);
        EXPECT_EQ(st.targetId, 477553000U);
        ASSERT_TRUE(st.targetPosition.has_value());
        EXPECT_NEAR(st.targetPosition->latitudeDeg, 41.9278, 1e-3);
        EXPECT_NEAR(st.targetPosition->longitudeDeg, 81.876, 1e-3);

        EXPECT_TRUE(geoLock->isEngaged());
        ASSERT_TRUE(geoLock->currentTarget().has_value());
        EXPECT_NEAR(geoLock->currentTarget()->latitudeDeg, 41.9278, 1e-3);
    }

    TEST(TestNmeaSlavingBridge, PredictiveCoasting)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        auto geoLock = std::make_shared<GeoLockController>(payload);
        auto transport = std::make_shared<MockNmeaTransport>();
        auto nmeaDevice = std::make_shared<Nmea::NmeaDevice>(transport);
        ASSERT_TRUE(nmeaDevice->start());

        NmeaSlavingBridge bridge(nmeaDevice, geoLock);
        bridge.setPredictiveCoasting(true);

        const std::string gga
            = Nmea::NmeaChecksum::frameSentence("GPGGA,123519,3700.000,N,12200.000,W,1,08,0.9,10.0,M,0.0,M,,");
        const std::string hdt = Nmea::NmeaChecksum::frameSentence("HEHDT,000.0,T");
        transport->injectString(gga);
        transport->injectString(hdt);

        ASSERT_TRUE(bridge.slaveToRadarTarget(2U));

        // Fast moving vessel (60 kt due North)
        const std::string ttm
            = Nmea::NmeaChecksum::frameSentence("RATTM,02,1.0,000.0,T,60.0,000.0,T,0.0,0.0,K,FAST_FERRY,T,,123519,A");
        transport->injectString(ttm);

        const auto initialTarget = geoLock->currentTarget();
        ASSERT_TRUE(initialTarget.has_value());

        // Sleep to accumulate elapsed time for dead-reckoning coast
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        bridge.update();

        const auto coastedTarget = geoLock->currentTarget();
        ASSERT_TRUE(coastedTarget.has_value());
        // In 600ms at 60 kt (30.8 m/s), vessel traveled ~18 meters North
        EXPECT_GT(coastedTarget->latitudeDeg, initialTarget->latitudeDeg);
    }

    TEST(TestNmeaSlavingBridge, TargetTimeoutAndLossPolicy)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        auto geoLock = std::make_shared<GeoLockController>(payload);
        auto transport = std::make_shared<MockNmeaTransport>();
        auto nmeaDevice = std::make_shared<Nmea::NmeaDevice>(transport);
        ASSERT_TRUE(nmeaDevice->start());

        NmeaSlavingBridge bridge(nmeaDevice, geoLock);
        bridge.setTargetTimeout(std::chrono::milliseconds(20));
        bridge.setTargetLossPolicy(TargetLossPolicy::Disengage);

        std::atomic<bool> lostCallbackFired { false };
        bridge.addTargetLostCallback([&](MarineTargetType type, std::uint32_t id) {
            if (type == MarineTargetType::RadarArpa && id == 8U) {
                lostCallbackFired = true;
            }
        });

        const std::string gga
            = Nmea::NmeaChecksum::frameSentence("GPGGA,123519,3700.000,N,12200.000,W,1,08,0.9,10.0,M,0.0,M,,");
        transport->injectString(gga);

        const std::string ttm
            = Nmea::NmeaChecksum::frameSentence("RATTM,08,1.0,000.0,T,10.0,000.0,T,0.0,0.0,K,PATROL,T,,123519,A");
        transport->injectString(ttm);

        ASSERT_TRUE(bridge.slaveToRadarTarget(8U));
        EXPECT_TRUE(bridge.isSlaving());

        // Wait beyond target timeout
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        bridge.update();

        EXPECT_TRUE(lostCallbackFired.load());
        EXPECT_FALSE(bridge.isSlaving());
        EXPECT_FALSE(geoLock->isEngaged());
    }

    TEST(TestNmeaSlavingBridge, RadarCursorSlaving)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        auto geoLock = std::make_shared<GeoLockController>(payload);
        auto transport = std::make_shared<MockNmeaTransport>();
        auto nmeaDevice = std::make_shared<Nmea::NmeaDevice>(transport);
        ASSERT_TRUE(nmeaDevice->start());

        NmeaSlavingBridge bridge(nmeaDevice, geoLock);

        // Own ship at (37.0, -122.0)
        const std::string gga
            = Nmea::NmeaChecksum::frameSentence("GPGGA,123519,3700.000,N,12200.000,W,1,08,0.9,10.0,M,0.0,M,,");
        transport->injectString(gga);
        const std::string hdt = Nmea::NmeaChecksum::frameSentence("HEHDT,000.0,T");
        transport->injectString(hdt);

        Nmea::RsdData rsd {};
        rsd.cursorRangeNmi = 2.0; // 2 Nautical Miles North
        rsd.cursorBearingDeg = 0.0;
        rsd.valid = true;

        EXPECT_TRUE(bridge.slaveToRadarCursor(rsd));
        EXPECT_TRUE(bridge.isSlaving());
        EXPECT_EQ(bridge.status().targetType, MarineTargetType::RadarCursor);

        const auto target = geoLock->currentTarget();
        ASSERT_TRUE(target.has_value());
        EXPECT_GT(target->latitudeDeg, 37.0);
        EXPECT_NEAR(target->longitudeDeg, -122.0, 1e-4);
    }

    TEST(TestNmeaSlavingBridge, WaypointSlaving)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        auto geoLock = std::make_shared<GeoLockController>(payload);
        auto transport = std::make_shared<MockNmeaTransport>();
        auto nmeaDevice = std::make_shared<Nmea::NmeaDevice>(transport);
        ASSERT_TRUE(nmeaDevice->start());

        NmeaSlavingBridge bridge(nmeaDevice, geoLock);

        Nmea::BwcData bwc {};
        bwc.waypointCoordinates = { 36.5, -121.8 };
        bwc.waypointId = "MONTEREY_BUOY";
        bwc.valid = true;

        EXPECT_TRUE(bridge.slaveToWaypoint(bwc));
        EXPECT_TRUE(bridge.isSlaving());
        EXPECT_EQ(bridge.status().targetType, MarineTargetType::Waypoint);
        EXPECT_EQ(bridge.status().targetName, "MONTEREY_BUOY");

        const auto target = geoLock->currentTarget();
        ASSERT_TRUE(target.has_value());
        EXPECT_DOUBLE_EQ(target->latitudeDeg, 36.5);
        EXPECT_DOUBLE_EQ(target->longitudeDeg, -121.8);
    }

    class TestAisBitWriter {
    public:
        void writeBits(std::uint32_t val, std::size_t numBits)
        {
            for (int i = static_cast<int>(numBits) - 1; i >= 0; --i) {
                m_bits.push_back(static_cast<std::uint8_t>((val >> static_cast<std::uint32_t>(i)) & 1U));
            }
        }

        void writeString(const std::string& str)
        {
            for (const char c : str) {
                std::uint8_t val { 0U };
                if (c >= '@' && c <= '_') {
                    val = static_cast<std::uint8_t>(c - 64);
                } else if (c >= ' ' && c <= '?') {
                    val = static_cast<std::uint8_t>(c);
                }
                writeBits(val, 6U);
            }
        }

        [[nodiscard]] std::pair<std::string, std::size_t> toArmoredPayload() const
        {
            std::vector<std::uint8_t> bytes {};
            std::size_t bitIdx { 0U };
            while (bitIdx < m_bits.size()) {
                std::uint8_t b { 0U };
                for (std::size_t i { 0U }; i < 6U; ++i) {
                    b = static_cast<std::uint8_t>(b << 1U);
                    if (bitIdx < m_bits.size()) {
                        b = static_cast<std::uint8_t>(b | m_bits[bitIdx++]);
                    }
                }
                bytes.push_back(b);
            }
            const std::size_t fillBits = (bytes.size() * 6U - m_bits.size()) % 6U;
            std::string payload {};
            payload.reserve(bytes.size());
            for (std::uint8_t v : bytes) {
                v &= 0x3FU;
                const char c = (v <= 40U) ? static_cast<char>(v + 48U) : static_cast<char>(v + 56U);
                payload.push_back(c);
            }
            return { payload, fillBits };
        }

    private:
        std::vector<std::uint8_t> m_bits {};
    };

    [[nodiscard]] std::string createTestClassAPosition(std::uint32_t mmsi, Nmea::AisNavStatus navStatus, double latDeg,
        double lonDeg, double sogKnots = 0.0, double cogDeg = 0.0)
    {
        TestAisBitWriter writer {};
        writer.writeBits(1U, 6U); // Type 1
        writer.writeBits(0U, 2U); // Repeat indicator
        writer.writeBits(mmsi, 30U); // Source MMSI
        writer.writeBits(static_cast<std::uint32_t>(navStatus), 4U);
        writer.writeBits(0U, 8U); // ROT
        const auto sogRaw = static_cast<std::uint32_t>(sogKnots * 10.0);
        writer.writeBits(sogRaw, 10U);
        writer.writeBits(1U, 1U); // Accuracy high

        const auto lonRaw = static_cast<std::int32_t>(lonDeg * 600000.0);
        const auto latRaw = static_cast<std::int32_t>(latDeg * 600000.0);
        writer.writeBits(static_cast<std::uint32_t>(lonRaw), 28U);
        writer.writeBits(static_cast<std::uint32_t>(latRaw), 27U);

        const auto cogRaw = static_cast<std::uint32_t>(cogDeg * 10.0);
        writer.writeBits(cogRaw, 12U);
        writer.writeBits(511U, 9U); // Heading unavailable
        writer.writeBits(60U, 6U); // Time stamp unavailable
        writer.writeBits(0U, 2U); // Maneuver
        writer.writeBits(0U, 4U); // Spare (3) + RAIM (1)
        writer.writeBits(0U, 19U); // Radio status

        const auto [payload, fillBits] = writer.toArmoredPayload();
        const std::string body = "AIVDM,1,1,,A," + payload + "," + std::to_string(fillBits);
        return Nmea::NmeaChecksum::frameSentence(body, '!');
    }

    [[nodiscard]] std::string createTestSafetyBroadcast(std::uint32_t mmsi, const std::string& text)
    {
        TestAisBitWriter writer {};
        writer.writeBits(14U, 6U); // Type 14
        writer.writeBits(0U, 2U); // Repeat indicator
        writer.writeBits(mmsi, 30U); // Source MMSI
        writer.writeBits(0U, 2U); // Spare
        writer.writeString(text);

        const auto [payload, fillBits] = writer.toArmoredPayload();
        const std::string body = "AIVDM,1,1,,A," + payload + "," + std::to_string(fillBits);
        return Nmea::NmeaChecksum::frameSentence(body, '!');
    }

    TEST(TestNmeaSlavingBridge, EmergencyBeaconPreemptionAndSlew)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        auto geoLock = std::make_shared<GeoLockController>(payload);
        auto transport = std::make_shared<MockNmeaTransport>();
        auto nmeaDevice = std::make_shared<Nmea::NmeaDevice>(transport);
        ASSERT_TRUE(nmeaDevice->start());

        NmeaSlavingBridge bridge(nmeaDevice, geoLock);

        // Feed own-ship navigation at (37.0, -122.0)
        const std::string gga
            = Nmea::NmeaChecksum::frameSentence("GPGGA,123519,3700.000,N,12200.000,W,1,08,0.9,10.0,M,0.0,M,,");
        const std::string hdt = Nmea::NmeaChecksum::frameSentence("HEHDT,000.0,T");
        transport->injectString(gga);
        transport->injectString(hdt);

        // 1. Initially slave to ARPA radar target #5
        const std::string ttm
            = Nmea::NmeaChecksum::frameSentence("RATTM,05,2.0,090.0,T,10.0,180.0,T,1.0,5.0,N,FERRY,T,,123456,A");
        transport->injectString(ttm);
        EXPECT_TRUE(bridge.slaveToRadarTarget(5U));
        EXPECT_EQ(bridge.status().targetType, MarineTargetType::RadarArpa);
        EXPECT_EQ(bridge.status().targetId, 5U);
        EXPECT_FALSE(bridge.status().isEmergencyActive);

        std::atomic<bool> slewCallbackFired { false };
        bridge.addEmergencySlewCallback([&slewCallbackFired](const Nmea::AisEmergencyAlert& alert) {
            if (alert.mmsi == 972054321U) {
                slewCallbackFired.store(true);
            }
        });

        // 2. Incoming AIS-MOB distress burst (MMSI 972054321) at (37.8044, -122.4678)
        const std::string mobSentence
            = createTestClassAPosition(972054321U, Nmea::AisNavStatus::AisSartActive, 37.8044, -122.4678, 1.2, 45.0);
        transport->injectString(mobSentence);

        // Emergency beacon should immediately preempt the radar target
        EXPECT_TRUE(slewCallbackFired.load());
        const auto st = bridge.status();
        EXPECT_TRUE(st.active);
        EXPECT_TRUE(st.isEmergencyActive);
        EXPECT_EQ(st.targetType, MarineTargetType::EmergencyBeacon);
        EXPECT_EQ(st.targetId, 972054321U);
        EXPECT_EQ(st.emergencyType, Nmea::AisBeaconType::AisMob);

        ASSERT_TRUE(st.targetPosition.has_value());
        EXPECT_NEAR(st.targetPosition->latitudeDeg, 37.8044, 1e-3);
        EXPECT_NEAR(st.targetPosition->longitudeDeg, -122.4678, 1e-3);

        const auto geoLockTarget = geoLock->currentTarget();
        ASSERT_TRUE(geoLockTarget.has_value());
        EXPECT_NEAR(geoLockTarget->latitudeDeg, 37.8044, 1e-3);
        EXPECT_NEAR(geoLockTarget->longitudeDeg, -122.4678, 1e-3);

        // 3. Clear emergency slew and verify that radar target #5 is restored!
        bridge.clearEmergencySlew();
        const auto restoredSt = bridge.status();
        EXPECT_FALSE(restoredSt.isEmergencyActive);
        EXPECT_EQ(restoredSt.targetType, MarineTargetType::RadarArpa);
        EXPECT_EQ(restoredSt.targetId, 5U);
    }

    TEST(TestNmeaSlavingBridge, EmergencyTestModeRejection)
    {
        auto payload = std::make_shared<SimulatedPayload>();
        ASSERT_TRUE(payload->connect());

        auto geoLock = std::make_shared<GeoLockController>(payload);
        auto transport = std::make_shared<MockNmeaTransport>();
        auto nmeaDevice = std::make_shared<Nmea::NmeaDevice>(transport);
        ASSERT_TRUE(nmeaDevice->start());

        NmeaSlavingBridge bridge(nmeaDevice, geoLock);
        EXPECT_TRUE(bridge.isIgnoringEmergencyTestMode());

        // Currently slaved to manual geodetic target
        Klv::GeoPoint3D initialTarget { 37.1, -122.2, 0.0 };
        EXPECT_TRUE(bridge.slaveToGeodeticTarget(initialTarget));
        EXPECT_EQ(bridge.status().targetType, MarineTargetType::GeodeticManual);

        // Transmit routine crew SART test broadcast
        const std::string sartTest = createTestSafetyBroadcast(970010123U, "SART TEST");
        transport->injectString(sartTest);

        // Tracking should NOT be pre-empted
        EXPECT_FALSE(bridge.status().isEmergencyActive);
        EXPECT_EQ(bridge.status().targetType, MarineTargetType::GeodeticManual);
        EXPECT_NEAR(geoLock->currentTarget()->latitudeDeg, 37.1, 1e-4);
    }

} // namespace
} // namespace PayloadHal
