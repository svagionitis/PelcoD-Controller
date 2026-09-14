#pragma once

/// @file MainWindow.h
/// @brief Main application window hosting dashboard tabs and traffic inspector.

#include "DeviceStatus.h"
#include "FujinonTypes.h"
#include "QPelcoDDevice.h"
#include "app-qt/tabs/AuxZonesTab.h"
#include "app-qt/tabs/DeviceSettingsTab.h"
#include "app-qt/tabs/FujinonSX800Tab.h"
#include "app-qt/tabs/OsdScreenTab.h"
#include "app-qt/tabs/PresetsTab.h"
#include "app-qt/tabs/PtzControlTab.h"
#include "app-qt/tabs/SystemTab.h"
#include "app-qt/widgets/ConnectionWidget.h"
#include "app-qt/widgets/TrafficInspectorWidget.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QTabWidget>

namespace PelcoDApp {

/// @class MainWindow
/// @brief Top-level application window for Pelco-D Device Controller.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void handleConnect(std::shared_ptr<PelcoD::ITransport> transport, std::uint8_t address);
    void handleDisconnect();
    void handleStatusUpdated(const PelcoD::DeviceStatus& status);
    void handleFujinonStatusUpdated(const PelcoD::FujinonStatus& status);

private:
    void setupUi();
    void setupConnections();

    PelcoDQt::QPelcoDDevice* m_device { nullptr };

    ConnectionWidget* m_connectionWidget { nullptr };
    QTabWidget* m_tabWidget { nullptr };

    PtzControlTab* m_ptzTab { nullptr };
    PresetsTab* m_presetsTab { nullptr };
    DeviceSettingsTab* m_settingsTab { nullptr };
    AuxZonesTab* m_auxZonesTab { nullptr };
    OsdScreenTab* m_osdTab { nullptr };
    SystemTab* m_systemTab { nullptr };
    FujinonSX800Tab* m_fujinonTab { nullptr };

    QDockWidget* m_dockInspector { nullptr };
    TrafficInspectorWidget* m_inspectorWidget { nullptr };
};

} // namespace PelcoDApp
