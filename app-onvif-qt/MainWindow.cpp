/// @file MainWindow.cpp
/// @brief Implementation of main application window for ONVIF control suite.

#include "MainWindow.h"

#include <cstdint>

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace OnvifApp {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_onvifDevice { new PelcoD::Qt::QOnvifDevice(this) }
    , m_onvifServer { new PelcoD::Qt::QOnvifServer({}, this) }
    , m_pelcoDevice { new PelcoDQt::QPelcoDDevice(nullptr, static_cast<std::uint8_t>(1U), this) }
{
    setupUi();
    setupConnections();
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("ONVIF Device Manager & Server - Profile S/T/G Control Suite"));
    resize(1200, 850);

    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName(QStringLiteral("centralWidget"));
    auto* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(8, 8, 8, 8);

    m_tabWidget = new QTabWidget(centralWidget);

    m_cameraTab = new OnvifCameraTab(m_onvifDevice, this);
    m_serverTab = new OnvifServerTab(m_onvifServer, m_pelcoDevice, this);

    m_tabWidget->addTab(m_cameraTab, tr("ONVIF Camera (Client)"));
    m_tabWidget->addTab(m_serverTab, tr("ONVIF Server (Daemon)"));

    centralLayout->addWidget(m_tabWidget);
    setCentralWidget(centralWidget);

    statusBar()->showMessage(tr("Ready. Discover devices or configure ONVIF service."));
}

void MainWindow::setupConnections()
{
    connect(m_cameraTab, &OnvifCameraTab::streamUriSelected, this, &MainWindow::handleStreamUri);

    if (m_onvifDevice != nullptr) {
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::connected, this, &MainWindow::handleCameraConnected);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::disconnected, this, &MainWindow::handleCameraDisconnected);
        connect(m_onvifDevice, &PelcoD::Qt::QOnvifDevice::errorOccurred, this, &MainWindow::handleCameraError);
    }
}

void MainWindow::handleStreamUri(const QString& uri)
{
    if (QClipboard* const clip = QApplication::clipboard(); clip != nullptr) {
        clip->setText(uri);
    }
    static_cast<void>(QDesktopServices::openUrl(QUrl(uri)));
    statusBar()->showMessage(tr("ONVIF RTSP URI copied to clipboard & launched: %1").arg(uri), 5000);
}

void MainWindow::handleCameraConnected(const QString& endpoint, const QString& model)
{
    statusBar()->showMessage(tr("Connected to ONVIF camera [%1] at %2").arg(model, endpoint), 5000);
}

void MainWindow::handleCameraDisconnected()
{
    statusBar()->showMessage(tr("ONVIF camera disconnected"), 3000);
}

void MainWindow::handleCameraError(const QString& error)
{
    statusBar()->showMessage(tr("ONVIF Error: %1").arg(error), 6000);
}

} // namespace OnvifApp
