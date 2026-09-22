/// @file SystemDiagnosticsTab.cpp
/// @brief Implementation of system maintenance and telemetry metrics tab.

#include "SystemDiagnosticsTab.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>

namespace ViscaApp {

SystemDiagnosticsTab::SystemDiagnosticsTab(QViscaSonyDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_device(device)
{
    setupUi();

    if (m_device) {
        connect(m_device, &QViscaSonyDevice::statusUpdated, this, &SystemDiagnosticsTab::updateTelemetry);
    }
}

void SystemDiagnosticsTab::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(16);

    // ==========================================
    // Left Box: Real-Time Telemetry Metrics
    // ==========================================
    auto* grpTelem = new QGroupBox(tr("Real-Time Telemetry (Block Inquiries 00–04)"), this);
    auto* telemLayout = new QVBoxLayout(grpTelem);
    telemLayout->setSpacing(10);

    tableTelemetry = new QTableWidget(this);
    tableTelemetry->setColumnCount(2);
    tableTelemetry->setHorizontalHeaderLabels({ tr("Telemetry Metric"), tr("Value / State") });
    tableTelemetry->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableTelemetry->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tableTelemetry->verticalHeader()->setVisible(false);
    tableTelemetry->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableTelemetry->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableTelemetry->setAlternatingRowColors(true);

    const QStringList metricNames = {
        tr("Camera Power"),
        tr("Zoom Optical Position"),
        tr("Digital Zoom Position"),
        tr("Focus Position"),
        tr("Focus Mode"),
        tr("Exposure Mode"),
        tr("Shutter Speed Code"),
        tr("Iris Aperture Code"),
        tr("Sensor Gain Code"),
        tr("White Balance Mode"),
        tr("Image Stabilizer"),
        tr("Defog"),
        tr("Visibility Enhancer (VE)"),
        tr("IR Cut Filter (Night)"),
        tr("Camera ID"),
    };

    tableTelemetry->setRowCount(static_cast<int>(metricNames.size()));
    for (int i = 0; i < metricNames.size(); ++i) {
        tableTelemetry->setItem(i, 0, new QTableWidgetItem(metricNames[i]));
        tableTelemetry->setItem(i, 1, new QTableWidgetItem(tr("--")));
    }

    telemLayout->addWidget(tableTelemetry);

    btnPollNow = new QPushButton(tr("↻ Poll Status Now"), grpTelem);
    telemLayout->addWidget(btnPollNow);

    mainLayout->addWidget(grpTelem);

    // ==========================================
    // Right Box: Interface Maintenance Tools
    // ==========================================
    auto* grpTools = new QGroupBox(tr("Interface Maintenance & Bus Recovery"), this);
    auto* toolsLayout = new QVBoxLayout(grpTools);
    toolsLayout->setSpacing(14);

    auto* lblDesc = new QLabel(
        tr("Execute raw interface-level VISCA commands to reset stalled sockets or power down the camera sensor."),
        grpTools);
    lblDesc->setWordWrap(true);
    lblDesc->setStyleSheet("color: #8b949e;");
    toolsLayout->addWidget(lblDesc);

    // Power
    auto* rowPower = new QHBoxLayout();
    btnPowerOn = new QPushButton(tr("Camera Power ON"), grpTools);
    btnPowerOn->setObjectName("btnPrimary");
    btnPowerStandby = new QPushButton(tr("Standby / Power OFF"), grpTools);
    rowPower->addWidget(btnPowerOn);
    rowPower->addWidget(btnPowerStandby);
    toolsLayout->addLayout(rowPower);

    // IF_Clear
    btnIfClear = new QPushButton(tr("IF_Clear (Reset VISCA Interface Buffer)"), grpTools);
    btnIfClear->setToolTip(tr("Sends 8x 01 00 01 FF to clear internal command buffers."));
    toolsLayout->addWidget(btnIfClear);

    // Cancel Sockets
    auto* rowCancel = new QHBoxLayout();
    btnCancelSock1 = new QPushButton(tr("Cancel Socket 1 (8x 21 FF)"), grpTools);
    btnCancelSock2 = new QPushButton(tr("Cancel Socket 2 (8x 22 FF)"), grpTools);
    rowCancel->addWidget(btnCancelSock1);
    rowCancel->addWidget(btnCancelSock2);
    toolsLayout->addLayout(rowCancel);

    toolsLayout->addStretch();
    mainLayout->addWidget(grpTools);

    // Wire events
    connect(btnPollNow, &QPushButton::clicked, this, &SystemDiagnosticsTab::handlePollNow);
    connect(btnPowerOn, &QPushButton::clicked, this, &SystemDiagnosticsTab::handlePowerOn);
    connect(btnPowerStandby, &QPushButton::clicked, this, &SystemDiagnosticsTab::handlePowerStandby);
    connect(btnIfClear, &QPushButton::clicked, this, &SystemDiagnosticsTab::handleIfClear);
    connect(btnCancelSock1, &QPushButton::clicked, this, &SystemDiagnosticsTab::handleCancelSocket1);
    connect(btnCancelSock2, &QPushButton::clicked, this, &SystemDiagnosticsTab::handleCancelSocket2);
}

void SystemDiagnosticsTab::handlePowerOn()
{
    if (m_device) {
        m_device->setPower(true);
    }
}

void SystemDiagnosticsTab::handlePowerStandby()
{
    if (m_device) {
        m_device->setPower(false);
    }
}

void SystemDiagnosticsTab::handleIfClear()
{
    if (m_device) {
        m_device->ifClear();
    }
}

void SystemDiagnosticsTab::handleCancelSocket1()
{
    if (m_device) {
        m_device->cancelSocket(1);
    }
}

void SystemDiagnosticsTab::handleCancelSocket2()
{
    if (m_device) {
        m_device->cancelSocket(2);
    }
}

void SystemDiagnosticsTab::handlePollNow()
{
    if (m_device) {
        m_device->pollStatus();
    }
}

void SystemDiagnosticsTab::updateTelemetry(const Visca::Sony::SonyFCBStatus& status)
{
    auto setVal = [this](int row, const QString& val) {
        if (auto* item = tableTelemetry->item(row, 1)) {
            item->setText(val);
        }
    };

    setVal(0, status.powerOn ? tr("ON") : tr("STANDBY"));
    setVal(1, QStringLiteral("0x%1 (%2)").arg(status.zoomPosition, 4, 16, QLatin1Char('0')).arg(status.zoomPosition));
    setVal(2,
        QStringLiteral("0x%1 (%2)")
            .arg(status.digitalZoomPosition, 4, 16, QLatin1Char('0'))
            .arg(status.digitalZoomPosition));
    setVal(3, QStringLiteral("0x%1 (%2)").arg(status.focusPosition, 4, 16, QLatin1Char('0')).arg(status.focusPosition));
    setVal(4, status.focusAuto ? tr("Auto Focus (Active)") : tr("Manual Focus"));
    setVal(5, QString::number(static_cast<int>(status.exposureMode)));
    setVal(6, QStringLiteral("0x%1").arg(status.shutterPosition, 2, 16, QLatin1Char('0')));
    setVal(7, QStringLiteral("0x%1").arg(status.irisPosition, 2, 16, QLatin1Char('0')));
    setVal(8,
        QStringLiteral("0x%1 (+%2 dB)").arg(status.gainPosition, 2, 16, QLatin1Char('0')).arg(status.gainPosition * 3));
    setVal(9, QString::number(static_cast<int>(status.wbMode)));
    setVal(10,
        status.stabilizerOn ? QStringLiteral("ON (Level %1)").arg(static_cast<int>(status.stabilizerLevel))
                            : tr("OFF"));
    setVal(11, status.defogOn ? QStringLiteral("ON (Level %1)").arg(static_cast<int>(status.defogLevel)) : tr("OFF"));
    setVal(12, status.veOn ? tr("ON") : tr("OFF"));
    setVal(13, status.icrOn ? tr("Night (BW)") : tr("Day (Color)"));
    setVal(14, QStringLiteral("0x%1").arg(status.cameraId, 8, 16, QLatin1Char('0')));
}

} // namespace ViscaApp
