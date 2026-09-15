#pragma once

/// @file VideoStreamTab.h
/// @brief UI tab hosting live RTSP / video stream player with tactical HUD telemetry overlays and camera controls.

#include "DeviceStatus.h"
#include "FujinonTypes.h"
#include "QPelcoDDevice.h"
#include "QVideoStreamWorker.h"
#include "app-qt/widgets/VideoOverlayWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

namespace PelcoDApp {

/// @class VideoStreamTab
/// @brief Dashboard tab providing live video streaming, overlay toggles, and integrated PTZ controls.
class VideoStreamTab : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor.
    /// @param[in] device Pointer to QPelcoDDevice controller.
    /// @param[in] parent Optional parent widget.
    explicit VideoStreamTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent = nullptr);

    /// @brief Destructor.
    ~VideoStreamTab() override;

public slots:
    /// @brief Receives base Pelco-D telemetry and feeds into HUD.
    void handleDeviceStatusUpdated(const PelcoD::DeviceStatus& status);

    /// @brief Receives Fujinon SX800 extended telemetry and feeds into HUD.
    void handleFujinonStatusUpdated(const PelcoD::FujinonStatus& status);

    /// @brief Receives RTT profiler measurements and feeds into HUD diagnostics.
    void handleRttStatsUpdated(double avgRttMs, double jitterMs);

    /// @brief Toggles fullscreen display mode for the video canvas.
    void toggleFullscreen();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onSnapshotClicked();
    void onColorSchemeChanged(int index);
    void onWorkerStatusChanged(PelcoD::Video::StreamState state, const QString& message);
    void onWorkerStatsUpdated(double fps, double avgDecodeMs);

    // Interactive PTZ handling
    void handleOverlayPanTiltRequested(int panSpeed, int tiltSpeed, bool left, bool right, bool up, bool down);
    void handleOverlayStopRequested();
    void handleOverlayZoomRequested(bool zoomIn);

private:
    void setupUi();
    void setupConnections();

    PelcoDQt::QPelcoDDevice* m_device { nullptr };
    PelcoD::Video::QVideoStreamWorker* m_worker { nullptr };

    // UI Widgets
    VideoOverlayWidget* m_overlayWidget { nullptr };

    // Stream Controls Bar
    QComboBox* m_sourceCombo { nullptr };
    QComboBox* m_backendCombo { nullptr };
    QPushButton* m_btnConnect { nullptr };
    QPushButton* m_btnDisconnect { nullptr };
    QPushButton* m_btnSnapshot { nullptr };
    QPushButton* m_btnFullscreen { nullptr };
    QLabel* m_statusLabel { nullptr };

    // Overlay Toggles Panel
    QCheckBox* m_chkCrosshair { nullptr };
    QCheckBox* m_chkCompass { nullptr };
    QCheckBox* m_chkPitchLadder { nullptr };
    QCheckBox* m_chkOpticsHud { nullptr };
    QCheckBox* m_chkDiagnostics { nullptr };
    QCheckBox* m_chkInteractivePtz { nullptr };
    QComboBox* m_comboColorScheme { nullptr };

    // Quick PTZ Controls
    QSlider* m_speedSlider { nullptr };
    QLabel* m_speedLabel { nullptr };

    bool m_isFullscreen { false };
    QWidget* m_fullscreenContainer { nullptr };
};

} // namespace PelcoDApp
