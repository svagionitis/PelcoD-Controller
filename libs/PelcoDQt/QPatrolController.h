#pragma once

/// @file QPatrolController.h
/// @brief Qt wrapper providing signals and slots for PelcoD::PatrolController.

#include "PatrolController.h"

#include <QObject>
#include <QString>
#include <memory>
#include <vector>

namespace PelcoDQt {

class QPelcoDDevice;

/// @class QPatrolController
/// @brief Qt QObject adapter for PelcoD::PatrolController with thread-safe signals.
class QPatrolController : public QObject {
    Q_OBJECT

public:
    explicit QPatrolController(QPelcoDDevice* device, QObject* parent = nullptr);
    explicit QPatrolController(QObject* parent = nullptr);
    ~QPatrolController() override;

    // Non-copyable, non-movable
    QPatrolController(const QPatrolController&) = delete;
    QPatrolController& operator=(const QPatrolController&) = delete;
    QPatrolController(QPatrolController&&) = delete;
    QPatrolController& operator=(QPatrolController&&) = delete;

    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] bool isPaused() const;
    [[nodiscard]] PelcoD::PatrolState getState() const;
    [[nodiscard]] bool isLooping() const;

    [[nodiscard]] int stepCount() const;
    [[nodiscard]] int currentStepIndex() const;
    [[nodiscard]] int remainingDwellSeconds() const;
    [[nodiscard]] std::vector<PelcoD::PatrolStep> steps() const;

    /// @brief Direct access to underlying PelcoD::PatrolController instance.
    [[nodiscard]] PelcoD::PatrolController* coreController() const noexcept
    {
        return m_controller.get();
    }

    void setDevice(QPelcoDDevice* device);

public slots:
    bool start();
    void stop();
    void pause();
    void resume();
    void nextStep();
    void previousStep();
    void setLoop(bool loop);

    void addStep(int presetId, int dwellTimeSeconds, const QString& name = QString(), int speed = 0);
    void insertStep(int index, int presetId, int dwellTimeSeconds, const QString& name = QString(), int speed = 0);
    bool removeStep(int index);
    void moveStepUp(int index);
    void moveStepDown(int index);
    void clearSteps();

signals:
    void stateChanged(PelcoD::PatrolState state);
    void stepChanged(int stepIndex, int presetId, const QString& name);
    void dwellTick(int stepIndex, int remainingSeconds);
    void tourFinished();

private:
    void setupCallbacks();

    std::unique_ptr<PelcoD::PatrolController> m_controller;
    QPelcoDDevice* m_device { nullptr };
};

} // namespace PelcoDQt
