/// @file QPatrolController.cpp
/// @brief Implementation of Qt wrapper for PelcoD::PatrolController.

#include "QPatrolController.h"
#include "QPelcoDDevice.h"

#include <QMetaObject>
#include <algorithm>
#include <utility>

namespace PelcoDQt {

QPatrolController::QPatrolController(QPelcoDDevice* device, QObject* parent)
    : QObject(parent)
    , m_controller(std::make_unique<PelcoD::PatrolController>())
    , m_device(device)
{
    setDevice(device);
    setupCallbacks();
}

QPatrolController::QPatrolController(QObject* parent)
    : QObject(parent)
    , m_controller(std::make_unique<PelcoD::PatrolController>())
{
    setupCallbacks();
}

QPatrolController::~QPatrolController()
{
    if (m_controller) {
        m_controller->stop();
    }
}

void QPatrolController::setDevice(QPelcoDDevice* device)
{
    m_device = device;
    if (m_controller) {
        if (m_device != nullptr) {
            m_controller->setCommandDispatcher([device](std::uint8_t presetId) {
                device->goToPreset(static_cast<int>(presetId));
            });
        } else {
            m_controller->setCommandDispatcher(nullptr);
        }
    }
}

void QPatrolController::setupCallbacks()
{
    m_controller->setStateChangedCallback([this](PelcoD::PatrolState state) {
        QMetaObject::invokeMethod(
            this, [this, state] { emit stateChanged(state); }, Qt::QueuedConnection);
    });

    m_controller->setStepChangedCallback([this](std::size_t stepIndex, const PelcoD::PatrolStep& step) {
        const QString name = QString::fromStdString(step.name);
        const int idx = static_cast<int>(stepIndex);
        const int presetId = static_cast<int>(step.presetId);
        QMetaObject::invokeMethod(
            this, [this, idx, presetId, name] { emit stepChanged(idx, presetId, name); }, Qt::QueuedConnection);
    });

    m_controller->setDwellTickCallback([this](std::size_t stepIndex, std::uint32_t remSec) {
        const int idx = static_cast<int>(stepIndex);
        const int sec = static_cast<int>(remSec);
        QMetaObject::invokeMethod(
            this, [this, idx, sec] { emit dwellTick(idx, sec); }, Qt::QueuedConnection);
    });

    m_controller->setTourFinishedCallback([this]() {
        QMetaObject::invokeMethod(
            this, [this] { emit tourFinished(); }, Qt::QueuedConnection);
    });
}

bool QPatrolController::start()
{
    return m_controller ? m_controller->start() : false;
}

void QPatrolController::stop()
{
    if (m_controller) {
        m_controller->stop();
    }
}

void QPatrolController::pause()
{
    if (m_controller) {
        m_controller->pause();
    }
}

void QPatrolController::resume()
{
    if (m_controller) {
        m_controller->resume();
    }
}

void QPatrolController::nextStep()
{
    if (m_controller) {
        m_controller->nextStep();
    }
}

void QPatrolController::previousStep()
{
    if (m_controller) {
        m_controller->previousStep();
    }
}

void QPatrolController::setLoop(bool loop)
{
    if (m_controller) {
        m_controller->setLoop(loop);
    }
}

bool QPatrolController::isRunning() const
{
    return m_controller ? m_controller->isRunning() : false;
}

bool QPatrolController::isPaused() const
{
    return m_controller ? m_controller->isPaused() : false;
}

PelcoD::PatrolState QPatrolController::getState() const
{
    return m_controller ? m_controller->getState() : PelcoD::PatrolState::Idle;
}

bool QPatrolController::isLooping() const
{
    return m_controller ? m_controller->isLooping() : true;
}

int QPatrolController::stepCount() const
{
    return m_controller ? static_cast<int>(m_controller->stepCount()) : 0;
}

int QPatrolController::currentStepIndex() const
{
    return m_controller ? static_cast<int>(m_controller->getCurrentStepIndex()) : 0;
}

int QPatrolController::remainingDwellSeconds() const
{
    return m_controller ? static_cast<int>(m_controller->getRemainingDwellSeconds()) : 0;
}

std::vector<PelcoD::PatrolStep> QPatrolController::steps() const
{
    return m_controller ? m_controller->getSteps() : std::vector<PelcoD::PatrolStep> {};
}

void QPatrolController::addStep(int presetId, int dwellTimeSeconds, const QString& name, int speed)
{
    if (!m_controller) {
        return;
    }
    PelcoD::PatrolStep step;
    step.presetId = static_cast<std::uint8_t>(std::clamp(presetId, 1, 255));
    step.dwellTimeSeconds = static_cast<std::uint32_t>(std::max(1, dwellTimeSeconds));
    step.name = name.toStdString();
    step.speed = static_cast<std::uint8_t>(std::clamp(speed, 0, 63));
    m_controller->addStep(step);
}

void QPatrolController::insertStep(int index, int presetId, int dwellTimeSeconds, const QString& name, int speed)
{
    if (!m_controller || index < 0) {
        return;
    }
    PelcoD::PatrolStep step;
    step.presetId = static_cast<std::uint8_t>(std::clamp(presetId, 1, 255));
    step.dwellTimeSeconds = static_cast<std::uint32_t>(std::max(1, dwellTimeSeconds));
    step.name = name.toStdString();
    step.speed = static_cast<std::uint8_t>(std::clamp(speed, 0, 63));
    m_controller->insertStep(static_cast<std::size_t>(index), step);
}

bool QPatrolController::removeStep(int index)
{
    if (!m_controller || index < 0) {
        return false;
    }
    return m_controller->removeStep(static_cast<std::size_t>(index));
}

void QPatrolController::moveStepUp(int index)
{
    if (!m_controller || index <= 0) {
        return;
    }
    auto s = m_controller->getSteps();
    if (static_cast<std::size_t>(index) >= s.size()) {
        return;
    }
    std::swap(s[static_cast<std::size_t>(index)], s[static_cast<std::size_t>(index - 1)]);
    m_controller->setSteps(s);
}

void QPatrolController::moveStepDown(int index)
{
    if (!m_controller || index < 0) {
        return;
    }
    auto s = m_controller->getSteps();
    if (static_cast<std::size_t>(index) + 1 >= s.size()) {
        return;
    }
    std::swap(s[static_cast<std::size_t>(index)], s[static_cast<std::size_t>(index + 1)]);
    m_controller->setSteps(s);
}

void QPatrolController::clearSteps()
{
    if (m_controller) {
        m_controller->clearSteps();
    }
}

} // namespace PelcoDQt
