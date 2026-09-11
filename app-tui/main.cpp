/// @file main.cpp
/// @brief CLI entry point for the Pelco-D UTF-8 Terminal User Interface (app-tui).

#include "TuiApp.h"
#include "views/ConnectionModal.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

static void printUsage(std::string_view progName)
{
    std::cout << "Pelco-D Controller Terminal User Interface (app-tui)\n\n"
              << "Usage: " << progName << " [options]\n\n"
              << "Options:\n"
              << "  --mock                      Start in standalone Mock simulator mode (default)\n"
              << "  --tcp <host:port>           Connect via TCP network socket (e.g. 192.168.1.100:4001)\n"
              << "  --serial <port> [baud]      Connect via RS-485 serial port (e.g. /dev/ttyUSB0 9600)\n"
              << "  --address <id>              Set Pelco-D camera address 1–254 (default: 1)\n"
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

int main(int argc, char* argv[])
{
    PelcoDTui::ConnectionConfig config {};
    config.type = PelcoDTui::TransportType::Mock;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }

        if (arg == "--mock") {
            config.type = PelcoDTui::TransportType::Mock;
        } else if (arg == "--tcp" && i + 1 < argc) {
            config.type = PelcoDTui::TransportType::Tcp;
            const std::string endpoint = argv[++i];
            const auto colonPos = endpoint.find(':');
            if (colonPos != std::string::npos) {
                config.tcpHost = endpoint.substr(0, colonPos);
                config.tcpPort = static_cast<std::uint16_t>(std::stoi(endpoint.substr(colonPos + 1)));
            } else {
                config.tcpHost = endpoint;
                config.tcpPort = 9000U;
            }
        } else if (arg == "--serial" && i + 1 < argc) {
            config.type = PelcoDTui::TransportType::Serial;
            config.serialPort = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                config.serialBaud = static_cast<std::uint32_t>(std::stoul(argv[++i]));
            }
        } else if (arg == "--address" && i + 1 < argc) {
            const int addrVal = std::stoi(argv[++i]);
            if (addrVal >= 1 && addrVal <= 254) {
                config.address = static_cast<std::uint8_t>(addrVal);
            }
        }
    }

    try {
        PelcoDTui::TuiApp app(config);
        app.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
