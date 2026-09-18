#pragma once

/// @file PatrolController.h
/// @brief Automated Preset Tour / Patrol Sequence Controller for Pelco-D PTZ devices.

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace PelcoD {

class PelcoDDevice;

/// @enum PatrolState
/// @brief Operational state of the patrol tour sequence.
enum class PatrolState : std::uint8_t { Idle, Running, Paused };

/// @struct PatrolStep
/// @brief A single preset destination and dwell time in a patrol sequence.
struct PatrolStep {
    std::uint8_t presetId { 1U };
    std::uint32_t dwellTimeSeconds { 5U };
    std::string name {};
    std::uint8_t speed { 0U };
};

/// @class PatrolController
/// @brief Background sequence controller touring through PTZ presets.
/// @details Zero Qt dependency, pure C++17 thread-safe background execution.
class PatrolController {
public:
    using GoToPresetCallback = std::function<void(std::uint8_t presetId)>;
    using StepChangedCallback = std::function<void(std::size_t stepIndex, const PatrolStep& step)>;
    using StateChangedCallback = std::function<void(PatrolState state)>;
    using DwellTickCallback = std::function<void(std::size_t stepIndex, std::uint32_t remainingSeconds)>;
    using TourFinishedCallback = std::function<void()>;

    explicit PatrolController(PelcoDDevice* device = nullptr);
    explicit PatrolController(GoToPresetCallback dispatcher);
    ~PatrolController();

    // Non-copyable, non-movable
    PatrolController(const PatrolController&) = delete;
    PatrolController& operator=(const PatrolController&) = delete;
    PatrolController(PatrolController&&) = delete;
    PatrolController& operator=(PatrolController&&) = delete;

    // Sequence Execution
    bool start();
    void stop();
    void pause();
    void resume();
    void nextStep();
    void previousStep();

    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] bool isPaused() const noexcept;
    [[nodiscard]] PatrolState getState() const noexcept;
    [[nodiscard]] bool isWorkerActive() const noexcept;

    // Configuration & Step Management
    void addStep(const PatrolStep& step);
    void insertStep(std::size_t index, const PatrolStep& step);
    bool removeStep(std::size_t index);
    bool setStep(std::size_t index, const PatrolStep& step);
    void clearSteps();

    [[nodiscard]] std::vector<PatrolStep> getSteps() const;
    void setSteps(const std::vector<PatrolStep>& steps);

    [[nodiscard]] std::size_t stepCount() const;
    [[nodiscard]] std::size_t getCurrentStepIndex() const;
    [[nodiscard]] std::uint32_t getRemainingDwellSeconds() const;

    void setLoop(bool loop) noexcept;
    [[nodiscard]] bool isLooping() const noexcept;

    // Callbacks & Dispatcher
    void setDevice(PelcoDDevice* device);
    void setCommandDispatcher(GoToPresetCallback dispatcher);

    void setStepChangedCallback(StepChangedCallback cb);
    void setStateChangedCallback(StateChangedCallback cb);
    void setDwellTickCallback(DwellTickCallback cb);
    void setTourFinishedCallback(TourFinishedCallback cb);

private:
    void workerLoop();
    void dispatchCurrentStep(std::unique_lock<std::mutex>& lock);
    void notifyStateChange(std::unique_lock<std::mutex>& lock, PatrolState state, bool relock = true);

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_worker;

    std::atomic<bool> m_workerRunning { false };
    std::atomic<PatrolState> m_state { PatrolState::Idle };
    std::atomic<bool> m_loop { true };

    std::vector<PatrolStep> m_steps;
    std::size_t m_currentStepIndex { 0U };
    std::uint32_t m_remainingDwellSeconds { 0U };
    bool m_needsDispatch { false };
    bool m_stepAdvanceRequested { false };
    int m_stepAdvanceDelta { 0 };

    GoToPresetCallback m_dispatcher;
    StepChangedCallback m_stepChangedCb;
    StateChangedCallback m_stateChangedCb;
    DwellTickCallback m_dwellTickCb;
    TourFinishedCallback m_tourFinishedCb;
};

} // namespace PelcoD
