/// @file SystemTab.cpp
/// @brief Implementation of device diagnostic queries and polling tab.

#include "SystemTab.h"
#include "app-qt/dialogs/RttProfilerDialog.h"

namespace PelcoDApp {

SystemTab::SystemTab(PelcoDQt::QPelcoDDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device { device }
{
    setupUi();
}

void SystemTab::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(16);

    // Group 1: Device Information Display
    auto* grpInfo = new QGroupBox(tr("Device Information & Health"), this);
    auto* gridInfo = new QGridLayout(grpInfo);

    gridInfo->addWidget(new QLabel(tr("Model / Identification:")), 0, 0);
    lblModelName = new QLabel(tr("Unknown (Query Required)"));
    lblModelName->setObjectName("lblTelemetry");
    gridInfo->addWidget(lblModelName, 0, 1);

    gridInfo->addWidget(new QLabel(tr("Software Version:")), 1, 0);
    lblSwType = new QLabel("--");
    lblSwType->setObjectName("lblTelemetry");
    gridInfo->addWidget(lblSwType, 1, 1);

    gridInfo->addWidget(new QLabel(tr("Hardware Revision:")), 2, 0);
    lblHwType = new QLabel("--");
    lblHwType->setObjectName("lblTelemetry");
    gridInfo->addWidget(lblHwType, 2, 1);

    gridInfo->addWidget(new QLabel(tr("Active Alarms Byte:")), 3, 0);
    lblAlarms = new QLabel("0x00 (No Alarms)");
    lblAlarms->setObjectName("lblTelemetry");
    gridInfo->addWidget(lblAlarms, 3, 1);

    gridInfo->addWidget(new QLabel(tr("Magnification (raw):")), 4, 0);
    lblMagnification = new QLabel("--");
    lblMagnification->setObjectName("lblTelemetry");
    gridInfo->addWidget(lblMagnification, 4, 1);

    gridInfo->addWidget(new QLabel(tr("Diagnostic Temp (raw):")), 0, 2);
    lblDiagTemp = new QLabel("--");
    lblDiagTemp->setObjectName("lblTelemetry");
    gridInfo->addWidget(lblDiagTemp, 0, 3);

    gridInfo->addWidget(new QLabel(tr("Diagnostic Sensor ID:")), 1, 2);
    lblDiagSensorId = new QLabel("--");
    lblDiagSensorId->setObjectName("lblTelemetry");
    gridInfo->addWidget(lblDiagSensorId, 1, 3);

    mainLayout->addWidget(grpInfo);

    // Group 2: Diagnostic Queries
    auto* grpQuery = new QGroupBox(tr("Telemetry & Status Queries"), this);
    auto* layoutQuery = new QHBoxLayout(grpQuery);

    btnQueryAll = new QPushButton(tr("Query All"));
    btnQueryAll->setObjectName("btnPrimary");
    btnQueryPan = new QPushButton(tr("Query Pan"));
    btnQueryTilt = new QPushButton(tr("Query Tilt"));
    btnQueryZoom = new QPushButton(tr("Query Zoom"));
    btnQueryMag = new QPushButton(tr("Query Magnification"));
    btnQueryDevType = new QPushButton(tr("Query Device Type"));
    btnQueryGeneral = new QPushButton(tr("Query General Info"));
    btnQueryDiagnostics = new QPushButton(tr("Query Diagnostics"));

    layoutQuery->addWidget(btnQueryAll);
    layoutQuery->addWidget(btnQueryPan);
    layoutQuery->addWidget(btnQueryTilt);
    layoutQuery->addWidget(btnQueryZoom);
    layoutQuery->addWidget(btnQueryMag);
    layoutQuery->addWidget(btnQueryDevType);
    layoutQuery->addWidget(btnQueryGeneral);
    layoutQuery->addWidget(btnQueryDiagnostics);
    layoutQuery->addStretch();

    mainLayout->addWidget(grpQuery);

    // Group 3: Continuous Polling
    auto* grpPoll = new QGroupBox(tr("Automatic Telemetry Polling"), this);
    auto* layoutPoll = new QHBoxLayout(grpPoll);

    chkPolling = new QCheckBox(tr("Enable Background Polling (Pan/Tilt/Zoom)"));
    layoutPoll->addWidget(chkPolling);

    layoutPoll->addWidget(new QLabel(tr("Polling Interval (ms):")));
    spinInterval = new QSpinBox();
    spinInterval->setRange(100, 10000);
    spinInterval->setValue(1000);
    layoutPoll->addWidget(spinInterval);

    btnLaunchProfiler = new QPushButton(tr("RTT & Jitter Profiler..."));
    layoutPoll->addWidget(btnLaunchProfiler);
    layoutPoll->addStretch();

    mainLayout->addWidget(grpPoll);

    // Group 4: Device Maintenance & Resets
    auto* grpMaint = new QGroupBox(tr("Device Maintenance"), this);
    auto* layoutMaint = new QHBoxLayout(grpMaint);

    btnRemoteReset = new QPushButton(tr("Remote Reboot / Reset"));
    btnRemoteReset->setObjectName("btnDanger");
    btnResetDefaults = new QPushButton(tr("Reset Camera Defaults"));

    // Baud rate setter
    layoutMaint->addWidget(new QLabel(tr("Set Remote Baud Rate:")));
    cmbBaudRate = new QComboBox();
    cmbBaudRate->addItem("2400", 2400);
    cmbBaudRate->addItem("4800", 4800);
    cmbBaudRate->addItem("9600", 9600);
    cmbBaudRate->addItem("19200", 19200);
    cmbBaudRate->addItem("38400", 38400);
    cmbBaudRate->addItem("115200", 115200);
    cmbBaudRate->setCurrentIndex(2); // default 9600
    btnSetBaudRate = new QPushButton(tr("Apply Baud Rate"));

    layoutMaint->addWidget(btnRemoteReset);
    layoutMaint->addWidget(btnResetDefaults);
    layoutMaint->addWidget(cmbBaudRate);
    layoutMaint->addWidget(btnSetBaudRate);
    layoutMaint->addStretch();

    mainLayout->addWidget(grpMaint);
    mainLayout->addStretch();

    // Signal Connections
    connect(btnQueryAll, &QPushButton::clicked, m_device, &PelcoDQt::QPelcoDDevice::queryAll);
    connect(btnQueryPan, &QPushButton::clicked, m_device, &PelcoDQt::QPelcoDDevice::queryPan);
    connect(btnQueryTilt, &QPushButton::clicked, m_device, &PelcoDQt::QPelcoDDevice::queryTilt);
    connect(btnQueryZoom, &QPushButton::clicked, m_device, &PelcoDQt::QPelcoDDevice::queryZoom);
    connect(btnQueryMag, &QPushButton::clicked, m_device, &PelcoDQt::QPelcoDDevice::queryMagnification);
    connect(btnQueryDevType, &QPushButton::clicked, m_device, &PelcoDQt::QPelcoDDevice::queryDeviceType);
    connect(btnQueryGeneral, &QPushButton::clicked, m_device, &PelcoDQt::QPelcoDDevice::queryGeneral);
    connect(btnQueryDiagnostics, &QPushButton::clicked, m_device, &PelcoDQt::QPelcoDDevice::queryDiagnostics);

    connect(chkPolling, &QCheckBox::toggled, this, &SystemTab::handlePollingToggled);
    connect(spinInterval, QOverload<int>::of(&QSpinBox::valueChanged), this, &SystemTab::handlePollIntervalChanged);

    connect(btnRemoteReset, &QPushButton::clicked, m_device, &PelcoDQt::QPelcoDDevice::remoteReset);
    connect(btnResetDefaults, &QPushButton::clicked, m_device, &PelcoDQt::QPelcoDDevice::resetDefaults);
    connect(btnSetBaudRate, &QPushButton::clicked, this,
        [this] { m_device->setBaudRate(cmbBaudRate->currentData().toInt()); });
    connect(btnLaunchProfiler, &QPushButton::clicked, this, &SystemTab::handleLaunchProfiler);
}

void SystemTab::handleLaunchProfiler()
{
    if (!m_device) {
        return;
    }
    auto coreDev = m_device->sharedCoreDevice();
    if (!coreDev) {
        return;
    }
    RttProfilerDialog dialog(coreDev, this);
    dialog.exec();
}

void SystemTab::handlePollingToggled(bool checked)
{
    m_device->setTelemetryPolling(checked, spinInterval->value());
}

void SystemTab::handlePollIntervalChanged(int val)
{
    if (chkPolling->isChecked()) {
        m_device->setTelemetryPolling(true, val);
    }
}

void SystemTab::updateStatus(const PelcoD::DeviceStatus& status)
{
    const auto info = m_device->deviceInfo();
    if (!info.modelName.empty()) {
        lblModelName->setText(QString::fromStdString(info.modelName));
    }
    if (info.softwareType != 0U) {
        lblSwType->setText(QString("0x%1").arg(info.softwareType, 2, 16, QLatin1Char('0')));
    }
    if (info.hardwareType != 0U) {
        lblHwType->setText(QString("0x%1").arg(info.hardwareType, 2, 16, QLatin1Char('0')));
    }

    if (status.alarms != 0U) {
        lblAlarms->setText(QString("0x%1 (Active)").arg(status.alarms, 2, 16, QLatin1Char('0')));
        lblAlarms->setStyleSheet("color: #f85149; font-weight: bold;");
    } else {
        lblAlarms->setText("0x00 (Clear)");
        lblAlarms->setStyleSheet("color: #7ee787;");
    }

    if (status.magnification != 0U) {
        lblMagnification->setText(QString::number(status.magnification));
    }
    if (status.diagnosticTemp != 0U) {
        lblDiagTemp->setText(QString::number(status.diagnosticTemp));
    }
    if (status.diagnosticSensorId != 0U) {
        lblDiagSensorId->setText(QString("0x%1").arg(status.diagnosticSensorId, 2, 16, QLatin1Char('0')));
    }
}

} // namespace PelcoDApp
