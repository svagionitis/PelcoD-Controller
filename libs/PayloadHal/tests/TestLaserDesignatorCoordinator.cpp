#include "LaserDesignatorCoordinator.h"
#include "LaserDesignatorTypes.h"
#include "GimbalSectorBlanking.h"
#include "sim/SimulatedPayload.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <thread>

using namespace PayloadHal;

// Mock PTU for deterministic testing
class MockPtu : public IPanTiltUnit {
public:
    MockPtu() = default;

    bool connect() override { m_connected = true; return true; }
    void disconnect() override { m_connected = false; }
    [[nodiscard]] bool isConnected() const noexcept override { return m_connected; }
    [[nodiscard]] DeviceState state() const noexcept override { return m_connected ? DeviceState::Ready : DeviceState::Disconnected; }
    [[nodiscard]] DeviceInfo info() const noexcept override { return DeviceInfo{"Mock", "Ptu", "1.0"}; }
    void registerStateCallback(StateCallback) override {}

    bool setRate(double panDegPerSec, double tiltDegPerSec) override
    {
        m_panRate = panDegPerSec;
        m_tiltRate = tiltDegPerSec;
        return true;
    }
    bool setNormalizedVelocity(float, float) override { return true; }
    bool setAbsoluteAngles(double panDeg, double tiltDeg) override
    {
        m_pan = panDeg;
        m_tilt = tiltDeg;
        return true;
    }
    bool setRelativeNudge(double, double) override { return true; }
    bool stopMotion() override
    {
        m_panRate = 0.0;
        m_tiltRate = 0.0;
        return true;
    }

    [[nodiscard]] bool supportsStabilization() const noexcept override { return false; }
    bool setStabilizationMode(StabilizationMode) override { return false; }
    [[nodiscard]] StabilizationMode stabilizationMode() const noexcept override { return StabilizationMode::Disabled; }
    bool zeroGyroDrift() override { return false; }

    bool getLimits(double& minPan, double& maxPan, double& minTilt, double& maxTilt) const override
    {
        minPan = -180.0; maxPan = 180.0; minTilt = -90.0; maxTilt = 45.0;
        return true;
    }
    bool savePreset(std::uint8_t, const std::string&) override { return false; }
    bool recallPreset(std::uint8_t) override { return false; }

    void registerTelemetryCallback(TelemetryCallback) override {}

    [[nodiscard]] GimbalTelemetry currentTelemetry() const override
    {
        GimbalTelemetry telem {};
        telem.panAngleDeg = m_pan;
        telem.tiltAngleDeg = m_tilt;
        telem.panRateDegPerSec = m_panRate;
        telem.tiltRateDegPerSec = m_tiltRate;
        return telem;
    }

    void setPanTilt(double panDeg, double tiltDeg)
    {
        m_pan = panDeg;
        m_tilt = tiltDeg;
    }

    double m_pan { 0.0 };
    double m_tilt { -15.0 };
    double m_panRate { 0.0 };
    double m_tiltRate { 0.0 };
    bool m_connected { true };
};

// =============================================================================
// Test 1: STANAG 3733 PRF Code Validation
// =============================================================================

TEST(TestLaserDesignatorCoordinator, Stanag3733CodeValidation)
{
    // Band I valid (1111 - 1488, digits 1-8)
    EXPECT_TRUE(Stanag3733::isValidCode(1111));
    EXPECT_TRUE(Stanag3733::isValidCode(1234));
    EXPECT_TRUE(Stanag3733::isValidCode(1488));
    EXPECT_EQ(Stanag3733::bandOf(1111), PrfBand::BandI);
    EXPECT_EQ(Stanag3733::bandOf(1488), PrfBand::BandI);

    // Band II valid (1511 - 1788, digits 1-8)
    EXPECT_TRUE(Stanag3733::isValidCode(1511));
    EXPECT_TRUE(Stanag3733::isValidCode(1688));
    EXPECT_TRUE(Stanag3733::isValidCode(1788));
    EXPECT_EQ(Stanag3733::bandOf(1511), PrfBand::BandII);
    EXPECT_EQ(Stanag3733::bandOf(1788), PrfBand::BandII);

    // Invalid: out of range or digits with 0 or 9
    EXPECT_FALSE(Stanag3733::isValidCode(1000));
    EXPECT_FALSE(Stanag3733::isValidCode(1119)); // 9 not allowed in octal-based notation
    EXPECT_FALSE(Stanag3733::isValidCode(1204)); // 0 not allowed
    EXPECT_FALSE(Stanag3733::isValidCode(1491)); // 9 in tens place
    EXPECT_FALSE(Stanag3733::isValidCode(1499));
    EXPECT_FALSE(Stanag3733::isValidCode(1500)); // < 1511
    EXPECT_FALSE(Stanag3733::isValidCode(1789)); // 9
    EXPECT_FALSE(Stanag3733::isValidCode(1811)); // > 1788
    EXPECT_FALSE(Stanag3733::isValidCode(9999));
}

// =============================================================================
// Test 2: STANAG 3733 Timing and Frequency Formulations
// =============================================================================

TEST(TestLaserDesignatorCoordinator, Stanag3733TimingAndFrequency)
{
    // Code 1111: Band I lower limit
    const double interval1111 = Stanag3733::pulseIntervalUs(1111);
    EXPECT_GT(interval1111, 0.0);
    const double freq1111 = Stanag3733::pulseFrequencyHz(1111);
    EXPECT_NEAR(freq1111, 1000000.0 / interval1111, 1e-4);

    // Code 1688: Band II default
    const double interval1688 = Stanag3733::pulseIntervalUs(1688);
    EXPECT_GT(interval1688, 0.0);
    const double freq1688 = Stanag3733::pulseFrequencyHz(1688);
    EXPECT_NEAR(freq1688, 1000000.0 / interval1688, 1e-4);

    // Coordinator methods
    auto ptu = std::make_shared<MockPtu>();
    LaserDesignatorCoordinator coordinator(ptu, nullptr, nullptr);

    EXPECT_TRUE(coordinator.setPrfCode(1688));
    EXPECT_EQ(coordinator.prfCode(), 1688);
    EXPECT_EQ(coordinator.prfBand(), PrfBand::BandII);
    EXPECT_DOUBLE_EQ(coordinator.pulseIntervalMicroseconds(), interval1688);
    EXPECT_DOUBLE_EQ(coordinator.pulseFrequencyHz(), freq1688);

    // Invalid code rejection
    EXPECT_FALSE(coordinator.setPrfCode(9999));
    EXPECT_EQ(coordinator.prfCode(), 1688); // unchanged
}

// =============================================================================
// Test 3: Capacitor Bank Charging State Machine and Emergency Dump
// =============================================================================

TEST(TestLaserDesignatorCoordinator, CapacitorBankChargingAndDump)
{
    auto ptu = std::make_shared<MockPtu>();
    LaserDesignatorCoordinator coordinator(ptu, nullptr, nullptr);

    // Initially disarmed and uncharged
    EXPECT_EQ(coordinator.designatorState(), DesignatorState::Disarmed);
    EXPECT_FALSE(coordinator.isArmed());
    EXPECT_DOUBLE_EQ(coordinator.capacitorTelemetry().voltageVolts, 0.0);
    EXPECT_EQ(coordinator.capacitorTelemetry().state, CapacitorState::Discharged);

    // Arming begins charging
    EXPECT_TRUE(coordinator.armDesignator());
    EXPECT_TRUE(coordinator.isArmed());
    EXPECT_EQ(coordinator.designatorState(), DesignatorState::Armed);
    EXPECT_EQ(coordinator.capacitorTelemetry().state, CapacitorState::Charging);

    // Advance charging simulation
    for (int i = 0; i < 50; ++i) {
        coordinator.update(0.1);
    }

    // Capacitor should be fully charged and ready
    EXPECT_GE(coordinator.capacitorTelemetry().voltageVolts, coordinator.config().firingVoltageVolts * 0.95);
    EXPECT_EQ(coordinator.capacitorTelemetry().state, CapacitorState::Ready);
    EXPECT_EQ(coordinator.designatorState(), DesignatorState::ReadyToFire);

    // Emergency dump
    coordinator.triggerEmergencyDump();
    EXPECT_EQ(coordinator.designatorState(), DesignatorState::Disarmed);
    EXPECT_FALSE(coordinator.isArmed());

    // Advance simulation to allow bleed resistor to discharge capacitor
    for (int i = 0; i < 10; ++i) {
        coordinator.update(0.1);
    }
    EXPECT_DOUBLE_EQ(coordinator.capacitorTelemetry().voltageVolts, 0.0);
    EXPECT_EQ(coordinator.capacitorTelemetry().state, CapacitorState::Discharged);
}

// =============================================================================
// Test 4: Thermal Dissipation Model and Over-Temperature Cutoff
// =============================================================================

TEST(TestLaserDesignatorCoordinator, ThermalDissipationAndOverTemperatureCutoff)
{
    auto ptu = std::make_shared<MockPtu>();
    LaserDesignatorCoordinator coordinator(ptu, nullptr, nullptr);

    DesignatorConfig cfg = coordinator.config();
    cfg.thermalCapacitanceJPerC = 2.0; // Fast thermal response for deterministic unit test
    cfg.thermalDissipationWPerC = 0.5;
    cfg.warningTempThresholdC = 50.0;
    cfg.cutoffTempThresholdC = 65.0;
    cfg.safeRecoveryTempThresholdC = 45.0;
    cfg.pulseEnergyJoules = 2.0; // 2.0 J high-power pulse for accelerated thermal testing
    cfg.enforcedCooldownRatio = 0.1;
    coordinator.setConfig(cfg);

    coordinator.armDesignator();
    // Simulate charging to full readiness
    for (int i = 0; i < 50; ++i) {
        coordinator.update(0.1);
    }
    ASSERT_EQ(coordinator.designatorState(), DesignatorState::ReadyToFire);

    // Start designation
    EXPECT_TRUE(coordinator.startDesignation());
    EXPECT_TRUE(coordinator.isDesignating());
    EXPECT_EQ(coordinator.designatorState(), DesignatorState::Designating);

    // Simulate firing until over-temp cutoff is triggered (> 65 C)
    bool cutoffTripped = false;
    for (int i = 0; i < 500; ++i) {
        coordinator.update(0.1);
        if (coordinator.designatorState() == DesignatorState::ThermalCutoff) {
            cutoffTripped = true;
            break;
        }
    }

    EXPECT_TRUE(cutoffTripped);
    EXPECT_FALSE(coordinator.isDesignating());
    EXPECT_GE(coordinator.thermalTelemetry().diodeTempC, cfg.cutoffTempThresholdC);
    EXPECT_EQ(coordinator.thermalTelemetry().zone, LtdThermalZone::Critical);
    EXPECT_GE(coordinator.thermalTelemetry().thermalTripCount, 1U);

    // Advance cooling until temperature drops below recovery threshold
    for (int i = 0; i < 1000; ++i) {
        coordinator.update(0.1);
        if (coordinator.thermalTelemetry().diodeTempC <= cfg.safeRecoveryTempThresholdC) {
            break;
        }
    }

    EXPECT_LE(coordinator.thermalTelemetry().diodeTempC, cfg.safeRecoveryTempThresholdC);
    EXPECT_EQ(coordinator.designatorState(), DesignatorState::Disarmed);
}

// =============================================================================
// Test 5: Sector Blanking Safety Interlock
// =============================================================================

TEST(TestLaserDesignatorCoordinator, SectorBlankingSafetyInterlock)
{
    auto ptu = std::make_shared<MockPtu>();
    auto sectorBlanking = std::make_shared<GimbalSectorBlanking>();

    // Add keep-out zone from pan 10 to 30 deg, tilt -20 to 0 deg
    BlankingZone zone {};
    zone.id = "AircraftWing";
    zone.description = "Aircraft Wing / Propeller";
    zone.azMinDeg = 10.0;
    zone.azMaxDeg = 30.0;
    zone.elMinDeg = -20.0;
    zone.elMaxDeg = 0.0;
    zone.safetyMarginDeg = 0.0;
    zone.type = SectorZoneType::LaserInhibit;
    sectorBlanking->addZone(zone);

    LaserDesignatorCoordinator coordinator(ptu, nullptr, sectorBlanking);

    // Aim outside zone (0 deg pan, -10 deg tilt)
    ptu->setPanTilt(0.0, -10.0);
    EXPECT_TRUE(coordinator.armDesignator());
    for (int i = 0; i < 50; ++i) coordinator.update(0.1);
    EXPECT_EQ(coordinator.designatorState(), DesignatorState::ReadyToFire);

    // Starting designation outside zone should succeed
    EXPECT_TRUE(coordinator.startDesignation());
    EXPECT_TRUE(coordinator.isDesignating());

    // Pan inside keep-out zone (20 deg pan, -10 deg tilt)
    ptu->setPanTilt(20.0, -10.0);
    coordinator.update(0.1);

    // Sector blanking interlock must automatically halt active designation
    EXPECT_FALSE(coordinator.isDesignating());
}

// =============================================================================
// Test 6: MIL-HDBK-828 Laser Hazard Fan Footprint & Attack Corridor
// =============================================================================

TEST(TestLaserDesignatorCoordinator, MilHdbk828HazardFanFootprint)
{
    auto ptu = std::make_shared<MockPtu>();
    LaserDesignatorCoordinator coordinator(ptu, nullptr, nullptr);

    // Platform at 37.7749 N, -122.4194 E, 1500m altitude MSL
    Klv::GeoPoint3D platform { 37.7749, -122.4194, 1500.0 };
    double headingDeg = 0.0; // North
    ptu->setPanTilt(45.0, -30.0); // 45 deg azimuth, -30 deg depression

    LaserHazardFan hazardFan = coordinator.computeHazardFan(platform, headingDeg);

    EXPECT_GT(hazardFan.nohdMeters, 0.0);
    EXPECT_GT(hazardFan.enohdMeters, hazardFan.nohdMeters);
    EXPECT_FALSE(hazardFan.hazardPolygon.empty());
    EXPECT_GT(hazardFan.bufferHalfAngleDeg, 0.0);
    EXPECT_NE(hazardFan.minSafeAttackHeadingDeg, hazardFan.maxSafeAttackHeadingDeg);
}

// =============================================================================
// Test 7: 4-Quadrant Laser Energy Discrimination
// =============================================================================

TEST(TestLaserDesignatorCoordinator, QuadrantSpotTrackerDiscrimination)
{
    auto ptu = std::make_shared<MockPtu>();
    LaserDesignatorCoordinator coordinator(ptu, nullptr, nullptr);

    coordinator.startLstSearch();
    EXPECT_EQ(coordinator.lstState(), LstState::Searching);

    // Case 1: Centered spot (equal power in all 4 quadrants)
    coordinator.inject4QuadrantEnergies(10.0, 10.0, 10.0, 10.0, 1688);
    auto spotCentered = coordinator.currentSpotMeasurement();
    ASSERT_TRUE(spotCentered.has_value());
    EXPECT_TRUE(spotCentered->valid);
    EXPECT_NEAR(spotCentered->errorX, 0.0, 1e-4);
    EXPECT_NEAR(spotCentered->errorY, 0.0, 1e-4);
    EXPECT_EQ(spotCentered->detectedPrfCode, 1688);

    // Case 2: Displaced Top-Right
    // A: TL=5, B: TR=25, C: BL=5, D: BR=15
    // Horizontal = ((B+D) - (A+C)) / Total = ((25+15) - (5+5)) / 50 = (40 - 10) / 50 = +0.6 (Right)
    // Vertical = ((A+B) - (C+D)) / Total = ((5+25) - (5+15)) / 50 = (30 - 20) / 50 = +0.2 (Up)
    coordinator.inject4QuadrantEnergies(5.0, 25.0, 5.0, 15.0, 1688);
    auto spotTopRight = coordinator.currentSpotMeasurement();
    ASSERT_TRUE(spotTopRight.has_value());
    EXPECT_GT(spotTopRight->errorX, 0.0);
    EXPECT_GT(spotTopRight->errorY, 0.0);
}

// =============================================================================
// Test 8: LST Seeker State Machine and PRF Matching
// =============================================================================

TEST(TestLaserDesignatorCoordinator, LstSeekerStateMachineAndPrfMatching)
{
    auto ptu = std::make_shared<MockPtu>();
    LaserDesignatorCoordinator coordinator(ptu, nullptr, nullptr);

    coordinator.setLstTargetPrfCode(1688);
    coordinator.startLstSearch();
    EXPECT_EQ(coordinator.lstState(), LstState::Searching);

    // Pulse with mismatched PRF (1111 != 1688) should be rejected
    coordinator.injectSimulatedSpot(0.1, 0.2, 1111);
    EXPECT_EQ(coordinator.lstState(), LstState::Searching);

    // Pulse with matching PRF (1688) transitions to Acquired on update
    coordinator.injectSimulatedSpot(0.1, 0.2, 1688);
    coordinator.update(0.05);
    EXPECT_EQ(coordinator.lstState(), LstState::Acquired);

    // Consecutive matching pulses transition to Tracking
    for (int i = 0; i < 5; ++i) {
        coordinator.injectSimulatedSpot(0.1, 0.2, 1688);
        coordinator.update(0.05);
    }
    EXPECT_EQ(coordinator.lstState(), LstState::Tracking);

    // Clear spot and advance past coast timeout
    coordinator.clearSimulatedSpot();
    coordinator.update(0.5);
    EXPECT_EQ(coordinator.lstState(), LstState::Coasting);

    // Advance beyond total coast timeout (2.0s) -> Lost
    for (int i = 0; i < 30; ++i) {
        coordinator.update(0.1);
    }
    EXPECT_EQ(coordinator.lstState(), LstState::Lost);
}

// =============================================================================
// Test 9: Closed-Loop PTU Auto-Cueing
// =============================================================================

TEST(TestLaserDesignatorCoordinator, ClosedLoopPtuAutoCueing)
{
    auto ptu = std::make_shared<MockPtu>();
    ptu->setPanTilt(0.0, 0.0);

    LaserDesignatorCoordinator coordinator(ptu, nullptr, nullptr);
    coordinator.setLstTargetPrfCode(1688);
    coordinator.setAutoCueingEnabled(true);
    EXPECT_TRUE(coordinator.isAutoCueingEnabled());

    coordinator.startLstSearch();

    // Lock onto an offset spot (X = +0.5 Right, Y = -0.3 Down)
    for (int i = 0; i < 5; ++i) {
        coordinator.injectSimulatedSpot(0.5, -0.3, 1688);
        coordinator.update(0.05);
    }
    ASSERT_EQ(coordinator.lstState(), LstState::Tracking);

    // MockPtu should have received non-zero pan and tilt speeds to null error
    EXPECT_NE(ptu->m_panRate, 0.0);
    EXPECT_NE(ptu->m_tiltRate, 0.0);
}

// =============================================================================
// Test 10: Integration with SimulatedPayload
// =============================================================================

TEST(TestLaserDesignatorCoordinator, SimulatedPayloadIntegration)
{
    auto simPayload = std::make_shared<SimulatedPayload>();
    ASSERT_TRUE(simPayload->connect());

    auto designator = simPayload->laserDesignator();
    ASSERT_NE(designator, nullptr);

    // Initial state through SimulatedPayload
    EXPECT_EQ(designator->designatorState(), DesignatorState::Disarmed);

    // Configure PRF code 1688 (Band II)
    EXPECT_TRUE(designator->setPrfCode(1688));
    EXPECT_EQ(designator->prfCode(), 1688);
    EXPECT_EQ(designator->prfBand(), PrfBand::BandII);

    // Test arming through simulated payload
    EXPECT_TRUE(designator->armDesignator());
    for (int i = 0; i < 50; ++i) {
        designator->update(0.1);
    }
    EXPECT_EQ(designator->designatorState(), DesignatorState::ReadyToFire);

    // Verify disarm
    EXPECT_TRUE(designator->disarmDesignator());
    EXPECT_EQ(designator->designatorState(), DesignatorState::Disarmed);

    simPayload->disconnect();
}
