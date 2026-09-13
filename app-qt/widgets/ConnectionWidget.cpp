/// @file ConnectionWidget.cpp
/// @brief UI widget for configuring and initiating Pelco-D device transport connections.

#include "ConnectionWidget.h"

#include "MockPelcoDDevice.h"
#include "SerialTransport.h"
#include "TcpTransport.h"
#include "UdpTransport.h"

#include <QCheckBox>
#include <QCollator>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>

#include <algorithm>
#include <string>

namespace PelcoDApp {

ConnectionWidget::ConnectionWidget(QWidget* parent)
    : QWidget(parent)
{
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ConnectionWidget::onReconnectTimerTimeout);

    setupUi();
    refreshSerialPorts();
    updateLedState(false);
}

void ConnectionWidget::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // Mode Selector
    auto* lblMode = new QLabel(tr("Mode:"), this);
    cmbMode = new QComboBox(this);
    cmbMode->addItem(tr("Mock Device (Offline)"), 0);
    cmbMode->addItem(tr("Serial Port (RS-485)"), 1);
    cmbMode->addItem(tr("TCP Socket Bridge"), 2);
    cmbMode->addItem(tr("UDP/IP Datagram"), 3);

    mainLayout->addWidget(lblMode);
    mainLayout->addWidget(cmbMode);

    // Stacked Configuration Pages
    stackedConfig = new QStackedWidget(this);

    // Page 0: Mock
    pageMock = new QWidget(this);
    auto* mockLayout = new QHBoxLayout(pageMock);
    mockLayout->setContentsMargins(0, 0, 0, 0);
    auto* lblMockDesc = new QLabel(tr("Simulating virtual Pelco-D device"), pageMock);
    lblMockDesc->setStyleSheet("color: #7ee787; font-style: italic;");
    mockLayout->addWidget(lblMockDesc);
    mockLayout->addStretch();
    stackedConfig->addWidget(pageMock);

    // Page 1: Serial
    pageSerial = new QWidget(this);
    auto* serialLayout = new QHBoxLayout(pageSerial);
    serialLayout->setContentsMargins(0, 0, 0, 0);
    serialLayout->setSpacing(6);

    auto* lblPort = new QLabel(tr("Port:"), pageSerial);
    cmbSerialPort = new QComboBox(pageSerial);
    cmbSerialPort->setEditable(true);

    btnRefreshPorts = new QPushButton(tr("Scan"), pageSerial);
    btnRefreshPorts->setToolTip(tr("Scan system for serial ports"));

    auto* lblBaud = new QLabel(tr("Baud:"), pageSerial);
    cmbBaudRate = new QComboBox(pageSerial);
    for (const auto baud : PelcoD::SerialTransport::StandardBaudRates) {
        cmbBaudRate->addItem(QString::number(baud));
    }
    cmbBaudRate->setCurrentText("2400");

    serialLayout->addWidget(lblPort);
    serialLayout->addWidget(cmbSerialPort);
    serialLayout->addWidget(btnRefreshPorts);
    serialLayout->addWidget(lblBaud);
    serialLayout->addWidget(cmbBaudRate);
    stackedConfig->addWidget(pageSerial);

    // Page 2: TCP
    pageTcp = new QWidget(this);
    auto* tcpLayout = new QHBoxLayout(pageTcp);
    tcpLayout->setContentsMargins(0, 0, 0, 0);
    tcpLayout->setSpacing(6);

    auto* lblHost = new QLabel(tr("Host:"), pageTcp);
    editTcpHost = new QLineEdit("192.168.1.100", pageTcp);
    editTcpHost->setFixedWidth(110);

    auto* lblTcpPort = new QLabel(tr("Port:"), pageTcp);
    spinTcpPort = new QSpinBox(pageTcp);
    spinTcpPort->setRange(1, 65535);
    spinTcpPort->setValue(4001);

    tcpLayout->addWidget(lblHost);
    tcpLayout->addWidget(editTcpHost);
    tcpLayout->addWidget(lblTcpPort);
    tcpLayout->addWidget(spinTcpPort);
    stackedConfig->addWidget(pageTcp);

    // Page 3: UDP
    pageUdp = new QWidget(this);
    auto* udpLayout = new QHBoxLayout(pageUdp);
    udpLayout->setContentsMargins(0, 0, 0, 0);
    udpLayout->setSpacing(6);

    auto* lblUdpHost = new QLabel(tr("Host:"), pageUdp);
    editUdpHost = new QLineEdit("192.168.1.100", pageUdp);
    editUdpHost->setFixedWidth(110);

    auto* lblUdpPort = new QLabel(tr("Port:"), pageUdp);
    spinUdpPort = new QSpinBox(pageUdp);
    spinUdpPort->setRange(1, 65535);
    spinUdpPort->setValue(4001);

    auto* lblUdpLocalPort = new QLabel(tr("Local Port:"), pageUdp);
    spinUdpLocalPort = new QSpinBox(pageUdp);
    spinUdpLocalPort->setRange(0, 65535);
    spinUdpLocalPort->setValue(0);
    spinUdpLocalPort->setSpecialValueText(tr("Auto"));
    spinUdpLocalPort->setToolTip(tr("Local UDP binding port (0 = OS dynamic allocation)"));

    udpLayout->addWidget(lblUdpHost);
    udpLayout->addWidget(editUdpHost);
    udpLayout->addWidget(lblUdpPort);
    udpLayout->addWidget(spinUdpPort);
    udpLayout->addWidget(lblUdpLocalPort);
    udpLayout->addWidget(spinUdpLocalPort);
    stackedConfig->addWidget(pageUdp);

    mainLayout->addWidget(stackedConfig);

    // Device Address (1 to 255)
    auto* lblAddress = new QLabel(tr("Address:"), this);
    spinAddress = new QSpinBox(this);
    spinAddress->setRange(1, 255);
    spinAddress->setValue(1);
    spinAddress->setToolTip(tr("Pelco-D Device Address (1 - 255)"));

    mainLayout->addWidget(lblAddress);
    mainLayout->addWidget(spinAddress);

    // Auto-Reconnect Checkbox
    chkAutoReconnect = new QCheckBox(tr("Auto-Reconnect"), this);
    chkAutoReconnect->setToolTip(tr("Automatically retry connection with exponential backoff (1s, 2s, 5s, 10s)"));
    mainLayout->addWidget(chkAutoReconnect);

    // Connect / Disconnect Button
    btnConnect = new QPushButton(tr("Connect"), this);
    btnConnect->setObjectName("btnPrimary");
    btnConnect->setMinimumWidth(85);
    mainLayout->addWidget(btnConnect);

    // Status LED and Label
    lblLed = new QLabel(this);
    lblLed->setFixedSize(12, 12);
    lblStatusText = new QLabel(tr("Disconnected"), this);

    mainLayout->addWidget(lblLed);
    mainLayout->addWidget(lblStatusText);
    mainLayout->addStretch();

    // Reconnect Timer setup
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ConnectionWidget::onReconnectTimerTimeout);

    connect(chkAutoReconnect, &QCheckBox::toggled, this, [this](bool checked) {
        if (!checked) {
            stopAutoReconnect();
            if (!isConnected) {
                btnConnect->setText(tr("Connect"));
                btnConnect->setObjectName("btnPrimary");
                btnConnect->style()->unpolish(btnConnect);
                btnConnect->style()->polish(btnConnect);
                cmbMode->setEnabled(true);
                spinAddress->setEnabled(true);
                stackedConfig->setEnabled(true);
                lblStatusText->setText(tr("Offline"));
                lblStatusText->setStyleSheet("color: #8b949e;");
                lblLed->setStyleSheet("background-color: #f85149; border-radius: 6px; border: 1px solid #da3633;");
            }
        }
    });

    // Connections
    connect(cmbMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConnectionWidget::handleModeChanged);
    connect(btnRefreshPorts, &QPushButton::clicked, this, &ConnectionWidget::refreshSerialPorts);
    connect(btnConnect, &QPushButton::clicked, this, &ConnectionWidget::handleConnectClicked);
}

void ConnectionWidget::handleModeChanged(int index)
{
    stackedConfig->setCurrentIndex(index);
}

QStringList ConnectionWidget::displayedSerialPorts() const
{
    QStringList result;
    if (!cmbSerialPort) {
        return result;
    }
    for (int i = 0; i < cmbSerialPort->count(); ++i) {
        result.append(cmbSerialPort->itemText(i));
    }
    return result;
}

QStringList ConnectionWidget::enumerateSerialPorts()
{
    QStringList ports;
    for (const auto& port : PelcoD::SerialTransport::enumeratePorts()) {
        ports.append(QString::fromStdString(port));
    }
    return ports;
}

void ConnectionWidget::refreshSerialPorts()
{
    const QString previousSelection = cmbSerialPort ? cmbSerialPort->currentText() : QString();
    cmbSerialPort->clear();

    const QStringList ports = enumerateSerialPorts();
    for (const QString& port : ports) {
        cmbSerialPort->addItem(port, port);
    }

    if (cmbSerialPort->count() == 0) {
        cmbSerialPort->addItem(tr("No serial ports detected"), "");
    } else if (!previousSelection.isEmpty()) {
        const int idx = cmbSerialPort->findText(previousSelection);
        if (idx >= 0) {
            cmbSerialPort->setCurrentIndex(idx);
        }
    }
}

void ConnectionWidget::handleConnectClicked()
{
    if (m_reconnectTimer && m_reconnectTimer->isActive()) {
        // User canceled pending auto-reconnect
        m_manualDisconnect = true;
        stopAutoReconnect();
        setConnectionState(false);
        return;
    }

    if (isConnected) {
        m_manualDisconnect = true;
        stopAutoReconnect();
        emit disconnectRequested();
        return;
    }

    m_manualDisconnect = false;
    m_reconnectAttempt = 0;
    triggerConnect();
}

void ConnectionWidget::triggerConnect()
{
    const auto address = static_cast<std::uint8_t>(spinAddress->value());
    const int mode = cmbMode->currentIndex();
    std::shared_ptr<PelcoD::ITransport> transport { nullptr };

    if (mode == 0) {
        // Mock Simulator
        transport = std::make_shared<PelcoD::MockPelcoDDevice>(address);
    } else if (mode == 1) {
        // Serial Port
        QString portPath = cmbSerialPort->currentData().toString();
        if (portPath.isEmpty()) {
            portPath = cmbSerialPort->currentText();
        }
        const qint32 baud = cmbBaudRate->currentText().toInt();
        transport = std::make_shared<PelcoD::SerialTransport>(portPath.toStdString(), static_cast<std::uint32_t>(baud));
    } else if (mode == 2) {
        // TCP Socket
        const QString host = editTcpHost->text();
        const auto port = static_cast<quint16>(spinTcpPort->value());
        transport = std::make_shared<PelcoD::TcpTransport>(host.toStdString(), static_cast<std::uint16_t>(port));
    } else if (mode == 3) {
        // UDP Socket
        const QString host = editUdpHost->text();
        const auto port = static_cast<quint16>(spinUdpPort->value());
        const auto localPort = static_cast<quint16>(spinUdpLocalPort->value());
        transport = std::make_shared<PelcoD::UdpTransport>(
            host.toStdString(), static_cast<std::uint16_t>(port), static_cast<std::uint16_t>(localPort));
    }

    if (transport) {
        emit connectRequested(transport, address);
    }
}

void ConnectionWidget::scheduleReconnect()
{
    if (!chkAutoReconnect || !chkAutoReconnect->isChecked() || m_manualDisconnect) {
        return;
    }

    const int delayMs = kBackoffScheduleMs[std::min(m_reconnectAttempt, kMaxBackoffSteps - 1)];
    const int attemptDisplay = m_reconnectAttempt + 1;
    m_reconnectTimer->start(delayMs);

    lblLed->setStyleSheet("background-color: #e3b341; border-radius: 6px; border: 1px solid #f0c040;");
    lblStatusText->setText(tr("Reconnecting in %1s (attempt %2)…").arg(delayMs / 1000).arg(attemptDisplay));
    lblStatusText->setStyleSheet("color: #e3b341; font-weight: bold;");

    btnConnect->setText(tr("Cancel"));
    btnConnect->setEnabled(true);
    btnConnect->setObjectName("btnWarning");
    btnConnect->style()->unpolish(btnConnect);
    btnConnect->style()->polish(btnConnect);

    cmbMode->setEnabled(false);
    spinAddress->setEnabled(false);
    stackedConfig->setEnabled(false);
}

void ConnectionWidget::onReconnectTimerTimeout()
{
    if (m_manualDisconnect || !chkAutoReconnect || !chkAutoReconnect->isChecked()) {
        return;
    }
    m_reconnectAttempt++;
    triggerConnect();
}

void ConnectionWidget::stopAutoReconnect()
{
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    m_reconnectAttempt = 0;
}

void ConnectionWidget::setConnectionState(bool connected)
{
    isConnected = connected;
    updateLedState(connected);

    if (connected) {
        stopAutoReconnect();
        m_manualDisconnect = false;
        btnConnect->setText(tr("Disconnect"));
        btnConnect->setObjectName("btnDanger");
        cmbMode->setEnabled(false);
        spinAddress->setEnabled(false);
        stackedConfig->setEnabled(false);
    } else {
        if (!m_manualDisconnect && chkAutoReconnect && chkAutoReconnect->isChecked()) {
            scheduleReconnect();
        } else {
            stopAutoReconnect();
            btnConnect->setText(tr("Connect"));
            btnConnect->setObjectName("btnPrimary");
            cmbMode->setEnabled(true);
            spinAddress->setEnabled(true);
            stackedConfig->setEnabled(true);
        }
    }

    btnConnect->style()->unpolish(btnConnect);
    btnConnect->style()->polish(btnConnect);
}

void ConnectionWidget::setConnecting(bool connecting)
{
    if (connecting) {
        btnConnect->setEnabled(false);
        btnConnect->setText(tr("Connecting…"));
        cmbMode->setEnabled(false);
        spinAddress->setEnabled(false);
        stackedConfig->setEnabled(false);
        lblLed->setStyleSheet("background-color: #e3b341; border-radius: 6px; border: 1px solid #f0c040;");
        lblStatusText->setText(tr("Connecting…"));
        lblStatusText->setStyleSheet("color: #e3b341; font-weight: bold;");
    } else {
        if (!isReconnecting()) {
            btnConnect->setEnabled(true);
        }
    }
}

void ConnectionWidget::updateLedState(bool connected)
{
    if (connected) {
        lblLed->setStyleSheet("background-color: #2ea043; border-radius: 6px; border: 1px solid #3fb950;");
        lblStatusText->setText(tr("Online"));
        lblStatusText->setStyleSheet("color: #3fb950; font-weight: bold;");
    } else {
        lblLed->setStyleSheet("background-color: #f85149; border-radius: 6px; border: 1px solid #da3633;");
        lblStatusText->setText(tr("Offline"));
        lblStatusText->setStyleSheet("color: #8b949e;");
    }
}

bool ConnectionWidget::isAutoReconnectEnabled() const noexcept
{
    return chkAutoReconnect && chkAutoReconnect->isChecked();
}

void ConnectionWidget::setAutoReconnectEnabled(bool enabled)
{
    if (chkAutoReconnect) {
        chkAutoReconnect->setChecked(enabled);
    }
}

bool ConnectionWidget::isReconnecting() const noexcept
{
    return m_reconnectTimer && m_reconnectTimer->isActive();
}

} // namespace PelcoDApp
