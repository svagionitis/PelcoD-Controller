#pragma once

/// @file MainWindow.h
/// @brief Main application window for ONVIF Profile S/T/G camera and server suite.

#include "QOnvifDevice.h"
#include "QOnvifServer.h"
#include "app-onvif-qt/tabs/OnvifCameraTab.h"
#include "app-onvif-qt/tabs/OnvifServerTab.h"

#include <QMainWindow>
#include <QTabWidget>

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
    void handleStreamUri(const QString& uri);
    void handleCameraConnected(const QString& endpoint, const QString& model);
    void handleCameraDisconnected();
    void handleCameraError(const QString& error);

private:
    void setupUi();
    void setupConnections();

    PelcoD::Qt::QOnvifDevice* m_onvifDevice { nullptr };
    PelcoD::Qt::QOnvifServer* m_onvifServer { nullptr };

    QTabWidget* m_tabWidget { nullptr };
    OnvifCameraTab* m_cameraTab { nullptr };
    OnvifServerTab* m_serverTab { nullptr };
};

} // namespace OnvifApp
