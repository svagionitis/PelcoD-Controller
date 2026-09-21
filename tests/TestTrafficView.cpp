/// @file TestTrafficView.cpp
/// @brief Unit tests verifying TrafficView direction filtering (TX/RX) and export functionality.

#include "Canvas.h"
#include "MockPelcoDDevice.h"
#include "PelcoDDevice.h"
#include "views/TrafficView.h"

#include <gtest/gtest.h>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace PelcoDTui;

TEST(TestTrafficView, TrafficFiltering)
{
    TrafficView tv;
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::PelcoDDevice dev(mock, 1U);

    EXPECT_EQ(tv.getFilter(), TrafficFilter::All);
    EXPECT_EQ(tv.getPacketCount(), 0U);

    // Add TX packet (Pan Right)
    const std::vector<std::uint8_t> txPacket { 0xFF, 0x01, 0x00, 0x02, 0x20, 0x00, 0x23 };
    tv.addPacket(true, txPacket);

    // Add RX packet (ACK)
    const std::vector<std::uint8_t> rxPacket { 0xFF, 0x01, 0x00, 0x01, 0x02, 0x01, 0x05 };
    tv.addPacket(false, rxPacket);

    // Add another TX packet
    const std::vector<std::uint8_t> txPacket2 { 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01 };
    tv.addPacket(true, txPacket2);

    EXPECT_EQ(tv.getPacketCount(), 3U);
    EXPECT_EQ(tv.getFilteredPacketCount(), 3U);

    // Press 'f' to switch to TxOnly
    InputEvent fEv { Key::Character, 'f', {} };
    tv.handleInput(fEv, dev);
    EXPECT_EQ(tv.getFilter(), TrafficFilter::TxOnly);
    EXPECT_EQ(tv.getFilteredPacketCount(), 2U);

    // Press 'f' to switch to RxOnly
    tv.handleInput(fEv, dev);
    EXPECT_EQ(tv.getFilter(), TrafficFilter::RxOnly);
    EXPECT_EQ(tv.getFilteredPacketCount(), 1U);

    // Press 'F' (uppercase) to cycle back to All
    InputEvent fUpperEv { Key::Character, 'F', {} };
    tv.handleInput(fUpperEv, dev);
    EXPECT_EQ(tv.getFilter(), TrafficFilter::All);
    EXPECT_EQ(tv.getFilteredPacketCount(), 3U);
}

TEST(TestTrafficView, TrafficExport)
{
    TrafficView tv;

    const std::vector<std::uint8_t> txPacket { 0xFF, 0x01, 0x00, 0x02, 0x20, 0x00, 0x23 };
    const std::vector<std::uint8_t> rxPacket { 0xFF, 0x01, 0x00, 0x01, 0x02, 0x01, 0x05 };

    tv.addPacket(true, txPacket);
    tv.addPacket(false, rxPacket);

    const std::string tmpLogPath = "test_traffic_export_tmp.log";

    // Export ALL
    tv.setFilter(TrafficFilter::All);
    EXPECT_TRUE(tv.exportToFile(tmpLogPath));

    // Verify file content
    {
        std::ifstream ifs(tmpLogPath);
        ASSERT_TRUE(ifs.is_open());
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        EXPECT_NE(content.find("Pelco-D Controller - Traffic Capture Log"), std::string::npos);
        EXPECT_NE(content.find("Filter Mode : ALL"), std::string::npos);
        EXPECT_NE(content.find("TX"), std::string::npos);
        EXPECT_NE(content.find("RX"), std::string::npos);
        EXPECT_NE(content.find("Total records exported: 2"), std::string::npos);
    }
    std::remove(tmpLogPath.c_str());

    // Export TX ONLY
    tv.setFilter(TrafficFilter::TxOnly);
    EXPECT_TRUE(tv.exportToFile(tmpLogPath));
    {
        std::ifstream ifs(tmpLogPath);
        ASSERT_TRUE(ifs.is_open());
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        EXPECT_NE(content.find("Filter Mode : TX ONLY"), std::string::npos);
        EXPECT_NE(content.find("Total records exported: 1"), std::string::npos);
    }
    std::remove(tmpLogPath.c_str());
}

TEST(TestTrafficView, TrafficRenderingAndControls)
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
    EXPECT_EQ(tv.getPacketCount(), 1U);

    // Unpause
    tv.handleInput(pauseEv, dev);

    // Clear
    InputEvent clearEv { Key::Character, 'c', {} };
    tv.handleInput(clearEv, dev);
    EXPECT_EQ(tv.getPacketCount(), 0U);
}
