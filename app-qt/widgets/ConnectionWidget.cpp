/// @file ConnectionWidget.cpp
/// @brief Implementation of transport connection toolbar widget.

#include "ConnectionWidget.h"
#include "MockPelcoDDevice.h"
#include "SerialTransport.h"
#include "TcpTransport.h"

#include <QDir>
#include <QStyle>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace PelcoDApp {

ConnectionWidget::ConnectionWidget(QWidget* parent)
    : QWidget(parent)
{
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
    const QStringList bauds { "2400", "4800", "9600", "19200", "38400", "57600", "115200" };
    cmbBaudRate->addItems(bauds);
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

    mainLayout->addWidget(stackedConfig);

    // Device Address (1 to 255)
    auto* lblAddress = new QLabel(tr("Address:"), this);
    spinAddress = new QSpinBox(this);
    spinAddress->setRange(1, 255);
    spinAddress->setValue(1);
    spinAddress->setToolTip(tr("Pelco-D Device Address (1 - 255)"));

    mainLayout->addWidget(lblAddress);
    mainLayout->addWidget(spinAddress);

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

    // Connections
    connect(cmbMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConnectionWidget::handleModeChanged);
    connect(btnRefreshPorts, &QPushButton::clicked, this, &ConnectionWidget::refreshSerialPorts);
    connect(btnConnect, &QPushButton::clicked, this, &ConnectionWidget::handleConnectClicked);
}

void ConnectionWidget::handleModeChanged(int index)
{
    stackedConfig->setCurrentIndex(index);
}

void ConnectionWidget::refreshSerialPorts()
{
    cmbSerialPort->clear();

#ifdef _WIN32
    for (int i = 1; i <= 32; ++i) {
        const std::string portName = "\\\\.\\COM" + std::to_string(i);
        HANDLE hComm
            = ::CreateFileA(portName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hComm != INVALID_HANDLE_VALUE) {
            ::CloseHandle(hComm);
            const QString comName = QString("COM%1").arg(i);
            cmbSerialPort->addItem(comName, comName);
        }
    }
    if (cmbSerialPort->count() == 0) {
        cmbSerialPort->addItem("COM1", "COM1");
        cmbSerialPort->addItem("COM3", "COM3");
    }
#else
    QDir devDir("/dev");
    QStringList filters;
    filters << "ttyUSB*"
            << "ttyACM*"
            << "ttyS*";
    const auto entries = devDir.entryList(filters, QDir::System);
    for (const auto& entry : entries) {
        cmbSerialPort->addItem("/dev/" + entry, "/dev/" + entry);
    }

    if (cmbSerialPort->count() == 0) {
        cmbSerialPort->addItem("/dev/ttyUSB0", "/dev/ttyUSB0");
        cmbSerialPort->addItem("/dev/ttyS0", "/dev/ttyS0");
    }
#endif
}

void ConnectionWidget::handleConnectClicked()
{
    if (isConnected) {
        emit disconnectRequested();
        return;
    }

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
    }

    if (transport) {
        emit connectRequested(transport, address);
    }
}

void ConnectionWidget::setConnectionState(bool connected)
{
    isConnected = connected;
    updateLedState(connected);

    if (connected) {
        btnConnect->setText(tr("Disconnect"));
        btnConnect->setObjectName("btnDanger");
        cmbMode->setEnabled(false);
        spinAddress->setEnabled(false);
        stackedConfig->setEnabled(false);
    } else {
        btnConnect->setText(tr("Connect"));
        btnConnect->setObjectName("btnPrimary");
        cmbMode->setEnabled(true);
        spinAddress->setEnabled(true);
        stackedConfig->setEnabled(true);
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
        // Re-enable inputs; actual connected/offline state is set by setConnectionState().
        btnConnect->setEnabled(true);
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

} // namespace PelcoDApp
