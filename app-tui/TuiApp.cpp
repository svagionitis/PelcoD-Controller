#include "TuiApp.h"

#include <chrono>
#include <thread>

namespace PelcoDTui {

TuiApp::TuiApp(
    const ConnectionConfig& initialConfig, const std::string& videoSource, videodecoder::BackendType videoBackend)
    : m_currentConfig(initialConfig)
    , m_videoSource(videoSource.empty() ? "mock:smpte" : videoSource)
    , m_videoBackend(videoBackend)
{
    const auto size = m_terminal.getSize();
    m_canvas.resize(size.width, size.height);
    m_connectionModal.setConfig(initialConfig);
    setupDevice(initialConfig);
    startVideoWorker();
}

TuiApp::~TuiApp()
{
    stopVideoWorker();
    if (m_device) {
        m_device->stop();
    }
}

void TuiApp::setVideoConfig(const std::string& source, videodecoder::BackendType backend)
{
    stopVideoWorker();
    m_videoSource = source.empty() ? "mock:smpte" : source;
    m_videoBackend = backend;
    startVideoWorker();
}

#if defined(PELCOD_ENABLE_ONVIF)
void TuiApp::startOnvifServer()
{
    m_onvifServerView.startServer();
}

void TuiApp::setOnvifServerConfig(int port, const std::string& name, const std::string& rtsp)
{
    auto& cfg = m_onvifServerView.config();
    cfg.port = port;
    if (!name.empty()) {
        cfg.deviceName = name;
    }
    if (!rtsp.empty()) {
        cfg.rtspStreamUri = rtsp;
    }
}
#endif

void TuiApp::startVideoWorker()
{
    stopVideoWorker();
    m_videoRunning = true;
    m_videoThread = std::thread(&TuiApp::videoWorkerLoop, this);
}

void TuiApp::stopVideoWorker()
{
    m_videoRunning = false;
    if (m_videoThread.joinable()) {
        m_videoThread.join();
    }
}

void TuiApp::videoWorkerLoop()
{
    m_videoDecoder = videodecoder::DecoderFactory::create(m_videoBackend);
    if (!m_videoDecoder) {
        m_videoView.setStreamInfo(videodecoder::StreamState::Error, m_videoSource, "Failed to create decoder");
        return;
    }

    const std::string backendName = (m_videoBackend == videodecoder::BackendType::Mock) ? "Mock"
        : (m_videoBackend == videodecoder::BackendType::FFmpeg)                         ? "FFmpeg"
                                                                                        : "GStreamer";

    m_videoView.setStreamInfo(videodecoder::StreamState::Connecting, m_videoSource, backendName);

    if (!m_videoDecoder->initialize(m_videoSource)) {
        m_videoView.setStreamInfo(videodecoder::StreamState::Error, m_videoSource, "Failed to initialize");
        return;
    }

    const auto metadata = m_videoDecoder->getVideoMetadata();
    const double fps = (metadata.frameRate > 0.0) ? metadata.frameRate : 30.0;
    m_videoView.setStreamInfo(videodecoder::StreamState::Streaming, m_videoSource, backendName, fps);

    double lastPts = -1.0;
    auto lastFrameTime = std::chrono::steady_clock::now();

    while (m_videoRunning) {
        if (m_videoView.isPaused()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (m_videoDecoder->decodeNextFrame()) {
            const auto frame = m_videoDecoder->getRawFrameData();
            if (frame.data && frame.width > 0 && frame.height > 0) {
                m_videoView.updateFrame(frame.data, frame.width, frame.height, frame.timestamp, frame.decodeTimeMs);
                m_videoView.setStreamInfo(videodecoder::StreamState::Streaming, m_videoSource, backendName, fps);
            }

            // Frame pacing
            if (lastPts >= 0.0) {
                const double ptsDiff = frame.timestamp - lastPts;
                if (ptsDiff > 0.001 && ptsDiff < 5.0) {
                    const auto now = std::chrono::steady_clock::now();
                    const std::chrono::duration<double> actualElapsed = now - lastFrameTime;
                    const double sleepTime = ptsDiff - actualElapsed.count();
                    if (sleepTime > 0.001) {
                        std::this_thread::sleep_for(
                            std::chrono::microseconds(static_cast<long long>(sleepTime * 1000000.0)));
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(1000.0 / fps)));
            }
            lastPts = frame.timestamp;
            lastFrameTime = std::chrono::steady_clock::now();
        } else {
            // EOF or disconnected
            if (m_videoView.isLoop()) {
                m_videoDecoder->seek(0.0);
                lastPts = -1.0;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            } else {
                m_videoView.setStreamInfo(videodecoder::StreamState::Disconnected, m_videoSource, backendName, fps);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    m_videoDecoder->close();
    m_videoDecoder.reset();
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
    m_device = std::make_unique<PelcoD::FujinonSX800Device>(transport, config.address);

    m_trafficConnection = m_device->addTrafficCallback(
        [this](bool isTx, const std::vector<std::uint8_t>& frame) { m_trafficView.addPacket(isTx, frame); });

    const bool started = m_device->start();
    if (!started) {
        m_footerView.setStatusMessage("Warning: Failed to open device transport");
    }
    m_device->setTelemetryPolling(true, 1000U);
#if defined(PELCOD_ENABLE_ONVIF)
    m_onvifServerView.bindDevice(m_device.get(), m_presetsView.patrolController());
#endif
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
#if defined(PELCOD_ENABLE_ONVIF)
    constexpr int kTotalTabs = 9;
#else
    constexpr int kTotalTabs = 8;
#endif

    // Mouse click handling
    if (event.key == Key::MouseClick && !event.mouse.isRelease) {
        const int clickedTab = m_headerView.handleMouseClick(event.mouse.x, event.mouse.y);
        if (clickedTab >= 0 && clickedTab < kTotalTabs) {
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
    if (event.ch >= '1' && event.ch <= static_cast<char>('0' + kTotalTabs)) {
        m_activeTab = event.ch - '1';
        return;
    }

    if (event.key >= Key::F1 && static_cast<int>(event.key) < static_cast<int>(Key::F1) + kTotalTabs) {
        m_activeTab = static_cast<int>(event.key) - static_cast<int>(Key::F1);
        return;
    }

    if (event.key == Key::Tab) {
        m_activeTab = (m_activeTab + 1) % kTotalTabs;
        return;
    }

    if (event.key == Key::Backtab) {
        m_activeTab = (m_activeTab + kTotalTabs - 1) % kTotalTabs;
        return;
    }

#if defined(PELCOD_ENABLE_ONVIF)
    if (m_activeTab == 8) {
        m_onvifServerView.handleInput(event);
        return;
    }
#endif

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
        case 6:
            m_fujinonView.handleInput(event, *m_device);
            break;
        case 7:
            m_videoView.handleInput(event, *m_device);
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
        case 6:
            m_fujinonView.render(m_canvas, viewStartY, width, viewHeight,
                m_device ? m_device->getFujinonStatus() : PelcoD::FujinonStatus {});
            break;
        case 7:
            m_videoView.render(m_canvas, viewStartY, width, viewHeight, status);
            break;
#if defined(PELCOD_ENABLE_ONVIF)
        case 8:
            m_onvifServerView.render(m_canvas, viewStartY, width, viewHeight);
            break;
#endif
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
