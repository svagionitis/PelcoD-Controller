/// @file ViscaConnectionWidget.cpp
/// @brief Implementation of VISCA connection management toolbar widget.

#include "ViscaConnectionWidget.h"
#include "../dialogs/ViscaBusScanDialog.h"
#include <MockSonyCamera.h>
#include <Transport/SerialTransport.h>
#include <Transport/TcpTransport.h>
#include <Transport/UdpTransport.h>

#include <QDir>
#include <QHBoxLayout>
#include <QMessageBox>

#if defined(Q_OS_WIN)
#include <QSettings>
#endif

namespace ViscaApp {

QStringList ViscaConnectionWidget::enumerateSerialPorts()
{
    QStringList ports;

#if defined(Q_OS_WIN)
    QSettings settings("HKEY_LOCAL_MACHINE\\HARDWARE\\DEVICEMAP\\SERIALCOMM", QSettings::NativeFormat);
    const QStringList keys = settings.allKeys();
    for (const QString& key : keys) {
        ports.append(settings.value(key).toString());
    }
#else
    QDir devDir("/dev");
    const QStringList filters = { "ttyUSB*", "ttyACM*", "ttyS*", "rfcomm*" };
    const QFileInfoList entries = devDir.entryInfoList(filters, QDir::System);
    for (const auto& entry : entries) {
        ports.append(entry.absoluteFilePath());
    }
#endif

    ports.sort();
    return ports;
}

ViscaConnectionWidget::ViscaConnectionWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ViscaConnectionWidget::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(4, 2, 4, 2);
    mainLayout->setSpacing(8);

    // Mode Selector
    cmbMode = new QComboBox(this);
    cmbMode->addItem(tr("Serial (RS-232/422)"), 0);
    cmbMode->addItem(tr("TCP / IP"), 1);
    cmbMode->addItem(tr("UDP / IP"), 2);
    cmbMode->addItem(tr("Mock Sony Camera"), 3);
    mainLayout->addWidget(new QLabel(tr("Mode:"), this));
    mainLayout->addWidget(cmbMode);

    // Stacked configuration
    stackedConfig = new QStackedWidget(this);

    // Page 0: Serial
    pageSerial = new QWidget(this);
    auto* serialLayout = new QHBoxLayout(pageSerial);
    serialLayout->setContentsMargins(0, 0, 0, 0);
    serialLayout->setSpacing(6);

    cmbSerialPort = new QComboBox(pageSerial);
    cmbSerialPort->setEditable(true);
    cmbSerialPort->setMinimumWidth(130);

    cmbBaudRate = new QComboBox(pageSerial);
    cmbBaudRate->addItems({ "9600", "19200", "38400", "115200" });
    cmbBaudRate->setCurrentText("9600");

    btnRefreshPorts = new QPushButton(tr("↻"), pageSerial);
    btnRefreshPorts->setToolTip(tr("Refresh Serial Ports"));
    btnRefreshPorts->setFixedWidth(28);

    serialLayout->addWidget(new QLabel(tr("Port:"), pageSerial));
    serialLayout->addWidget(cmbSerialPort);
    serialLayout->addWidget(btnRefreshPorts);
    serialLayout->addWidget(new QLabel(tr("Baud:"), pageSerial));
    serialLayout->addWidget(cmbBaudRate);
    stackedConfig->addWidget(pageSerial);

    // Page 1: TCP
    pageTcp = new QWidget(this);
    auto* tcpLayout = new QHBoxLayout(pageTcp);
    tcpLayout->setContentsMargins(0, 0, 0, 0);
    tcpLayout->setSpacing(6);

    editTcpHost = new QLineEdit("192.168.0.100", pageTcp);
    editTcpHost->setMinimumWidth(110);
    spinTcpPort = new QSpinBox(pageTcp);
    spinTcpPort->setRange(1, 65535);
    spinTcpPort->setValue(52381);

    tcpLayout->addWidget(new QLabel(tr("Host:"), pageTcp));
    tcpLayout->addWidget(editTcpHost);
    tcpLayout->addWidget(new QLabel(tr("Port:"), pageTcp));
    tcpLayout->addWidget(spinTcpPort);
    stackedConfig->addWidget(pageTcp);

    // Page 2: UDP
    pageUdp = new QWidget(this);
    auto* udpLayout = new QHBoxLayout(pageUdp);
    udpLayout->setContentsMargins(0, 0, 0, 0);
    udpLayout->setSpacing(6);

    editUdpHost = new QLineEdit("192.168.0.100", pageUdp);
    editUdpHost->setMinimumWidth(110);
    spinUdpPort = new QSpinBox(pageUdp);
    spinUdpPort->setRange(1, 65535);
    spinUdpPort->setValue(52381);

    udpLayout->addWidget(new QLabel(tr("Host:"), pageUdp));
    udpLayout->addWidget(editUdpHost);
    udpLayout->addWidget(new QLabel(tr("Port:"), pageUdp));
    udpLayout->addWidget(spinUdpPort);
    stackedConfig->addWidget(pageUdp);

    // Page 3: Mock Sony Camera
    pageMock = new QWidget(this);
    auto* mockLayout = new QHBoxLayout(pageMock);
    mockLayout->setContentsMargins(0, 0, 0, 0);
    mockLayout->setSpacing(6);

    cmbMockModel = new QComboBox(pageMock);
    cmbMockModel->addItem(tr("Sony FCB-EV9520L (STARVIS 2 FHD)"), 0);
    cmbMockModel->addItem(tr("Sony FCB-EW9500H (STARVIS 4K)"), 1);

    mockLayout->addWidget(new QLabel(tr("Simulate Model:"), pageMock));
    mockLayout->addWidget(cmbMockModel);
    stackedConfig->addWidget(pageMock);

    mainLayout->addWidget(stackedConfig);

    // Camera Address
    mainLayout->addWidget(new QLabel(tr("Camera Addr:"), this));
    spinAddress = new QSpinBox(this);
    spinAddress->setRange(1, 7);
    spinAddress->setValue(1);
    mainLayout->addWidget(spinAddress);

    // Scan Bus button
    btnScanBus = new QPushButton(tr("Scan Daisy Chain..."), this);
    mainLayout->addWidget(btnScanBus);

    // Connect button
    btnConnect = new QPushButton(tr("Connect"), this);
    btnConnect->setObjectName("btnConnect");
    btnConnect->setFixedWidth(100);
    mainLayout->addWidget(btnConnect);

    // Status LED & text
    lblLed = new QLabel(this);
    lblLed->setFixedSize(14, 14);
    lblLed->setStyleSheet("background-color: #8b949e; border-radius: 7px;");
    mainLayout->addWidget(lblLed);

    lblStatusText = new QLabel(tr("Disconnected"), this);
    lblStatusText->setStyleSheet("color: #8b949e; font-weight: bold;");
    mainLayout->addWidget(lblStatusText);

    mainLayout->addStretch();

    // Wire events
    connect(
        cmbMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViscaConnectionWidget::handleModeChanged);
    connect(btnRefreshPorts, &QPushButton::clicked, this, &ViscaConnectionWidget::refreshSerialPorts);
    connect(btnConnect, &QPushButton::clicked, this, &ViscaConnectionWidget::handleConnectClicked);
    connect(btnScanBus, &QPushButton::clicked, this, &ViscaConnectionWidget::handleScanBus);

    refreshSerialPorts();
    handleModeChanged(0);
}

void ViscaConnectionWidget::refreshSerialPorts()
{
    const QString current = cmbSerialPort->currentText();
    cmbSerialPort->clear();

    const QStringList ports = enumerateSerialPorts();
    cmbSerialPort->addItems(ports);

    if (!current.isEmpty()) {
        const int idx = cmbSerialPort->findText(current);
        if (idx >= 0) {
            cmbSerialPort->setCurrentIndex(idx);
        } else {
            cmbSerialPort->setEditText(current);
        }
    } else if (!ports.isEmpty()) {
        cmbSerialPort->setCurrentIndex(0);
    }
}

uint8_t ViscaConnectionWidget::selectedAddress() const noexcept
{
    return static_cast<uint8_t>(spinAddress->value());
}

void ViscaConnectionWidget::setSelectedAddress(uint8_t address)
{
    if (address >= 1 && address <= 7) {
        spinAddress->setValue(address);
    }
}

void ViscaConnectionWidget::handleModeChanged(int index)
{
    stackedConfig->setCurrentIndex(index);
}

void ViscaConnectionWidget::handleConnectClicked()
{
    if (m_connected) {
        emit disconnectRequested();
    } else {
        auto transport = createConfiguredTransport();
        if (!transport) {
            QMessageBox::warning(this, tr("Connection Error"), tr("Unable to configure transport."));
            return;
        }
        emit connectRequested(transport, selectedAddress());
    }
}

std::shared_ptr<::Transport::ITransport> ViscaConnectionWidget::createConfiguredTransport() const
{
    const int mode = cmbMode->currentIndex();

    if (mode == 0) {
        // Serial
        const QString portName = cmbSerialPort->currentText().trimmed();
        const uint32_t baud = cmbBaudRate->currentText().toUInt();
        if (portName.isEmpty()) {
            return nullptr;
        }
        return std::make_shared<::Transport::SerialTransport>(portName.toStdString(), baud);
    }

    if (mode == 1) {
        // TCP
        const QString host = editTcpHost->text().trimmed();
        const uint16_t port = static_cast<uint16_t>(spinTcpPort->value());
        return std::make_shared<::Transport::TcpTransport>(host.toStdString(), port);
    }

    if (mode == 2) {
        // UDP
        const QString host = editUdpHost->text().trimmed();
        const uint16_t port = static_cast<uint16_t>(spinUdpPort->value());
        return std::make_shared<::Transport::UdpTransport>(host.toStdString(), port, 0);
    }

    if (mode == 3) {
        // Mock Sony Camera
        const auto model = (cmbMockModel->currentIndex() == 1) ? Visca::Sony::SonyCameraModelType::FCB_EW9500H
                                                               : Visca::Sony::SonyCameraModelType::FCB_EV9520L;
        return std::make_shared<Visca::Sony::MockSonyCamera>(model, selectedAddress());
    }

    return nullptr;
}

void ViscaConnectionWidget::handleScanBus()
{
    auto transport = createConfiguredTransport();
    if (!transport) {
        QMessageBox::warning(this, tr("Scan Bus"), tr("Please select and configure a valid transport first."));
        return;
    }

    ViscaBusScanDialog dlg(transport, this);
    connect(&dlg, &ViscaBusScanDialog::cameraSelected, this,
        [this](uint8_t address, const QString& /*model*/) { setSelectedAddress(address); });
    dlg.exec();
}

void ViscaConnectionWidget::setConnectionState(bool connected)
{
    m_connected = connected;
    updateLedState(connected);

    btnConnect->setText(connected ? tr("Disconnect") : tr("Connect"));
    btnConnect->setEnabled(true);
    cmbMode->setEnabled(!connected);
    stackedConfig->setEnabled(!connected);
    spinAddress->setEnabled(!connected);
    btnScanBus->setEnabled(!connected);
}

void ViscaConnectionWidget::setConnecting(bool connecting)
{
    if (connecting) {
        btnConnect->setEnabled(false);
        lblStatusText->setText(tr("Connecting..."));
        lblLed->setStyleSheet("background-color: #d29922; border-radius: 7px;");
    }
}

void ViscaConnectionWidget::setModelBadge(const QString& modelName)
{
    if (m_connected) {
        lblStatusText->setText(tr("Connected (%1)").arg(modelName));
    }
}

void ViscaConnectionWidget::updateLedState(bool connected)
{
    if (connected) {
        lblLed->setStyleSheet("background-color: #3fb950; border-radius: 7px;");
        lblStatusText->setText(tr("Connected"));
        lblStatusText->setStyleSheet("color: #3fb950; font-weight: bold;");
    } else {
        lblLed->setStyleSheet("background-color: #f85149; border-radius: 7px;");
        lblStatusText->setText(tr("Disconnected"));
        lblStatusText->setStyleSheet("color: #8b949e; font-weight: bold;");
    }
}

} // namespace ViscaApp
