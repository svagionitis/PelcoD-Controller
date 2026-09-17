#include "OnvifServerTab.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

namespace PelcoDApp {

OnvifServerTab::OnvifServerTab(PelcoD::Qt::QOnvifServer* server, PelcoDQt::QPelcoDDevice* device, QWidget* parent)
    : QWidget(parent)
    , m_server(server)
    , m_device(device)
{
    setupUi();
    setupConnections();
    syncUiFromConfig();
}

void OnvifServerTab::setupUi()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(8);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    // =========================================================================
    // Left Scrollable Panel: Service Control & Configuration
    // =========================================================================
    auto* leftScroll = new QScrollArea(splitter);
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);

    auto* leftContainer = new QWidget(leftScroll);
    auto* leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(6, 6, 6, 6);
    leftLayout->setSpacing(10);

    // Group 1: Service State & Control
    auto* grpControl = new QGroupBox(tr("Service State & Endpoint"), leftContainer);
    auto* ctrlLayout = new QVBoxLayout(grpControl);
    ctrlLayout->setSpacing(8);

    auto* statusRow = new QHBoxLayout();
    statusRow->addWidget(new QLabel(tr("Status:"), grpControl));
    m_statusBadge = new QLabel(tr("● STOPPED"), grpControl);
    m_statusBadge->setStyleSheet("color: #F44336; font-weight: bold; font-size: 13px;");
    statusRow->addWidget(m_statusBadge);
    statusRow->addStretch();
    ctrlLayout->addLayout(statusRow);

    auto* urlRow = new QHBoxLayout();
    urlRow->addWidget(new QLabel(tr("Endpoint:"), grpControl));
    m_endpointEdit = new QLineEdit(grpControl);
    m_endpointEdit->setReadOnly(true);
    m_endpointEdit->setPlaceholderText(QStringLiteral("http://127.0.0.1:8080/onvif/device_service"));
    m_btnCopyEndpoint = new QPushButton(tr("Copy"), grpControl);
    m_btnCopyEndpoint->setToolTip(tr("Copy ONVIF endpoint URL to clipboard"));
    urlRow->addWidget(m_endpointEdit, 1);
    urlRow->addWidget(m_btnCopyEndpoint);
    ctrlLayout->addLayout(urlRow);

    auto* btnRow = new QHBoxLayout();
    m_btnStart = new QPushButton(tr("Start Server"), grpControl);
    m_btnStart->setStyleSheet("background-color: #2E7D32; color: white; font-weight: bold; padding: 6px 14px;");
    m_btnStop = new QPushButton(tr("Stop Server"), grpControl);
    m_btnStop->setStyleSheet("background-color: #C62828; color: white; font-weight: bold; padding: 6px 14px;");
    m_btnStop->setEnabled(false);
    m_btnRestart = new QPushButton(tr("Restart"), grpControl);
    m_btnRestart->setEnabled(false);
    btnRow->addWidget(m_btnStart);
    btnRow->addWidget(m_btnStop);
    btnRow->addWidget(m_btnRestart);
    ctrlLayout->addLayout(btnRow);

    leftLayout->addWidget(grpControl);

    // Group 2: Network & WS-Discovery
    auto* grpNetwork = new QGroupBox(tr("Network & WS-Discovery"), leftContainer);
    auto* netForm = new QFormLayout(grpNetwork);
    netForm->setSpacing(8);

    m_spinPort = new QSpinBox(grpNetwork);
    m_spinPort->setRange(1, 65535);
    m_spinPort->setValue(8080);
    netForm->addRow(tr("HTTP Port:"), m_spinPort);

    m_editBindAddress = new QLineEdit(grpNetwork);
    m_editBindAddress->setText(QStringLiteral("0.0.0.0"));
    m_editBindAddress->setPlaceholderText(tr("0.0.0.0 (all interfaces) or specific IP"));
    netForm->addRow(tr("Bind Address:"), m_editBindAddress);

    m_chkDiscovery = new QCheckBox(tr("Enable WS-Discovery Multicast (UDP 3702)"), grpNetwork);
    m_chkDiscovery->setChecked(true);
    netForm->addRow(QString(), m_chkDiscovery);

    leftLayout->addWidget(grpNetwork);

    // Group 3: Device Identification
    auto* grpIdentity = new QGroupBox(tr("Device Information (Profile S & T)"), leftContainer);
    auto* identForm = new QFormLayout(grpIdentity);
    identForm->setSpacing(8);

    m_editDeviceName = new QLineEdit(grpIdentity);
    identForm->addRow(tr("Device Name:"), m_editDeviceName);

    m_editManufacturer = new QLineEdit(grpIdentity);
    identForm->addRow(tr("Manufacturer:"), m_editManufacturer);

    m_editModel = new QLineEdit(grpIdentity);
    identForm->addRow(tr("Model:"), m_editModel);

    m_editFirmware = new QLineEdit(grpIdentity);
    identForm->addRow(tr("Firmware:"), m_editFirmware);

    m_editSerial = new QLineEdit(grpIdentity);
    identForm->addRow(tr("Serial Number:"), m_editSerial);

    leftLayout->addWidget(grpIdentity);

    // Group 4: Media Routing & PTZ Hardware Bridge
    auto* grpBridge = new QGroupBox(tr("Media Routing & Pelco-D PTZ Bridge"), leftContainer);
    auto* bridgeForm = new QFormLayout(grpBridge);
    bridgeForm->setSpacing(8);

    m_editRtspUri = new QLineEdit(grpBridge);
    m_editRtspUri->setPlaceholderText(QStringLiteral("rtsp://127.0.0.1:8554/live"));
    bridgeForm->addRow(tr("RTSP Stream URI:"), m_editRtspUri);

    m_chkBridgePtz = new QCheckBox(tr("Forward ONVIF PTZ commands to Pelco-D camera"), grpBridge);
    m_chkBridgePtz->setChecked(true);
    bridgeForm->addRow(QString(), m_chkBridgePtz);

    m_spinPelcoDAddress = new QSpinBox(grpBridge);
    m_spinPelcoDAddress->setRange(1, 254);
    m_spinPelcoDAddress->setValue(1);
    bridgeForm->addRow(tr("Pelco-D Camera ID:"), m_spinPelcoDAddress);

    leftLayout->addWidget(grpBridge);

    m_btnApplyConfig = new QPushButton(tr("Apply Configuration Changes"), leftContainer);
    m_btnApplyConfig->setStyleSheet("font-weight: bold; padding: 6px 14px;");
    leftLayout->addWidget(m_btnApplyConfig);
    leftLayout->addStretch();

    leftScroll->setWidget(leftContainer);
    splitter->addWidget(leftScroll);

    // =========================================================================
    // Right Panel: Real-time SOAP Activity Log
    // =========================================================================
    auto* rightPanel = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(6, 6, 6, 6);
    rightLayout->setSpacing(6);

    auto* logHeader = new QHBoxLayout();
    logHeader->addWidget(new QLabel(tr("Incoming ONVIF SOAP Transactions:"), rightPanel));
    logHeader->addStretch();
    m_lblRequestCount = new QLabel(tr("Requests: 0"), rightPanel);
    m_lblRequestCount->setStyleSheet("font-weight: bold; color: #58A6FF;");
    logHeader->addWidget(m_lblRequestCount);
    m_btnClearLog = new QPushButton(tr("Clear"), rightPanel);
    logHeader->addWidget(m_btnClearLog);
    rightLayout->addLayout(logHeader);

    m_logTable = new QTableWidget(0, 4, rightPanel);
    m_logTable->setHorizontalHeaderLabels({ tr("Time"), tr("Client IP"), tr("Service"), tr("Action / Operation") });
    m_logTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_logTable->verticalHeader()->setVisible(false);
    rightLayout->addWidget(m_logTable, 1);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    outerLayout->addWidget(splitter);
}

void OnvifServerTab::setupConnections()
{
    connect(m_btnStart, &QPushButton::clicked, this, &OnvifServerTab::onStartClicked);
    connect(m_btnStop, &QPushButton::clicked, this, &OnvifServerTab::onStopClicked);
    connect(m_btnRestart, &QPushButton::clicked, this, &OnvifServerTab::onRestartClicked);
    connect(m_btnApplyConfig, &QPushButton::clicked, this, &OnvifServerTab::onApplyConfigClicked);
    connect(m_btnCopyEndpoint, &QPushButton::clicked, this, &OnvifServerTab::onCopyEndpointClicked);
    connect(m_btnClearLog, &QPushButton::clicked, this, &OnvifServerTab::onClearLogClicked);

    if (m_server != nullptr) {
        connect(m_server, &PelcoD::Qt::QOnvifServer::serverStarted, this, &OnvifServerTab::onServerStarted);
        connect(m_server, &PelcoD::Qt::QOnvifServer::serverStopped, this, &OnvifServerTab::onServerStopped);
        connect(m_server, &PelcoD::Qt::QOnvifServer::errorOccurred, this, &OnvifServerTab::onErrorOccurred);
        connect(m_server, &PelcoD::Qt::QOnvifServer::requestLogged, this, &OnvifServerTab::onRequestLogged);
    }
}

void OnvifServerTab::syncUiFromConfig()
{
    if (m_server == nullptr) {
        return;
    }
    const auto cfg = m_server->config();
    m_spinPort->setValue(cfg.port);
    m_editBindAddress->setText(QString::fromStdString(cfg.bindAddress));
    m_editDeviceName->setText(QString::fromStdString(cfg.deviceName));
    m_editManufacturer->setText(QString::fromStdString(cfg.manufacturer));
    m_editModel->setText(QString::fromStdString(cfg.model));
    m_editFirmware->setText(QString::fromStdString(cfg.firmwareVersion));
    m_editSerial->setText(QString::fromStdString(cfg.serialNumber));
    m_editRtspUri->setText(QString::fromStdString(cfg.rtspStreamUri));
    m_endpointEdit->setText(m_server->endpointUrl());
}

void OnvifServerTab::applyUiToConfig()
{
    if (m_server == nullptr) {
        return;
    }
    auto cfg = m_server->config();
    cfg.port = m_spinPort->value();
    cfg.bindAddress = m_editBindAddress->text().trimmed().toStdString();
    cfg.deviceName = m_editDeviceName->text().trimmed().toStdString();
    cfg.manufacturer = m_editManufacturer->text().trimmed().toStdString();
    cfg.model = m_editModel->text().trimmed().toStdString();
    cfg.firmwareVersion = m_editFirmware->text().trimmed().toStdString();
    cfg.serialNumber = m_editSerial->text().trimmed().toStdString();
    cfg.rtspStreamUri = m_editRtspUri->text().trimmed().toStdString();

    m_server->setConfig(cfg);

    if (m_chkBridgePtz->isChecked() && m_device != nullptr) {
        m_server->bindDevice(m_device);
    } else {
        m_server->bindDevice(nullptr);
    }

    m_endpointEdit->setText(m_server->endpointUrl());
}

void OnvifServerTab::onStartClicked()
{
    if (m_server == nullptr) {
        return;
    }
    applyUiToConfig();
    m_server->start();
}

void OnvifServerTab::onStopClicked()
{
    if (m_server != nullptr) {
        m_server->stop();
    }
}

void OnvifServerTab::onRestartClicked()
{
    if (m_server == nullptr) {
        return;
    }
    applyUiToConfig();
    m_server->restart();
}

void OnvifServerTab::onApplyConfigClicked()
{
    applyUiToConfig();
    QMessageBox::information(this, tr("Configuration Applied"),
        tr("ONVIF server configuration has been updated.\n(If the server is currently running, restart it to take "
           "effect)."));
}

void OnvifServerTab::onServerStarted(const QString& endpointUrl)
{
    m_statusBadge->setText(tr("● RUNNING"));
    m_statusBadge->setStyleSheet("color: #4CAF50; font-weight: bold; font-size: 13px;");
    m_endpointEdit->setText(endpointUrl);
    m_btnStart->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_btnRestart->setEnabled(true);
}

void OnvifServerTab::onServerStopped()
{
    m_statusBadge->setText(tr("● STOPPED"));
    m_statusBadge->setStyleSheet("color: #F44336; font-weight: bold; font-size: 13px;");
    m_btnStart->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_btnRestart->setEnabled(false);
}

void OnvifServerTab::onErrorOccurred(const QString& error)
{
    QMessageBox::warning(this, tr("ONVIF Server Error"), error);
}

void OnvifServerTab::onRequestLogged(const QString& service, const QString& action, const QString& clientIp)
{
    ++m_requestCount;
    m_lblRequestCount->setText(tr("Requests: %1").arg(m_requestCount));

    const int row = m_logTable->rowCount();
    m_logTable->insertRow(row);

    const QString timeStr = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
    m_logTable->setItem(row, 0, new QTableWidgetItem(timeStr));
    m_logTable->setItem(row, 1, new QTableWidgetItem(clientIp));
    m_logTable->setItem(row, 2, new QTableWidgetItem(service));
    m_logTable->setItem(row, 3, new QTableWidgetItem(action));

    // Auto-scroll to bottom
    m_logTable->scrollToBottom();

    // Limit maximum rows
    if (m_logTable->rowCount() > 200) {
        m_logTable->removeRow(0);
    }
}

void OnvifServerTab::onClearLogClicked()
{
    m_logTable->setRowCount(0);
    m_requestCount = 0;
    m_lblRequestCount->setText(tr("Requests: 0"));
}

void OnvifServerTab::onCopyEndpointClicked()
{
    QApplication::clipboard()->setText(m_endpointEdit->text());
}

} // namespace PelcoDApp
