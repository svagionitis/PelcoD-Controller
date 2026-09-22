/// @file TestSerialTransport.cpp
/// @brief Unit tests for PelcoD::SerialTransport and port enumeration.

#include "SerialTransport.h"
#include "TestHelpers.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace Transport;

/// @brief Verify SerialTransport::enumeratePorts() returns valid, unique, and non-empty port names.
TEST(SerialTransportTest, EnumeratePorts)
{
    const std::vector<std::string> ports = SerialTransport::enumeratePorts();

    // Verify all returned port strings are non-empty and unique
    for (std::size_t i = 0; i < ports.size(); ++i) {
        EXPECT_FALSE(ports[i].empty()) << "Port name must not be empty";
        for (std::size_t j = i + 1; j < ports.size(); ++j) {
            EXPECT_NE(ports[i], ports[j]) << "Enumerated port names must be unique";
        }
    }
}

/// @brief Verify SerialTransport constructor, accessors, and setters.
TEST(SerialTransportTest, TransportAccessors)
{
    SerialTransport transport("COM3", 19200U);

    EXPECT_EQ(transport.getPortName(), "COM3");
    EXPECT_EQ(transport.getBaudRate(), 19200U);
    EXPECT_FALSE(transport.isOpen());

    transport.setPortName("COM7");
    EXPECT_EQ(transport.getPortName(), "COM7");

    transport.setBaudRate(115200U);
    EXPECT_EQ(transport.getBaudRate(), 115200U);

    // Closing an un-opened transport must be a safe no-op
    transport.close();
    EXPECT_FALSE(transport.isOpen());
}

/// @brief Verify StandardBaudRates definitions and isValidBaudRate() validation.
TEST(SerialTransportTest, StandardBaudRates)
{
    constexpr auto& bauds = SerialTransport::StandardBaudRates;
    ASSERT_EQ(bauds.size(), 7U);

    // Verify rates are in strictly ascending order
    for (std::size_t i { 1U }; i < bauds.size(); ++i) {
        EXPECT_GT(bauds[i], bauds[i - 1U]) << "Baud rates must be sorted in strictly ascending order";
    }

    // Verify all defined rates pass validation
    for (const auto rate : bauds) {
        EXPECT_TRUE(SerialTransport::isValidBaudRate(rate)) << "Standard rate must be recognized";
    }

    // Specific expected values
    EXPECT_EQ(bauds[0], 2400U);
    EXPECT_EQ(bauds[1], 4800U);
    EXPECT_EQ(bauds[2], 9600U);
    EXPECT_EQ(bauds[3], 19200U);
    EXPECT_EQ(bauds[4], 38400U);
    EXPECT_EQ(bauds[5], 57600U);
    EXPECT_EQ(bauds[6], 115200U);

    // Verify non-standard rates are rejected
    EXPECT_FALSE(SerialTransport::isValidBaudRate(0U));
    EXPECT_FALSE(SerialTransport::isValidBaudRate(1200U));
    EXPECT_FALSE(SerialTransport::isValidBaudRate(14400U));
    EXPECT_FALSE(SerialTransport::isValidBaudRate(99999U));
}

/// @brief Verify that unopened or closed serial transport consistently rejects sendData and reports closed.
TEST(SerialTransportTest, SerialClosedStateRejection)
{
    SerialTransport transport("NON_EXISTENT_PORT_12345", 9600U);
    EXPECT_FALSE(transport.isOpen());

    // sendData on closed transport must immediately return false
    const std::vector<std::uint8_t> frame { 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 };
    EXPECT_FALSE(transport.sendData(frame));

    // open() to a non-existent port must return false
    const bool opened = transport.open();
    EXPECT_FALSE(opened);
    EXPECT_FALSE(transport.isOpen());
    EXPECT_FALSE(transport.sendData(frame));

    transport.close();
    EXPECT_FALSE(transport.isOpen());
    EXPECT_FALSE(transport.sendData(frame));
}

} // namespace
