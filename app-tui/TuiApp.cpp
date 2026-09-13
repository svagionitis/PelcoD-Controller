#include "TuiApp.h"

#include <chrono>
#include <thread>

namespace PelcoDTui {

TuiApp::TuiApp(const ConnectionConfig& initialConfig)
    : m_currentConfig(initialConfig)
{
    const auto size = m_terminal.getSize();
    m_canvas.resize(size.width, size.height);
    m_connectionModal.setConfig(initialConfig);
    setupDevice(initialConfig);
}

TuiApp::~TuiApp()
{
    if (m_device) {
        m_device->stop();
    }
}

void TuiApp::setupDevice(const ConnectionConfig& config)
{
    if (m_device) {
        m_device->stop();
        m_device.reset();
    }

    m_currentConfig = config;
    m_connectionModal.setConfig(config);

    switch (config.type) {
    case TransportType::Mock:
        m_transportName = "Mock Mode";
        break;
    case TransportType::Tcp:
        m_transportName = "TCP " + config.tcpHost + ":" + std::to_string(config.tcpPort);
        break;
    case TransportType::Udp:
        m_transportName = "UDP " + config.udpHost + ":" + std::to_string(config.udpPort);
        break;
    case TransportType::Serial:
        m_transportName = "Serial " + config.serialPort;
        break;
    }

    auto transport = m_connectionModal.createTransport();
    m_device = std::make_unique<PelcoD::PelcoDDevice>(transport, config.address);

    m_device->addTrafficCallback(
        [this](bool isTx, const std::vector<std::uint8_t>& frame) { m_trafficView.addPacket(isTx, frame); });

    const bool started = m_device->start();
    if (!started) {
        m_footerView.setStatusMessage("Warning: Failed to open device transport");
    }
    m_device->setTelemetryPolling(true, 1000U);
    m_canvas.invalidate();
}

void TuiApp::run()
{
    while (m_running) {
        if (m_terminal.checkResize()) {
            const auto size = m_terminal.getSize();
            m_canvas.resize(size.width, size.height);
            m_canvas.invalidate();
        }

        const InputEvent event = m_terminal.pollEvent(20);
        if (event.key != Key::None || event.ch != '\0') {
            handleGlobalInput(event);
        }

        renderFrame();
    }
}

void TuiApp::handleGlobalInput(const InputEvent& event)
{
    // Mouse click handling
    if (event.key == Key::MouseClick && !event.mouse.isRelease) {
        const int clickedTab = m_headerView.handleMouseClick(event.mouse.x, event.mouse.y);
        if (clickedTab >= 0 && clickedTab < 6) {
            m_activeTab = clickedTab;
            return;
        }
    }

    // Modal takes precedence when open
    if (m_connectionModal.isOpen()) {
        m_connectionModal.handleInput(event);
        if (m_connectionModal.hasPendingConnect()) {
            setupDevice(m_connectionModal.getConfig());
        }
        return;
    }

    // Global Hotkeys
    if (event.ch == 'q' || event.ch == 'Q') {
        m_running = false;
        return;
    }

    if (event.ch == 'c' || event.ch == 'C') {
        m_connectionModal.setOpen(true);
        return;
    }

    // Tab shortcuts
    if (event.ch >= '1' && event.ch <= '6') {
        m_activeTab = event.ch - '1';
        return;
    }

    if (event.key >= Key::F1 && event.key <= Key::F6) {
        m_activeTab = static_cast<int>(event.key) - static_cast<int>(Key::F1);
        return;
    }

    if (event.key == Key::Tab) {
        m_activeTab = (m_activeTab + 1) % 6;
        return;
    }

    if (event.key == Key::Backtab) {
        m_activeTab = (m_activeTab + 5) % 6;
        return;
    }

    // Pass input to active view
    if (m_device) {
        switch (m_activeTab) {
        case 0:
            m_ptzView.handleInput(event, *m_device);
            break;
        case 1:
            m_presetsView.handleInput(event, *m_device);
            break;
        case 2:
            m_settingsView.handleInput(event, *m_device);
            break;
        case 3:
            m_auxOsdView.handleInput(event, *m_device);
            break;
        case 4:
            m_diagnosticsView.handleInput(event, *m_device);
            break;
        case 5:
            m_trafficView.handleInput(event, *m_device);
            break;
        default:
            break;
        }
    }
}

void TuiApp::renderFrame()
{
    const int width = m_canvas.getWidth();
    const int height = m_canvas.getHeight();

    const bool connected = m_device && m_device->isConnected();
    const std::uint8_t address = m_device ? m_device->getAddress() : m_currentConfig.address;
    const auto status = m_device ? m_device->getStatus() : PelcoD::DeviceStatus {};
    const auto info = m_device ? m_device->getInfo() : PelcoD::DeviceInfo {};

    m_canvas.clear();

    // 1. Header (Rows 0-3)
    m_headerView.render(m_canvas, width, m_activeTab, connected, m_transportName, address);

    // 2. Main View (Rows 4 to height - 3)
    const int viewStartY = 4;
    const int viewHeight = height - 5;

    if (viewHeight > 4) {
        switch (m_activeTab) {
        case 0:
            m_ptzView.render(m_canvas, viewStartY, width, viewHeight, status);
            break;
        case 1:
            m_presetsView.render(m_canvas, viewStartY, width, viewHeight);
            break;
        case 2:
            m_settingsView.render(m_canvas, viewStartY, width, viewHeight);
            break;
        case 3:
            m_auxOsdView.render(m_canvas, viewStartY, width, viewHeight);
            break;
        case 4:
            m_diagnosticsView.render(m_canvas, viewStartY, width, viewHeight, status, info);
            break;
        case 5:
            m_trafficView.render(m_canvas, viewStartY, width, viewHeight);
            break;
        default:
            break;
        }
    }

    // 3. Footer (Row height - 1)
    m_footerView.render(m_canvas, height - 1, width, m_activeTab);

    // 4. Modal (Overlay)
    if (m_connectionModal.isOpen()) {
        m_connectionModal.render(m_canvas, width, height);
    }

    m_canvas.renderDelta(m_terminal);
}

} // namespace PelcoDTui
