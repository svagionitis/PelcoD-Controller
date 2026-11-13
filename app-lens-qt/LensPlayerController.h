#pragma once

/// @file LensPlayerController.h
/// @brief Master QML ViewModel bridging UI events with LensWorker and LensController subsystems.

#include <cstdint>

#include <QObject>
#include <QString>
#include <QThread>

#include "LensConfigModel.h"

namespace LensApp {

class LensWorker;

/// @class LensPlayerController
/// @brief Master ViewModel managing multi-channel optical actuation, modes, and telemetry.
class LensPlayerController : public QObject {
    Q_OBJECT

    Q_PROPERTY(int activeChannel READ activeChannel WRITE setActiveChannel NOTIFY activeChannelChanged)
    Q_PROPERTY(int lensMode READ lensMode WRITE setLensMode NOTIFY lensModeChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

    Q_PROPERTY(int currentFocus READ currentFocus NOTIFY telemetryUpdated)
    Q_PROPERTY(int currentZoom READ currentZoom NOTIFY telemetryUpdated)
    Q_PROPERTY(int focusMin READ focusMin NOTIFY limitsChanged)
    Q_PROPERTY(int focusMax READ focusMax NOTIFY limitsChanged)
    Q_PROPERTY(int zoomMin READ zoomMin NOTIFY limitsChanged)
    Q_PROPERTY(int zoomMax READ zoomMax NOTIFY limitsChanged)
    Q_PROPERTY(int limitFlags READ limitFlags NOTIFY telemetryUpdated)
    Q_PROPERTY(bool isAfRunning READ isAfRunning NOTIFY afStateChanged)

    Q_PROPERTY(LensConfigModel* configModel READ configModel CONSTANT)

public:
    explicit LensPlayerController(QObject* parent = nullptr);
    ~LensPlayerController() override;

    LensPlayerController(const LensPlayerController&) = delete;
    LensPlayerController& operator=(const LensPlayerController&) = delete;
    LensPlayerController(LensPlayerController&&) = delete;
    LensPlayerController& operator=(LensPlayerController&&) = delete;

    [[nodiscard]] int activeChannel() const noexcept;
    [[nodiscard]] int lensMode() const noexcept;
    [[nodiscard]] QString statusMessage() const;

    [[nodiscard]] int currentFocus() const noexcept;
    [[nodiscard]] int currentZoom() const noexcept;
    [[nodiscard]] int focusMin() const noexcept;
    [[nodiscard]] int focusMax() const noexcept;
    [[nodiscard]] int zoomMin() const noexcept;
    [[nodiscard]] int zoomMax() const noexcept;
    [[nodiscard]] int limitFlags() const noexcept;
    [[nodiscard]] bool isAfRunning() const noexcept;

    [[nodiscard]] LensConfigModel* configModel() const noexcept;

public slots:
    void setActiveChannel(int channel);
    void setLensMode(int mode);

    void moveFocusStep(int steps);
    void moveZoomStep(int steps);
    void setFocusTarget(int position);
    void setZoomTarget(int position);
    void setFocusSpeed(int speed);
    void setZoomSpeed(int speed);
    void stopMotion();
    void emergencyStop();

signals:
    void activeChannelChanged();
    void lensModeChanged();
    void statusMessageChanged();
    void telemetryUpdated();
    void limitsChanged();
    void afStateChanged();

    // Internal inter-thread dispatch signals (< 30 chars)
    void requestMoveFocus(int channel, int steps);
    void requestMoveZoom(int channel, int steps);
    void requestSetFocus(int channel, int position);
    void requestSetZoom(int channel, int position);
    void requestSetFocusSpeed(int channel, int speed);
    void requestSetZoomSpeed(int channel, int speed);
    void requestStopMotion(int channel);
    void requestEmergencyStop();
    void requestSetChannel(int channel);
    void requestSetMode(int channel, int mode);

private slots:
    void onStatusUpdated(int focus, int zoom, int limits, int mode, bool afActive);

private:
    void setupWorkerThread();

    QThread m_workerThread { };
    LensWorker* m_worker { nullptr };
    LensConfigModel* m_configModel { nullptr };

    int m_activeChannel { 0 };
    int m_lensMode { 0 };
    int m_currentFocus { 0 };
    int m_currentZoom { 0 };
    int m_focusMin { 0 };
    int m_focusMax { 65535 };
    int m_zoomMin { 0 };
    int m_zoomMax { 65535 };
    int m_limitFlags { 0 };
    bool m_isAfRunning { false };
    QString m_statusMessage { QStringLiteral("Ready") };
};

} // namespace LensApp
