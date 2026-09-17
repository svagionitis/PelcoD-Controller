#pragma once

/// @file OnvifServerTab.h
/// @brief Dashboard tab for configuring and monitoring the embedded ONVIF Profile S/T server.

#include "QOnvifServer.h"
#include "QPelcoDDevice.h"

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QWidget>

namespace PelcoDApp {

/// @class OnvifServerTab
/// @brief UI widget providing service controls, network parameters, device metadata, and request logs.
class OnvifServerTab : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructs OnvifServerTab bound to server and device instances.
    /// @param[in] server Pointer to active QOnvifServer service adapter.
    /// @param[in] device Pointer to active QPelcoDDevice controller.
    /// @param[in] parent Optional parent widget.
    explicit OnvifServerTab(
        PelcoD::Qt::QOnvifServer* server, PelcoDQt::QPelcoDDevice* device, QWidget* parent = nullptr);
    ~OnvifServerTab() override = default;

private slots:
    void onStartClicked();
    void onStopClicked();
    void onRestartClicked();
    void onApplyConfigClicked();
    void onServerStarted(const QString& endpointUrl);
    void onServerStopped();
    void onErrorOccurred(const QString& error);
    void onRequestLogged(const QString& service, const QString& action, const QString& clientIp);
    void onClearLogClicked();
    void onCopyEndpointClicked();

private:
    void setupUi();
    void setupConnections();
    void syncUiFromConfig();
    void applyUiToConfig();

    PelcoD::Qt::QOnvifServer* m_server { nullptr };
    PelcoDQt::QPelcoDDevice* m_device { nullptr };

    // Service Controls
    QLabel* m_statusBadge { nullptr };
    QLineEdit* m_endpointEdit { nullptr };
    QPushButton* m_btnCopyEndpoint { nullptr };
    QPushButton* m_btnStart { nullptr };
    QPushButton* m_btnStop { nullptr };
    QPushButton* m_btnRestart { nullptr };

    // Network & Discovery
    QSpinBox* m_spinPort { nullptr };
    QLineEdit* m_editBindAddress { nullptr };
    QCheckBox* m_chkDiscovery { nullptr };

    // Device Identification
    QLineEdit* m_editDeviceName { nullptr };
    QLineEdit* m_editManufacturer { nullptr };
    QLineEdit* m_editModel { nullptr };
    QLineEdit* m_editFirmware { nullptr };
    QLineEdit* m_editSerial { nullptr };

    // Media Routing & PTZ Bridge
    QLineEdit* m_editRtspUri { nullptr };
    QCheckBox* m_chkBridgePtz { nullptr };
    QSpinBox* m_spinPelcoDAddress { nullptr };

    QPushButton* m_btnApplyConfig { nullptr };

    // Activity Log
    QTableWidget* m_logTable { nullptr };
    QPushButton* m_btnClearLog { nullptr };
    QLabel* m_lblRequestCount { nullptr };
    int m_requestCount { 0 };
};

} // namespace PelcoDApp
