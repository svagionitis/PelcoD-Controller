/// @file TestMotionGatedAf.cpp
#include "MotionGatedAfManager.h"

#include <gtest/gtest.h>

namespace LensControl {
namespace {

    TEST(TestMotionGatedAf, DefaultStateIsActive)
    {
        const MotionGatedAfManager manager { };
        EXPECT_EQ(manager.gatingState(), AfGatingState::Active);
        EXPECT_FALSE(manager.isSuppressed());
        EXPECT_FLOAT_EQ(manager.compositeRate().value(), 0.0F);
    }

    TEST(TestMotionGatedAf, DeadbandFiltersLowLevelNoise)
    {
        MotionGatedAfManager manager { };
        // Noise well within deadband (0.2 deg/s)
        manager.updateRates(AngularVelocityDegPerSec { 0.1F }, AngularVelocityDegPerSec { 0.1F }, 0.02F);

        EXPECT_FLOAT_EQ(manager.compositeRate().value(), 0.0F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::Active);
        EXPECT_FALSE(manager.isSuppressed());
    }

    TEST(TestMotionGatedAf, CompositeRateCalculation)
    {
        MotionGatedAfManager manager { };
        // 3.0 pan, 4.0 tilt -> sqrt(9 + 16) = 5.0
        manager.updateRates(AngularVelocityDegPerSec { 3.0F }, AngularVelocityDegPerSec { 4.0F }, 0.02F);

        EXPECT_NEAR(manager.compositeRate().value(), 5.0F, 1e-4F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::Suppressed);
        EXPECT_TRUE(manager.isSuppressed());
    }

    TEST(TestMotionGatedAf, SuppressionThresholdTransition)
    {
        MotionGatedAfManager manager { };
        // Below threshold (1.5 deg/s)
        manager.updateRates(AngularVelocityDegPerSec { 1.0F }, AngularVelocityDegPerSec { 0.0F }, 0.02F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::Active);
        EXPECT_FALSE(manager.isSuppressed());

        // At or above threshold (1.5 deg/s)
        manager.updateRates(AngularVelocityDegPerSec { 1.5F }, AngularVelocityDegPerSec { 0.0F }, 0.02F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::Suppressed);
        EXPECT_TRUE(manager.isSuppressed());
    }

    TEST(TestMotionGatedAf, HysteresisPreventsPrematureRelease)
    {
        MotionGatedAfManager manager { };
        // Trip suppression at 2.0 deg/s
        manager.updateRates(AngularVelocityDegPerSec { 2.0F }, AngularVelocityDegPerSec { 0.0F }, 0.02F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::Suppressed);

        // Drop to 1.2 deg/s (between release 1.0 and suppress 1.5)
        manager.updateRates(AngularVelocityDegPerSec { 1.2F }, AngularVelocityDegPerSec { 0.0F }, 0.02F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::Suppressed);
        EXPECT_TRUE(manager.isSuppressed());
    }

    TEST(TestMotionGatedAf, SettlingHoldoffTimerCompletion)
    {
        MotionGatedAfManager::Config cfg { };
        cfg.holdoffMs = 150U;
        MotionGatedAfManager manager { cfg };

        // Slew
        manager.updateRates(AngularVelocityDegPerSec { 2.0F }, AngularVelocityDegPerSec { 0.0F }, 0.02F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::Suppressed);

        // Stop motion (0.0 deg/s) -> enters SettlingHoldoff
        manager.updateRates(AngularVelocityDegPerSec { 0.0F }, AngularVelocityDegPerSec { 0.0F }, 0.05F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::SettlingHoldoff);
        EXPECT_TRUE(manager.isSuppressed());

        // Advance 50 ms (total 100 ms) -> still holdoff
        manager.updateRates(AngularVelocityDegPerSec { 0.0F }, AngularVelocityDegPerSec { 0.0F }, 0.05F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::SettlingHoldoff);
        EXPECT_TRUE(manager.isSuppressed());

        // Advance another 60 ms (total 160 ms >= 150 ms) -> transitions to Active
        manager.updateRates(AngularVelocityDegPerSec { 0.0F }, AngularVelocityDegPerSec { 0.0F }, 0.06F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::Active);
        EXPECT_FALSE(manager.isSuppressed());
    }

    TEST(TestMotionGatedAf, SlewInterruptionDuringHoldoffResetsTimer)
    {
        MotionGatedAfManager::Config cfg { };
        cfg.holdoffMs = 150U;
        MotionGatedAfManager manager { cfg };

        // Slew
        manager.updateRates(AngularVelocityDegPerSec { 2.0F }, AngularVelocityDegPerSec { 0.0F }, 0.02F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::Suppressed);

        // Slew stops
        manager.updateRates(AngularVelocityDegPerSec { 0.0F }, AngularVelocityDegPerSec { 0.0F }, 0.10F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::SettlingHoldoff);

        // Platform moves again before holdoff finishes (1.2 deg/s >= release 1.0)
        manager.updateRates(AngularVelocityDegPerSec { 1.2F }, AngularVelocityDegPerSec { 0.0F }, 0.02F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::Suppressed);

        // Stop again -> timer restarts from 0
        manager.updateRates(AngularVelocityDegPerSec { 0.0F }, AngularVelocityDegPerSec { 0.0F }, 0.10F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::SettlingHoldoff);

        // 60 ms more -> total 160 ms from restart
        manager.updateRates(AngularVelocityDegPerSec { 0.0F }, AngularVelocityDegPerSec { 0.0F }, 0.06F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::Active);
        EXPECT_FALSE(manager.isSuppressed());
    }

    TEST(TestMotionGatedAf, ResetRestoresActiveState)
    {
        MotionGatedAfManager manager { };
        manager.updateRates(AngularVelocityDegPerSec { 5.0F }, AngularVelocityDegPerSec { 0.0F }, 0.02F);
        EXPECT_EQ(manager.gatingState(), AfGatingState::Suppressed);

        manager.reset();
        EXPECT_EQ(manager.gatingState(), AfGatingState::Active);
        EXPECT_FALSE(manager.isSuppressed());
        EXPECT_FLOAT_EQ(manager.compositeRate().value(), 0.0F);
    }

} // namespace
} // namespace LensControl
