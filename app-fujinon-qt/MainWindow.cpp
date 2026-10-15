/// @file MainWindow.cpp
/// @brief Implementation of main application window for Fujinon SX800 controller.

#include "MainWindow.h"

#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

namespace FujinonApp {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_device { new PelcoDQt::QPelcoDFujinonDevice(nullptr, 1U, this) }
{
    setupUi();
    setupConnections();
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("Fujinon SX800 Optical Camera Controller - Optics & Command Dashboard"));
    resize(1150, 800);

    // Top Connection Toolbar
    auto* toolBar = addToolBar(tr("Connection"));
    toolBar->setMovable(false);
    m_connectionWidget = new ConnectionWidget(this);
    toolBar->addWidget(m_connectionWidget);

    // Central Tab Widget
    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    auto* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(8, 8, 8, 8);

    m_tabWidget = new QTabWidget(centralWidget);

    m_fujinonTab = new FujinonSX800Tab(m_device, this);
    m_tabWidget->addTab(m_fujinonTab, tr("Fujinon SX800"));

    centralLayout->addWidget(m_tabWidget);
    setCentralWidget(centralWidget);

    // Dockable Traffic Inspector
    m_dockInspector = new QDockWidget(tr("Live Protocol Traffic Inspector"), this);
    m_dockInspector->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
    m_inspectorWidget = new TrafficInspectorWidget(m_dockInspector);
    m_dockInspector->setWidget(m_inspectorWidget);
    m_dockInspector->setMinimumHeight(200);
    addDockWidget(Qt::BottomDockWidgetArea, m_dockInspector);

    // Status bar
    statusBar()->showMessage(tr("Ready. Select transport mode and press Connect."));
}

void MainWindow::setupConnections()
{
    // Toolbar connect/disconnect signals
    connect(m_connectionWidget, &ConnectionWidget::connectRequested, this, &MainWindow::handleConnect);
    connect(m_connectionWidget, &ConnectionWidget::disconnectRequested, this, &MainWindow::handleDisconnect);

    // Device events
    connect(m_device, &PelcoDQt::QPelcoDFujinonDevice::statusUpdated, this, &MainWindow::handleStatusUpdated);
    connect(
        m_device, &PelcoDQt::QPelcoDFujinonDevice::fujinonStatusUpdated, this, &MainWindow::handleFujinonStatusUpdated);
    connect(
        m_device, &PelcoDQt::QPelcoDFujinonDevice::trafficLogged, m_inspectorWidget, &TrafficInspectorWidget::logFrame);
    connect(m_device, &PelcoDQt::QPelcoDFujinonDevice::connectionStateChanged, m_connectionWidget,
        &ConnectionWidget::setConnectionState);
    connect(m_device, &PelcoDQt::QPelcoDFujinonDevice::connectingStateChanged, m_connectionWidget,
        &ConnectionWidget::setConnecting);

    // Raw hex injection from inspector
    connect(m_inspectorWidget, &TrafficInspectorWidget::sendRawHexRequested, m_device,
        &PelcoDQt::QPelcoDFujinonDevice::sendRawHex);

    connect(m_device, &PelcoDQt::QPelcoDFujinonDevice::connectionStateChanged, this, [this](bool connected) {
        if (connected) {
            statusBar()->showMessage(tr("Connected to Fujinon SX800 Device"));
        } else {
            statusBar()->showMessage(tr("Connection failed / disconnected"));
        }
    });

    // Query timeout notification
    connect(m_device, &PelcoDQt::QPelcoDFujinonDevice::queryTimeoutOccurred, this,
        [this](const QString& tag) { statusBar()->showMessage(tr("Warning: Query '%1' timed out").arg(tag), 3000); });
}

void MainWindow::handleConnect(std::shared_ptr<Transport::ITransport> transport, std::uint8_t address)
{
    m_device->setTransport(std::move(transport), address);
    statusBar()->showMessage(tr("Connecting…"));
    m_device->connectDeviceAsync();
}

void MainWindow::handleDisconnect()
{
    m_device->disconnectDevice();
    statusBar()->showMessage(tr("Disconnected"));
}

void MainWindow::handleStatusUpdated(const PelcoD::DeviceStatus& /*status*/)
{
    if (m_inspectorWidget && m_device) {
        m_inspectorWidget->updateTransportStats(m_device->transportStats(), m_device->protocolStats());
    }
}

void MainWindow::handleFujinonStatusUpdated(const PelcoD::FujinonStatus& status)
{
    if (m_fujinonTab) {
        m_fujinonTab->updateFujinonStatus(status);
    }
}

} // namespace FujinonApp
