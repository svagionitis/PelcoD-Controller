/// @file MacroPlayer.cpp
/// @brief Implementation of asynchronous MacroPlayer execution engine.

#include "MacroPlayer.h"

#include <chrono>

namespace PelcoD {

MacroPlayer::MacroPlayer(FrameDispatchCallback dispatchCb)
    : m_dispatchCb(std::move(dispatchCb))
{
}

MacroPlayer::~MacroPlayer()
{
    stop();
}

bool MacroPlayer::joinWorker()
{
    bool joined { true };
    if (m_worker.joinable()) {
        if (std::this_thread::get_id() == m_worker.get_id()) {
            // A thread cannot join itself (std::system_error / std::terminate).
            joined = false;
        } else {
            m_worker.join();
        }
    }
    return joined;
}

void MacroPlayer::setDispatchCallback(FrameDispatchCallback cb)
{
    std::scoped_lock lock(m_mutex);
    m_dispatchCb = std::move(cb);
}

void MacroPlayer::setStepCallback(StepCallback cb)
{
    std::scoped_lock lock(m_mutex);
    m_stepCb = std::move(cb);
}

void MacroPlayer::setStateCallback(StateCallback cb)
{
    std::scoped_lock lock(m_mutex);
    m_stateCb = std::move(cb);
}

void MacroPlayer::loadSequence(MacroSequence sequence)
{
    stop();
    StateCallback cb { nullptr };
    {
        std::scoped_lock lock(m_mutex);
        m_sequence = std::move(sequence);
        m_currentStep.store(0U);
        m_currentLoop.store(1U);
        m_state.store(MacroPlayerState::Idle);
        cb = m_stateCb;
    }
    if (cb) {
        cb(MacroPlayerState::Idle, "");
    }
}

MacroSequence MacroPlayer::sequence() const
{
    std::scoped_lock lock(m_mutex);
    return m_sequence;
}

bool MacroPlayer::start()
{
    // A callback running on the worker thread cannot restart or replace its own thread
    if (m_worker.joinable() && (std::this_thread::get_id() == m_worker.get_id())) {
        return false;
    }

    StateCallback cb { nullptr };
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_sequence.steps.empty()) {
            return false;
        }

        if (m_state.load() == MacroPlayerState::Playing) {
            return true;
        }

        if (m_state.load() == MacroPlayerState::Paused && m_workerRunning.load()) {
            m_state.store(MacroPlayerState::Playing);
            cb = m_stateCb;
            m_cv.notify_all();
            lock.unlock();
            if (cb) {
                cb(MacroPlayerState::Playing, "");
            }
            return true;
        }

        m_stopRequested.store(false);
        if (m_state.load() != MacroPlayerState::Paused) {
            m_currentStep.store(0U);
            m_currentLoop.store(1U);
        }
    }

    // Join without holding m_mutex before spawning new worker
    if (!joinWorker()) {
        return false;
    }

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_sequence.steps.empty()) {
            return false;
        }

        m_state.store(MacroPlayerState::Playing);
        m_workerRunning.store(true);
        cb = m_stateCb;
        m_worker = std::thread(&MacroPlayer::workerThreadFunc, this);
    }

    if (cb) {
        cb(MacroPlayerState::Playing, "");
    }
    return true;
}

void MacroPlayer::pause()
{
    StateCallback cb { nullptr };
    {
        std::scoped_lock lock(m_mutex);
        if (m_state.load() == MacroPlayerState::Playing) {
            m_state.store(MacroPlayerState::Paused);
            cb = m_stateCb;
            m_cv.notify_all();
        }
    }
    if (cb) {
        cb(MacroPlayerState::Paused, "");
    }
}

void MacroPlayer::resume()
{
    if (m_worker.joinable() && (std::this_thread::get_id() == m_worker.get_id())) {
        return;
    }

    StateCallback cb { nullptr };
    bool needsSpawn { false };
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_state.load() != MacroPlayerState::Paused || m_sequence.steps.empty()) {
            return;
        }

        if (m_workerRunning.load()) {
            m_state.store(MacroPlayerState::Playing);
            cb = m_stateCb;
            m_cv.notify_all();
        } else {
            needsSpawn = true;
        }
    }

    if (needsSpawn) {
        if (!joinWorker()) {
            return;
        }
        {
            std::scoped_lock lock(m_mutex);
            if (m_sequence.steps.empty()) {
                return;
            }
            m_stopRequested.store(false);
            m_state.store(MacroPlayerState::Playing);
            m_workerRunning.store(true);
            cb = m_stateCb;
            m_worker = std::thread(&MacroPlayer::workerThreadFunc, this);
        }
    }

    if (cb) {
        cb(MacroPlayerState::Playing, "");
    }
}

void MacroPlayer::stop()
{
    StateCallback cb { nullptr };
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_state.load() != MacroPlayerState::Stopped) {
            m_stopRequested.store(true);
            m_state.store(MacroPlayerState::Stopped);
            cb = m_stateCb;
            m_cv.notify_all();
        }
    }

    static_cast<void>(joinWorker());

    {
        std::scoped_lock lock(m_mutex);
        m_currentStep.store(0U);
        m_currentLoop.store(1U);
        m_workerRunning.store(false);
    }

    if (cb) {
        cb(MacroPlayerState::Stopped, "");
    }
}

bool MacroPlayer::stepNext()
{
    MacroStep step {};
    std::size_t stepIdx { 0U };
    std::size_t totalSteps { 0U };
    std::size_t currentLoop { 0U };
    std::size_t totalLoops { 0U };
    FrameDispatchCallback dispatchCb { nullptr };
    StepCallback stepCb { nullptr };
    StateCallback stateCb { nullptr };

    {
        std::scoped_lock lock(m_mutex);
        if (m_sequence.steps.empty()) {
            return false;
        }

        if (m_state.load() == MacroPlayerState::Playing) {
            return false; // Cannot single-step while actively running
        }

        totalSteps = m_sequence.steps.size();
        totalLoops = m_sequence.repeatCount;
        currentLoop = m_currentLoop.load();

        if (m_currentStep.load() >= totalSteps) {
            m_currentStep.store(0U); // Wrap around for manual stepping
        }

        stepIdx = m_currentStep.load();
        step = m_sequence.steps[stepIdx];

        dispatchCb = m_dispatchCb;
        stepCb = m_stepCb;
        stateCb = m_stateCb;

        std::size_t nextStep = stepIdx + 1U;
        if (nextStep >= totalSteps) {
            nextStep = 0U;
        }
        m_currentStep.store(nextStep);
        m_state.store(MacroPlayerState::Paused);
    }

    if (dispatchCb && !step.frame.empty()) {
        dispatchCb(step.frame);
    }

    if (stepCb) {
        stepCb(stepIdx, totalSteps, step, currentLoop, totalLoops);
    }

    if (stateCb) {
        stateCb(MacroPlayerState::Paused, "");
    }

    return true;
}

void MacroPlayer::setSpeedMultiplier(double multiplier) noexcept
{
    if (multiplier < 0.05) {
        multiplier = 0.05;
    } else if (multiplier > 20.0) {
        multiplier = 20.0;
    }
    m_speedMultiplier = multiplier;
}

double MacroPlayer::speedMultiplier() const noexcept
{
    return m_speedMultiplier.load();
}

MacroPlayerState MacroPlayer::state() const noexcept
{
    return m_state.load();
}

std::size_t MacroPlayer::currentStep() const noexcept
{
    return m_currentStep.load();
}

std::size_t MacroPlayer::currentLoop() const noexcept
{
    return m_currentLoop.load();
}

void MacroPlayer::workerThreadFunc()
{
    m_workerRunning.store(true);
    while (!m_stopRequested.load()) {
        MacroStep step {};
        std::size_t stepIdx { 0U };
        std::size_t totalSteps { 0U };
        std::size_t currentLoop { 0U };
        std::size_t totalLoops { 0U };
        FrameDispatchCallback dispatchCb { nullptr };
        StepCallback stepCb { nullptr };
        StateCallback stateCb { nullptr };
        bool completed { false };

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() {
                return m_stopRequested.load() || m_state.load() == MacroPlayerState::Playing;
            });

            if (m_stopRequested.load()) {
                break;
            }

            totalSteps = m_sequence.steps.size();
            totalLoops = m_sequence.repeatCount;
            stepIdx = m_currentStep.load();
            currentLoop = m_currentLoop.load();

            if (stepIdx >= totalSteps) {
                // Check repeat condition
                if (totalLoops == 0U || currentLoop < totalLoops) {
                    // Start next repeat loop
                    m_currentStep.store(0U);
                    m_currentLoop.store(currentLoop + 1U);
                    continue;
                }

                // Macro sequence completed
                m_state.store(MacroPlayerState::Completed);
                stateCb = m_stateCb;
                completed = true;
            } else {
                step = m_sequence.steps[stepIdx];
                dispatchCb = m_dispatchCb;
                stepCb = m_stepCb;
            }
        }

        if (completed) {
            if (stateCb) {
                stateCb(MacroPlayerState::Completed, "Playback completed");
            }
            break;
        }

        // Dispatch frame outside lock
        if (dispatchCb && !step.frame.empty()) {
            dispatchCb(step.frame);
        }

        // Notify step callback outside lock
        if (stepCb) {
            stepCb(stepIdx, totalSteps, step, currentLoop, totalLoops);
        }

        // Advance step counter
        m_currentStep.store(stepIdx + 1U);

        // Calculate scaled delay with overflow protection (H10)
        const double speed = m_speedMultiplier.load();
        const double effectiveSpeed = (speed >= 0.05) ? speed : 0.05;
        const double scaledDelay = static_cast<double>(step.delayMs) / effectiveSpeed;
        const auto delayMs = (scaledDelay > static_cast<double>(UINT32_MAX))
            ? UINT32_MAX
            : static_cast<std::uint32_t>(scaledDelay);

        // Sleep with interruptible check
        if (delayMs > 0U) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(delayMs), [this]() {
                return m_stopRequested.load() || m_state.load() != MacroPlayerState::Playing;
            });
            if (m_stopRequested.load()) {
                break;
            }
        }
    }
    m_workerRunning.store(false);
}

} // namespace PelcoD
