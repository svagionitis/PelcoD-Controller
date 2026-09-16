#pragma once

/// @file VideoStreamTab.h
/// @brief UI tab hosting live RTSP / video stream player with tactical HUD telemetry overlays and camera controls.

#include "DeviceStatus.h"
#include "FujinonTypes.h"
#include "PtzAutoTracker.h"
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
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace PelcoD::Video {
class CentroidTargetTrackerFilter;
}

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
    void onSourceTypeChanged(int index);
    void onBrowseFileClicked();
    void onRefreshDevicesClicked();
    void onLoopFileToggled(bool checked);
    void onConnectClicked();
    void onDisconnectClicked();
    void onSnapshotClicked();
    void onColorSchemeChanged(int index);
    void onWorkerStatusChanged(PelcoD::Video::StreamState state, const QString& message);
    void onWorkerStatsUpdated(double fps, double avgDecodeMs);
#if defined(PELCOD_HAS_FILTERS)
    void onFilterConfigurationChanged();
    void onAutoFollowTick();
#endif

    // Interactive PTZ handling
    void handleOverlayPanTiltRequested(int panSpeed, int tiltSpeed, bool left, bool right, bool up, bool down);
    void handleOverlayStopRequested();
    void handleOverlayZoomRequested(bool zoomIn);

private:
    void setupUi();
    void setupConnections();
    void populateCaptureDevices();

    PelcoDQt::QPelcoDDevice* m_device { nullptr };
    PelcoD::Video::QVideoStreamWorker* m_worker { nullptr };

    // UI Widgets
    VideoOverlayWidget* m_overlayWidget { nullptr };

    // Stream Controls Bar
    QComboBox* m_sourceTypeCombo { nullptr };
    QWidget* m_rtspContainer { nullptr };
    QWidget* m_fileContainer { nullptr };
    QWidget* m_deviceContainer { nullptr };

    QComboBox* m_sourceCombo { nullptr };
    QLineEdit* m_filePathEdit { nullptr };
    QPushButton* m_btnBrowseFile { nullptr };
    QCheckBox* m_chkLoopFile { nullptr };
    QComboBox* m_deviceCombo { nullptr };
    QPushButton* m_btnRefreshDevices { nullptr };

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

#if defined(PELCOD_HAS_FILTERS)
    // Vision & Tactical Image Enhancement Controls
    QComboBox* m_comboPalette { nullptr };
    QCheckBox* m_chkDcpDehaze { nullptr };
    QCheckBox* m_chkStabilizer { nullptr };
    QCheckBox* m_chkWhiteBalance { nullptr };
    QCheckBox* m_chkChromaticAberration { nullptr };
    QCheckBox* m_chkLapHaze { nullptr };
    QCheckBox* m_chkClahe { nullptr };
    QCheckBox* m_chkDenoise { nullptr };
    QCheckBox* m_chkSharpen { nullptr };
    QCheckBox* m_chkEdgeDetect { nullptr };
    // Tactical & Thermal Analytics Controls
    QCheckBox* m_chkIsotherm { nullptr };
    QComboBox* m_comboIsothermPreset { nullptr };
    QCheckBox* m_chkHotspotTracker { nullptr };
    QCheckBox* m_chkMtiMotion { nullptr };
    QCheckBox* m_chkReticleHud { nullptr };
    QComboBox* m_comboReticleStyle { nullptr };
    // Motion & Target Tracking Controls
    QCheckBox* m_chkOpticalFlow { nullptr };
    QComboBox* m_comboFlowMode { nullptr };
    QCheckBox* m_chkTargetLock { nullptr };
    QCheckBox* m_chkAutoFollowPtz { nullptr };
    QCheckBox* m_chkTripwire { nullptr };
    QComboBox* m_comboTripwireDir { nullptr };
    QCheckBox* m_chkHeatmap { nullptr };
    QTimer* m_autoFollowTimer { nullptr };
    std::unique_ptr<PelcoD::PtzAutoTracker> m_autoTracker;
    std::shared_ptr<PelcoD::Video::CentroidTargetTrackerFilter> m_targetTracker;
    // Privacy & Operational Overlays Controls
    QCheckBox* m_chkPrivacyMask { nullptr };
    QComboBox* m_comboPrivacyMode { nullptr };
    QCheckBox* m_chkForensicWatermark { nullptr };
    QCheckBox* m_chkTelemetryOsd { nullptr };
    QCheckBox* m_chkPictureInPicture { nullptr };
    QComboBox* m_comboPipMode { nullptr };
#endif

    // Quick PTZ Controls
    QSlider* m_speedSlider { nullptr };
    QLabel* m_speedLabel { nullptr };

    bool m_isFullscreen { false };
    QWidget* m_fullscreenContainer { nullptr };
};

} // namespace PelcoDApp
