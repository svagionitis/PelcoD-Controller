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
#include "views/VideoView.h"

#if defined(PELCOD_ENABLE_ONVIF)
#include "views/OnvifServerView.h"
#endif

#include "DecoderFactory.h"
#include "IVideoDecoder.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace PelcoDTui {

/// @class TuiApp
/// @brief Top-level TUI controller coordinating render frames, device communication, and video.
class TuiApp {
public:
    explicit TuiApp(const ConnectionConfig& initialConfig, const std::string& videoSource = "mock:smpte",
        videodecoder::BackendType videoBackend = videodecoder::BackendType::Mock);
    ~TuiApp();

    // Non-copyable, non-movable
    TuiApp(const TuiApp&) = delete;
    TuiApp& operator=(const TuiApp&) = delete;
    TuiApp(TuiApp&&) = delete;
    TuiApp& operator=(TuiApp&&) = delete;

    /// @brief Start application event loop until user requests exit.
    void run();

    /// @brief Configure video stream target source and backend.
    void setVideoConfig(const std::string& source, videodecoder::BackendType backend);

#if defined(PELCOD_ENABLE_ONVIF)
    /// @brief Starts the background ONVIF server bridge.
    void startOnvifServer();

    /// @brief Configures ONVIF server parameters prior to startup.
    /// @param[in] port Listening HTTP/SOAP port.
    /// @param[in] name Device display name.
    /// @param[in] rtsp Advertised RTSP stream URI.
    void setOnvifServerConfig(int port, const std::string& name, const std::string& rtsp);
#endif

private:
    void setupDevice(const ConnectionConfig& config);
    void handleGlobalInput(const InputEvent& event);
    void renderFrame();

    void startVideoWorker();
    void stopVideoWorker();
    void videoWorkerLoop();

    Terminal m_terminal {};
    Canvas m_canvas { 80, 24 };
    std::atomic<bool> m_running { true };

    int m_activeTab { 0 };
    std::string m_transportName { "Mock Mode" };
    ConnectionConfig m_currentConfig {};

    std::unique_ptr<PelcoD::FujinonSX800Device> m_device;
    PelcoD::ScopedConnection m_trafficConnection {};

    // Video decoding worker state
    std::string m_videoSource { "mock:smpte" };
    videodecoder::BackendType m_videoBackend { videodecoder::BackendType::Mock };
    std::unique_ptr<videodecoder::IVideoDecoder> m_videoDecoder;
    std::thread m_videoThread;
    std::atomic<bool> m_videoRunning { false };

    // View Components
    HeaderView m_headerView {};
    PtzView m_ptzView {};
    PresetsView m_presetsView {};
    SettingsView m_settingsView {};
    AuxOsdView m_auxOsdView {};
    DiagnosticsView m_diagnosticsView {};
    TrafficView m_trafficView {};
    FujinonView m_fujinonView {};
    VideoView m_videoView {};
#if defined(PELCOD_ENABLE_ONVIF)
    OnvifServerView m_onvifServerView {};
#endif
    ConnectionModal m_connectionModal {};
    FooterView m_footerView {};
};

} // namespace PelcoDTui
