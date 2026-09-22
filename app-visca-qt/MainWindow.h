#pragma once

/// @file MainWindow.h
/// @brief Main application window for ViscaAppQt Sony camera command dashboard.

#include "QViscaSonyDevice.h"
#include "tabs/ExposureImagingTab.h"
#include "tabs/LensOpticsTab.h"
#include "tabs/RegistersHardwareTab.h"
#include "tabs/SystemDiagnosticsTab.h"
#include "widgets/ViscaConnectionWidget.h"
#include "widgets/ViscaTrafficInspectorWidget.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QTabWidget>

namespace ViscaApp {

/// @class MainWindow
/// @brief Top-level window hosting dashboard tabs and real-time protocol inspector.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void handleConnect(std::shared_ptr<::Transport::ITransport> transport, uint8_t address);
    void handleDisconnect();
    void handleModelDiscovered(Visca::Sony::SonyCameraModelType modelType, const Visca::Sony::CameraCapabilities& caps);
    void handleCommandFailed(const QString& reason);

private:
    void setupUi();
    void setupConnections();

    QViscaSonyDevice* m_device { nullptr };

    ViscaConnectionWidget* m_connectionWidget { nullptr };
    QTabWidget* m_tabWidget { nullptr };

    LensOpticsTab* m_lensTab { nullptr };
    ExposureImagingTab* m_exposureTab { nullptr };
    RegistersHardwareTab* m_registersTab { nullptr };
    SystemDiagnosticsTab* m_diagnosticsTab { nullptr };

    QDockWidget* m_dockInspector { nullptr };
    ViscaTrafficInspectorWidget* m_inspectorWidget { nullptr };
};

} // namespace ViscaApp
