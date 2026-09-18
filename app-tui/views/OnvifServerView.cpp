#include "OnvifServerView.h"

#include "UtfSymbols.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace PelcoDTui {

OnvifServerView::OnvifServerView()
{
}

OnvifServerView::~OnvifServerView()
{
#if defined(PELCOD_ENABLE_ONVIF)
    stopServer();
#endif
}

#if defined(PELCOD_ENABLE_ONVIF)
void OnvifServerView::bindDevice(PelcoD::PelcoDDevice* device, PelcoD::PatrolController* patrol)
{
    m_device = device;
    m_patrol = patrol;
    if (m_device != nullptr && m_ptzBridgeEnabled) {
        auto nonOwning = std::shared_ptr<PelcoD::PelcoDDevice>(m_device, [](PelcoD::PelcoDDevice*) {});
        m_ptzAdapter = std::make_shared<PelcoD::Onvif::PelcoDPtzAdapter>(nonOwning);
        if (m_patrol != nullptr) {
            m_ptzAdapter->setPatrolController(m_patrol);
        }
    } else {
        m_ptzAdapter.reset();
    }

    if (m_server != nullptr) {
        m_server->setPtzHandler(m_ptzAdapter);
        m_server->setImagingHandler(m_ptzAdapter);
    }
}

bool OnvifServerView::startServer()
{
    if (m_server != nullptr && m_server->isRunning()) {
        m_lastMessage = "Server already running on port " + std::to_string(m_config.port);
        return true;
    }

    if (m_ptzBridgeEnabled && m_device != nullptr && m_ptzAdapter == nullptr) {
        auto nonOwning = std::shared_ptr<PelcoD::PelcoDDevice>(m_device, [](PelcoD::PelcoDDevice*) {});
        m_ptzAdapter = std::make_shared<PelcoD::Onvif::PelcoDPtzAdapter>(nonOwning);
        if (m_patrol != nullptr) {
            m_ptzAdapter->setPatrolController(m_patrol);
        }
    }

    try {
        m_server = std::make_unique<PelcoD::Onvif::OnvifServer>(m_config, m_ptzAdapter, m_ptzAdapter);
        m_server->setRequestLogCallback(
            [this](const std::string& service, const std::string& action, const std::string& clientIp) {
                const auto now = std::chrono::system_clock::now();
                const std::time_t t = std::chrono::system_clock::to_time_t(now);
                std::tm tmBuf {};
#ifdef _WIN32
                localtime_s(&tmBuf, &t);
#else
                localtime_r(&t, &tmBuf);
#endif
                char timeStr[16];
                std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tmBuf);

                std::lock_guard<std::mutex> lock(m_logMutex);
                ++m_totalRequests;
                m_logs.push_back(LogEntry { timeStr, service, action, clientIp });
                if (m_logs.size() > 50) {
                    m_logs.pop_front();
                }
            });

        if (!m_server->start()) {
            m_server.reset();
            m_lastMessage = "Failed to start ONVIF server on port " + std::to_string(m_config.port);
            return false;
        }

        m_lastMessage = "ONVIF Server RUNNING on http://" + m_config.bindAddress + ":" + std::to_string(m_config.port);
        return true;
    } catch (const std::exception& ex) {
        m_server.reset();
        m_lastMessage = std::string("Start error: ") + ex.what();
        return false;
    }
}

void OnvifServerView::stopServer()
{
    if (m_server != nullptr) {
        m_server->stop();
        m_server.reset();
        m_lastMessage = "ONVIF Server STOPPED.";
    }
}

bool OnvifServerView::isRunning() const noexcept
{
    return m_server != nullptr && m_server->isRunning();
}
#endif

void OnvifServerView::render(Canvas& canvas, int startY, int width, int height)
{
    const Style& borderStyle = Styles::Border;
    const Style& headerStyle = Styles::Title;
    const Style& labelStyle = Styles::Text;
    const Style& valStyle = Styles::Highlight;
    const Style& keyStyle = Styles::Ok;

    const int splitX = std::max(36, width / 2);
    const int panelH = height - 1;

    // Outer panel borders
    canvas.drawPanel(1, startY, splitX - 1, panelH, " ONVIF Server Configuration ", borderStyle, headerStyle);
    canvas.drawPanel(splitX + 1, startY, width - splitX - 2, panelH, " Live Request Log ", borderStyle, headerStyle);

    // Left Panel: Configuration & Status
    int y = startY + 2;

    bool running = false;
    int port = 8080;
    std::string bindIp = "0.0.0.0";
    std::string devName = "Pelco-D ONVIF Bridge";
    std::string rtspUri = "rtsp://127.0.0.1:8554/live";

#if defined(PELCOD_ENABLE_ONVIF)
    running = isRunning();
    port = m_config.port;
    bindIp = m_config.bindAddress;
    devName = m_config.deviceName;
    rtspUri = m_config.rtspStreamUri;
#endif

    canvas.drawString(3, y++, "Service Status:  ", labelStyle);
    if (running) {
        canvas.drawString(
            20, y - 1, "[● RUNNING]", Style { Colors::Green, Colors::PanelBg, true, false, false, false, false });
    } else {
        canvas.drawString(
            20, y - 1, "[● STOPPED]", Style { Colors::Red, Colors::PanelBg, true, false, false, false, false });
    }

    y++;
    canvas.drawString(3, y++, "HTTP Port:       " + std::to_string(port), valStyle);
    canvas.drawString(3, y++, "Bind Address:    " + bindIp, valStyle);
    canvas.drawString(3, y++, "Device Name:     " + devName, valStyle);
    canvas.drawString(3, y++, "RTSP Stream:     " + rtspUri, valStyle);
    canvas.drawString(3, y++, "PTZ Forwarding:  " + std::string(m_ptzBridgeEnabled ? "ENABLED" : "DISABLED"), valStyle);
    canvas.drawString(3, y++, "WS-Discovery:    ENABLED (UDP 3702)", valStyle);

    y++;
    canvas.drawString(3, y++, "Endpoint URL:", labelStyle);
    std::string url = "http://" + (bindIp == "0.0.0.0" ? "127.0.0.1" : bindIp) + ":" + std::to_string(port)
        + "/onvif/device_service";
    canvas.drawString(3, y++, url, Style { Colors::Cyan, Colors::PanelBg, false, false, false, false, false });

    y += 2;
    canvas.drawString(3, y++, "Interactive Controls:", headerStyle);
    canvas.drawString(3, y++, " [S] ", keyStyle);
    canvas.drawString(8, y - 1, running ? "Stop ONVIF Server" : "Start ONVIF Server", labelStyle);

    canvas.drawString(3, y++, " [P] ", keyStyle);
    canvas.drawString(8, y - 1, "Cycle Port (8080, 8081, 8082, 8088, 8000)", labelStyle);

    canvas.drawString(3, y++, " [B] ", keyStyle);
    canvas.drawString(8, y - 1, "Toggle PTZ Pelco-D Hardware Bridge", labelStyle);

    canvas.drawString(3, y++, " [C] ", keyStyle);
    canvas.drawString(8, y - 1, "Clear Transaction Log", labelStyle);

    // Right Panel: Live Request Log
    int rightY = startY + 2;
    std::string totalStr = "Total Received: " + std::to_string(m_totalRequests);
    canvas.drawString(
        splitX + 3, rightY++, totalStr, Style { Colors::Yellow, Colors::PanelBg, true, false, false, false, false });

    canvas.drawString(splitX + 3, rightY++, "TIME      IP             SERVICE  ACTION",
        Style { Colors::DarkGray, Colors::PanelBg, true, false, false, false, false });

    {
        std::lock_guard<std::mutex> lock(m_logMutex);
        const int maxRows = panelH - 5;
        int count = 0;
        for (auto it = m_logs.rbegin(); it != m_logs.rend() && count < maxRows; ++it, ++count) {
            std::ostringstream oss;
            oss << std::left << std::setw(9) << it->time << " " << std::setw(14) << it->clientIp.substr(0, 14) << " "
                << std::setw(8) << it->service << " " << it->action;
            std::string line = oss.str();
            const int maxLen = width - splitX - 4;
            if (maxLen > 0 && static_cast<int>(line.size()) > maxLen) {
                line = line.substr(0, static_cast<std::size_t>(maxLen));
            }
            canvas.drawString(splitX + 3, rightY++, line, labelStyle);
        }
    }

    // Bottom Message Bar
    canvas.drawString(2, startY + panelH - 1, "» " + m_lastMessage,
        Style { Colors::White, Colors::PanelBg, true, false, false, false, false });
}

bool OnvifServerView::handleInput(const InputEvent& event)
{
    if (event.key == Key::MouseClick) {
        return false;
    }

    const char ch = event.ch;
#if defined(PELCOD_ENABLE_ONVIF)
    if (ch == 's' || ch == 'S') {
        if (isRunning()) {
            stopServer();
        } else {
            startServer();
        }
        return true;
    }

    if (ch == 'p' || ch == 'P') {
        if (isRunning()) {
            m_lastMessage = "Cannot change port while server is running. Stop first.";
            return true;
        }
        if (m_config.port == 8080) {
            m_config.port = 8081;
        } else if (m_config.port == 8081) {
            m_config.port = 8082;
        } else if (m_config.port == 8082) {
            m_config.port = 8088;
        } else if (m_config.port == 8088) {
            m_config.port = 8000;
        } else {
            m_config.port = 8080;
        }
        m_lastMessage = "Port set to " + std::to_string(m_config.port);
        return true;
    }

    if (ch == 'b' || ch == 'B') {
        m_ptzBridgeEnabled = !m_ptzBridgeEnabled;
        bindDevice(m_device);
        m_lastMessage = m_ptzBridgeEnabled ? "PTZ Bridge ENABLED." : "PTZ Bridge DISABLED.";
        return true;
    }
#endif

    if (ch == 'c' || ch == 'C') {
        std::lock_guard<std::mutex> lock(m_logMutex);
        m_logs.clear();
        m_totalRequests = 0;
        m_lastMessage = "Request log cleared.";
        return true;
    }

    return false;
}

} // namespace PelcoDTui
