/// @file TestMacroPlayback.cpp
/// @brief Comprehensive unit tests for MacroScript serialization and MacroPlayer async engine.

#include "MacroPlayer.h"
#include "MacroScript.h"
#include "PelcoDFrame.h"

#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define ASSERT_TRUE(cond)                                                                      \
    do {                                                                                        \
        if (!(cond)) {                                                                         \
            std::cerr << "Assertion failed: " #cond " at line " << __LINE__ << std::endl;      \
            return 1;                                                                          \
        }                                                                                       \
    } while (0)

#define ASSERT_FALSE(cond)                                                                     \
    do {                                                                                        \
        if ((cond)) {                                                                          \
            std::cerr << "Assertion failed: NOT(" #cond ") at line " << __LINE__ << std::endl; \
            return 1;                                                                          \
        }                                                                                       \
    } while (0)

#define ASSERT_EQ(a, b)                                                                        \
    do {                                                                                        \
        if ((a) != (b)) {                                                                      \
            std::cerr << "Assertion failed: " #a " == " #b " at line " << __LINE__ << std::endl; \
            return 1;                                                                          \
        }                                                                                       \
    } while (0)

int testScriptParsingAndSerialization()
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
    ASSERT_EQ(seq.name, std::string("Test Pan Sweep"));
    ASSERT_EQ(seq.description, std::string("Verifies camera pan sweep"));
    ASSERT_EQ(seq.repeatCount, 3U);
    ASSERT_EQ(seq.steps.size(), 3U);

    ASSERT_EQ(seq.steps[0].label, std::string("Pan Left Speed 32"));
    ASSERT_EQ(seq.steps[0].delayMs, 150U);
    ASSERT_EQ(seq.steps[0].frame.size(), 7U);
    ASSERT_EQ(seq.steps[0].frame[0], 0xFFU);
    ASSERT_EQ(seq.steps[0].frame[1], 0x01U);
    ASSERT_EQ(seq.steps[0].frame[2], 0x00U);
    ASSERT_EQ(seq.steps[0].frame[3], 0x04U);
    ASSERT_EQ(seq.steps[0].frame[4], 0x20U);
    ASSERT_EQ(seq.steps[0].frame[5], 0x00U);
    ASSERT_EQ(seq.steps[0].frame[6], 0x25U);

    ASSERT_EQ(seq.steps[1].label, std::string("Pan Right Speed 32"));
    ASSERT_EQ(seq.steps[1].delayMs, 200U);

    ASSERT_EQ(seq.steps[2].label, std::string("Stop Motion"));
    ASSERT_EQ(seq.steps[2].delayMs, 50U);

    // Validation
    std::string err;
    ASSERT_TRUE(PelcoD::MacroSerializer::validate(seq, &err));

    // Export back to script and re-parse
    std::string exportedScript = PelcoD::MacroSerializer::toScript(seq);
    PelcoD::MacroSequence reloaded = PelcoD::MacroSerializer::fromScript(exportedScript);
    ASSERT_EQ(reloaded.steps.size(), 3U);
    ASSERT_EQ(reloaded.repeatCount, 3U);
    ASSERT_EQ(reloaded.steps[0].delayMs, 150U);
    ASSERT_EQ(reloaded.steps[0].frame, seq.steps[0].frame);

    // JSON round-trip
    std::string json = PelcoD::MacroSerializer::toJson(seq);
    PelcoD::MacroSequence jsonSeq = PelcoD::MacroSerializer::fromJson(json);
    ASSERT_EQ(jsonSeq.name, std::string("Test Pan Sweep"));
    ASSERT_EQ(jsonSeq.description, std::string("Verifies camera pan sweep"));
    ASSERT_EQ(jsonSeq.repeatCount, 3U);
    ASSERT_EQ(jsonSeq.steps.size(), 3U);
    ASSERT_EQ(jsonSeq.steps[0].frame, seq.steps[0].frame);
    ASSERT_EQ(jsonSeq.steps[1].frame, seq.steps[1].frame);
    ASSERT_EQ(jsonSeq.steps[2].frame, seq.steps[2].frame);
    ASSERT_EQ(jsonSeq.steps[1].delayMs, 200U);

    return 0;
}

int testMacroValidation()
{
    PelcoD::MacroSequence emptySeq;
    std::string err;
    ASSERT_FALSE(PelcoD::MacroSerializer::validate(emptySeq, &err));
    ASSERT_FALSE(err.empty());

    PelcoD::MacroSequence invalidChecksumSeq;
    PelcoD::MacroStep badStep;
    badStep.frame = { 0xFF, 0x01, 0x00, 0x04, 0x20, 0x00, 0x99 }; // Corrupted checksum
    invalidChecksumSeq.steps.push_back(badStep);
    ASSERT_FALSE(PelcoD::MacroSerializer::validate(invalidChecksumSeq, &err));

    return 0;
}

int testMacroPlayerExecution()
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
    ASSERT_TRUE(player.start());
    ASSERT_EQ(player.state(), PelcoD::MacroPlayerState::Playing);

    // Wait for completion (2 loops * 2 steps = 4 dispatches)
    auto status = completedFuture.wait_for(std::chrono::seconds(2));
    ASSERT_TRUE(status == std::future_status::ready);

    {
        std::lock_guard<std::mutex> lock(mtx);
        ASSERT_EQ(dispatchedFrames.size(), 4U);
        ASSERT_EQ(dispatchedFrames[0], step1.frame);
        ASSERT_EQ(dispatchedFrames[1], step2.frame);
        ASSERT_EQ(dispatchedFrames[2], step1.frame);
        ASSERT_EQ(dispatchedFrames[3], step2.frame);
    }

    ASSERT_EQ(player.state(), PelcoD::MacroPlayerState::Completed);

    // Single stepping test
    player.loadSequence(seq);
    ASSERT_EQ(player.state(), PelcoD::MacroPlayerState::Idle);

    dispatchedFrames.clear();
    ASSERT_TRUE(player.stepNext());
    ASSERT_EQ(player.state(), PelcoD::MacroPlayerState::Paused);
    ASSERT_EQ(dispatchedFrames.size(), 1U);
    ASSERT_EQ(dispatchedFrames[0], step1.frame);

    ASSERT_TRUE(player.stepNext());
    ASSERT_EQ(dispatchedFrames.size(), 2U);
    ASSERT_EQ(dispatchedFrames[1], step2.frame);

    return 0;
}

int main()
{
    std::cout << "[TestMacroPlayback] Running MacroScript parsing and serialization tests..." << std::endl;
    if (testScriptParsingAndSerialization() != 0) {
        return 1;
    }

    std::cout << "[TestMacroPlayback] Running MacroScript validation tests..." << std::endl;
    if (testMacroValidation() != 0) {
        return 1;
    }

    std::cout << "[TestMacroPlayback] Running MacroPlayer execution tests..." << std::endl;
    if (testMacroPlayerExecution() != 0) {
        return 1;
    }

    std::cout << "[TestMacroPlayback] All macro playback and hex scripting tests passed successfully!" << std::endl;
    return 0;
}
