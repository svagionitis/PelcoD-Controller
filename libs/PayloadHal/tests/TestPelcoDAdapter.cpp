#include "adapters/PelcoDPtzAdapter.h"
#include "MockPelcoDDevice.h"
#include "PelcoDCore/PelcoDDevice.h"
#include <gtest/gtest.h>

namespace PayloadHal {
namespace {

TEST(TestPelcoDAdapter, LifecycleAndStabilizationGating) {
    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto pelcoDevice = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    PelcoDPtzAdapter adapter(pelcoDevice);

    EXPECT_FALSE(adapter.supportsStabilization());
    EXPECT_TRUE(adapter.setStabilizationMode(StabilizationMode::Disabled));
    EXPECT_FALSE(adapter.setStabilizationMode(StabilizationMode::RateStabilized));
    EXPECT_FALSE(adapter.setStabilizationMode(StabilizationMode::GeoHold));
    EXPECT_EQ(adapter.stabilizationMode(), StabilizationMode::Disabled);
    EXPECT_FALSE(adapter.zeroGyroDrift());

    double minPan { 0.0 };
    double maxPan { 0.0 };
    double minTilt { 0.0 };
    double maxTilt { 0.0 };
    EXPECT_TRUE(adapter.getLimits(minPan, maxPan, minTilt, maxTilt));
    EXPECT_DOUBLE_EQ(minPan, 0.0);
    EXPECT_DOUBLE_EQ(maxPan, 360.0);
    EXPECT_DOUBLE_EQ(minTilt, -90.0);
    EXPECT_DOUBLE_EQ(maxTilt, 90.0);
}

TEST(TestPelcoDAdapter, MotionCommands) {
    auto mockTransport = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    auto pelcoDevice = std::make_shared<PelcoD::PelcoDDevice>(mockTransport, 1U);
    PelcoDPtzAdapter adapter(pelcoDevice);

    ASSERT_TRUE(adapter.connect());
    EXPECT_TRUE(adapter.isConnected());

    // Normalized velocity commands
    EXPECT_TRUE(adapter.setNormalizedVelocity(1.0f, -0.5f));
    EXPECT_TRUE(adapter.stopMotion());

    // Absolute angles
    EXPECT_TRUE(adapter.setAbsoluteAngles(180.0, -25.0));

    // Relative nudge
    EXPECT_TRUE(adapter.setRelativeNudge(5.0, 2.0));

    // Presets
    EXPECT_TRUE(adapter.savePreset(1));
    EXPECT_TRUE(adapter.recallPreset(1));

    adapter.disconnect();
    EXPECT_FALSE(adapter.isConnected());
}

} // namespace
} // namespace PayloadHal
