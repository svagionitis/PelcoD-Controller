/// @file MainWindow.cpp
/// @brief Implementation of top-level application window for Sony VISCA camera controller.

#include "MainWindow.h"

#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

namespace ViscaApp {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_device(new QViscaSonyDevice(nullptr, 1, this))
{
    setupUi();
    setupConnections();
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("VISCA Sony Camera Controller - FCB-EV9520L & FCB-EW9500H"));
    resize(1180, 840);

    // Top Connection Toolbar
    auto* toolBar = addToolBar(tr("Connection"));
    toolBar->setMovable(false);
    m_connectionWidget = new ViscaConnectionWidget(this);
    toolBar->addWidget(m_connectionWidget);

    // Central Tab Widget
    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    auto* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(8, 8, 8, 8);

    m_tabWidget = new QTabWidget(centralWidget);

    m_lensTab = new LensOpticsTab(m_device, this);
    m_exposureTab = new ExposureImagingTab(m_device, this);
    m_registersTab = new RegistersHardwareTab(m_device, this);
    m_diagnosticsTab = new SystemDiagnosticsTab(m_device, this);

    m_tabWidget->addTab(m_lensTab, tr("Lens & Optics"));
    m_tabWidget->addTab(m_exposureTab, tr("Exposure & Imaging"));
    m_tabWidget->addTab(m_registersTab, tr("Hardware & Registers"));
    m_tabWidget->addTab(m_diagnosticsTab, tr("Diagnostics & Maintenance"));

    centralLayout->addWidget(m_tabWidget);
    setCentralWidget(centralWidget);

    // Dockable Traffic Inspector
    m_dockInspector = new QDockWidget(tr("Live Protocol Traffic Inspector"), this);
    m_dockInspector->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
    m_inspectorWidget = new ViscaTrafficInspectorWidget(m_dockInspector);
    m_dockInspector->setWidget(m_inspectorWidget);
    m_dockInspector->setMinimumHeight(220);
    addDockWidget(Qt::BottomDockWidgetArea, m_dockInspector);

    // Status bar
    statusBar()->showMessage(tr("Ready. Configure transport mode and press Connect."));
}

void MainWindow::setupConnections()
{
    // Toolbar connect/disconnect signals
    connect(m_connectionWidget, &ViscaConnectionWidget::connectRequested, this, &MainWindow::handleConnect);
    connect(m_connectionWidget, &ViscaConnectionWidget::disconnectRequested, this, &MainWindow::handleDisconnect);

    // Device events
    connect(m_device, &QViscaSonyDevice::connectionStateChanged, m_connectionWidget,
        &ViscaConnectionWidget::setConnectionState);
    connect(
        m_device, &QViscaSonyDevice::connectingStateChanged, m_connectionWidget, &ViscaConnectionWidget::setConnecting);
    connect(m_device, &QViscaSonyDevice::modelDiscovered, this, &MainWindow::handleModelDiscovered);
    connect(m_device, &QViscaSonyDevice::trafficLogged, m_inspectorWidget, &ViscaTrafficInspectorWidget::logFrame);
    connect(m_device, &QViscaSonyDevice::commandFailed, this, &MainWindow::handleCommandFailed);

    // Traffic Inspector injection
    connect(
        m_inspectorWidget, &ViscaTrafficInspectorWidget::sendRawHexRequested, m_device, &QViscaSonyDevice::sendRawHex);
}

void MainWindow::handleConnect(std::shared_ptr<::Transport::ITransport> transport, uint8_t address)
{
    m_device->setTransport(transport, address);
    m_device->connectDeviceAsync();
}

void MainWindow::handleDisconnect()
{
    m_device->disconnectDevice();
    statusBar()->showMessage(tr("Camera disconnected."));
}

void MainWindow::handleModelDiscovered(
    Visca::Sony::SonyCameraModelType /*modelType*/, const Visca::Sony::CameraCapabilities& caps)
{
    const QString name = QString::fromStdString(caps.modelName);
    m_connectionWidget->setModelBadge(name);
    statusBar()->showMessage(
        tr("Identified camera: %1 (%2)").arg(name).arg(QString::fromStdString(caps.sensorDescription)), 7000);
}

void MainWindow::handleCommandFailed(const QString& reason)
{
    statusBar()->showMessage(tr("VISCA Notice: %1").arg(reason), 5000);
}

} // namespace ViscaApp
