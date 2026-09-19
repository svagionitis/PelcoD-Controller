#pragma once

/// @file MacroPlayer.h
/// @brief Asynchronous execution engine for Pelco-D macro sequences with millisecond scheduling.

#include "MacroScript.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace PelcoD {

/// @enum MacroPlayerState
/// @brief Operational state of the MacroPlayer engine.
enum class MacroPlayerState {
    Idle,       ///< Player is idle, no macro loaded or executing.
    Playing,    ///< Macro is actively executing steps.
    Paused,     ///< Playback is paused at current step.
    Completed,  ///< Macro finished all steps and repeats successfully.
    Stopped,    ///< Playback was cancelled or stopped by user.
    Error       ///< Playback aborted due to an error.
};

/// @class MacroPlayer
/// @brief Thread-safe execution engine for timed command macros.
/// @details Dispatches raw frames to a user-provided callback or transport with configurable speed scaling.
class MacroPlayer {
public:
    /// @brief Frame dispatch callback signature.
    using FrameDispatchCallback = std::function<void(const std::vector<std::uint8_t>& frame)>;

    /// @brief Progress notification callback signature.
    /// @param currentStep 0-indexed step currently being executed.
    /// @param totalSteps Total number of steps in sequence.
    /// @param step Reference to the current MacroStep.
    /// @param currentLoop Current repeat loop count (1-indexed).
    /// @param totalLoops Total repeat loops configured (0 = infinite).
    using StepCallback = std::function<void(
        std::size_t currentStep,
        std::size_t totalSteps,
        const MacroStep& step,
        std::size_t currentLoop,
        std::size_t totalLoops)>;

    /// @brief State transition callback signature.
    using StateCallback = std::function<void(MacroPlayerState newState, const std::string& message)>;

    /// @brief Constructs a MacroPlayer instance.
    /// @param[in] dispatchCb Callback invoked to send raw frame bytes.
    explicit MacroPlayer(FrameDispatchCallback dispatchCb = nullptr);

    /// @brief Destructor. Automatically stops playback thread.
    ~MacroPlayer();

    // Prevent copying, allow moving
    MacroPlayer(const MacroPlayer&) = delete;
    MacroPlayer& operator=(const MacroPlayer&) = delete;
    MacroPlayer(MacroPlayer&&) noexcept = default;
    MacroPlayer& operator=(MacroPlayer&&) noexcept = default;

    /// @brief Sets the frame dispatch callback.
    /// @param[in] cb Callback invoked to send frame bytes.
    void setDispatchCallback(FrameDispatchCallback cb);

    /// @brief Sets the step execution notification callback.
    /// @param[in] cb Progress callback.
    void setStepCallback(StepCallback cb);

    /// @brief Sets the player state transition callback.
    /// @param[in] cb State callback.
    void setStateCallback(StateCallback cb);

    /// @brief Loads a macro sequence into the player.
    /// @param[in] sequence Macro sequence to load.
    void loadSequence(MacroSequence sequence);

    /// @brief Retrieves the currently loaded macro sequence.
    /// @return Const reference to MacroSequence.
    [[nodiscard]] const MacroSequence& sequence() const noexcept;

    /// @brief Starts or resumes playback of loaded sequence.
    /// @return True if playback started/resumed, false if no steps or already playing.
    bool start();

    /// @brief Pauses active playback at current step.
    void pause();

    /// @brief Resumes paused playback.
    void resume();

    /// @brief Stops playback and resets execution position to the beginning.
    void stop();

    /// @brief Executes only the single next step manually, then pauses.
    /// @return True if a step was executed, false if at end or not paused/idle.
    bool stepNext();

    /// @brief Sets the playback speed multiplier.
    /// @param[in] multiplier Scaling factor applied to delays (e.g. 1.0 = normal, 2.0 = 2x faster, 0.5 = 2x slower).
    void setSpeedMultiplier(double multiplier) noexcept;

    /// @brief Gets current speed multiplier.
    /// @return Speed scaling factor.
    [[nodiscard]] double speedMultiplier() const noexcept;

    /// @brief Queries current operational state.
    /// @return MacroPlayerState enum value.
    [[nodiscard]] MacroPlayerState state() const noexcept;

    /// @brief Gets current step index being executed or paused at.
    /// @return 0-indexed step counter.
    [[nodiscard]] std::size_t currentStep() const noexcept;

    /// @brief Gets current loop execution count.
    /// @return 1-indexed loop iteration.
    [[nodiscard]] std::size_t currentLoop() const noexcept;

private:
    void workerThreadFunc();
    void setState(MacroPlayerState newState, const std::string& msg = "");

    MacroSequence m_sequence {};
    FrameDispatchCallback m_dispatchCb { nullptr };
    StepCallback m_stepCb { nullptr };
    StateCallback m_stateCb { nullptr };

    mutable std::mutex m_mutex {};
    std::condition_variable m_cv {};
    std::thread m_worker {};

    std::atomic<MacroPlayerState> m_state { MacroPlayerState::Idle };
    std::atomic<std::size_t> m_currentStep { 0U };
    std::atomic<std::size_t> m_currentLoop { 1U };
    std::atomic<double> m_speedMultiplier { 1.0 };
    std::atomic<bool> m_stopRequested { false };
    std::atomic<bool> m_stepOnceRequested { false };
};

} // namespace PelcoD
