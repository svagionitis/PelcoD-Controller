#pragma once

/// @file LensWorker.h
/// @brief Dedicated background worker managing LensController and mock hardware simulation.

#include <cstddef>
#include <cstdint>

#include <memory>

#include <QObject>
#include <QTimer>

#include "LensController.h"
#include "LensTypes.h"
#include "MockLensDriver.h"

namespace LensApp {

/// @class LensWorker
/// @brief Worker object executing on dedicated QThread for continuous lens polling and actuation.
class LensWorker : public QObject {
    Q_OBJECT

public:
    explicit LensWorker(QObject* parent = nullptr);
    ~LensWorker() override;

    LensWorker(const LensWorker&) = delete;
    LensWorker& operator=(const LensWorker&) = delete;
    LensWorker(LensWorker&&) = delete;
    LensWorker& operator=(LensWorker&&) = delete;

    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] int activeChannel() const noexcept;

public slots:
    void initWorker();
    void stopWorker();

    void onSetActiveChannel(int channel);
    void onSetLensMode(int channel, int mode);

    void onMoveFocusStep(int channel, int steps);
    void onMoveZoomStep(int channel, int steps);
    void onSetFocusTarget(int channel, int position);
    void onSetZoomTarget(int channel, int position);
    void onSetFocusSpeed(int channel, int speed);
    void onSetZoomSpeed(int channel, int speed);
    void onStopMotion(int channel);
    void onEmergencyStop();

signals:
    void workerInitialized();
    void statusUpdated(int focus, int zoom, int limits, int mode, bool afActive);

private slots:
    void processLoopTick();

private:
    LensControl::LensController m_controller { };
    QTimer* m_timer { nullptr };
    int m_activeChannel { 0 };
    bool m_isRunning { false };
};

} // namespace LensApp
