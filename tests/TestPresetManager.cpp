/// @file TestPresetManager.cpp
/// @brief Unit tests for PresetManager preset retrieval and JSON serialization.

#include <cstdint>
#include <memory>

#include <QCoreApplication>
#include <QTemporaryFile>
#include <gtest/gtest.h>

#include "PresetManager.h"
#include "StabilizerTypes.h"
#include "VideoStabilizerEngine.h"

namespace StabilizerApp {

class PresetManagerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (qApp == nullptr) {
            static int argc = 1;
            static char appName[] = "TestPresetManager";
            static char* argv[] = { appName, nullptr };
            static auto app = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
};

TEST_F(PresetManagerTest, FactoryPresetsValid)
{
    PresetManager manager;
    const auto names = manager.presetNames();
    EXPECT_GE(names.size(), 5);

    for (const auto& name : names) {
        Stabilizer::VideoStabilizerConfig cfg { };
        EXPECT_TRUE(manager.getPreset(name, cfg)) << "Failed to get preset: " << name.toStdString();
        // Mode disabled doesn't require all invariants (it's bypass)
        if (cfg.mode != Stabilizer::StabilizationMode::Disabled) {
            EXPECT_TRUE(cfg.isValid()) << "Preset " << name.toStdString() << " is invalid!";
        }
    }
}

TEST_F(PresetManagerTest, UnknownPresetFails)
{
    PresetManager manager;
    Stabilizer::VideoStabilizerConfig cfg { };
    EXPECT_FALSE(manager.getPreset(QStringLiteral("NonExistentPreset"), cfg));
}

TEST_F(PresetManagerTest, JsonExportAndImportRoundTrip)
{
    QTemporaryFile tempFile;
    ASSERT_TRUE(tempFile.open());
    const QString filePath = tempFile.fileName();
    tempFile.close();

    Stabilizer::VideoStabilizerConfig original { };
    original.mode = Stabilizer::StabilizationMode::Similarity;
    original.minTrackedFeatures = 45U;
    original.replenishIntervalFrames = 20U;
    original.filter.windowSize = 21U;

    EXPECT_TRUE(PresetManager::exportJson(filePath, original));

    Stabilizer::VideoStabilizerConfig imported { };
    EXPECT_TRUE(PresetManager::importJson(filePath, imported));

    EXPECT_EQ(imported.mode, original.mode);
    EXPECT_EQ(imported.minTrackedFeatures, original.minTrackedFeatures);
    EXPECT_EQ(imported.replenishIntervalFrames, original.replenishIntervalFrames);
    EXPECT_EQ(imported.filter.windowSize, original.filter.windowSize);
}

} // namespace StabilizerApp
