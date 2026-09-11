#pragma once

/// @file SystemTab.h
/// @brief Device queries, telemetry polling settings, and system maintenance tab.

#include "DeviceStatus.h"
#include "QPelcoDDevice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace PelcoDApp {

/// @class SystemTab
/// @brief UI tab for interrogating device status, configuring polling, and triggering resets.
class SystemTab : public QWidget {
    Q_OBJECT

public:
    explicit SystemTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent = nullptr);
    ~SystemTab() override = default;

public slots:
    void updateStatus(const PelcoD::DeviceStatus& status);

private slots:
    void handlePollingToggled(bool checked);
    void handlePollIntervalChanged(int val);

private:
    void setupUi();

    PelcoDQt::QPelcoDDevice* m_device { nullptr };

    // Query buttons
    QPushButton* btnQueryAll { nullptr };
    QPushButton* btnQueryPan { nullptr };
    QPushButton* btnQueryTilt { nullptr };
    QPushButton* btnQueryZoom { nullptr };
    QPushButton* btnQueryMag { nullptr };
    QPushButton* btnQueryDevType { nullptr };
    QPushButton* btnQueryGeneral { nullptr };
    QPushButton* btnQueryDiagnostics { nullptr };

    // Telemetry Polling
    QCheckBox* chkPolling { nullptr };
    QSpinBox* spinInterval { nullptr };

    // Maintenance
    QPushButton* btnRemoteReset { nullptr };
    QPushButton* btnResetDefaults { nullptr };
    QComboBox* cmbBaudRate { nullptr };
    QPushButton* btnSetBaudRate { nullptr };

    // Info Labels
    QLabel* lblModelName { nullptr };
    QLabel* lblSwType { nullptr };
    QLabel* lblHwType { nullptr };
    QLabel* lblAlarms { nullptr };
    QLabel* lblMagnification { nullptr };
    QLabel* lblDiagTemp { nullptr };
    QLabel* lblDiagSensorId { nullptr };
};

} // namespace PelcoDApp
