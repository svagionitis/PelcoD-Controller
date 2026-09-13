#include "BusScanner.h"
#include "MockPelcoDDevice.h"
#include "SerialTransport.h"
#include "TcpTransport.h"
#include "TuiApp.h"
#include "UdpTransport.h"
#include "views/ConnectionModal.h"

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
    std::uint8_t scanStart { 1U };
    std::uint8_t scanEnd { 32U };
    std::uint32_t scanTimeoutMs { 150U };
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

/// @brief Display command-line usage information and keyboard shortcuts.
/// @param progName Executable name invoked in the shell.
void printUsage(std::string_view progName)
{
    std::cout << "Pelco-D Controller Terminal User Interface (app-tui)\n\n"
              << "Usage: " << progName << " [options]\n\n"
              << "Options:\n"
              << "  --mock                      Start in standalone Mock simulator mode (default)\n"
              << "  --tcp <host:port>           Connect via TCP network socket (e.g. 192.168.1.100:4001)\n"
              << "  --udp <host:port>           Connect via UDP network socket (e.g. 192.168.1.100:4001)\n"
              << "  --serial <port> [baud]      Connect via RS-485 serial port (e.g. /dev/ttyUSB0 9600)\n"
              << "  --address <id>              Set Pelco-D camera address 1–254 (default: 1)\n"
              << "  --scan [start-end]          Scan bus for active Pelco-D devices (e.g. --scan 1-32)\n"
              << "  --help, -h                  Display this help message and exit\n\n"
              << "Keyboard Shortcuts:\n"
              << "  1–6 / F1–F6                 Switch between application tabs\n"
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
                        if (!parseInteger(startStr, sVal) || sVal < 1 || sVal > 254
                            || !parseInteger(endStr, eVal) || eVal < 1 || eVal > 254 || sVal > eVal) {
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

    if (parseResult.scanMode) {
        try {
            std::shared_ptr<PelcoD::ITransport> transport;
            switch (parseResult.config.type) {
            case PelcoDTui::TransportType::Mock:
                transport = std::make_shared<PelcoD::MockPelcoDDevice>(parseResult.config.address);
                break;
            case PelcoDTui::TransportType::Tcp:
                transport = std::make_shared<PelcoD::TcpTransport>(
                    parseResult.config.tcpHost, parseResult.config.tcpPort);
                break;
            case PelcoDTui::TransportType::Udp:
                transport = std::make_shared<PelcoD::UdpTransport>(
                    parseResult.config.udpHost, parseResult.config.udpPort, parseResult.config.udpLocalPort);
                break;
            case PelcoDTui::TransportType::Serial:
                transport = std::make_shared<PelcoD::SerialTransport>(
                    parseResult.config.serialPort, parseResult.config.serialBaud);
                break;
            default:
                transport = std::make_shared<PelcoD::MockPelcoDDevice>(parseResult.config.address);
                break;
            }

            std::cout << "Starting Pelco-D Bus Scan on range ["
                      << static_cast<int>(parseResult.scanStart) << ".."
                      << static_cast<int>(parseResult.scanEnd) << "] (timeout: "
                      << parseResult.scanTimeoutMs << "ms)...\n\n";

            PelcoD::BusScanner scanner(std::move(transport));
            PelcoD::ScanConfig scanCfg;
            scanCfg.startAddress = parseResult.scanStart;
            scanCfg.endAddress = parseResult.scanEnd;
            scanCfg.timeoutMs = parseResult.scanTimeoutMs;

            scanner.setScanProgressCallback([](std::uint8_t current, std::size_t scanned, std::size_t total) {
                std::cout << "\rScanning address " << static_cast<int>(current)
                          << " (" << scanned << "/" << total << ")..." << std::flush;
            });

            scanner.setDeviceDiscoveredCallback([](const PelcoD::DiscoveredDevice& dev) {
                std::cout << "\n[+] Found Pelco-D device at address " << static_cast<int>(dev.address)
                          << " (response time: " << dev.responseTimeMs << "ms";
                if (dev.hasPanPosition) {
                    std::cout << ", pan: " << std::fixed << std::setprecision(2)
                              << (dev.panCentidegrees / 100.0) << "\xC2\xB0";
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
                std::cout << "--------------------------------------------------------\n";
                std::cout << std::left << std::setw(12) << "Address ID"
                          << std::setw(18) << "Response Time"
                          << "Status / Telemetry\n";
                std::cout << "--------------------------------------------------------\n";
                for (const auto& d : found) {
                    std::string info = "Online (Responded)";
                    if (d.hasPanPosition) {
                        std::ostringstream ss;
                        ss << "Online (Pan: " << std::fixed << std::setprecision(2)
                           << (d.panCentidegrees / 100.0) << "\xC2\xB0)";
                        info = ss.str();
                    }
                    std::cout << std::left << std::setw(12) << static_cast<int>(d.address)
                              << std::setw(18) << (std::to_string(d.responseTimeMs) + " ms")
                              << info << "\n";
                }
                std::cout << "--------------------------------------------------------\n";
            }
            return 0;
        } catch (const std::exception& ex) {
            std::cerr << "Bus scanner error: " << ex.what() << "\n";
            return 1;
        }
    }

    try {
        PelcoDTui::TuiApp app(parseResult.config);
        app.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
