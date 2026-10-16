#pragma once

/// @file MainWindow.h
/// @brief Main application window for Fujinon SX800 optical surveillance camera suite.

#include "DeviceStatus.h"
#include "FujinonTypes.h"
#include "app-fujinon-qt/tabs/DayNightTab.h"
#include "app-fujinon-qt/tabs/ExposureTab.h"
#include "app-fujinon-qt/tabs/FujinonSX800Tab.h"
#include "app-fujinon-qt/tabs/ImageEnhanceTab.h"
#include "app-fujinon-qt/tabs/OpticsTab.h"
#include "app-fujinon-qt/tabs/OsdDisplayTab.h"
#include "app-fujinon-qt/tabs/SystemMediaTab.h"
#include "app-fujinon-qt/widgets/ConnectionWidget.h"
#include "app-fujinon-qt/widgets/TrafficInspectorWidget.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QTabWidget>

namespace FujinonApp {

class RttProfilerDialog;

/// @class MainWindow
/// @brief Top-level application window for Fujinon SX800 Camera Controller.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void handleConnect(std::shared_ptr<Transport::ITransport> transport, std::uint8_t address);
    void handleDisconnect();
    void handleStatusUpdated(const PelcoD::DeviceStatus& status);
    void handleFujinonStatusUpdated(const PelcoD::FujinonStatus& status);
    /// @brief Opens or activates the RTT latency & jitter profiler dialog.
    void handleOpenProfiler();

private:
    void setupUi();
    void setupConnections();

    PelcoDQt::QPelcoDFujinonDevice* m_device { nullptr };

    ConnectionWidget* m_connectionWidget { nullptr };
    QTabWidget* m_tabWidget { nullptr };

    OpticsTab* m_opticsTab { nullptr };
    ExposureTab* m_exposureTab { nullptr };
    ImageEnhanceTab* m_imageTab { nullptr };
    DayNightTab* m_dayNightTab { nullptr };
    OsdDisplayTab* m_osdTab { nullptr };
    SystemMediaTab* m_systemTab { nullptr };
    FujinonSX800Tab* m_overviewTab { nullptr };

    QDockWidget* m_dockInspector { nullptr };
    TrafficInspectorWidget* m_inspectorWidget { nullptr };
    RttProfilerDialog* m_profilerDialog { nullptr };
};

} // namespace FujinonApp
