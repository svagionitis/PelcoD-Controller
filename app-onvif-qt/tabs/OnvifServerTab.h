#pragma once

/// @file OnvifServerTab.h
/// @brief Dashboard tab for configuring and monitoring the embedded ONVIF Profile S/T server.

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QTableWidget>
#include <QWidget>

#include "QOnvifServer.h"
#include "QPelcoDDevice.h"

namespace OnvifApp {

/// @class OnvifServerTab
/// @brief UI widget providing service controls, network parameters, device metadata, and request logs.
class OnvifServerTab : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructs OnvifServerTab bound to server and device instances.
    /// @param[in] server Pointer to active QOnvifServer service adapter.
    /// @param[in] device Pointer to active QPelcoDDevice controller (optional).
    /// @param[in] parent Optional parent widget.
    explicit OnvifServerTab(
        PelcoD::Qt::QOnvifServer* server, PelcoDQt::QPelcoDDevice* device = nullptr, QWidget* parent = nullptr);
    ~OnvifServerTab() override = default;

private slots:
    /// @brief Applies UI configuration and starts the ONVIF server.
    void onStartClicked();

    /// @brief Stops the running ONVIF server.
    void onStopClicked();

    /// @brief Applies UI configuration and restarts the ONVIF server.
    void onRestartClicked();

    /// @brief Persists UI fields into the QOnvifServer configuration.
    void onApplyConfigClicked();

    /// @brief Updates status badge and controls when server starts listening.
    /// @param[in] endpointUrl Active service endpoint URL.
    void onServerStarted(const QString& endpointUrl);

    /// @brief Updates status badge and controls when server stops.
    void onServerStopped();

    /// @brief Displays a warning dialog when a server error occurs.
    /// @param[in] error Diagnostic error description.
    void onErrorOccurred(const QString& error);

    /// @brief Appends an incoming SOAP transaction entry to the activity table.
    /// @param[in] service Target ONVIF service name.
    /// @param[in] action SOAP action/operation name.
    /// @param[in] clientIp Remote client IP address.
    void onRequestLogged(const QString& service, const QString& action, const QString& clientIp);

    /// @brief Clears the SOAP transaction log table and resets the counter.
    void onClearLogClicked();

    /// @brief Copies the advertised ONVIF endpoint URL to the system clipboard.
    void onCopyEndpointClicked();

private:
    /// @brief Builds the splitter layout, configuration forms, and log table.
    void setupUi();

    /// @brief Connects UI buttons and QOnvifServer signals.
    void setupConnections();

    /// @brief Populates UI fields from the current QOnvifServer configuration.
    void syncUiFromConfig();

    /// @brief Reads UI fields and updates QOnvifServer and Pelco-D bridge settings.
    void applyUiToConfig();

    /// @brief Updates the provisioning status badge based on configured credentials.
    void updateProvisioningBadge();

    PelcoD::Qt::QOnvifServer* m_server { nullptr };
    PelcoDQt::QPelcoDDevice* m_device { nullptr };

    // Service Controls
    QLabel* m_statusBadge { nullptr };
    QLabel* m_provBadge { nullptr };
    QLineEdit* m_endpointEdit { nullptr };
    QPushButton* m_btnCopyEndpoint { nullptr };
    QPushButton* m_btnStart { nullptr };
    QPushButton* m_btnStop { nullptr };
    QPushButton* m_btnRestart { nullptr };

    // Network & Discovery
    QSpinBox* m_spinPort { nullptr };
    QLineEdit* m_editBindAddress { nullptr };
    QCheckBox* m_chkDiscovery { nullptr };

    // Authentication (seeded Administrator account)
    QLineEdit* m_editAdminUser { nullptr };
    QLineEdit* m_editAdminPass { nullptr };

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

} // namespace OnvifApp
