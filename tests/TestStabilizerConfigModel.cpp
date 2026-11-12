/// @file TestStabilizerConfigModel.cpp
/// @brief Unit tests for StabilizerConfigModel property reflection and invariant validation.

#include <cstdint>
#include <memory>

#include <QCoreApplication>
#include <QSignalSpy>
#include <gtest/gtest.h>

#include "StabilizerConfigModel.h"
#include "StabilizerTypes.h"
#include "VideoStabilizerEngine.h"

namespace StabilizerApp {

class StabilizerConfigModelTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qApp == nullptr) {
            static int argc = 1;
            static char appName[] = "TestStabilizerConfigModel";
            static char* argv[] = { appName, nullptr };
            static auto app = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
};

TEST_F(StabilizerConfigModelTest, DefaultConfigIsValid)
{
    StabilizerConfigModel model;
    const auto cfg = model.toConfig();
    EXPECT_TRUE(cfg.isValid());
    EXPECT_EQ(model.stabilizationMode(), static_cast<int>(Stabilizer::StabilizationMode::Affine));
    EXPECT_EQ(model.minTrackedFeatures(), 20);
    EXPECT_NEAR(model.minInlierRatio(), 0.25, 1e-4);
}

TEST_F(StabilizerConfigModelTest, ParameterClampingAndSignalEmission)
{
    StabilizerConfigModel model;
    QSignalSpy spy(&model, &StabilizerConfigModel::configChanged);

    // Min tracked features clamped [5 .. 500]
    model.setMinTrackedFeatures(1);
    EXPECT_EQ(model.minTrackedFeatures(), 5);
    EXPECT_EQ(spy.count(), 1);

    model.setMinTrackedFeatures(1000);
    EXPECT_EQ(model.minTrackedFeatures(), 500);
    EXPECT_EQ(spy.count(), 2);

    // Max crop margin clamped [0.0 .. 0.30]
    model.setMaxCropMargin(0.50);
    EXPECT_NEAR(model.maxCropMargin(), 0.30, 1e-4);
    EXPECT_EQ(spy.count(), 3);

    // Inlier threshold clamped [0.1 .. 20.0]
    model.setInlierThreshold(50.0);
    EXPECT_NEAR(model.inlierThreshold(), 20.0, 1e-4);
    EXPECT_EQ(spy.count(), 4);

    // Optical flow window size odd enforcement/half-width
    model.setFlowWindowHalfWidth(1);
    EXPECT_EQ(model.flowWindowHalfWidth(), 2); // Clamped min 2

    model.setFlowWindowHalfWidth(30);
    EXPECT_EQ(model.flowWindowHalfWidth(), 16); // Clamped max 16
}

TEST_F(StabilizerConfigModelTest, ResetSectionMaintainsOtherSections)
{
    StabilizerConfigModel model;

    // Modify Section 0 (Core) and Section 1 (Filter)
    model.setMinTrackedFeatures(100);
    model.setMaxCropMargin(0.20);

    EXPECT_EQ(model.minTrackedFeatures(), 100);
    EXPECT_NEAR(model.maxCropMargin(), 0.20, 1e-4);

    // Reset only Section 0
    model.resetSection(0);

    EXPECT_EQ(model.minTrackedFeatures(), 20); // Default restored
    EXPECT_NEAR(model.maxCropMargin(), 0.20, 1e-4); // Section 1 untouched

    // Reset All
    model.resetAll();
    EXPECT_NEAR(model.maxCropMargin(), 0.08, 1e-4); // Restored default
}

TEST_F(StabilizerConfigModelTest, LoadFromConfigRoundTrip)
{
    StabilizerConfigModel model;
    QSignalSpy spy(&model, &StabilizerConfigModel::configChanged);

    Stabilizer::VideoStabilizerConfig customCfg { };
    customCfg.mode = Stabilizer::StabilizationMode::TranslationRotation;
    customCfg.minTrackedFeatures = 42U;
    customCfg.filter.windowSize = 15U;

    model.loadFromConfig(customCfg);

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(model.stabilizationMode(), static_cast<int>(Stabilizer::StabilizationMode::TranslationRotation));
    EXPECT_EQ(model.minTrackedFeatures(), 42);
    EXPECT_EQ(model.windowSize(), 15);
}

} // namespace StabilizerApp
