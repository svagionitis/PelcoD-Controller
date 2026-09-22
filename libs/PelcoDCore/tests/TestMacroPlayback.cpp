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
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

/// @brief Verify plaintext script parsing, step metadata extraction, and JSON round-trip serialization.
/// @details Checks parsing headers (# Name, # Description, # Repeat), step commands, delay timings,
///          comments, script export/reimport consistency, and JSON serialization equivalence.
TEST(MacroPlaybackTest, ScriptParsingAndSerialization)
{
    const std::string script = "# Name: Test Pan Sweep\n"
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

/// @brief Verify validation checks on empty sequences and corrupted frame checksums.
/// @details Ensures validate returns false and populates an error string when a sequence is empty
///          or contains packets with corrupted frame checksums.
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

/// @brief Verify asynchronous macro playback execution and frame dispatch callback sequencing.
/// @details Executes a multi-step macro using MacroPlayer with 2x speed multiplier, verifying
///          completion state transitions and single stepping mode.
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
        std::scoped_lock lock(mtx);
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
        std::scoped_lock lock(mtx);
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

/// @brief Verify player pause, resume, and stop state transitions.
/// @details Verifies that invoking pause() transitions player to Paused state, resume() continues
///          playback, and stop() transitions state to Stopped and resets currentStep to 0.
TEST(MacroPlaybackTest, MacroPlayerPauseResumeStop)
{
    PelcoD::MacroSequence seq;
    seq.name = "PauseResumeTest";
    seq.repeatCount = 1U;

    PelcoD::MacroStep step;
    step.label = "Step A";
    step.frame = PelcoD::PelcoDFrame::createFrame(1, 0, 0x04, 0x20, 0x00);
    step.delayMs = 200U;
    seq.steps.push_back(step);
    seq.steps.push_back(step);

    PelcoD::MacroPlayer player([](const std::vector<std::uint8_t>&) {});
    player.loadSequence(seq);

    EXPECT_TRUE(player.start());
    EXPECT_EQ(player.state(), PelcoD::MacroPlayerState::Playing);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    player.pause();
    EXPECT_EQ(player.state(), PelcoD::MacroPlayerState::Paused);

    player.resume();
    EXPECT_EQ(player.state(), PelcoD::MacroPlayerState::Playing);

    player.stop();
    EXPECT_EQ(player.state(), PelcoD::MacroPlayerState::Stopped);
    EXPECT_EQ(player.currentStep(), 0U);
}

/// @brief Verify speed multiplier scaling applied to step delays.
/// @details Tests setting speed multiplier to 0.5x, 1.0x, and 4.0x, ensuring speedMultiplier() getter
///          reflects the configuration accurately.
TEST(MacroPlaybackTest, MacroPlayerSpeedMultiplier)
{
    PelcoD::MacroPlayer player;
    EXPECT_DOUBLE_EQ(player.speedMultiplier(), 1.0);

    player.setSpeedMultiplier(0.5);
    EXPECT_DOUBLE_EQ(player.speedMultiplier(), 0.5);

    player.setSpeedMultiplier(4.0);
    EXPECT_DOUBLE_EQ(player.speedMultiplier(), 4.0);

    // Negative or zero multipliers are clamped or safely handled
    player.setSpeedMultiplier(0.0);
    EXPECT_GT(player.speedMultiplier(), 0.0);
}

/// @brief Verify deserialization resilience on malformed scripts and non-JSON input.
/// @details Ensures fromScript and fromJson return empty/default sequences and validate() fails
///          when presented with empty or malformed input text.
TEST(MacroPlaybackTest, MacroSerializerMalformedInputs)
{
    // Empty JSON produces empty sequence
    const auto emptyJsonSeq = PelcoD::MacroSerializer::fromJson("");
    EXPECT_TRUE(emptyJsonSeq.steps.empty());
    std::string err;
    EXPECT_FALSE(PelcoD::MacroSerializer::validate(emptyJsonSeq, &err));

    // Invalid JSON syntax produces empty sequence
    const auto badJsonSeq = PelcoD::MacroSerializer::fromJson("{ not valid json }");
    EXPECT_TRUE(badJsonSeq.steps.empty());

    // JSON missing steps array
    const auto noStepsSeq = PelcoD::MacroSerializer::fromJson("{\"name\":\"test\"}");
    EXPECT_TRUE(noStepsSeq.steps.empty());
    EXPECT_FALSE(PelcoD::MacroSerializer::validate(noStepsSeq, &err));

    // Malformed hex in plaintext script produces empty steps
    const std::string badHexScript = "ZZ ZZ NOT HEX 100 # Comment\n";
    const auto badHexSeq = PelcoD::MacroSerializer::fromScript(badHexScript);
    EXPECT_TRUE(badHexSeq.steps.empty());
    EXPECT_FALSE(PelcoD::MacroSerializer::validate(badHexSeq, &err));
}

/// @brief Verify starting an empty sequence is safely rejected.
/// @details Checks that start() returns false when no sequence or an empty sequence is loaded.
TEST(MacroPlaybackTest, MacroPlayerEmptySequence)
{
    PelcoD::MacroPlayer player;
    EXPECT_FALSE(player.start());
    EXPECT_EQ(player.state(), PelcoD::MacroPlayerState::Idle);

    PelcoD::MacroSequence emptySeq;
    player.loadSequence(emptySeq);
    EXPECT_FALSE(player.start());
    EXPECT_EQ(player.state(), PelcoD::MacroPlayerState::Idle);
}

} // namespace
