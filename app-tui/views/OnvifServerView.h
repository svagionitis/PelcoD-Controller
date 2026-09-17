#pragma once

/// @file OnvifServerView.h
/// @brief TUI view for managing and monitoring the embedded ONVIF Profile S/T server.

#include "Canvas.h"
#include "PelcoDDevice.h"
#include "Terminal.h"

#if defined(PELCOD_ENABLE_ONVIF)
#include "PelcoDOnvif/OnvifServer.h"
#include "PelcoDOnvif/OnvifServerTypes.h"
#include "PelcoDOnvif/PelcoDPtzAdapter.h"
#endif

#include <deque>
#include <memory>
#include <mutex>
#include <string>

namespace PelcoDTui {

/// @class OnvifServerView
/// @brief Interactive terminal view rendering server state, network parameters, and live request log.
class OnvifServerView {
public:
    OnvifServerView();
    ~OnvifServerView();

    // Non-copyable, non-movable
    OnvifServerView(const OnvifServerView&) = delete;
    OnvifServerView& operator=(const OnvifServerView&) = delete;
    OnvifServerView(OnvifServerView&&) = delete;
    OnvifServerView& operator=(OnvifServerView&&) = delete;

#if defined(PELCOD_ENABLE_ONVIF)
    /// @brief Associates an active PelcoDDevice instance with the ONVIF PTZ bridge.
    /// @param[in] device Pointer to active PelcoDDevice.
    void bindDevice(PelcoD::PelcoDDevice* device);

    /// @brief Starts the ONVIF server with current configuration.
    /// @return True if started.
    bool startServer();

    /// @brief Stops the ONVIF server.
    void stopServer();

    /// @brief Checks whether the server is actively running.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Accesses mutable server configuration.
    [[nodiscard]] PelcoD::Onvif::OnvifServerConfig& config() noexcept
    {
        return m_config;
    }

    /// @brief Accesses const server configuration.
    [[nodiscard]] const PelcoD::Onvif::OnvifServerConfig& config() const noexcept
    {
        return m_config;
    }
#endif

    /// @brief Renders the ONVIF server status and log dashboard into the canvas.
    /// @param[in,out] canvas Terminal frame canvas.
    /// @param[in] startY Top row offset for view area.
    /// @param[in] width Total view width.
    /// @param[in] height Total view height.
    void render(Canvas& canvas, int startY, int width, int height);

    /// @brief Processes interactive keyboard shortcuts.
    /// @param[in] event Key or mouse event.
    /// @return True if event was consumed.
    bool handleInput(const InputEvent& event);

private:
    struct LogEntry {
        std::string time {};
        std::string service {};
        std::string action {};
        std::string clientIp {};
    };

#if defined(PELCOD_ENABLE_ONVIF)
    PelcoD::Onvif::OnvifServerConfig m_config {};
    PelcoD::PelcoDDevice* m_device { nullptr };
    std::shared_ptr<PelcoD::Onvif::PelcoDPtzAdapter> m_ptzAdapter { nullptr };
    std::unique_ptr<PelcoD::Onvif::OnvifServer> m_server { nullptr };
#endif

    mutable std::mutex m_logMutex {};
    std::deque<LogEntry> m_logs {};
    int m_totalRequests { 0 };
    bool m_ptzBridgeEnabled { true };
    std::string m_lastMessage { "Ready. Press [S] to Start ONVIF Server." };
};

} // namespace PelcoDTui
