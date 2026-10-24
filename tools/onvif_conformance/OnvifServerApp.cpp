/// @file OnvifServerApp.cpp
/// @brief Standalone ONVIF Server daemon for Profile S conformance testing and ODTT validation.

#include <cstddef>
#include <cstdint>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <Onvif/OnvifServer.h>
#include <Onvif/OnvifServerTypes.h>

namespace {

std::atomic<bool> g_running { true };

void handleSignal(int sig)
{
    static_cast<void>(sig);
    g_running.store(false);
}

class AppPtzHandler : public Onvif::IPtzHandler {
public:
    void handleContinuousMove(float panSpeed, float tiltSpeed, float zoomSpeed,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) override
    {
        static_cast<void>(timeout);
        m_pan = panSpeed;
        m_tilt = tiltSpeed;
        m_zoom = zoomSpeed;
        m_moving = true;
    }

    void handleAbsoluteMove(std::optional<float> pan, std::optional<float> tilt, std::optional<float> zoom,
        const std::string& spaceUri = "") override
    {
        static_cast<void>(spaceUri);
        m_pan = pan.value_or(0.0f);
        m_tilt = tilt.value_or(0.0f);
        m_zoom = zoom.value_or(0.0f);
    }

    void handleStop(bool stopPanTilt, bool stopZoom) override
    {
        if (stopPanTilt) {
            m_pan = 0.0f;
            m_tilt = 0.0f;
        }
        if (stopZoom) {
            m_zoom = 0.0f;
        }
        m_moving = false;
    }

    std::string handleSetPreset(const std::string& name, const std::string& token) override
    {
        const std::string tok = token.empty() ? std::to_string(m_presets.size() + 1U) : token;
        m_presets.push_back({ tok, name });
        return tok;
    }

    bool handleGotoPreset(const std::string& token) override
    {
        for (const auto& p : m_presets) {
            if (p.token == token) {
                return true;
            }
        }
        return false;
    }

    bool handleRemovePreset(const std::string& token) override
    {
        for (auto it = m_presets.begin(); it != m_presets.end(); ++it) {
            if (it->token == token) {
                m_presets.erase(it);
                return true;
            }
        }
        return false;
    }

    std::vector<Onvif::PtzPreset> handleGetPresets() override
    {
        return m_presets;
    }

    Onvif::PtzStatus handleGetStatus() override
    {
        Onvif::PtzStatus st { };
        st.pan = static_cast<double>(m_pan);
        st.tilt = static_cast<double>(m_tilt);
        st.zoom = static_cast<double>(m_zoom);
        st.isMoving = m_moving;
        return st;
    }

private:
    float m_pan { 0.0f };
    float m_tilt { 0.0f };
    float m_zoom { 0.0f };
    bool m_moving { false };
    std::vector<Onvif::PtzPreset> m_presets { { "1", "HomePreset" } };
};

} // namespace

int main(int argc, char* argv[])
{
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    std::uint16_t port { 8080U };
    bool testMode { false };

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--port" && (i + 1) < argc) {
            const int parsed = std::atoi(argv[++i]);
            if (parsed > 0 && parsed <= 65535) {
                port = static_cast<std::uint16_t>(parsed);
            }
        } else if (arg == "--test-mode") {
            testMode = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: OnvifServerApp [--port <1-65535>] [--test-mode]\n";
            return 0;
        }
    }

    Onvif::OnvifServerConfig config { };
    config.bindAddress = "0.0.0.0";
    config.port = static_cast<int>(port);
    config.deviceName = "Pelco-D Controller";
    config.manufacturer = "Pelco";
    config.model = "Spectra-IV";
    config.firmwareVersion = "2.1.0";
    config.serialNumber = "SN-PELCOD-001";
    config.hardwareId = "HW-PELCOD-X1";
    config.rtspStreamUri = "rtsp://127.0.0.1:554/stream1";
    config.scopes = {
        "onvif://www.onvif.org/Profile/Streaming",
        "onvif://www.onvif.org/type/video_encoder",
        "onvif://www.onvif.org/hardware/Spectra-IV",
        "onvif://www.onvif.org/name/Pelco-D",
        "onvif://www.onvif.org/location/SecurityLab",
    };

    if (testMode) {
        config.defaultUsers = {
            { "admin", "AdminPass123!", Onvif::OnvifUserLevel::Administrator },
        };
        config.passwordPolicy.minClasses = 1;
        config.passwordPolicy.rejectCommonDefaults = false;
    }

    auto server = std::make_unique<Onvif::OnvifServer>(config);
    auto ptzHandler = std::make_shared<AppPtzHandler>();
    server->setPtzHandler(ptzHandler);

    std::cout << "[*] Starting OnvifServerApp on port " << port << "...\n";
    if (!server->start()) {
        std::cerr << "[!] Failed to bind or start OnvifServer on port " << port << "\n";
        return 1;
    }

    std::cout << "[+] OnvifServerApp is listening on port " << port << "\n";
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[*] Shutting down OnvifServerApp...\n";
    server->stop();
    std::cout << "[+] OnvifServerApp stopped cleanly.\n";

    return 0;
}
