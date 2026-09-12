/// @file TestSerialTransport.cpp
/// @brief Unit tests for PelcoD::SerialTransport and port enumeration.

#include "SerialTransport.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

/// @brief Verify SerialTransport::enumeratePorts() returns valid, unique, and non-empty port names.
static void testEnumeratePorts()
{
    const std::vector<std::string> ports = PelcoD::SerialTransport::enumeratePorts();

    // Verify all returned port strings are non-empty and unique
    for (std::size_t i = 0; i < ports.size(); ++i) {
        assert(!ports[i].empty() && "Port name must not be empty");
        for (std::size_t j = i + 1; j < ports.size(); ++j) {
            assert(ports[i] != ports[j] && "Enumerated port names must be unique");
        }
    }

    std::cout << "  testEnumeratePorts: discovered " << ports.size() << " port(s) - PASSED\n";
    for (const auto& port : ports) {
        std::cout << "    - " << port << "\n";
    }
}

/// @brief Verify SerialTransport constructor, accessors, and setters.
static void testTransportAccessors()
{
    PelcoD::SerialTransport transport("COM3", 19200U);

    assert(transport.getPortName() == "COM3");
    assert(transport.getBaudRate() == 19200U);
    assert(!transport.isOpen());

    transport.setPortName("COM7");
    assert(transport.getPortName() == "COM7");

    transport.setBaudRate(115200U);
    assert(transport.getBaudRate() == 115200U);

    // Closing an un-opened transport must be a safe no-op
    transport.close();
    assert(!transport.isOpen());

    std::cout << "  testTransportAccessors: PASSED\n";
}

int main()
{
    std::cout << "[TestSerialTransport] Running...\n";
    testEnumeratePorts();
    testTransportAccessors();
    std::cout << "[TestSerialTransport] All tests passed.\n";
    return 0;
}
