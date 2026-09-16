#pragma once

/// @file OnvifCameraTab.h
/// @brief Dashboard tab for discovering, connecting to, and controlling ONVIF Profile S IP cameras.

#include "QOnvifDevice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace PelcoDApp {

class VideoStreamTab;

/// @class OnvifCameraTab
/// @brief Dashboard tab providing ONVIF discovery, profile selection, PTZ, and preset management.
class OnvifCameraTab : public QWidget {
    Q_OBJECT

public:
    explicit OnvifCameraTab(PelcoD::Qt::QOnvifDevice* onvifDevice, VideoStreamTab* videoTab = nullptr,
        QWidget* parent = nullptr);
    ~OnvifCameraTab() override = default;

signals:
    void streamUriSelected(const QString& rtspUri);

private slots:
    // Discovery & Connection
    void handleStartDiscovery();
    void handleDiscoveryFinished(const QList<PelcoD::Onvif::DiscoveredDevice>& devices);
    void handleSelectDiscovered(int index);
    void handleConnect();
    void handleDisconnect();
    void handleDeviceConnected(const QString& endpoint, const QString& model);
    void handleDeviceDisconnected();
    void handleErrorOccurred(const QString& message);

    // Profiles & Video
    void handleProfileSelected(int index);
    void handleStreamUriResolved(const QString& uri);
    void handleSnapshotUriResolved(const QString& uri);
    void handleSendToVideoTab();
    void handleCopyRtsp();

    // Motion & Presets
    void handleContinuousPanTilt(double pan, double tilt);
    void handleContinuousZoom(int dir);
    void handleStopMotion();
    void handleRelativeMove();
    void handleGotoHome();
    void handleSetHome();
    void handleRefreshPresets();
    void handleGotoPreset();
    void handleSavePreset();
    void handleDeletePreset();
    void handlePresetsUpdated(const std::vector<PelcoD::Onvif::PtzPreset>& presets);
    void handleStatusUpdated(const PelcoD::Onvif::PtzStatus& status);

    // Maintenance
    void handleRebootCamera();
    void handleRebootCompleted(bool success);

private:
    void setupUi();
    void updateConnectionUi(bool connected);

    PelcoD::Qt::QOnvifDevice* m_onvifDevice { nullptr };
    VideoStreamTab* m_videoTab { nullptr };
    QList<PelcoD::Onvif::DiscoveredDevice> m_discoveredList {};

    // Discovery & Connection widgets
    QPushButton* btnDiscover { nullptr };
    QComboBox* cmbDiscovered { nullptr };
    QLineEdit* editEndpoint { nullptr };
    QLineEdit* editUsername { nullptr };
    QLineEdit* editPassword { nullptr };
    QPushButton* btnConnect { nullptr };
    QPushButton* btnDisconnect { nullptr };
    QLabel* lblConnectionStatus { nullptr };

    // Device Information widgets
    QLabel* lblManufacturer { nullptr };
    QLabel* lblModel { nullptr };
    QLabel* lblFirmware { nullptr };
    QLabel* lblSerial { nullptr };
    QLabel* lblHardwareId { nullptr };

    // Media & Streaming widgets
    QComboBox* cmbProfiles { nullptr };
    QLineEdit* editRtspUri { nullptr };
    QPushButton* btnCopyRtsp { nullptr };
    QPushButton* btnStreamInVideoTab { nullptr };
    QLineEdit* editSnapshotUri { nullptr };

    // PTZ Controls
    QSlider* sliderSpeed { nullptr };
    QLabel* lblSpeedVal { nullptr };
    QDoubleSpinBox* spinRelPan { nullptr };
    QDoubleSpinBox* spinRelTilt { nullptr };
    QDoubleSpinBox* spinRelZoom { nullptr };
    QPushButton* btnRelMove { nullptr };
    QPushButton* btnGotoHome { nullptr };
    QPushButton* btnSetHome { nullptr };
    QLabel* lblTelemetryPanTilt { nullptr };
    QLabel* lblTelemetryZoom { nullptr };
    QLabel* lblTelemetryMoving { nullptr };

    // Presets
    QTableWidget* tablePresets { nullptr };
    QLineEdit* editPresetName { nullptr };
    QPushButton* btnRefreshPresets { nullptr };
    QPushButton* btnGotoPreset { nullptr };
    QPushButton* btnSavePreset { nullptr };
    QPushButton* btnDeletePreset { nullptr };

    // Maintenance
    QPushButton* btnReboot { nullptr };
};

} // namespace PelcoDApp
