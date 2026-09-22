#pragma once

/// @file SystemDiagnosticsTab.h
/// @brief UI tab displaying real-time telemetry metrics, power control, IF_Clear, and socket cancellation.

#include "../QViscaSonyDevice.h"
#include <SonyViscaTypes.h>

#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

namespace ViscaApp {

/// @class SystemDiagnosticsTab
/// @brief Displays parsed Block Inquiry telemetry (00-04) and system-level interface maintenance tools.
class SystemDiagnosticsTab : public QWidget {
    Q_OBJECT

public:
    explicit SystemDiagnosticsTab(QViscaSonyDevice* device, QWidget* parent = nullptr);
    ~SystemDiagnosticsTab() override = default;

public slots:
    void updateTelemetry(const Visca::Sony::SonyFCBStatus& status);

private slots:
    void handlePowerOn();
    void handlePowerStandby();
    void handleIfClear();
    void handleCancelSocket1();
    void handleCancelSocket2();
    void handlePollNow();

private:
    void setupUi();

    QViscaSonyDevice* m_device { nullptr };

    QTableWidget* tableTelemetry { nullptr };

    QPushButton* btnPowerOn { nullptr };
    QPushButton* btnPowerStandby { nullptr };
    QPushButton* btnIfClear { nullptr };
    QPushButton* btnCancelSock1 { nullptr };
    QPushButton* btnCancelSock2 { nullptr };
    QPushButton* btnPollNow { nullptr };
};

} // namespace ViscaApp
