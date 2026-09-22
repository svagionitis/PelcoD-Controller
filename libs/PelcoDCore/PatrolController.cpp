/// @file PatrolController.cpp
/// @brief Implementation of automated Preset Tour / Patrol Sequence Controller.

#include "PatrolController.h"
#include "PelcoDDevice.h"

#include <chrono>

namespace PelcoD {

PatrolController::PatrolController(PelcoDDevice* device)
{
    setDevice(device);
}

PatrolController::PatrolController(GoToPresetCallback dispatcher)
    : m_dispatcher(std::move(dispatcher))
{
}

PatrolController::~PatrolController()
{
    m_workerRunning.store(false);
    m_state.store(PatrolState::Idle);
    m_cv.notify_all();

    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void PatrolController::notifyStateChange(std::unique_lock<std::mutex>& lock, PatrolState state, bool relock)
{
    const auto stateCb = m_stateChangedCb;
    m_cv.notify_all();

    if (stateCb) {
        lock.unlock();
        stateCb(state);
        if (relock) {
            lock.lock();
        }
    } else if (!relock && lock.owns_lock()) {
        lock.unlock();
    }
}

bool PatrolController::start()
{
    if (m_worker.joinable() && !m_workerRunning.load()) {
        m_worker.join();
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_steps.empty()) {
        return false;
    }

    if (m_state.load() == PatrolState::Running) {
        return true;
    }

    m_currentStepIndex = 0U;
    m_remainingDwellSeconds = m_steps[0].dwellTimeSeconds;
    m_stepAdvanceRequested = false;
    m_stepAdvanceDelta = 0;
    m_needsDispatch = true;
    m_state.store(PatrolState::Running);
    m_workerRunning.store(true);
    m_worker = std::thread(&PatrolController::workerLoop, this);

    notifyStateChange(lock, PatrolState::Running);
    return true;
}

void PatrolController::stop()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_state.load() == PatrolState::Idle) {
            return;
        }

        m_state.store(PatrolState::Idle);
        m_workerRunning.store(false);
        m_remainingDwellSeconds = 0U;
        m_needsDispatch = false;
        m_stepAdvanceRequested = false;
        m_stepAdvanceDelta = 0;

        notifyStateChange(lock, PatrolState::Idle, false);
    }

    if (m_worker.joinable() && std::this_thread::get_id() != m_worker.get_id()) {
        m_worker.join();
    }
}

void PatrolController::pause()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_state.load() != PatrolState::Running) {
        return;
    }

    m_state.store(PatrolState::Paused);
    notifyStateChange(lock, PatrolState::Paused);
}

void PatrolController::resume()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_state.load() != PatrolState::Paused) {
        return;
    }

    m_state.store(PatrolState::Running);
    notifyStateChange(lock, PatrolState::Running);
}

void PatrolController::nextStep()
{
    std::scoped_lock lock(m_mutex);
    if (m_state.load() == PatrolState::Idle || m_steps.empty()) {
        return;
    }
    m_stepAdvanceRequested = true;
    m_stepAdvanceDelta = 1;
    m_cv.notify_all();
}

void PatrolController::previousStep()
{
    std::scoped_lock lock(m_mutex);
    if (m_state.load() == PatrolState::Idle || m_steps.empty()) {
        return;
    }
    m_stepAdvanceRequested = true;
    m_stepAdvanceDelta = -1;
    m_cv.notify_all();
}

bool PatrolController::isRunning() const noexcept
{
    return m_state.load() == PatrolState::Running;
}

bool PatrolController::isPaused() const noexcept
{
    return m_state.load() == PatrolState::Paused;
}

PatrolState PatrolController::getState() const noexcept
{
    return m_state.load();
}

bool PatrolController::isWorkerActive() const noexcept
{
    return m_workerRunning.load();
}

void PatrolController::addStep(const PatrolStep& step)
{
    std::scoped_lock lock(m_mutex);
    m_steps.push_back(step);
}

void PatrolController::insertStep(std::size_t index, const PatrolStep& step)
{
    std::scoped_lock lock(m_mutex);
    if (index >= m_steps.size()) {
        m_steps.push_back(step);
    } else {
        m_steps.insert(m_steps.begin() + static_cast<std::ptrdiff_t>(index), step);
    }
}

bool PatrolController::removeStep(std::size_t index)
{
    std::scoped_lock lock(m_mutex);
    if (index >= m_steps.size()) {
        return false;
    }
    m_steps.erase(m_steps.begin() + static_cast<std::ptrdiff_t>(index));
    if (m_currentStepIndex >= m_steps.size() && !m_steps.empty()) {
        m_currentStepIndex = m_steps.size() - 1U;
    }
    return true;
}

bool PatrolController::setStep(std::size_t index, const PatrolStep& step)
{
    std::scoped_lock lock(m_mutex);
    if (index >= m_steps.size()) {
        return false;
    }
    m_steps[index] = step;
    return true;
}

void PatrolController::clearSteps()
{
    std::scoped_lock lock(m_mutex);
    m_steps.clear();
    m_currentStepIndex = 0U;
    m_remainingDwellSeconds = 0U;
}

std::vector<PatrolStep> PatrolController::getSteps() const
{
    std::scoped_lock lock(m_mutex);
    return m_steps;
}

void PatrolController::setSteps(const std::vector<PatrolStep>& steps)
{
    std::scoped_lock lock(m_mutex);
    m_steps = steps;
    m_currentStepIndex = 0U;
    m_remainingDwellSeconds = 0U;
}

std::size_t PatrolController::stepCount() const
{
    std::scoped_lock lock(m_mutex);
    return m_steps.size();
}

std::size_t PatrolController::getCurrentStepIndex() const
{
    std::scoped_lock lock(m_mutex);
    return m_currentStepIndex;
}

std::uint32_t PatrolController::getRemainingDwellSeconds() const
{
    std::scoped_lock lock(m_mutex);
    return m_remainingDwellSeconds;
}

void PatrolController::setLoop(bool loop) noexcept
{
    m_loop.store(loop);
}

bool PatrolController::isLooping() const noexcept
{
    return m_loop.load();
}

void PatrolController::setDevice(PelcoDDevice* device)
{
    std::scoped_lock lock(m_mutex);
    if (device != nullptr) {
        m_dispatcher = [device](std::uint8_t presetId) { device->goToPreset(presetId); };
    } else {
        m_dispatcher = nullptr;
    }
}

void PatrolController::setCommandDispatcher(GoToPresetCallback dispatcher)
{
    std::scoped_lock lock(m_mutex);
    m_dispatcher = std::move(dispatcher);
}

void PatrolController::setStepChangedCallback(StepChangedCallback cb)
{
    std::scoped_lock lock(m_mutex);
    m_stepChangedCb = std::move(cb);
}

void PatrolController::setStateChangedCallback(StateChangedCallback cb)
{
    std::scoped_lock lock(m_mutex);
    m_stateChangedCb = std::move(cb);
}

void PatrolController::setDwellTickCallback(DwellTickCallback cb)
{
    std::scoped_lock lock(m_mutex);
    m_dwellTickCb = std::move(cb);
}

void PatrolController::setTourFinishedCallback(TourFinishedCallback cb)
{
    std::scoped_lock lock(m_mutex);
    m_tourFinishedCb = std::move(cb);
}

void PatrolController::dispatchCurrentStep(std::unique_lock<std::mutex>& lock)
{
    if (m_steps.empty() || m_currentStepIndex >= m_steps.size()) {
        return;
    }
    const auto step = m_steps[m_currentStepIndex];
    const auto idx = m_currentStepIndex;
    const auto dispatcher = m_dispatcher;
    const auto stepCb = m_stepChangedCb;

    lock.unlock();
    if (dispatcher) {
        dispatcher(step.presetId);
    }
    if (stepCb) {
        stepCb(idx, step);
    }
    lock.lock();
}

void PatrolController::workerLoop()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    while (m_workerRunning.load()) {
        if (m_state.load() == PatrolState::Idle) {
            break;
        }

        if (m_state.load() == PatrolState::Paused) {
            m_cv.wait(lock, [this] { return !m_workerRunning.load() || m_state.load() != PatrolState::Paused; });
            if (!m_workerRunning.load()) {
                break;
            }
            continue;
        }

        if (m_needsDispatch) {
            m_needsDispatch = false;
            dispatchCurrentStep(lock);
            if (m_state.load() != PatrolState::Running || !m_workerRunning.load()) {
                continue;
            }
        }

        // Running state: wait for 1 second or an action signal
        m_cv.wait_for(lock, std::chrono::seconds(1), [this] {
            return !m_workerRunning.load() || m_state.load() != PatrolState::Running || m_stepAdvanceRequested;
        });

        if (!m_workerRunning.load()) {
            break;
        }

        if (m_state.load() != PatrolState::Running) {
            continue;
        }

        if (m_stepAdvanceRequested) {
            m_stepAdvanceRequested = false;
            const int delta = m_stepAdvanceDelta;
            m_stepAdvanceDelta = 0;

            if (!m_steps.empty()) {
                const int numSteps = static_cast<int>(m_steps.size());
                int nextIdx = static_cast<int>(m_currentStepIndex) + delta;
                if (nextIdx >= numSteps) {
                    nextIdx = m_loop.load() ? 0 : (numSteps - 1);
                } else if (nextIdx < 0) {
                    nextIdx = m_loop.load() ? (numSteps - 1) : 0;
                }
                m_currentStepIndex = static_cast<std::size_t>(nextIdx);
                m_remainingDwellSeconds = m_steps[m_currentStepIndex].dwellTimeSeconds;
                dispatchCurrentStep(lock);
            }
            continue;
        }

        // 1 second elapsed
        if (m_remainingDwellSeconds > 0U) {
            --m_remainingDwellSeconds;
        }

        const auto curStep = m_currentStepIndex;
        const auto remSec = m_remainingDwellSeconds;
        const auto dwellCb = m_dwellTickCb;

        if (dwellCb) {
            lock.unlock();
            dwellCb(curStep, remSec);
            lock.lock();
        }

        if (m_state.load() != PatrolState::Running || !m_workerRunning.load()) {
            continue;
        }

        if (m_remainingDwellSeconds == 0U) {
            // Current step dwell expired; advance to next step
            if (m_currentStepIndex + 1U < m_steps.size()) {
                ++m_currentStepIndex;
                m_remainingDwellSeconds = m_steps[m_currentStepIndex].dwellTimeSeconds;
                dispatchCurrentStep(lock);
            } else {
                // Last step reached
                if (m_loop.load() && !m_steps.empty()) {
                    m_currentStepIndex = 0U;
                    m_remainingDwellSeconds = m_steps[0].dwellTimeSeconds;
                    dispatchCurrentStep(lock);
                } else {
                    m_state.store(PatrolState::Idle);
                    m_workerRunning.store(false);
                    const auto stateCb = m_stateChangedCb;
                    const auto finishCb = m_tourFinishedCb;

                    lock.unlock();
                    if (stateCb) {
                        stateCb(PatrolState::Idle);
                    }
                    if (finishCb) {
                        finishCb();
                    }
                    lock.lock();
                    break;
                }
            }
        }
    }
}

} // namespace PelcoD
