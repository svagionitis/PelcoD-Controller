#include "ILaserIlluminator.h"
#include "PayloadHal.h"
#include "PayloadTypes.h"
#include "sim/SimulatedPayload.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>

using namespace PayloadHal;

/// @class LaserIlluminatorTest
/// @brief Test fixture verifying safety interlocks, mode switching, power modulation,
///        beam divergence, and telemetry feedback for ILaserIlluminator.
class LaserIlluminatorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_payload = std::make_unique<SimulatedPayload>();
        ASSERT_TRUE(m_payload->connect());
        m_illuminator = m_payload->illuminator();
        ASSERT_NE(m_illuminator, nullptr);
    }

    void TearDown() override
    {
        if (m_illuminator && m_illuminator->isArmed()) {
            m_illuminator->disarmLaser();
        }
        if (m_payload) {
            m_payload->disconnect();
        }
    }

    std::unique_ptr<SimulatedPayload> m_payload;
    std::shared_ptr<ILaserIlluminator> m_illuminator;
};

TEST_F(LaserIlluminatorTest, InitialStateIsDisarmedAndStandby)
{
    EXPECT_FALSE(m_illuminator->isArmed());
    EXPECT_FALSE(m_illuminator->isEmitting());
    EXPECT_EQ(m_illuminator->mode(), IlluminatorMode::Standby);

    const auto telem = m_illuminator->currentTelemetry();
    EXPECT_FALSE(telem.isArmed);
    EXPECT_FALSE(telem.isEmitting);
    EXPECT_EQ(telem.mode, IlluminatorMode::Standby);
}

TEST_F(LaserIlluminatorTest, SafetyInterlockPreventsEmissionWhenDisarmed)
{
    // Transmitter is disarmed; emission MUST fail unconditionally
    EXPECT_FALSE(m_illuminator->isArmed());
    EXPECT_FALSE(m_illuminator->startEmission());
    EXPECT_FALSE(m_illuminator->isEmitting());

    const auto telem = m_illuminator->currentTelemetry();
    EXPECT_FALSE(telem.isEmitting);
}

TEST_F(LaserIlluminatorTest, ArmingAndDisarmingLifecycle)
{
    EXPECT_TRUE(m_illuminator->armLaser());
    EXPECT_TRUE(m_illuminator->isArmed());

    EXPECT_TRUE(m_illuminator->startEmission());
    EXPECT_TRUE(m_illuminator->isEmitting());

    // Disarming laser must immediately cut off active emission
    EXPECT_TRUE(m_illuminator->disarmLaser());
    EXPECT_FALSE(m_illuminator->isArmed());
    EXPECT_FALSE(m_illuminator->isEmitting());

    // Subsequent start emission attempt must fail because it is now disarmed
    EXPECT_FALSE(m_illuminator->startEmission());
    EXPECT_FALSE(m_illuminator->isEmitting());
}

TEST_F(LaserIlluminatorTest, EmissionStopAndResumeWhileArmed)
{
    EXPECT_TRUE(m_illuminator->armLaser());
    EXPECT_TRUE(m_illuminator->startEmission());
    EXPECT_TRUE(m_illuminator->isEmitting());

    EXPECT_TRUE(m_illuminator->stopEmission());
    EXPECT_FALSE(m_illuminator->isEmitting());
    EXPECT_TRUE(m_illuminator->isArmed());

    // Can re-start emission without needing to re-arm
    EXPECT_TRUE(m_illuminator->startEmission());
    EXPECT_TRUE(m_illuminator->isEmitting());
}

TEST_F(LaserIlluminatorTest, OperationalModeConfiguration)
{
    EXPECT_TRUE(m_illuminator->setMode(IlluminatorMode::Continuous));
    EXPECT_EQ(m_illuminator->mode(), IlluminatorMode::Continuous);
    EXPECT_EQ(m_illuminator->currentTelemetry().mode, IlluminatorMode::Continuous);

    EXPECT_TRUE(m_illuminator->setMode(IlluminatorMode::Pulsed));
    EXPECT_EQ(m_illuminator->mode(), IlluminatorMode::Pulsed);

    EXPECT_TRUE(m_illuminator->setMode(IlluminatorMode::Strobe));
    EXPECT_EQ(m_illuminator->mode(), IlluminatorMode::Strobe);

    EXPECT_TRUE(m_illuminator->setMode(IlluminatorMode::Standby));
    EXPECT_EQ(m_illuminator->mode(), IlluminatorMode::Standby);
}

TEST_F(LaserIlluminatorTest, PowerAndPulseModulation)
{
    EXPECT_TRUE(m_illuminator->setPowerNormalized(0.75));
    EXPECT_NEAR(m_illuminator->currentTelemetry().powerNormalized, 0.75, 1e-3);

    // Clamping limits
    EXPECT_TRUE(m_illuminator->setPowerNormalized(-0.5));
    EXPECT_NEAR(m_illuminator->currentTelemetry().powerNormalized, 0.0, 1e-3);

    EXPECT_TRUE(m_illuminator->setPowerNormalized(1.5));
    EXPECT_NEAR(m_illuminator->currentTelemetry().powerNormalized, 1.0, 1e-3);

    // Frequency
    EXPECT_TRUE(m_illuminator->setPulseFrequency(12.5));
    EXPECT_NEAR(m_illuminator->currentTelemetry().pulseFrequencyHz, 12.5, 1e-3);

    EXPECT_TRUE(m_illuminator->setPulseFrequency(-2.0));
    EXPECT_GE(m_illuminator->currentTelemetry().pulseFrequencyHz, 0.0);
}

TEST_F(LaserIlluminatorTest, BeamDivergenceZoom)
{
    EXPECT_TRUE(m_illuminator->setBeamDivergenceNormalized(0.65));
    EXPECT_NEAR(m_illuminator->currentTelemetry().beamDivergenceNormalized, 0.65, 1e-3);

    EXPECT_TRUE(m_illuminator->setBeamDivergenceNormalized(-0.2));
    EXPECT_NEAR(m_illuminator->currentTelemetry().beamDivergenceNormalized, 0.0, 1e-3);

    EXPECT_TRUE(m_illuminator->setBeamDivergenceNormalized(1.8));
    EXPECT_NEAR(m_illuminator->currentTelemetry().beamDivergenceNormalized, 1.0, 1e-3);
}

TEST_F(LaserIlluminatorTest, TelemetryCallbacksAndThermalFeedback)
{
    std::atomic<bool> callbackFired { false };
    IlluminatorTelemetry lastTelem {};

    m_illuminator->registerTelemetryCallback([&](const IlluminatorTelemetry& telem) {
        callbackFired = true;
        lastTelem = telem;
    });

    EXPECT_TRUE(m_illuminator->armLaser());
    EXPECT_TRUE(callbackFired);
    EXPECT_TRUE(lastTelem.isArmed);

    callbackFired = false;
    EXPECT_TRUE(m_illuminator->setPowerNormalized(0.8));
    EXPECT_TRUE(callbackFired);
    EXPECT_NEAR(lastTelem.powerNormalized, 0.8, 1e-3);

    callbackFired = false;
    EXPECT_TRUE(m_illuminator->startEmission());
    EXPECT_TRUE(callbackFired);
    EXPECT_TRUE(lastTelem.isEmitting);
    // Emitting at 80% power should increase diode junction temperature
    EXPECT_GT(lastTelem.diodeTemperatureC, 25.0);

    callbackFired = false;
    EXPECT_TRUE(m_illuminator->stopEmission());
    EXPECT_TRUE(callbackFired);
    EXPECT_FALSE(lastTelem.isEmitting);
    EXPECT_NEAR(lastTelem.diodeTemperatureC, 25.0, 1e-3);
}

TEST_F(LaserIlluminatorTest, DisconnectDisarmsIlluminator)
{
    EXPECT_TRUE(m_illuminator->armLaser());
    EXPECT_TRUE(m_illuminator->startEmission());
    EXPECT_TRUE(m_illuminator->isEmitting());

    // Disconnecting the entire station must safely shut off emission and disarm
    m_payload->disconnect();
    EXPECT_FALSE(m_illuminator->isEmitting());
    EXPECT_FALSE(m_illuminator->isArmed());
    EXPECT_FALSE(m_illuminator->isConnected());
}
