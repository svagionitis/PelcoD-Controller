#include <gtest/gtest.h>

#include "PayloadHal.h"
#include "PayloadStowController.h"
#include "sim/SimulatedPayload.h"

#include <cmath>
#include <memory>

using namespace PayloadHal;

TEST(TestPayloadStowController, StowAndDeployTransitions)
{
    auto sim = std::make_shared<SimulatedPayload>();
    PayloadStowController controller(sim->panTilt(), sim->presetManager(), sim->primaryCamera());

    EXPECT_EQ(controller.state(), StowState::Deployed);
    EXPECT_TRUE(controller.isDeployed());
    EXPECT_FALSE(controller.isStowed());

    bool stateChangedFired = false;
    StowState oldSt = StowState::Deployed;
    StowState newSt = StowState::Deployed;

    controller.setStateChangedCallback([&](StowState prev, StowState curr, const std::string&) {
        stateChangedFired = true;
        oldSt = prev;
        newSt = curr;
    });

    // Zoom daylight camera in
    sim->primaryCamera()->setZoomNormalized(0.75);
    EXPECT_GT(sim->primaryCamera()->currentTelemetry().normalizedZoom, 0.5);

    // Command stow
    EXPECT_TRUE(controller.stow());
    EXPECT_EQ(controller.state(), StowState::Stowing);
    EXPECT_TRUE(stateChangedFired);
    EXPECT_EQ(oldSt, StowState::Deployed);
    EXPECT_EQ(newSt, StowState::Stowing);

    // Optical zoom should have retracted to wide (0.0)
    EXPECT_NEAR(sim->primaryCamera()->currentTelemetry().normalizedZoom, 0.0, 1e-4);

    // Gimbal arrived at stow pose (SimPtu sets angles immediately)
    stateChangedFired = false;
    controller.update(0.1);
    EXPECT_TRUE(stateChangedFired);
    EXPECT_EQ(controller.state(), StowState::Stowed);
    EXPECT_TRUE(controller.isStowed());
    EXPECT_FALSE(controller.isDeployed());

    // Command deploy
    stateChangedFired = false;
    EXPECT_TRUE(controller.deploy());
    EXPECT_EQ(controller.state(), StowState::Deploying);

    controller.update(0.1);
    EXPECT_EQ(controller.state(), StowState::Deployed);
    EXPECT_TRUE(controller.isDeployed());
}

TEST(TestPayloadStowController, MaintenanceStanceTransition)
{
    auto sim = std::make_shared<SimulatedPayload>();
    PayloadStowController controller(sim->panTilt(), sim->presetManager(), sim->primaryCamera());

    EXPECT_TRUE(controller.moveToMaintenance());
    EXPECT_EQ(controller.state(), StowState::MovingToMaintenance);

    controller.update(0.1);
    EXPECT_EQ(controller.state(), StowState::Maintenance);

    // Verify gimbal orientation is at maintenance pose (pan 90, tilt 0)
    const auto telem = sim->panTilt()->currentTelemetry();
    EXPECT_NEAR(telem.panAngleDeg, 90.0, 0.1);
    EXPECT_NEAR(telem.tiltAngleDeg, 0.0, 0.1);
}

TEST(TestPayloadStowController, VehicleMotionSafetyInterlock)
{
    auto sim = std::make_shared<SimulatedPayload>();
    PayloadStowController controller(sim->panTilt(), sim->presetManager(), sim->primaryCamera());

    // Deployed state is NOT safe for vehicle motion
    EXPECT_FALSE(controller.isSafeForVehicleMotion());

    bool interlockAlertFired = false;
    bool motionSafeReport = true;
    controller.setInterlockAlertCallback([&](bool safe, const std::string&) {
        interlockAlertFired = true;
        motionSafeReport = safe;
    });

    // Notify vehicle motion while deployed
    controller.setVehicleMotionActive(true);
    EXPECT_TRUE(controller.isVehicleMotionActive());
    EXPECT_TRUE(interlockAlertFired);
    EXPECT_FALSE(motionSafeReport);

    // With vehicle motion interlock enabled, deploy should be rejected while vehicle moves
    controller.stow();
    controller.update(0.1);
    EXPECT_TRUE(controller.isStowed());
    EXPECT_TRUE(controller.isSafeForVehicleMotion());

    // Attempt deploy while vehicle is still in motion
    interlockAlertFired = false;
    EXPECT_FALSE(controller.deploy());
    EXPECT_TRUE(interlockAlertFired);
    EXPECT_TRUE(controller.isStowed()); // Remained stowed

    // Vehicle stops moving
    controller.setVehicleMotionActive(false);
    EXPECT_FALSE(controller.isVehicleMotionActive());
    EXPECT_TRUE(controller.deploy());
    controller.update(0.1);
    EXPECT_TRUE(controller.isDeployed());

    // Test auto-stow on vehicle motion
    controller.setAutoStowOnVehicleMotion(true);
    EXPECT_TRUE(controller.isAutoStowOnVehicleMotionEnabled());

    controller.setVehicleMotionActive(true);
    EXPECT_EQ(controller.state(), StowState::Stowing);
    controller.update(0.1);
    EXPECT_TRUE(controller.isStowed());
}

TEST(TestPayloadStowController, MechanicalLockingPin)
{
    auto sim = std::make_shared<SimulatedPayload>();
    PayloadStowController controller(sim->panTilt(), sim->presetManager(), sim->primaryCamera());

    // Cannot engage mechanical lock while deployed
    EXPECT_FALSE(controller.engageMechanicalLock());
    EXPECT_FALSE(controller.isMechanicalLockEngaged());

    // Stow gimbal and engage lock
    EXPECT_TRUE(controller.stow());
    controller.update(0.1);
    EXPECT_TRUE(controller.isStowed());

    EXPECT_TRUE(controller.engageMechanicalLock());
    EXPECT_TRUE(controller.isMechanicalLockEngaged());

    // Deploying automatically disengages mechanical lock
    controller.setVehicleMotionActive(false);
    EXPECT_TRUE(controller.deploy());
    EXPECT_FALSE(controller.isMechanicalLockEngaged());
}

TEST(TestPayloadStowController, WindowHeaterAndThermostatDeIce)
{
    auto sim = std::make_shared<SimulatedPayload>();
    PayloadStowController controller(sim->panTilt(), sim->presetManager(), sim->primaryCamera());

    EXPECT_EQ(controller.heaterMode(), HeaterMode::Off);
    EXPECT_FALSE(controller.isHeaterActive());

    // Manual on
    controller.setHeaterMode(HeaterMode::ManualOn);
    EXPECT_TRUE(controller.isHeaterActive());

    // Turn off
    controller.setHeaterMode(HeaterMode::Off);
    EXPECT_FALSE(controller.isHeaterActive());

    // AutoThermostat mode
    controller.updateAmbientTemperature(15.0); // Mild temperature
    controller.setHeaterMode(HeaterMode::AutoThermostat);
    EXPECT_FALSE(controller.isHeaterActive());

    // Temperature drops below autoDeIceOnTempC (2.0°C)
    controller.updateAmbientTemperature(-3.0);
    EXPECT_TRUE(controller.isHeaterActive());

    // Temperature warms above autoDeIceOffTempC (8.0°C)
    controller.updateAmbientTemperature(9.5);
    EXPECT_FALSE(controller.isHeaterActive());

    // Max continuous heat safety timer
    controller.setHeaterMode(HeaterMode::ManualOn);
    EXPECT_TRUE(controller.isHeaterActive());

    // Advance 901 seconds (past 900s timeout)
    controller.update(901.0);
    EXPECT_FALSE(controller.isHeaterActive());
}

TEST(TestPayloadStowController, WindshieldWiperRoutines)
{
    auto sim = std::make_shared<SimulatedPayload>();
    PayloadStowController controller(sim->panTilt(), sim->presetManager(), sim->primaryCamera());

    EXPECT_EQ(controller.wiperMode(), WiperMode::Off);
    EXPECT_EQ(controller.wiperState(), WiperState::Parked);

    // Single wipe trigger
    EXPECT_TRUE(controller.triggerSingleWipe());
    EXPECT_EQ(controller.wiperMode(), WiperMode::SingleWipe);
    EXPECT_EQ(controller.wiperState(), WiperState::Wiping);

    // Advance 1.3 seconds (past 1.2s stroke duration)
    controller.update(1.3);
    EXPECT_EQ(controller.wiperMode(), WiperMode::Off);
    EXPECT_EQ(controller.wiperState(), WiperState::Parked);

    // Interval wipe mode
    controller.setWiperMode(WiperMode::IntervalWipe);
    EXPECT_EQ(controller.wiperMode(), WiperMode::IntervalWipe);
    EXPECT_EQ(controller.wiperState(), WiperState::Wiping);

    // Finish current wipe stroke
    controller.update(1.3);
    EXPECT_EQ(controller.wiperState(), WiperState::Parked);

    // Dwell during interval delay (10s)
    controller.update(5.0);
    EXPECT_EQ(controller.wiperState(), WiperState::Parked);

    // Advance past interval delay
    controller.update(5.5);
    EXPECT_EQ(controller.wiperState(), WiperState::Wiping);
}

TEST(TestPayloadStowController, CoordinatedWasherFluidCycle)
{
    auto sim = std::make_shared<SimulatedPayload>();
    PayloadStowController controller(sim->panTilt(), sim->presetManager(), sim->primaryCamera());

    EXPECT_EQ(controller.washerState(), WasherState::Idle);
    EXPECT_NEAR(controller.washerFluidLevel(), 1.0, 1e-4);

    // Start routine
    EXPECT_TRUE(controller.startWasherRoutine());
    EXPECT_EQ(controller.washerState(), WasherState::Spraying);
    EXPECT_NEAR(controller.washerFluidLevel(), 0.98, 1e-4);

    // Advance past spray (1.5s)
    controller.update(1.6);
    EXPECT_EQ(controller.washerState(), WasherState::Soaking);

    // Advance past soak dwell (0.5s)
    controller.update(0.6);
    EXPECT_EQ(controller.washerState(), WasherState::ClearingWipes);
    EXPECT_EQ(controller.wiperState(), WiperState::Wiping);

    // Advance through 4 wipe strokes (4 * 1.2s = 4.8s)
    for (int i = 0; i < 4; ++i) {
        controller.update(1.3);
    }

    EXPECT_EQ(controller.washerState(), WasherState::Complete);
    EXPECT_EQ(controller.wiperState(), WiperState::Parked);

    // One more tick resets Complete to Idle
    controller.update(0.1);
    EXPECT_EQ(controller.washerState(), WasherState::Idle);

    // Test fluid low warning
    controller.refillWasherFluid(0.10);
    auto status = controller.status();
    EXPECT_TRUE(status.washerFluidLow);

    // Test empty fluid tank
    controller.refillWasherFluid(0.0);
    EXPECT_FALSE(controller.startWasherRoutine());
}

TEST(TestPayloadStowController, EmergencyPark)
{
    auto sim = std::make_shared<SimulatedPayload>();
    PayloadStowController controller(sim->panTilt(), sim->presetManager(), sim->primaryCamera());

    // Start washer and wiper
    controller.startWasherRoutine();
    controller.setWiperMode(WiperMode::Continuous);
    controller.setHeaterMode(HeaterMode::ManualOn);

    // Trigger emergency park
    EXPECT_TRUE(controller.emergencyPark());
    EXPECT_EQ(controller.state(), StowState::EmergencyParking);
    EXPECT_EQ(controller.washerState(), WasherState::Idle);
    EXPECT_EQ(controller.wiperState(), WiperState::Parked);
    EXPECT_FALSE(controller.isHeaterActive());

    // Advance arrival
    controller.update(0.1);
    EXPECT_EQ(controller.state(), StowState::EmergencyParked);
    EXPECT_TRUE(controller.isSafeForVehicleMotion());
}

TEST(TestPayloadStowController, AntiTamperZeroization)
{
    auto sim = std::make_shared<SimulatedPayload>();
    PayloadStowController controller(sim->panTilt(), sim->presetManager(), sim->primaryCamera());

    // Add presets to preset manager
    PtzPreset p1 {};
    p1.id = 1;
    p1.name = "Forward Lookout";
    p1.panAngleDeg = 10.0;
    p1.tiltAngleDeg = -5.0;
    sim->presetManager()->savePreset(p1);

    PtzPreset p2 {};
    p2.id = 2;
    p2.name = "Aft Guard";
    p2.panAngleDeg = 180.0;
    p2.tiltAngleDeg = 0.0;
    sim->presetManager()->savePreset(p2);

    EXPECT_EQ(sim->presetManager()->listPresets().size(), 2U);

    bool zeroizeCbFired = false;
    ZeroizeReason recordedReason = ZeroizeReason::OperatorCommand;
    controller.setZeroizeCallback([&](ZeroizeReason reason, const std::string&) {
        zeroizeCbFired = true;
        recordedReason = reason;
    });

    // Execute zeroization
    EXPECT_TRUE(controller.zeroize(ZeroizeReason::TamperDetected));
    EXPECT_TRUE(controller.isZeroized());
    EXPECT_EQ(controller.state(), StowState::Zeroizing);
    EXPECT_TRUE(zeroizeCbFired);
    EXPECT_EQ(recordedReason, ZeroizeReason::TamperDetected);

    // Presets must be completely purged
    EXPECT_EQ(sim->presetManager()->listPresets().size(), 0U);

    // Slew arrival completes zeroization
    controller.update(0.1);
    EXPECT_EQ(controller.state(), StowState::Zeroized);

    // Subsequent commands must be rejected while zeroized
    EXPECT_FALSE(controller.deploy());
    EXPECT_FALSE(controller.stow());
    EXPECT_FALSE(controller.moveToMaintenance());

    // Administrative unlock
    EXPECT_TRUE(controller.resetZeroizeLock());
    EXPECT_FALSE(controller.isZeroized());
    EXPECT_EQ(controller.state(), StowState::Stowed);

    // Now deploy is accepted
    controller.setVehicleMotionActive(false);
    EXPECT_TRUE(controller.deploy());
    controller.update(0.1);
    EXPECT_TRUE(controller.isDeployed());
}

TEST(TestPayloadStowController, SimulatedPayloadIntegration)
{
    auto sim = std::make_shared<SimulatedPayload>();
    ASSERT_NE(sim->stowController(), nullptr);

    EXPECT_EQ(sim->stowController()->state(), StowState::Deployed);
    EXPECT_TRUE(sim->stowController()->stow());
    sim->stowController()->update(0.1);
    EXPECT_TRUE(sim->stowController()->isStowed());

    auto status = sim->stowController()->status();
    EXPECT_EQ(status.stowState, StowState::Stowed);
    EXPECT_TRUE(status.vehicleMotionSafe);
}
