#include "BusScanner.h"
#include "MockPelcoDDevice.h"
#include "DecoderTypes.h"
#include "SerialTransport.h"
#include "TcpTransport.h"
#include "UdpTransport.h"
#include "TuiApp.h"
#include "views/ConnectionModal.h"

#if defined(PELCOD_ENABLE_ONVIF)
#include "PelcoDOnvif/OnvifClient.h"
#include "PelcoDOnvif/OnvifDiscovery.h"
#include "PelcoDOnvif/OnvifTypes.h"
#endif

#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

namespace {

/// @enum ParseStatus
/// @brief Result status of CLI argument parsing.
enum class ParseStatus { Success, HelpRequested, Error };

/// @struct ParseResult
/// @brief Aggregated outcome of parsing CLI arguments.
struct ParseResult {
    ParseStatus status { ParseStatus::Success };
    PelcoDTui::ConnectionConfig config {};
    std::string errorMessage {};
    bool scanMode { false };
    bool multiBaud { false };
    std::uint8_t scanStart { 1U };
    std::uint8_t scanEnd { 32U };
    std::uint32_t scanTimeoutMs { 150U };
    std::string videoSource { "mock:smpte" };
    PelcoD::Video::BackendType videoBackend { PelcoD::Video::BackendType::Mock };
#if defined(PELCOD_ENABLE_ONVIF)
    bool onvifDiscover { false };
    std::uint32_t onvifTimeoutMs { 2000U };
    bool onvifInfo { false };
    std::string onvifEndpoint {};
    std::string onvifUser { "admin" };
    std::string onvifPass {};
#endif
};

/// @brief Exception-safe integer parser using std::from_chars.
/// @tparam T Integer type to parse.
/// @param str String view containing ASCII integer digits.
/// @param outVal Output value reference populated on success.
/// @return true if string was non-empty and fully parsed into outVal without overflow.
template <typename T> [[nodiscard]] bool parseInteger(std::string_view str, T& outVal) noexcept
{
    if (str.empty()) {
        return false;
    }
    const char* begin = str.data();
    const char* end = str.data() + str.size();
    auto [ptr, ec] = std::from_chars(begin, end, outVal);
    return (ec == std::errc {}) && (ptr == end);
}

/// @brief Exception-safe floating point parser using std::stod.
/// @param str String view containing ASCII floating point representation.
/// @param outVal Output value reference populated on success.
/// @return true if string was non-empty and fully parsed into outVal without exception.
[[nodiscard]] bool parseDouble(std::string_view str, double& outVal) noexcept
{
    if (str.empty()) {
        return false;
    }
    try {
        std::size_t idx = 0;
        const std::string s(str);
        outVal = std::stod(s, &idx);
        return (idx == s.size());
    } catch (...) {
        return false;
    }
}

/// @brief Display command-line usage information and keyboard shortcuts.
/// @param progName Executable name invoked in the shell.
void printUsage(std::string_view progName)
{
    std::cout << "Pelco-D Controller Terminal User Interface (app-tui)\n\n"
              << "Usage: " << progName << " [options]\n\n"
              << "Options:\n"
              << "  --mock                      Start in standalone Mock simulator mode (default)\n"
              << "  --mock-speed <deg/s>        Mock max PTZ slew speed in deg/s (e.g. 60.0)\n"
              << "  --mock-accel <deg/s^2>      Mock PTZ acceleration in deg/s^2 (e.g. 120.0)\n"
              << "  --mock-latency <ms>         Mock link transit base latency in ms (e.g. 50)\n"
              << "  --mock-jitter <ms>          Mock link latency jitter in ms (e.g. 15)\n"
              << "  --mock-drop <percent>       Mock link packet loss rate 0-100% (e.g. 5.0)\n"
              << "  --tcp <host:port>           Connect via TCP network socket (e.g. 192.168.1.100:4001)\n"
              << "  --udp <host:port>           Connect via UDP network socket (e.g. 192.168.1.100:4001)\n"
              << "  --serial <port> [baud]      Connect via RS-485 serial port (e.g. /dev/ttyUSB0 9600)\n"
              << "  --address <id>              Set Pelco-D camera address 1–254 (default: 1)\n"
              << "  --scan [start-end]          Scan bus for active Pelco-D devices (e.g. --scan 1-32)\n"
              << "  --multi-baud                Cycle standard baud rates (2400-115200) during --scan\n"
              << "  --video <source>            Video source (mock:smpte, rtsp://..., file.mp4, or camera device)\n"
              << "  --video-backend <backend>   Decoder backend (mock, ffmpeg, or gstreamer)\n"
#if defined(PELCOD_ENABLE_ONVIF)
              << "  --onvif-discover [timeout]  Discover ONVIF cameras on LAN (timeout in ms, default: 2000)\n"
              << "  --onvif-info <endpoint>     Query and display ONVIF camera device info and profiles\n"
              << "  --onvif-user <username>     Username for --onvif-info (default: admin)\n"
              << "  --onvif-pass <password>     Password for --onvif-info\n"
#endif
              << "  --help, -h                  Display this help message and exit\n\n"
              << "Keyboard Shortcuts:\n"
              << "  1–8 / F1–F8                 Switch between application tabs (Tab 8 is Video View)\n"
              << "  Tab / Backtab               Cycle tab focus forward / backward\n"
              << "  W / A / S / D or Arrows     PTZ motion: Pan Left/Right, Tilt Up/Down\n"
              << "  Space                       Emergency Stop all motion\n"
              << "  [ / ]                       Decrease / Increase pan & tilt speed\n"
              << "  Z / X                       Zoom Tele / Wide\n"
              << "  F / R                       Focus Near / Far\n"
              << "  I / O                       Iris Open / Close\n"
              << "  C                           Open connection setup modal dialog\n"
              << "  Q                           Quit application\n";
}

/// @brief Parse command-line arguments in an exception-safe manner.
/// @param argc Number of command-line arguments.
/// @param argv Array of command-line argument strings.
/// @return ParseResult containing the configuration or error information.
[[nodiscard]] ParseResult parseCommandLine(int argc, char* argv[])
{
    ParseResult result {};
    result.config.type = PelcoDTui::TransportType::Mock;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            result.status = ParseStatus::HelpRequested;
            return result;
        }

        if (arg == "--mock") {
            result.config.type = PelcoDTui::TransportType::Mock;
        } else if (arg == "--mock-speed") {
            if (i + 1 >= argc) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Option '--mock-speed' requires a speed in deg/s (e.g. 60.0)";
                return result;
            }
            const std::string_view valStr = argv[++i];
            double speedVal { 0.0 };
            if (!parseDouble(valStr, speedVal) || speedVal <= 0.0 || speedVal > 1000.0) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Invalid mock speed '" + std::string(valStr)
                    + "': speed must be a positive number between 0.1 and 1000.0 deg/s";
                return result;
            }
            result.config.kinematicsConfig.enabled = true;
            result.config.kinematicsConfig.maxPanSpeedDegPerSec = speedVal;
            result.config.kinematicsConfig.maxTiltSpeedDegPerSec = speedVal;
        } else if (arg == "--mock-accel") {
            if (i + 1 >= argc) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Option '--mock-accel' requires an acceleration in deg/s^2 (e.g. 120.0)";
                return result;
            }
            const std::string_view valStr = argv[++i];
            double accelVal { 0.0 };
            if (!parseDouble(valStr, accelVal) || accelVal <= 0.0 || accelVal > 5000.0) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Invalid mock acceleration '" + std::string(valStr)
                    + "': acceleration must be a positive number between 0.1 and 5000.0 deg/s^2";
                return result;
            }
            result.config.kinematicsConfig.enabled = true;
            result.config.kinematicsConfig.panAccelerationDegPerSec2 = accelVal;
            result.config.kinematicsConfig.tiltAccelerationDegPerSec2 = accelVal;
        } else if (arg == "--mock-latency") {
            if (i + 1 >= argc) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Option '--mock-latency' requires a latency in milliseconds (e.g. 50)";
                return result;
            }
            const std::string_view valStr = argv[++i];
            std::uint32_t latVal { 0U };
            if (!parseInteger(valStr, latVal) || latVal > 10000U) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Invalid mock latency '" + std::string(valStr)
                    + "': latency must be an integer between 0 and 10000 ms";
                return result;
            }
            result.config.latencyConfig.enabled = true;
            result.config.latencyConfig.baseLatencyMs = latVal;
        } else if (arg == "--mock-jitter") {
            if (i + 1 >= argc) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Option '--mock-jitter' requires a jitter in milliseconds (e.g. 15)";
                return result;
            }
            const std::string_view valStr = argv[++i];
            std::uint32_t jitVal { 0U };
            if (!parseInteger(valStr, jitVal) || jitVal > 5000U) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Invalid mock jitter '" + std::string(valStr)
                    + "': jitter must be an integer between 0 and 5000 ms";
                return result;
            }
            result.config.latencyConfig.enabled = true;
            result.config.latencyConfig.jitterMs = jitVal;
        } else if (arg == "--mock-drop") {
            if (i + 1 >= argc) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Option '--mock-drop' requires a drop percentage 0.0-100.0 (e.g. 5.0)";
                return result;
            }
            const std::string_view valStr = argv[++i];
            double dropVal { 0.0 };
            if (!parseDouble(valStr, dropVal) || dropVal < 0.0 || dropVal > 100.0) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Invalid mock drop rate '" + std::string(valStr)
                    + "': drop rate must be between 0.0 and 100.0 percent";
                return result;
            }
            result.config.latencyConfig.enabled = true;
            result.config.latencyConfig.packetDropPercent = dropVal;
        } else if (arg == "--tcp") {
            if (i + 1 >= argc) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Option '--tcp' requires an argument: <host[:port]>";
                return result;
            }
            const std::string_view endpoint = argv[++i];
            const auto colonPos = endpoint.find(':');
            if (colonPos != std::string_view::npos) {
                const std::string_view host = endpoint.substr(0, colonPos);
                const std::string_view portStr = endpoint.substr(colonPos + 1);

                if (host.empty()) {
                    result.status = ParseStatus::Error;
                    result.errorMessage = "TCP endpoint is missing host address: '" + std::string(endpoint) + "'";
                    return result;
                }
                if (portStr.empty()) {
                    result.status = ParseStatus::Error;
                    result.errorMessage = "TCP endpoint is missing port after colon: '" + std::string(endpoint) + "'";
                    return result;
                }

                std::uint16_t port { 0U };
                if (!parseInteger(portStr, port) || port == 0U) {
                    result.status = ParseStatus::Error;
                    result.errorMessage = "Invalid TCP port '" + std::string(portStr)
                        + "': port must be an integer between 1 and 65535";
                    return result;
                }

                result.config.tcpHost = std::string(host);
                result.config.tcpPort = port;
            } else {
                if (endpoint.empty()) {
                    result.status = ParseStatus::Error;
                    result.errorMessage = "TCP endpoint host cannot be empty";
                    return result;
                }
                result.config.tcpHost = std::string(endpoint);
                result.config.tcpPort = 9000U;
            }
            result.config.type = PelcoDTui::TransportType::Tcp;
        } else if (arg == "--udp") {
            if (i + 1 >= argc) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Option '--udp' requires an argument: <host[:port]>";
                return result;
            }
            const std::string_view endpoint = argv[++i];
            const auto colonPos = endpoint.find(':');
            if (colonPos != std::string_view::npos) {
                const std::string_view host = endpoint.substr(0, colonPos);
                const std::string_view portStr = endpoint.substr(colonPos + 1);

                if (host.empty()) {
                    result.status = ParseStatus::Error;
                    result.errorMessage = "UDP endpoint is missing host address: '" + std::string(endpoint) + "'";
                    return result;
                }
                if (portStr.empty()) {
                    result.status = ParseStatus::Error;
                    result.errorMessage = "UDP endpoint is missing port after colon: '" + std::string(endpoint) + "'";
                    return result;
                }

                std::uint16_t port { 0U };
                if (!parseInteger(portStr, port) || port == 0U) {
                    result.status = ParseStatus::Error;
                    result.errorMessage = "Invalid UDP port '" + std::string(portStr)
                        + "': port must be an integer between 1 and 65535";
                    return result;
                }

                result.config.udpHost = std::string(host);
                result.config.udpPort = port;
            } else {
                if (endpoint.empty()) {
                    result.status = ParseStatus::Error;
                    result.errorMessage = "UDP endpoint host cannot be empty";
                    return result;
                }
                result.config.udpHost = std::string(endpoint);
                result.config.udpPort = 9000U;
            }
            result.config.type = PelcoDTui::TransportType::Udp;
        } else if (arg == "--serial") {
            if (i + 1 >= argc) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Option '--serial' requires a port device path (e.g. /dev/ttyUSB0)";
                return result;
            }
            const std::string_view portPath = argv[++i];
            if (portPath.empty() || portPath.front() == '-') {
                result.status = ParseStatus::Error;
                result.errorMessage
                    = "Option '--serial' requires a valid device path, got: '" + std::string(portPath) + "'";
                return result;
            }
            result.config.serialPort = std::string(portPath);
            result.config.type = PelcoDTui::TransportType::Serial;

            // Optional baud rate
            if (i + 1 < argc) {
                const std::string_view nextArg = argv[i + 1];
                if (!nextArg.empty() && nextArg.front() != '-') {
                    std::uint32_t baud { 0U };
                    if (!parseInteger(nextArg, baud) || baud == 0U) {
                        result.status = ParseStatus::Error;
                        result.errorMessage
                            = "Invalid baud rate '" + std::string(nextArg) + "': baud rate must be a positive integer";
                        return result;
                    }
                    result.config.serialBaud = baud;
                    ++i;
                }
            }
        } else if (arg == "--address") {
            if (i + 1 >= argc) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Option '--address' requires a camera ID between 1 and 254";
                return result;
            }
            const std::string_view addrStr = argv[++i];
            int addrVal { 0 };
            if (!parseInteger(addrStr, addrVal) || addrVal < 1 || addrVal > 254) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Invalid camera address '" + std::string(addrStr)
                    + "': camera address must be an integer between 1 and 254";
                return result;
            }
            result.config.address = static_cast<std::uint8_t>(addrVal);
        } else if (arg == "--scan") {
            result.scanMode = true;
            if (i + 1 < argc) {
                const std::string_view nextArg = argv[i + 1];
                if (!nextArg.empty() && nextArg.front() != '-') {
                    ++i;
                    const auto hyphenPos = nextArg.find('-');
                    if (hyphenPos != std::string_view::npos) {
                        const auto startStr = nextArg.substr(0, hyphenPos);
                        const auto endStr = nextArg.substr(hyphenPos + 1);
                        int sVal { 0 };
                        int eVal { 0 };
                        if (!parseInteger(startStr, sVal) || sVal < 1 || sVal > 254 || !parseInteger(endStr, eVal)
                            || eVal < 1 || eVal > 254 || sVal > eVal) {
                            result.status = ParseStatus::Error;
                            result.errorMessage = "Invalid scan range '" + std::string(nextArg)
                                + "': range must be in format <start>-<end> with 1 <= start <= end <= 254";
                            return result;
                        }
                        result.scanStart = static_cast<std::uint8_t>(sVal);
                        result.scanEnd = static_cast<std::uint8_t>(eVal);
                    } else {
                        int eVal { 0 };
                        if (!parseInteger(nextArg, eVal) || eVal < 1 || eVal > 254) {
                            result.status = ParseStatus::Error;
                            result.errorMessage = "Invalid scan address '" + std::string(nextArg)
                                + "': address must be an integer between 1 and 254";
                            return result;
                        }
                        result.scanStart = 1U;
                        result.scanEnd = static_cast<std::uint8_t>(eVal);
                    }
                }
            }
        } else if (arg == "--multi-baud") {
            result.multiBaud = true;
        } else if (arg == "--video") {
            if (i + 1 >= argc) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Option '--video' requires a source path or URL (e.g. mock:smpte or rtsp://...)";
                return result;
            }
            result.videoSource = argv[++i];
        } else if (arg == "--video-backend") {
            if (i + 1 >= argc) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Option '--video-backend' requires a backend name (mock, ffmpeg, or gstreamer)";
                return result;
            }
            const std::string_view beStr = argv[++i];
            if (beStr == "mock") {
                result.videoBackend = PelcoD::Video::BackendType::Mock;
            } else if (beStr == "ffmpeg") {
                result.videoBackend = PelcoD::Video::BackendType::FFmpeg;
            } else if (beStr == "gstreamer") {
                result.videoBackend = PelcoD::Video::BackendType::GStreamer;
            } else {
                result.status = ParseStatus::Error;
                result.errorMessage
                    = "Unknown video backend '" + std::string(beStr) + "': expected mock, ffmpeg, or gstreamer";
                return result;
            }
#if defined(PELCOD_ENABLE_ONVIF)
        } else if (arg == "--onvif-discover") {
            result.onvifDiscover = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                const std::string_view valStr = argv[++i];
                std::uint32_t tVal { 2000U };
                if (parseInteger(valStr, tVal) && tVal > 0U) {
                    result.onvifTimeoutMs = tVal;
                }
            }
        } else if (arg == "--onvif-info") {
            if (i + 1 >= argc) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Option '--onvif-info' requires an ONVIF endpoint URL";
                return result;
            }
            result.onvifInfo = true;
            result.onvifEndpoint = argv[++i];
        } else if (arg == "--onvif-user") {
            if (i + 1 >= argc) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Option '--onvif-user' requires a username";
                return result;
            }
            result.onvifUser = argv[++i];
        } else if (arg == "--onvif-pass") {
            if (i + 1 >= argc) {
                result.status = ParseStatus::Error;
                result.errorMessage = "Option '--onvif-pass' requires a password";
                return result;
            }
            result.onvifPass = argv[++i];
#endif
        } else {
            result.status = ParseStatus::Error;
            result.errorMessage = "Unrecognized option or argument: '" + std::string(arg) + "'";
            return result;
        }
    }

    return result;
}

} // namespace

int main(int argc, char* argv[])
{
    ParseResult parseResult {};
    try {
        parseResult = parseCommandLine(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << "CLI parsing error: " << ex.what() << "\n";
        return 1;
    }

    if (parseResult.status == ParseStatus::HelpRequested) {
        printUsage(argv[0]);
        return 0;
    }

    if (parseResult.status == ParseStatus::Error) {
        std::cerr << "Error: " << parseResult.errorMessage << "\n\n"
                  << "Try '" << argv[0] << " --help' for more information.\n";
        return 1;
    }

#if defined(PELCOD_ENABLE_ONVIF)
    if (parseResult.onvifDiscover) {
        std::cout << "Starting WS-Discovery multicast probe on LAN (timeout: "
                  << parseResult.onvifTimeoutMs << " ms)...\n";
        const auto devices = PelcoD::Onvif::OnvifDiscovery::discoverDevices(
            std::chrono::milliseconds(parseResult.onvifTimeoutMs));
        std::cout << "Discovery completed. Total ONVIF devices detected: " << devices.size() << "\n";
        if (!devices.empty()) {
            std::cout << "--------------------------------------------------------------------------------\n";
            std::cout << std::left << std::setw(18) << "IPv4 Address" << std::setw(24) << "Hardware / Name"
                      << "Device Service Endpoint (XAddr)\n";
            std::cout << "--------------------------------------------------------------------------------\n";
            for (const auto& dev : devices) {
                std::string label = dev.hardware.empty() ? dev.name : dev.hardware;
                if (label.empty()) {
                    label = "Unknown";
                }
                std::cout << std::left << std::setw(18) << dev.ip << std::setw(24) << label << dev.endpoint << "\n";
            }
            std::cout << "--------------------------------------------------------------------------------\n";
        }
        return 0;
    }

    if (parseResult.onvifInfo) {
        std::cout << "Connecting to ONVIF camera: " << parseResult.onvifEndpoint << "...\n";
        PelcoD::Onvif::SecurityCredentials creds {};
        creds.username = parseResult.onvifUser;
        creds.password = parseResult.onvifPass;

        PelcoD::Onvif::OnvifClient client(parseResult.onvifEndpoint, creds);
        const auto caps = client.getCapabilities();
        if (!caps) {
            std::cerr << "Error: Failed to query capabilities from " << parseResult.onvifEndpoint << "\n";
            return 1;
        }

        std::cout << "\n[Capabilities]\n";
        std::cout << "  Device XAddr:  " << caps->deviceXAddr << "\n";
        std::cout << "  Media XAddr:   " << caps->mediaXAddr << "\n";
        std::cout << "  PTZ XAddr:     " << caps->ptzXAddr << "\n";

        const auto info = client.getDeviceInformation();
        if (info) {
            std::cout << "\n[Device Information]\n";
            std::cout << "  Manufacturer:  " << info->manufacturer << "\n";
            std::cout << "  Model:         " << info->model << "\n";
            std::cout << "  Firmware:      " << info->firmwareVersion << "\n";
            std::cout << "  Serial Number: " << info->serialNumber << "\n";
            std::cout << "  Hardware ID:   " << info->hardwareId << "\n";
        }

        const auto profiles = client.getProfiles();
        std::cout << "\n[Media Profiles: " << profiles.size() << "]\n";
        for (const auto& p : profiles) {
            std::cout << "  * Profile [" << p.token << "] \"" << p.name << "\": "
                      << p.videoWidth << "x" << p.videoHeight << " (" << p.videoEncoding << ")\n";
            const auto streamUri = client.getStreamUri(p.token, true);
            if (streamUri) {
                std::cout << "    RTSP URI:     " << streamUri->uri << "\n";
            }
            const auto snapUri = client.getSnapshotUri(p.token, true);
            if (snapUri) {
                std::cout << "    Snapshot URI: " << *snapUri << "\n";
            }
            const auto presets = client.getPresets(p.token);
            if (!presets.empty()) {
                std::cout << "    Presets (" << presets.size() << "): ";
                for (size_t pi = 0; pi < presets.size(); ++pi) {
                    std::cout << presets[pi].token << " (\"" << presets[pi].name << "\")"
                              << (pi + 1 < presets.size() ? ", " : "\n");
                }
            }
        }
        return 0;
    }
#endif

    if (parseResult.scanMode) {
        try {
            std::shared_ptr<PelcoD::ITransport> transport;
            switch (parseResult.config.type) {
            case PelcoDTui::TransportType::Mock: {
                auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(parseResult.config.address);
                mock->setKinematicsConfig(parseResult.config.kinematicsConfig);
                mock->setLatencyConfig(parseResult.config.latencyConfig);
                transport = mock;
                break;
            }
            case PelcoDTui::TransportType::Tcp:
                transport
                    = std::make_shared<PelcoD::TcpTransport>(parseResult.config.tcpHost, parseResult.config.tcpPort);
                break;
            case PelcoDTui::TransportType::Udp:
                transport = std::make_shared<PelcoD::UdpTransport>(
                    parseResult.config.udpHost, parseResult.config.udpPort, parseResult.config.udpLocalPort);
                break;
            case PelcoDTui::TransportType::Serial:
                transport = std::make_shared<PelcoD::SerialTransport>(
                    parseResult.config.serialPort, parseResult.config.serialBaud);
                break;
            default: {
                auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(parseResult.config.address);
                mock->setKinematicsConfig(parseResult.config.kinematicsConfig);
                mock->setLatencyConfig(parseResult.config.latencyConfig);
                transport = mock;
                break;
            }
            }

            std::cout << "Starting Pelco-D Bus Scan on range [" << static_cast<int>(parseResult.scanStart) << ".."
                      << static_cast<int>(parseResult.scanEnd) << "] (timeout: " << parseResult.scanTimeoutMs << "ms)"
                      << (parseResult.multiBaud ? " [Multi-Baud Auto-Discovery active]" : "") << "...\n\n";

            PelcoD::BusScanner scanner(std::move(transport));
            PelcoD::ScanConfig scanCfg;
            scanCfg.startAddress = parseResult.scanStart;
            scanCfg.endAddress = parseResult.scanEnd;
            scanCfg.timeoutMs = parseResult.scanTimeoutMs;
            if (parseResult.multiBaud) {
                scanCfg.baudRates = PelcoD::ScanConfig::standardBaudRates();
            }

            if (parseResult.multiBaud) {
                scanner.setMultiBaudProgressCallback(
                    [](std::uint32_t baud, std::uint8_t current, std::size_t scanned, std::size_t total) {
                        std::cout << "\rScanning " << baud << " bps, address " << static_cast<int>(current) << " ("
                                  << scanned << "/" << total << ")..." << std::flush;
                    });
            } else {
                scanner.setScanProgressCallback([](std::uint8_t current, std::size_t scanned, std::size_t total) {
                    std::cout << "\rScanning address " << static_cast<int>(current) << " (" << scanned << "/" << total
                              << ")..." << std::flush;
                });
            }

            scanner.setDeviceDiscoveredCallback([](const PelcoD::DiscoveredDevice& dev) {
                std::cout << "\n[+] Found Pelco-D device at address " << static_cast<int>(dev.address);
                if (dev.baudRate > 0U) {
                    std::cout << " @" << dev.baudRate << " bps";
                }
                std::cout << " (response time: " << dev.responseTimeMs << "ms";
                if (dev.hasPanPosition) {
                    std::cout << ", pan: " << std::fixed << std::setprecision(2) << (dev.panCentidegrees / 100.0)
                              << "\xC2\xB0";
                }
                std::cout << ")\n";
            });

            scanner.startScan(scanCfg);
            while (scanner.getState() == PelcoD::ScanState::Scanning) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            const auto found = scanner.getDiscoveredDevices();
            std::cout << "\n\nScan completed. Total devices found: " << found.size() << "\n";
            if (!found.empty()) {
                std::cout << "--------------------------------------------------------------------\n";
                std::cout << std::left << std::setw(12) << "Address ID" << std::setw(14) << "Baud Rate" << std::setw(18)
                          << "Response Time"
                          << "Status / Telemetry\n";
                std::cout << "--------------------------------------------------------------------\n";
                for (const auto& d : found) {
                    std::string info = "Online (Responded)";
                    if (d.hasPanPosition) {
                        std::ostringstream ss;
                        ss << "Online (Pan: " << std::fixed << std::setprecision(2) << (d.panCentidegrees / 100.0)
                           << "\xC2\xB0)";
                        info = ss.str();
                    }
                    const std::string baudStr = (d.baudRate > 0U) ? std::to_string(d.baudRate) : "Default";
                    std::cout << std::left << std::setw(12) << static_cast<int>(d.address) << std::setw(14) << baudStr
                              << std::setw(18) << (std::to_string(d.responseTimeMs) + " ms") << info << "\n";
                }
                std::cout << "--------------------------------------------------------------------\n";
            }
            return 0;
        } catch (const std::exception& ex) {
            std::cerr << "Bus scanner error: " << ex.what() << "\n";
            return 1;
        }
    }

    try {
        PelcoDTui::TuiApp app(parseResult.config, parseResult.videoSource, parseResult.videoBackend);
        app.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
