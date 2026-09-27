#include "Stanag4586Bridge.h"
#include "Stanag4586Types.h"
#include "sim/SimulatedPayload.h"
#include "PayloadStowController.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace PayloadHal;

// =============================================================================
// Test 1: CRC-16 CCITT and Big-Endian Wire Marshalling
// =============================================================================

TEST(TestStanag4586Bridge, Crc16CcittComputation)
{
    // Known test vector for CRC-16 CCITT (poly 0x1021, init 0xFFFF):
    // "123456789" (ASCII bytes) -> 0x29B1
    const char* testStr = "123456789";
    const auto crc = StanagWire::computeCrc16Ccitt(
        reinterpret_cast<const std::uint8_t*>(testStr), std::strlen(testStr));
    EXPECT_EQ(crc, 0x29B1U);
}

TEST(TestStanag4586Bridge, WireMarshallingScalars)
{
    std::uint8_t buffer[64] = { 0 };

    // 16-bit uint
    StanagWire::writeUint16Be(&buffer[0], 0xABCDU);
    EXPECT_EQ(buffer[0], 0xAB);
    EXPECT_EQ(buffer[1], 0xCD);
    EXPECT_EQ(StanagWire::readUint16Be(&buffer[0]), 0xABCDU);

    // 32-bit uint
    StanagWire::writeUint32Be(&buffer[2], 0x12345678U);
    EXPECT_EQ(buffer[2], 0x12);
    EXPECT_EQ(buffer[3], 0x34);
    EXPECT_EQ(buffer[4], 0x56);
    EXPECT_EQ(buffer[5], 0x78);
    EXPECT_EQ(StanagWire::readUint32Be(&buffer[2]), 0x12345678U);

    // 64-bit uint
    StanagWire::writeUint64Be(&buffer[6], 0x0123456789ABCDEFU);
    EXPECT_EQ(StanagWire::readUint64Be(&buffer[6]), 0x0123456789ABCDEFU);

    // 32-bit float
    const float testFloat = -45.125f;
    StanagWire::writeFloatBe(&buffer[14], testFloat);
    EXPECT_FLOAT_EQ(StanagWire::readFloatBe(&buffer[14]), testFloat);

    // 64-bit double
    const double testDouble = 37.7749295;
    StanagWire::writeDoubleBe(&buffer[18], testDouble);
    EXPECT_DOUBLE_EQ(StanagWire::readDoubleBe(&buffer[18]), testDouble);
}

// =============================================================================
// Test 2: Message #2000 Build and Serialization
// =============================================================================

TEST(TestStanag4586Bridge, Message2000BuildAndSerialization)
{
    auto sim = std::make_shared<SimulatedPayload>();
    StanagBridgeConfig config {};
    config.stationId = 1U;
    auto bridge = std::make_shared<Stanag4586Bridge>(sim, config);

    const auto msg = bridge->buildMessage2000(1U);
    EXPECT_EQ(msg.stationId, 1U);
    EXPECT_EQ(msg.payloadType, StanagPayloadType::MultiSensorEoIr);

    // Check capabilities bitmask
    EXPECT_TRUE((msg.capabilities & StanagCapabilities::PanTiltGimbal) != 0U);
    EXPECT_TRUE((msg.capabilities & StanagCapabilities::DaylightEoCamera) != 0U);
    EXPECT_TRUE((msg.capabilities & StanagCapabilities::ThermalIrCamera) != 0U);
    EXPECT_TRUE((msg.capabilities & StanagCapabilities::LaserRangeFinder) != 0U);
    EXPECT_TRUE((msg.capabilities & StanagCapabilities::LaserIlluminator) != 0U);
    EXPECT_TRUE((msg.capabilities & StanagCapabilities::WindowDeIceWiper) != 0U);
    EXPECT_TRUE((msg.capabilities & StanagCapabilities::GeoLockTracking) != 0U);
    EXPECT_TRUE((msg.capabilities & StanagCapabilities::RollStabilization) != 0U);

    // Serialize packet
    const auto packet = bridge->serializeMessage2000(1U);
    const std::size_t expectedSize = STANAG4586_HEADER_SIZE + STANAG_PAYLOAD_SIZE_2000 + 2U;
    ASSERT_EQ(packet.size(), expectedSize);

    // Check sync marker
    EXPECT_EQ(StanagWire::readUint16Be(&packet[0]), STANAG4586_SYNC_MARKER);
    EXPECT_EQ(StanagWire::readUint16Be(&packet[2]), static_cast<std::uint16_t>(StanagMessageId::PayloadConfiguration));
    EXPECT_EQ(StanagWire::readUint16Be(&packet[4]), static_cast<std::uint16_t>(STANAG_PAYLOAD_SIZE_2000));
    EXPECT_EQ(packet[24], 1U); // Station ID

    // Check CRC
    const std::uint16_t calculatedCrc = StanagWire::computeCrc16Ccitt(packet.data(), expectedSize - 2U);
    const std::uint16_t packetCrc = StanagWire::readUint16Be(&packet[expectedSize - 2U]);
    EXPECT_EQ(calculatedCrc, packetCrc);
}

// =============================================================================
// Test 3: Message #2001 Ingestion and #2000 Auto-Response
// =============================================================================

TEST(TestStanag4586Bridge, Message2001ConfigQueryAndAutoReply)
{
    auto sim = std::make_shared<SimulatedPayload>();
    StanagBridgeConfig config {};
    config.stationId = 2U;
    config.autoRespondToConfigRequests = true;
    auto bridge = std::make_shared<Stanag4586Bridge>(sim, config);

    std::vector<std::uint8_t> txPacket;
    bridge->setPacketTxCallback([&txPacket](const std::vector<std::uint8_t>& pkt) {
        txPacket = pkt;
    });

    // Manually construct framed Message #2001 requesting station 2
    std::vector<std::uint8_t> reqPacket(STANAG4586_HEADER_SIZE + STANAG_PAYLOAD_SIZE_2001 + 2U, 0U);
    StanagWire::writeUint16Be(&reqPacket[0], STANAG4586_SYNC_MARKER);
    StanagWire::writeUint16Be(&reqPacket[2], static_cast<std::uint16_t>(StanagMessageId::PayloadConfigurationReq));
    StanagWire::writeUint16Be(&reqPacket[4], static_cast<std::uint16_t>(STANAG_PAYLOAD_SIZE_2001));
    reqPacket[24] = 2U; // Station ID
    reqPacket[STANAG4586_HEADER_SIZE] = 2U; // Payload station ID
    const std::uint16_t crc = StanagWire::computeCrc16Ccitt(reqPacket.data(), reqPacket.size() - 2U);
    StanagWire::writeUint16Be(&reqPacket[reqPacket.size() - 2U], crc);

    const auto res = bridge->processInboundBytes(reqPacket.data(), reqPacket.size());
    EXPECT_EQ(res, StanagParseResult::Success);

    // Verify reply packet was transmitted
    ASSERT_FALSE(txPacket.empty());
    EXPECT_EQ(StanagWire::readUint16Be(&txPacket[2]), static_cast<std::uint16_t>(StanagMessageId::PayloadConfiguration));
    EXPECT_EQ(txPacket[24], 2U);
}

// =============================================================================
// Test 4: Message #2003 Operating Command Ingestion
// =============================================================================

TEST(TestStanag4586Bridge, Message2003OperatingCommandExecution)
{
    auto sim = std::make_shared<SimulatedPayload>();
    StanagBridgeConfig config {};
    config.stationId = 0U;
    auto bridge = std::make_shared<Stanag4586Bridge>(sim, config);

    // 1. Stow command
    Stanag4586Message2003 stowCmd {};
    stowCmd.stationId = 0U;
    stowCmd.commandedMode = StanagOperatingMode::Stowed;
    EXPECT_TRUE(bridge->dispatchMessage2003(stowCmd));
    sim->stowController()->update(0.1);
    EXPECT_TRUE(sim->stowController()->isStowed());

    // 2. Active deploy command
    Stanag4586Message2003 activeCmd {};
    activeCmd.stationId = 0U;
    activeCmd.commandedMode = StanagOperatingMode::Active;
    EXPECT_TRUE(bridge->dispatchMessage2003(activeCmd));
    sim->stowController()->update(0.1);
    EXPECT_TRUE(sim->stowController()->isDeployed());
    EXPECT_FALSE(sim->stowController()->isStowed());

    // 3. Focus & Iris command
    Stanag4586Message2003 opticsCmd {};
    opticsCmd.stationId = 0U;
    opticsCmd.focusMode = StanagFocusMode::ManualFar;
    opticsCmd.manualFocusVelocity = 0.75f;
    opticsCmd.irisMode = StanagIrisMode::ManualOpen;
    opticsCmd.manualIrisNormalized = 0.85f;
    EXPECT_TRUE(bridge->dispatchMessage2003(opticsCmd));

    // 4. LRF Arm and Fire
    Stanag4586Message2003 lrfArmCmd {};
    lrfArmCmd.stationId = 0U;
    lrfArmCmd.lrfCommand = StanagLaserCommand::Arm;
    EXPECT_TRUE(bridge->dispatchMessage2003(lrfArmCmd));
    EXPECT_TRUE(sim->lrf()->isArmed());

    Stanag4586Message2003 lrfFireCmd {};
    lrfFireCmd.stationId = 0U;
    lrfFireCmd.lrfCommand = StanagLaserCommand::FireSinglePulse;
    EXPECT_TRUE(bridge->dispatchMessage2003(lrfFireCmd));

    // 5. Illuminator Arm and Continuous
    Stanag4586Message2003 illCmd {};
    illCmd.stationId = 0U;
    illCmd.illuminatorCommand = StanagIlluminatorCommand::Continuous;
    EXPECT_TRUE(bridge->dispatchMessage2003(illCmd));
    EXPECT_TRUE(sim->illuminator()->isArmed());
    EXPECT_EQ(sim->illuminator()->mode(), IlluminatorMode::Continuous);

    // 6. Auxiliary controls: De-Ice on and single wipe
    Stanag4586Message2003 auxCmd {};
    auxCmd.stationId = 0U;
    auxCmd.auxiliaryControls = StanagAuxControls::DeIceHeaterOn | StanagAuxControls::WiperSingleCycle;
    EXPECT_TRUE(bridge->dispatchMessage2003(auxCmd));
    EXPECT_TRUE(sim->stowController()->isHeaterActive());
}

// =============================================================================
// Test 5: Message #2004 Steering Commands (Angle, Rate, Geo-Pointing)
// =============================================================================

TEST(TestStanag4586Bridge, Message2004SteeringCommandAngleAndRate)
{
    auto sim = std::make_shared<SimulatedPayload>();
    StanagBridgeConfig config {};
    config.stationId = 0U;
    auto bridge = std::make_shared<Stanag4586Bridge>(sim, config);

    // 1. Angle / Position Mode
    Stanag4586Message2004 angleCmd {};
    angleCmd.stationId = 0U;
    angleCmd.steeringMode = StanagSteeringMode::AnglePosition;
    angleCmd.commandedAzimuthDeg = 45.0f;
    angleCmd.commandedElevationDeg = -15.0f;
    angleCmd.commandedRollDeg = 5.0f;

    EXPECT_TRUE(bridge->dispatchMessage2004(angleCmd));
    const auto ptuTelem = sim->panTilt()->currentTelemetry();
    EXPECT_NEAR(ptuTelem.panAngleDeg, 45.0, 1e-2);
    EXPECT_NEAR(ptuTelem.tiltAngleDeg, -15.0, 1e-2);

    // 2. Rate / Velocity Mode
    Stanag4586Message2004 rateCmd {};
    rateCmd.stationId = 0U;
    rateCmd.steeringMode = StanagSteeringMode::RateVelocity;
    rateCmd.commandedAzimuthRateDegPerSec = 10.0f;
    rateCmd.commandedElevationRateDegPerSec = -5.0f;

    EXPECT_TRUE(bridge->dispatchMessage2004(rateCmd));
    const auto rateTelem = sim->panTilt()->currentTelemetry();
    EXPECT_NEAR(rateTelem.panRateDegPerSec, 10.0, 1e-2);
    EXPECT_NEAR(rateTelem.tiltRateDegPerSec, -5.0, 1e-2);

    // 3. Geo-Pointing Mode
    PlatformNavData nav {};
    nav.position = Klv::GeoPoint3D { 37.7749, -122.4194, 1000.0 };
    nav.headingDeg = 0.0;
    bridge->updateNavData(nav);

    Stanag4586Message2004 geoCmd {};
    geoCmd.stationId = 0U;
    geoCmd.steeringMode = StanagSteeringMode::GeoPointing;
    geoCmd.targetLatitudeDeg = 37.7800;
    geoCmd.targetLongitudeDeg = -122.4194;
    geoCmd.targetAltitudeMslMeters = 50.0f;

    EXPECT_TRUE(bridge->dispatchMessage2004(geoCmd));
    EXPECT_TRUE(sim->isGeoLocked());
    const auto lockedGeo = sim->geoLockTarget();
    ASSERT_TRUE(lockedGeo.has_value());
    EXPECT_NEAR(lockedGeo->latitudeDeg, 37.7800, 1e-4);
    EXPECT_NEAR(lockedGeo->longitudeDeg, -122.4194, 1e-4);
}

// =============================================================================
// Test 6: Message #2002 Operating State Generation
// =============================================================================

TEST(TestStanag4586Bridge, Message2002OperatingStateTelemetry)
{
    auto sim = std::make_shared<SimulatedPayload>();
    StanagBridgeConfig config {};
    config.stationId = 0U;
    auto bridge = std::make_shared<Stanag4586Bridge>(sim, config);

    // Position payload
    sim->panTilt()->setAbsoluteAngles(25.0, -10.0);
    sim->primaryCamera()->setZoomNormalized(0.5);
    sim->lrf()->armLaser();
    sim->setSimulatedSlantRange(2500.0);
    sim->lrf()->triggerSingleMeasurement();

    PlatformNavData nav {};
    nav.position = Klv::GeoPoint3D { 38.0, 24.0, 1500.0 };
    nav.headingDeg = 90.0;

    const auto telem = bridge->buildMessage2002(nav);
    EXPECT_EQ(telem.stationId, 0U);
    EXPECT_EQ(telem.operatingMode, StanagOperatingMode::Active);
    EXPECT_EQ(telem.activeSensor, StanagSensorChannel::DaylightEo);
    EXPECT_NEAR(telem.azimuthDeg, 25.0f, 1e-1f);
    EXPECT_NEAR(telem.elevationDeg, -10.0f, 1e-1f);
    EXPECT_EQ(telem.laserState, StanagLaserState::Armed);
    EXPECT_NEAR(telem.slantRangeMeters, 2500.0f, 1e-1f);
    EXPECT_EQ(telem.bitStatus, StanagBitStatus::Ok);
    EXPECT_EQ(telem.targetLocationValid, 1U);

    // Serialize packet
    const auto packet = bridge->serializeMessage2002(nav);
    const std::size_t expectedSize = STANAG4586_HEADER_SIZE + STANAG_PAYLOAD_SIZE_2002 + 2U;
    ASSERT_EQ(packet.size(), expectedSize);

    // Check packet parsing
    EXPECT_EQ(StanagWire::readUint16Be(&packet[2]), static_cast<std::uint16_t>(StanagMessageId::PayloadOperatingState));
    const std::uint16_t calculatedCrc = StanagWire::computeCrc16Ccitt(packet.data(), expectedSize - 2U);
    const std::uint16_t packetCrc = StanagWire::readUint16Be(&packet[expectedSize - 2U]);
    EXPECT_EQ(calculatedCrc, packetCrc);
}

// =============================================================================
// Test 7: Packet Ingestion Validation & Rejection
// =============================================================================

TEST(TestStanag4586Bridge, InboundPacketValidationFailures)
{
    auto sim = std::make_shared<SimulatedPayload>();
    StanagBridgeConfig config {};
    config.stationId = 0U;
    auto bridge = std::make_shared<Stanag4586Bridge>(sim, config);

    // 1. Buffer too short (< 28 bytes)
    std::uint8_t shortBuf[10] = { 0 };
    EXPECT_EQ(bridge->processInboundBytes(shortBuf, sizeof(shortBuf)), StanagParseResult::BufferTooShort);

    // 2. Invalid preamble sync bytes
    std::vector<std::uint8_t> badSync(STANAG4586_MIN_PACKET_SIZE, 0U);
    badSync[0] = 0xAA;
    badSync[1] = 0x55;
    EXPECT_EQ(bridge->processInboundBytes(badSync.data(), badSync.size()), StanagParseResult::InvalidSync);

    // 3. CRC mismatch
    auto validPacket = bridge->serializeMessage2000(0U);
    validPacket[STANAG4586_HEADER_SIZE + 2] ^= 0xFF; // Corrupt payload byte
    EXPECT_EQ(bridge->processInboundBytes(validPacket.data(), validPacket.size()), StanagParseResult::CrcMismatch);

    // 4. Station mismatch
    StanagBridgeConfig cfgOther {};
    cfgOther.stationId = 5U; // Expects station 5
    auto bridgeOther = std::make_shared<Stanag4586Bridge>(sim, cfgOther);

    auto pktStation0 = bridge->serializeMessage2000(0U);
    // Send packet targeted to station 0 to a bridge expecting station 5
    EXPECT_EQ(bridgeOther->processInboundBytes(pktStation0.data(), pktStation0.size()), StanagParseResult::StationMismatch);
}

// =============================================================================
// Test 8: Periodic Telemetry Thread Lifecycle
// =============================================================================

TEST(TestStanag4586Bridge, PeriodicTelemetryThreadLifecycle)
{
    auto sim = std::make_shared<SimulatedPayload>();
    StanagBridgeConfig config {};
    config.stationId = 0U;
    config.enablePeriodicTelemetry = false;
    auto bridge = std::make_shared<Stanag4586Bridge>(sim, config);

    std::atomic<int> packetCount { 0 };
    bridge->setPacketTxCallback([&packetCount](const std::vector<std::uint8_t>&) {
        packetCount.fetch_add(1);
    });

    EXPECT_FALSE(bridge->isPeriodicTelemetryRunning());
    bridge->startPeriodicTelemetry(std::chrono::milliseconds(20));
    EXPECT_TRUE(bridge->isPeriodicTelemetryRunning());

    // Sleep for 100ms; should receive around 4-5 packets
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    bridge->stopPeriodicTelemetry();
    EXPECT_FALSE(bridge->isPeriodicTelemetryRunning());
    EXPECT_GE(packetCount.load(), 2);
}

// =============================================================================
// Test 9: SimulatedPayload stanagBridge Integration
// =============================================================================

TEST(TestStanag4586Bridge, SimulatedPayloadAccessor)
{
    auto sim = std::make_shared<SimulatedPayload>();
    auto bridge = sim->stanagBridge();
    ASSERT_NE(bridge, nullptr);

    // Multiple calls return the same cached bridge instance
    auto bridge2 = sim->stanagBridge();
    EXPECT_EQ(bridge, bridge2);

    // Build configuration through the integrated bridge
    const auto cfg = bridge->buildMessage2000();
    EXPECT_TRUE((cfg.capabilities & StanagCapabilities::PanTiltGimbal) != 0U);
}
