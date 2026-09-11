/// @file TestTrafficView.cpp
/// @brief Unit tests verifying TrafficView direction filtering (TX/RX) and export functionality.

#include "Canvas.h"
#include "MockPelcoDDevice.h"
#include "PelcoDDevice.h"
#include "views/TrafficView.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

using namespace PelcoDTui;

static void testTrafficFiltering()
{
    TrafficView tv;
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice dev(mock, 1U);

    assert(tv.getFilter() == TrafficFilter::All);
    assert(tv.getPacketCount() == 0);

    // Add TX packet (Pan Right)
    const std::vector<std::uint8_t> txPacket { 0xFF, 0x01, 0x00, 0x02, 0x20, 0x00, 0x23 };
    tv.addPacket(true, txPacket);

    // Add RX packet (ACK)
    const std::vector<std::uint8_t> rxPacket { 0xFF, 0x01, 0x00, 0x01, 0x02, 0x01, 0x05 };
    tv.addPacket(false, rxPacket);

    // Add another TX packet
    const std::vector<std::uint8_t> txPacket2 { 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 };
    tv.addPacket(true, txPacket2);

    assert(tv.getPacketCount() == 3);
    assert(tv.getFilteredPacketCount() == 3);

    // Press 'f' to switch to TxOnly
    InputEvent fEv { Key::Character, 'f', {} };
    tv.handleInput(fEv, dev);
    assert(tv.getFilter() == TrafficFilter::TxOnly);
    assert(tv.getFilteredPacketCount() == 2);

    // Press 'f' to switch to RxOnly
    tv.handleInput(fEv, dev);
    assert(tv.getFilter() == TrafficFilter::RxOnly);
    assert(tv.getFilteredPacketCount() == 1);

    // Press 'F' (uppercase) to cycle back to All
    InputEvent fUpperEv { Key::Character, 'F', {} };
    tv.handleInput(fUpperEv, dev);
    assert(tv.getFilter() == TrafficFilter::All);
    assert(tv.getFilteredPacketCount() == 3);
}

static void testTrafficExport()
{
    TrafficView tv;

    const std::vector<std::uint8_t> txPacket { 0xFF, 0x01, 0x00, 0x02, 0x20, 0x00, 0x23 };
    const std::vector<std::uint8_t> rxPacket { 0xFF, 0x01, 0x00, 0x01, 0x02, 0x01, 0x05 };

    tv.addPacket(true, txPacket);
    tv.addPacket(false, rxPacket);

    const std::string tmpLogPath = "test_traffic_export_tmp.log";

    // Export ALL
    tv.setFilter(TrafficFilter::All);
    assert(tv.exportToFile(tmpLogPath));

    // Verify file content
    {
        std::ifstream ifs(tmpLogPath);
        assert(ifs.is_open());
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        assert(content.find("Pelco-D Controller - Traffic Capture Log") != std::string::npos);
        assert(content.find("Filter Mode : ALL") != std::string::npos);
        assert(content.find("TX") != std::string::npos);
        assert(content.find("RX") != std::string::npos);
        assert(content.find("Total records exported: 2") != std::string::npos);
    }
    std::remove(tmpLogPath.c_str());

    // Export TX ONLY
    tv.setFilter(TrafficFilter::TxOnly);
    assert(tv.exportToFile(tmpLogPath));
    {
        std::ifstream ifs(tmpLogPath);
        assert(ifs.is_open());
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        assert(content.find("Filter Mode : TX ONLY") != std::string::npos);
        assert(content.find("Total records exported: 1") != std::string::npos);
    }
    std::remove(tmpLogPath.c_str());
}

static void testTrafficRenderingAndControls()
{
    TrafficView tv;
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice dev(mock, 1U);

    const std::vector<std::uint8_t> txPacket { 0xFF, 0x01, 0x00, 0x02, 0x20, 0x00, 0x23 };
    tv.addPacket(true, txPacket);

    // Render on canvas
    Canvas canvas(80, 24);
    tv.render(canvas, 1, 80, 24);

    // Toggle Pause
    InputEvent pauseEv { Key::Character, 'p', {} };
    tv.handleInput(pauseEv, dev);

    // Try adding a packet while paused - should not be added
    const std::vector<std::uint8_t> txPacket2 { 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 };
    tv.addPacket(true, txPacket2);
    assert(tv.getPacketCount() == 1);

    // Unpause
    tv.handleInput(pauseEv, dev);

    // Clear
    InputEvent clearEv { Key::Character, 'c', {} };
    tv.handleInput(clearEv, dev);
    assert(tv.getPacketCount() == 0);
}

int main()
{
    std::cout << "[TestTrafficView] Running tests..." << std::endl;
    testTrafficFiltering();
    testTrafficExport();
    testTrafficRenderingAndControls();
    std::cout << "[TestTrafficView] All tests passed successfully." << std::endl;
    return 0;
}
