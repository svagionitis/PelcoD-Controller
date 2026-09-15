/// @file MainWindow.cpp
/// @brief Implementation of main application window.

#include "MainWindow.h"

#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

namespace PelcoDApp {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_device { new PelcoDQt::QPelcoDDevice(nullptr, 1U, this) }
{
    setupUi();
    setupConnections();
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("Pelco-D Device Controller - PTZ & Gimbal Command Dashboard"));
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

    m_videoTab = new VideoStreamTab(m_device, this);
    m_ptzTab = new PtzControlTab(m_device, this);
    m_presetsTab = new PresetsTab(m_device, this);
    m_settingsTab = new DeviceSettingsTab(m_device, this);
    m_auxZonesTab = new AuxZonesTab(m_device, this);
    m_osdTab = new OsdScreenTab(m_device, this);
    m_systemTab = new SystemTab(m_device, this);
    m_fujinonTab = new FujinonSX800Tab(m_device, this);

    m_tabWidget->addTab(m_videoTab, tr("Video Stream & HUD"));
    m_tabWidget->addTab(m_ptzTab, tr("PTZ Motion"));
    m_tabWidget->addTab(m_presetsTab, tr("Presets"));
    m_tabWidget->addTab(m_settingsTab, tr("Device Settings"));
    m_tabWidget->addTab(m_auxZonesTab, tr("Aux & Zones"));
    m_tabWidget->addTab(m_osdTab, tr("OSD Display"));
    m_tabWidget->addTab(m_systemTab, tr("System Diagnostics"));
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
    connect(m_device, &PelcoDQt::QPelcoDDevice::statusUpdated, this, &MainWindow::handleStatusUpdated);
    connect(m_device, &PelcoDQt::QPelcoDDevice::fujinonStatusUpdated, this, &MainWindow::handleFujinonStatusUpdated);
    connect(m_device, &PelcoDQt::QPelcoDDevice::trafficLogged, m_inspectorWidget, &TrafficInspectorWidget::logFrame);
    connect(m_device, &PelcoDQt::QPelcoDDevice::connectionStateChanged, m_connectionWidget,
        &ConnectionWidget::setConnectionState);
    connect(m_device, &PelcoDQt::QPelcoDDevice::connectingStateChanged, m_connectionWidget,
        &ConnectionWidget::setConnecting);

    // Raw hex injection from inspector
    connect(m_inspectorWidget, &TrafficInspectorWidget::sendRawHexRequested, m_device,
        &PelcoDQt::QPelcoDDevice::sendRawHex);

    connect(m_device, &PelcoDQt::QPelcoDDevice::connectionStateChanged, this, [this](bool connected) {
        if (connected) {
            statusBar()->showMessage(tr("Connected to Pelco-D Device #%1").arg(m_device->coreDevice() ? 1 : 0));
        } else {
            statusBar()->showMessage(tr("Connection failed"));
        }
    });

    // Query timeout notification
    connect(m_device, &PelcoDQt::QPelcoDDevice::queryTimeoutOccurred, this,
        [this](const QString& tag) { statusBar()->showMessage(tr("Warning: Query '%1' timed out").arg(tag), 3000); });
}

void MainWindow::handleConnect(std::shared_ptr<PelcoD::ITransport> transport, std::uint8_t address)
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

void MainWindow::handleStatusUpdated(const PelcoD::DeviceStatus& status)
{
    if (m_videoTab) {
        m_videoTab->handleDeviceStatusUpdated(status);
    }
    m_ptzTab->updateTelemetry(status);
    m_systemTab->updateStatus(status);
}

void MainWindow::handleFujinonStatusUpdated(const PelcoD::FujinonStatus& status)
{
    if (m_videoTab) {
        m_videoTab->handleFujinonStatusUpdated(status);
    }
    if (m_fujinonTab) {
        m_fujinonTab->updateFujinonStatus(status);
    }
}

} // namespace PelcoDApp
