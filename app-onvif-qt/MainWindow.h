#pragma once

/// @file MainWindow.h
/// @brief Main application window for ONVIF Profile S/T/G camera and server suite.

#include <QMainWindow>
#include <QString>
#include <QTabWidget>

#include "QOnvifDevice.h"
#include "QOnvifServer.h"
#include "QPelcoDDevice.h"
#include "app-onvif-qt/tabs/OnvifCameraTab.h"
#include "app-onvif-qt/tabs/OnvifServerTab.h"

namespace OnvifApp {

/// @class MainWindow
/// @brief Top-level application window hosting ONVIF client camera controls and server daemon tabs.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /// @brief Constructs the MainWindow instance.
    /// @param[in] parent Optional parent widget.
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    /// @brief Handles RTSP stream or replay URI selection from the camera tab.
    /// @param[in] uri Selected RTSP URI.
    void handleStreamUri(const QString& uri);

    /// @brief Updates status bar when ONVIF camera connects.
    /// @param[in] endpoint Connected service endpoint URL.
    /// @param[in] model Camera model string.
    void handleCameraConnected(const QString& endpoint, const QString& model);

    /// @brief Updates status bar when ONVIF camera disconnects.
    void handleCameraDisconnected();

    /// @brief Displays ONVIF client error message in status bar.
    /// @param[in] error Diagnostic error description.
    void handleCameraError(const QString& error);

private:
    /// @brief Initializes main window layout and child tabs.
    void setupUi();

    /// @brief Connects child tab and device signals to main window slots.
    void setupConnections();

    PelcoD::Qt::QOnvifDevice* m_onvifDevice { nullptr };
    PelcoD::Qt::QOnvifServer* m_onvifServer { nullptr };
    PelcoDQt::QPelcoDDevice* m_pelcoDevice { nullptr };

    QTabWidget* m_tabWidget { nullptr };
    OnvifCameraTab* m_cameraTab { nullptr };
    OnvifServerTab* m_serverTab { nullptr };
};

} // namespace OnvifApp
