/// @file TestSerialLrfAdapter.cpp
/// @brief Comprehensive unit tests for the physical SerialLrfAdapter driver and safety interlocks.

#include "adapters/SerialLrfAdapter.h"
#include "GeoreferenceUtils.h"
#include "PayloadFactory.h"
#include <gtest/gtest.h>

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace PayloadHal;

namespace {

class MockTransport : public Transport::ITransport {
public:
    MockTransport() = default;
    ~MockTransport() override = default;

    bool open() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_open = true;
        if (m_stateCb) {
            m_stateCb(Transport::TransportState::Connected, "");
        }
        return true;
    }

    void close() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_open = false;
        if (m_stateCb) {
            m_stateCb(Transport::TransportState::Disconnected, "Closed by host");
        }
    }

    bool isOpen() const noexcept override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_open;
    }

    bool sendData(const std::vector<std::uint8_t>& data) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sentPackets.push_back(data);
        return true;
    }

    void setDataCallback(DataReceivedCallback callback) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dataCb = std::move(callback);
    }

    void setStateCallback(StateChangedCallback callback) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stateCb = std::move(callback);
    }

    void injectData(const std::vector<std::uint8_t>& data)
    {
        DataReceivedCallback cb;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            cb = m_dataCb;
        }
        if (cb) {
            cb(data);
        }
    }

    void injectDisconnect()
    {
        StateChangedCallback cb;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_open = false;
            cb = m_stateCb;
        }
        if (cb) {
            cb(Transport::TransportState::Disconnected, "Physical link lost");
        }
    }

    std::vector<std::vector<std::uint8_t>> getSentPackets() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_sentPackets;
    }

    void clearSentPackets()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sentPackets.clear();
    }

private:
    mutable std::mutex m_mutex;
    bool m_open { false };
    DataReceivedCallback m_dataCb {};
    StateChangedCallback m_stateCb {};
    std::vector<std::vector<std::uint8_t>> m_sentPackets {};
};

} // namespace

// =============================================================================
// Safety Interlock Tests
// =============================================================================

TEST(TestSerialLrfAdapter, CannotFireWhenDisarmed)
{
    auto mockTransport = std::make_shared<MockTransport>();
    SerialLrfConfig cfg;
    cfg.enforceArmingInterlock = true;
    auto adapter = std::make_shared<SerialLrfAdapter>(mockTransport, cfg);

    ASSERT_TRUE(adapter->connect());
    EXPECT_FALSE(adapter->isArmed());

    // Firing while disarmed MUST be rejected immediately!
    EXPECT_FALSE(adapter->triggerSingleMeasurement());
    EXPECT_TRUE(mockTransport->getSentPackets().empty());
}

TEST(TestSerialLrfAdapter, ArmAndFirePulse)
{
    auto mockTransport = std::make_shared<MockTransport>();
    auto adapter = std::make_shared<SerialLrfAdapter>(mockTransport);

    ASSERT_TRUE(adapter->connect());
    EXPECT_TRUE(adapter->armLaser());
    EXPECT_TRUE(adapter->isArmed());

    // Arm command was dispatched
    auto packets = mockTransport->getSentPackets();
    ASSERT_FALSE(packets.empty());
    mockTransport->clearSentPackets();

    // Now firing is permitted
    EXPECT_TRUE(adapter->triggerSingleMeasurement());
    packets = mockTransport->getSentPackets();
    ASSERT_EQ(packets.size(), 1U);
}

TEST(TestSerialLrfAdapter, DisarmStopsLaser)
{
    auto mockTransport = std::make_shared<MockTransport>();
    auto adapter = std::make_shared<SerialLrfAdapter>(mockTransport);

    ASSERT_TRUE(adapter->connect());
    EXPECT_TRUE(adapter->armLaser());
    EXPECT_TRUE(adapter->isArmed());

    EXPECT_TRUE(adapter->disarmLaser());
    EXPECT_FALSE(adapter->isArmed());
    EXPECT_FALSE(adapter->triggerSingleMeasurement());
}

TEST(TestSerialLrfAdapter, InactivityWatchdogAutoDisarm)
{
    auto mockTransport = std::make_shared<MockTransport>();
    SerialLrfConfig cfg;
    cfg.autoDisarmTimeout = std::chrono::milliseconds(120); // Fast timeout for test
    auto adapter = std::make_shared<SerialLrfAdapter>(mockTransport, cfg);

    ASSERT_TRUE(adapter->connect());
    EXPECT_TRUE(adapter->armLaser());
    EXPECT_TRUE(adapter->isArmed());

    // Wait past watchdog timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Must be automatically disarmed!
    EXPECT_FALSE(adapter->isArmed());
    EXPECT_FALSE(adapter->triggerSingleMeasurement());
}

TEST(TestSerialLrfAdapter, DisarmOnTransportDisconnect)
{
    auto mockTransport = std::make_shared<MockTransport>();
    auto adapter = std::make_shared<SerialLrfAdapter>(mockTransport);

    ASSERT_TRUE(adapter->connect());
    EXPECT_TRUE(adapter->armLaser());
    EXPECT_TRUE(adapter->isArmed());

    // Sudden transport disconnection
    mockTransport->injectDisconnect();

    EXPECT_FALSE(adapter->isArmed());
    EXPECT_FALSE(adapter->isConnected());
    EXPECT_FALSE(adapter->triggerSingleMeasurement());
}

// =============================================================================
// Measurement Feedback & Range Gating Tests
// =============================================================================

TEST(TestSerialLrfAdapter, ReceiveNmeaMeasurement)
{
    auto mockTransport = std::make_shared<MockTransport>();
    auto adapter = std::make_shared<SerialLrfAdapter>(mockTransport);
    ASSERT_TRUE(adapter->connect());
    EXPECT_TRUE(adapter->armLaser());

    std::optional<LrfTargetMeasurement> receivedMeas;
    adapter->registerMeasurementCallback([&](const LrfTargetMeasurement& m) {
        receivedMeas = m;
    });

    const std::string echo = NmeaLrfParser::formatNmeaSentence("GPLRF,1780.25,M,OK");
    mockTransport->injectData({ echo.begin(), echo.end() });

    ASSERT_TRUE(receivedMeas.has_value());
    EXPECT_TRUE(receivedMeas->valid);
    EXPECT_NEAR(receivedMeas->slantRangeMeters, 1780.25, 0.01);

    auto last = adapter->lastMeasurement();
    ASSERT_TRUE(last.has_value());
    EXPECT_NEAR(last->slantRangeMeters, 1780.25, 0.01);
}

TEST(TestSerialLrfAdapter, RangeGatingRejectsClutter)
{
    auto mockTransport = std::make_shared<MockTransport>();
    auto adapter = std::make_shared<SerialLrfAdapter>(mockTransport);
    ASSERT_TRUE(adapter->connect());
    EXPECT_TRUE(adapter->armLaser());

    // Range gate: 100m to 2000m
    EXPECT_TRUE(adapter->setRangeGating(100.0, 2000.0));

    // 1. Clutter near target (50m)
    const std::string echoNear = NmeaLrfParser::formatNmeaSentence("GPLRF,50.0,M,OK");
    mockTransport->injectData({ echoNear.begin(), echoNear.end() });
    auto meas1 = adapter->lastMeasurement();
    ASSERT_TRUE(meas1.has_value());
    EXPECT_FALSE(meas1->valid); // Rejected by near-gate!

    // 2. Valid target inside gate (1500m)
    const std::string echoValid = NmeaLrfParser::formatNmeaSentence("GPLRF,1500.0,M,OK");
    mockTransport->injectData({ echoValid.begin(), echoValid.end() });
    auto meas2 = adapter->lastMeasurement();
    ASSERT_TRUE(meas2.has_value());
    EXPECT_TRUE(meas2->valid);
    EXPECT_NEAR(meas2->slantRangeMeters, 1500.0, 0.01);

    // 3. Target beyond max gate (3500m)
    const std::string echoFar = NmeaLrfParser::formatNmeaSentence("GPLRF,3500.0,M,OK");
    mockTransport->injectData({ echoFar.begin(), echoFar.end() });
    auto meas3 = adapter->lastMeasurement();
    ASSERT_TRUE(meas3.has_value());
    EXPECT_FALSE(meas3->valid); // Rejected by far-gate!
}

TEST(TestSerialLrfAdapter, BinaryProtocolIngestion)
{
    auto mockTransport = std::make_shared<MockTransport>();
    SerialLrfConfig cfg;
    cfg.protocolType = LrfProtocolType::Binary;
    auto adapter = std::make_shared<SerialLrfAdapter>(mockTransport, cfg);
    ASSERT_TRUE(adapter->connect());
    EXPECT_TRUE(adapter->armLaser());

    // 2500.0 meters = 2500000 mm = 0x002625A0
    std::vector<uint8_t> frame = {
        0xAA, 0x55, 0x10, 0x07,
        0x00, 0x00, 0x26, 0x25, 0xA0, 250, 24
    };
    const uint16_t crc = BinaryLrfParser::computeCrc16(frame.data(), frame.size());
    frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(crc & 0xFF));

    mockTransport->injectData(frame);

    auto last = adapter->lastMeasurement();
    ASSERT_TRUE(last.has_value());
    EXPECT_TRUE(last->valid);
    EXPECT_NEAR(last->slantRangeMeters, 2500.0, 0.01);
}

// =============================================================================
// Factory & Composite LRF Binding Tests
// =============================================================================

TEST(TestSerialLrfAdapter, FactoryCreateLrfFromUri)
{
    auto simLrf = PayloadFactory::createLrfFromUri("lrf://sim");
    ASSERT_NE(simLrf, nullptr);

    auto serialLrf = PayloadFactory::createLrfFromUri("lrf://serial/COM3?baud=115200&proto=nmea");
    ASSERT_NE(serialLrf, nullptr);

    auto tcpLrf = PayloadFactory::createLrfFromUri("lrf://tcp/127.0.0.1:4001?proto=binary");
    ASSERT_NE(tcpLrf, nullptr);

    auto asciiLrf = PayloadFactory::createLrfFromUri("lrf://udp/127.0.0.1:4002?proto=ascii");
    ASSERT_NE(asciiLrf, nullptr);
}

TEST(TestSerialLrfAdapter, CompositeSlantRangeTargeting)
{
    auto payload = PayloadFactory::createSimulatedPayload();
    ASSERT_NE(payload, nullptr);
    auto lrf = payload->lrf();
    ASSERT_NE(lrf, nullptr);

    const Klv::GeoPoint2D platformGps { 37.7749, -122.4194 };
    const double heading = 0.0;
    const double alt = 1000.0;

    auto ptu = payload->panTilt();
    ASSERT_NE(ptu, nullptr);
    ASSERT_TRUE(ptu->setAbsoluteAngles(0.0, -30.0));

    auto targetNoLrf = payload->calculateTargetCoordinates(platformGps, heading, alt);
    ASSERT_TRUE(targetNoLrf.has_value());

    ASSERT_TRUE(lrf->connect());
    ASSERT_TRUE(lrf->armLaser());
    ASSERT_TRUE(lrf->triggerSingleMeasurement());

    auto targetWithLrf = payload->calculateTargetCoordinates(platformGps, heading, alt);
    ASSERT_TRUE(targetWithLrf.has_value());
}

TEST(TestSerialLrfAdapter, FactoryCompositeWithLrfBinding)
{
    auto payload = PayloadFactory::createFromUri("sim://?lrf=sim");
    ASSERT_NE(payload, nullptr);
    EXPECT_NE(payload->lrf(), nullptr);
}

