/// @file TestConnectionModal.cpp
/// @brief Unit tests verifying interactive inline text editing in ConnectionModal.

#include "Canvas.h"
#include "Terminal.h"
#include "views/ConnectionModal.h"

#include <cassert>
#include <iostream>
#include <string>

using namespace PelcoDTui;

static void testTcpEditing()
{
    ConnectionModal modal;
    ConnectionConfig initialCfg;
    initialCfg.type = TransportType::Tcp;
    initialCfg.tcpHost = "127.0.0.1";
    initialCfg.tcpPort = 9000U;
    initialCfg.address = 1U;

    modal.setConfig(initialCfg);
    modal.setOpen(true);
    assert(modal.isOpen());

    // Navigate to Field 2 (TCP Host)
    InputEvent downEv { Key::Down, '\0', {} };
    modal.handleInput(downEv); // to Field 1
    modal.handleInput(downEv); // to Field 2 (TCP Host)

    // Backspace to clear "0.1" from "127.0.0.1"
    InputEvent bsEv { Key::Backspace, '\0', {} };
    modal.handleInput(bsEv);
    modal.handleInput(bsEv);
    modal.handleInput(bsEv);

    // Type "5.10"
    modal.handleInput(InputEvent { Key::Character, '5', {} });
    modal.handleInput(InputEvent { Key::Character, '.', {} });
    modal.handleInput(InputEvent { Key::Character, '1', {} });
    modal.handleInput(InputEvent { Key::Character, '0', {} });

    assert(modal.getConfig().tcpHost == "127.0.5.10");

    // Navigate to Field 3 (TCP Port)
    modal.handleInput(downEv);

    // Clear port "9000"
    for (int i = 0; i < 4; ++i) {
        modal.handleInput(bsEv);
    }

    // Try typing invalid letter 'a' (should be rejected for port)
    modal.handleInput(InputEvent { Key::Character, 'a', {} });

    // Type "4001"
    modal.handleInput(InputEvent { Key::Character, '4', {} });
    modal.handleInput(InputEvent { Key::Character, '0', {} });
    modal.handleInput(InputEvent { Key::Character, '0', {} });
    modal.handleInput(InputEvent { Key::Character, '1', {} });

    // Press Enter to submit
    InputEvent enterEv { Key::Enter, '\0', {} };
    modal.handleInput(enterEv);

    assert(!modal.isOpen());
    assert(modal.hasPendingConnect());
    assert(modal.getConfig().tcpHost == "127.0.5.10");
    assert(modal.getConfig().tcpPort == 4001U);
}

static void testSerialEditingAndHotkeys()
{
    ConnectionModal modal;
    ConnectionConfig cfg;
    cfg.type = TransportType::Serial;
    cfg.serialPort = "/dev/ttyUSB0";
    cfg.serialBaud = 9600U;

    modal.setConfig(cfg);
    modal.setOpen(true);

    // Navigate to Field 2 (Serial Port)
    InputEvent downEv { Key::Down, '\0', {} };
    modal.handleInput(downEv); // Field 1
    modal.handleInput(downEv); // Field 2

    // Backspace last char ('0')
    InputEvent bsEv { Key::Backspace, '\0', {} };
    modal.handleInput(bsEv);

    // Type 'q' and '1' - 'q' should NOT quit modal while editing text!
    modal.handleInput(InputEvent { Key::Character, 'q', {} });
    assert(modal.isOpen());
    modal.handleInput(bsEv);

    modal.handleInput(InputEvent { Key::Character, '1', {} });
    assert(modal.getConfig().serialPort == "/dev/ttyUSB1");

    // Press Enter
    InputEvent enterEv { Key::Enter, '\0', {} };
    modal.handleInput(enterEv);

    assert(!modal.isOpen());
    assert(modal.hasPendingConnect());
    assert(modal.getConfig().serialPort == "/dev/ttyUSB1");
}

static void testValidationAndRendering()
{
    ConnectionModal modal;
    ConnectionConfig cfg;
    cfg.type = TransportType::Tcp;
    cfg.tcpHost = "10.0.0.1";
    cfg.tcpPort = 9000U;

    modal.setConfig(cfg);
    modal.setOpen(true);

    // Move to Port field and empty it
    InputEvent downEv { Key::Down, '\0', {} };
    modal.handleInput(downEv);
    modal.handleInput(downEv);
    modal.handleInput(downEv);

    InputEvent bsEv { Key::Backspace, '\0', {} };
    for (int i = 0; i < 4; ++i) {
        modal.handleInput(bsEv);
    }

    // Submit with empty port
    InputEvent enterEv { Key::Enter, '\0', {} };
    modal.handleInput(enterEv);

    // Should fail validation and remain open
    assert(modal.isOpen());
    assert(!modal.hasPendingConnect());

    // Render canvas
    Canvas canvas(80, 24);
    modal.render(canvas, 80, 24);

    // Close via Escape
    InputEvent escEv { Key::Escape, '\0', {} };
    modal.handleInput(escEv);
    assert(!modal.isOpen());
}

static void testSerialPortCycling()
{
    ConnectionModal modal;
    ConnectionConfig cfg;
    cfg.type = TransportType::Serial;
    cfg.serialPort = "COM1";

    modal.setConfig(cfg);
    modal.setOpen(true);

    // Navigate to Field 2 (Serial Port)
    InputEvent downEv { Key::Down, '\0', {} };
    modal.handleInput(downEv); // Field 1
    modal.handleInput(downEv); // Field 2

    // PageDown / PageUp cycling
    InputEvent pgDn { Key::PageDown, '\0', {} };
    InputEvent pgUp { Key::PageUp, '\0', {} };
    modal.handleInput(pgDn);
    modal.handleInput(pgUp);
    assert(!modal.getConfig().serialPort.empty());

    // Close via Escape
    InputEvent escEv { Key::Escape, '\0', {} };
    modal.handleInput(escEv);
    assert(!modal.isOpen());
}

int main()
{
    std::cout << "[TestConnectionModal] Running tests..." << std::endl;
    testTcpEditing();
    testSerialEditingAndHotkeys();
    testValidationAndRendering();
    testSerialPortCycling();
    std::cout << "[TestConnectionModal] All tests passed successfully." << std::endl;
    return 0;
}
