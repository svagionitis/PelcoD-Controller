/// @file TestMacroPlayback.cpp
/// @brief Comprehensive unit tests for MacroScript serialization and MacroPlayer async engine.

#include "MacroPlayer.h"
#include "MacroScript.h"
#include "PelcoDFrame.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <future>
#include <mutex>
#include <string>
#include <vector>

namespace {

TEST(MacroPlaybackTest, ScriptParsingAndSerialization)
{
    const std::string script =
        "# Name: Test Pan Sweep\n"
        "# Description: Verifies camera pan sweep\n"
        "# Repeat: 3\n"
        "# Syntax: HEX DELAY # COMMENT\n\n"
        "FF 01 00 04 20 00 25  150  # Pan Left Speed 32\n"
        "FF 01 00 02 20 00 23  200  # Pan Right Speed 32\n"
        "FF 01 00 00 00 00 01  50   # Stop Motion\n";

    PelcoD::MacroSequence seq = PelcoD::MacroSerializer::fromScript(script);
    EXPECT_EQ(seq.name, "Test Pan Sweep");
    EXPECT_EQ(seq.description, "Verifies camera pan sweep");
    EXPECT_EQ(seq.repeatCount, 3U);
    ASSERT_EQ(seq.steps.size(), 3U);

    EXPECT_EQ(seq.steps[0].label, "Pan Left Speed 32");
    EXPECT_EQ(seq.steps[0].delayMs, 150U);
    ASSERT_EQ(seq.steps[0].frame.size(), 7U);
    EXPECT_EQ(seq.steps[0].frame[0], 0xFFU);
    EXPECT_EQ(seq.steps[0].frame[1], 0x01U);
    EXPECT_EQ(seq.steps[0].frame[2], 0x00U);
    EXPECT_EQ(seq.steps[0].frame[3], 0x04U);
    EXPECT_EQ(seq.steps[0].frame[4], 0x20U);
    EXPECT_EQ(seq.steps[0].frame[5], 0x00U);
    EXPECT_EQ(seq.steps[0].frame[6], 0x25U);

    EXPECT_EQ(seq.steps[1].label, "Pan Right Speed 32");
    EXPECT_EQ(seq.steps[1].delayMs, 200U);

    EXPECT_EQ(seq.steps[2].label, "Stop Motion");
    EXPECT_EQ(seq.steps[2].delayMs, 50U);

    // Validation
    std::string err;
    EXPECT_TRUE(PelcoD::MacroSerializer::validate(seq, &err));

    // Export back to script and re-parse
    std::string exportedScript = PelcoD::MacroSerializer::toScript(seq);
    PelcoD::MacroSequence reloaded = PelcoD::MacroSerializer::fromScript(exportedScript);
    ASSERT_EQ(reloaded.steps.size(), 3U);
    EXPECT_EQ(reloaded.repeatCount, 3U);
    EXPECT_EQ(reloaded.steps[0].delayMs, 150U);
    EXPECT_EQ(reloaded.steps[0].frame, seq.steps[0].frame);

    // JSON round-trip
    std::string json = PelcoD::MacroSerializer::toJson(seq);
    PelcoD::MacroSequence jsonSeq = PelcoD::MacroSerializer::fromJson(json);
    EXPECT_EQ(jsonSeq.name, "Test Pan Sweep");
    EXPECT_EQ(jsonSeq.description, "Verifies camera pan sweep");
    EXPECT_EQ(jsonSeq.repeatCount, 3U);
    ASSERT_EQ(jsonSeq.steps.size(), 3U);
    EXPECT_EQ(jsonSeq.steps[0].frame, seq.steps[0].frame);
    EXPECT_EQ(jsonSeq.steps[1].frame, seq.steps[1].frame);
    EXPECT_EQ(jsonSeq.steps[2].frame, seq.steps[2].frame);
    EXPECT_EQ(jsonSeq.steps[1].delayMs, 200U);
}

TEST(MacroPlaybackTest, MacroValidation)
{
    PelcoD::MacroSequence emptySeq;
    std::string err;
    EXPECT_FALSE(PelcoD::MacroSerializer::validate(emptySeq, &err));
    EXPECT_FALSE(err.empty());

    PelcoD::MacroSequence invalidChecksumSeq;
    PelcoD::MacroStep badStep;
    badStep.frame = { 0xFF, 0x01, 0x00, 0x04, 0x20, 0x00, 0x99 }; // Corrupted checksum
    invalidChecksumSeq.steps.push_back(badStep);
    EXPECT_FALSE(PelcoD::MacroSerializer::validate(invalidChecksumSeq, &err));
}

TEST(MacroPlaybackTest, MacroPlayerExecution)
{
    PelcoD::MacroSequence seq;
    seq.name = "Quick Execution";
    seq.repeatCount = 2U;

    PelcoD::MacroStep step1;
    step1.label = "Step 1";
    step1.frame = PelcoD::PelcoDFrame::createFrame(1, 0, 0x04, 0x10, 0x00);
    step1.delayMs = 20U;
    seq.steps.push_back(step1);

    PelcoD::MacroStep step2;
    step2.label = "Step 2";
    step2.frame = PelcoD::PelcoDFrame::createFrame(1, 0, 0x00, 0x00, 0x00);
    step2.delayMs = 20U;
    seq.steps.push_back(step2);

    std::vector<std::vector<std::uint8_t>> dispatchedFrames;
    std::mutex mtx;
    std::promise<void> completedPromise;
    auto completedFuture = completedPromise.get_future();

    PelcoD::MacroPlayer player([&](const std::vector<std::uint8_t>& frame) {
        std::lock_guard<std::mutex> lock(mtx);
        dispatchedFrames.push_back(frame);
    });

    player.setStateCallback([&](PelcoD::MacroPlayerState state, const std::string&) {
        if (state == PelcoD::MacroPlayerState::Completed) {
            completedPromise.set_value();
        }
    });

    player.loadSequence(seq);
    player.setSpeedMultiplier(2.0); // 2x speed for fast test
    EXPECT_TRUE(player.start());
    EXPECT_EQ(player.state(), PelcoD::MacroPlayerState::Playing);

    // Wait for completion (2 loops * 2 steps = 4 dispatches)
    auto status = completedFuture.wait_for(std::chrono::seconds(2));
    ASSERT_TRUE(status == std::future_status::ready);

    {
        std::lock_guard<std::mutex> lock(mtx);
        ASSERT_EQ(dispatchedFrames.size(), 4U);
        EXPECT_EQ(dispatchedFrames[0], step1.frame);
        EXPECT_EQ(dispatchedFrames[1], step2.frame);
        EXPECT_EQ(dispatchedFrames[2], step1.frame);
        EXPECT_EQ(dispatchedFrames[3], step2.frame);
    }

    EXPECT_EQ(player.state(), PelcoD::MacroPlayerState::Completed);

    // Single stepping test
    player.loadSequence(seq);
    EXPECT_EQ(player.state(), PelcoD::MacroPlayerState::Idle);

    dispatchedFrames.clear();
    EXPECT_TRUE(player.stepNext());
    EXPECT_EQ(player.state(), PelcoD::MacroPlayerState::Paused);
    ASSERT_EQ(dispatchedFrames.size(), 1U);
    EXPECT_EQ(dispatchedFrames[0], step1.frame);

    EXPECT_TRUE(player.stepNext());
    ASSERT_EQ(dispatchedFrames.size(), 2U);
    EXPECT_EQ(dispatchedFrames[1], step2.frame);
}

} // namespace
