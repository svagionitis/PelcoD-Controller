#pragma once

/// @file TuiApp.h
/// @brief Main application coordinator managing event loop, views, and device lifecycle.

#include "Canvas.h"
#include "FujinonSX800Device.h"
#include "FujinonTypes.h"
#include "PelcoDDevice.h"
#include "Terminal.h"
#include "views/AuxOsdView.h"
#include "views/ConnectionModal.h"
#include "views/DiagnosticsView.h"
#include "views/FooterView.h"
#include "views/FujinonView.h"
#include "views/HeaderView.h"
#include "views/PresetsView.h"
#include "views/PtzView.h"
#include "views/SettingsView.h"
#include "views/TrafficView.h"

#include <atomic>
#include <memory>
#include <string>

namespace PelcoDTui {

/// @class TuiApp
/// @brief Top-level TUI controller coordinating render frames and device communication.
class TuiApp {
public:
    explicit TuiApp(const ConnectionConfig& initialConfig);
    ~TuiApp();

    // Non-copyable, non-movable
    TuiApp(const TuiApp&) = delete;
    TuiApp& operator=(const TuiApp&) = delete;
    TuiApp(TuiApp&&) = delete;
    TuiApp& operator=(TuiApp&&) = delete;

    /// @brief Start application event loop until user requests exit.
    void run();

private:
    void setupDevice(const ConnectionConfig& config);
    void handleGlobalInput(const InputEvent& event);
    void renderFrame();

    Terminal m_terminal {};
    Canvas m_canvas { 80, 24 };
    std::atomic<bool> m_running { true };

    int m_activeTab { 0 };
    std::string m_transportName { "Mock Mode" };
    ConnectionConfig m_currentConfig {};

    std::unique_ptr<PelcoD::FujinonSX800Device> m_device;
    PelcoD::ScopedConnection m_trafficConnection {};

    // View Components
    HeaderView m_headerView {};
    PtzView m_ptzView {};
    PresetsView m_presetsView {};
    SettingsView m_settingsView {};
    AuxOsdView m_auxOsdView {};
    DiagnosticsView m_diagnosticsView {};
    TrafficView m_trafficView {};
    FujinonView m_fujinonView {};
    ConnectionModal m_connectionModal {};
    FooterView m_footerView {};
};

} // namespace PelcoDTui
