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
    std::scoped_lock lock(m_mutex);
    m_sequence = std::move(sequence);
    m_currentStep = 0U;
    m_currentLoop = 1U;
    setState(MacroPlayerState::Idle);
}

const MacroSequence& MacroPlayer::sequence() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_sequence;
}

bool MacroPlayer::start()
{
    {
        std::scoped_lock lock(m_mutex);
        if (m_sequence.steps.empty()) {
            return false;
        }

        if (m_state == MacroPlayerState::Playing) {
            return true;
        }

        if (m_state == MacroPlayerState::Paused) {
            setState(MacroPlayerState::Playing);
            m_cv.notify_all();
            return true;
        }

        // Fresh start from beginning
        m_stopRequested = false;
        m_currentStep = 0U;
        m_currentLoop = 1U;
        setState(MacroPlayerState::Playing);

        if (m_worker.joinable()) {
            m_cv.notify_all();
            m_worker.join();
        }

        m_worker = std::thread(&MacroPlayer::workerThreadFunc, this);
    }

    return true;
}

void MacroPlayer::pause()
{
    std::scoped_lock lock(m_mutex);
    if (m_state == MacroPlayerState::Playing) {
        setState(MacroPlayerState::Paused);
        m_cv.notify_all();
    }
}

void MacroPlayer::resume()
{
    std::scoped_lock lock(m_mutex);
    if (m_state == MacroPlayerState::Paused) {
        setState(MacroPlayerState::Playing);
        m_cv.notify_all();
    }
}

void MacroPlayer::stop()
{
    {
        std::scoped_lock lock(m_mutex);
        m_stopRequested = true;
        setState(MacroPlayerState::Stopped);
        m_cv.notify_all();
    }

    if (m_worker.joinable()) {
        m_worker.join();
    }

    std::scoped_lock lock(m_mutex);
    m_currentStep = 0U;
    m_currentLoop = 1U;
}

bool MacroPlayer::stepNext()
{
    std::scoped_lock lock(m_mutex);
    if (m_sequence.steps.empty()) {
        return false;
    }

    if (m_state == MacroPlayerState::Playing) {
        return false; // Cannot single-step while actively running
    }

    if (m_currentStep >= m_sequence.steps.size()) {
        m_currentStep = 0U; // Wrap around for manual stepping
    }

    const std::size_t stepIdx = m_currentStep.load();
    const auto& step = m_sequence.steps[stepIdx];

    if (m_dispatchCb && !step.frame.empty()) {
        m_dispatchCb(step.frame);
    }

    if (m_stepCb) {
        m_stepCb(stepIdx, m_sequence.steps.size(), step, m_currentLoop.load(), m_sequence.repeatCount);
    }

    m_currentStep = stepIdx + 1;
    if (m_currentStep >= m_sequence.steps.size()) {
        m_currentStep = 0U;
    }

    setState(MacroPlayerState::Paused);
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

void MacroPlayer::setState(MacroPlayerState newState, const std::string& msg)
{
    m_state = newState;
    if (m_stateCb) {
        m_stateCb(newState, msg);
    }
}

void MacroPlayer::workerThreadFunc()
{
    while (!m_stopRequested) {
        MacroStep step;
        std::size_t stepIdx = 0U;
        std::size_t totalSteps = 0U;
        std::size_t currentLoop = 0U;
        std::size_t totalLoops = 0U;
        FrameDispatchCallback dispatchCb = nullptr;
        StepCallback stepCb = nullptr;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() {
                return m_stopRequested || m_state == MacroPlayerState::Playing;
            });

            if (m_stopRequested) {
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
                    m_currentStep = 0U;
                    m_currentLoop = currentLoop + 1;
                    continue;
                } else {
                    // Macro sequence completed
                    setState(MacroPlayerState::Completed, "Playback completed");
                    break;
                }
            }

            step = m_sequence.steps[stepIdx];
            dispatchCb = m_dispatchCb;
            stepCb = m_stepCb;
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
        m_currentStep = stepIdx + 1;

        // Calculate scaled delay
        const double speed = m_speedMultiplier.load();
        const auto delayMs = static_cast<std::uint32_t>(step.delayMs / (speed > 0.0 ? speed : 1.0));

        // Sleep with interruptible check
        if (delayMs > 0U) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(delayMs), [this]() {
                return m_stopRequested || m_state != MacroPlayerState::Playing;
            });
            if (m_stopRequested) {
                break;
            }
        }
    }
}

} // namespace PelcoD
