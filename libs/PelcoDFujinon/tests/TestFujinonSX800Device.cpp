/// @file TestFujinonSX800Device.cpp
/// @brief Integration tests for PelcoD::FujinonSX800Device profile with mock transport.

#include "FujinonBuilder.h"
#include "FujinonSX800Device.h"
#include "MockPelcoDDevice.h"
#include "PelcoDFrame.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace {

TEST(FujinonSX800DeviceTest, DeviceLifecycleAndCommands)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(1U);
    PelcoD::FujinonSX800Device device(mock, 1U);

    ASSERT_TRUE(device.start());
    EXPECT_TRUE(device.isConnected());

    std::atomic<bool> statusReceived { false };
    std::atomic<std::uint16_t> reportedFocus { 0U };
    std::atomic<int> reportedOIS { 0 };
    std::atomic<int> reportedDefog { 0 };

    device.addFujinonStatusCallback([&](const PelcoD::FujinonStatus& st) {
        statusReceived.store(true);
        reportedFocus.store(st.absoluteFocusPosition);
        reportedOIS.store(static_cast<int>(st.oisMode));
        reportedDefog.store(static_cast<int>(st.defogLevel));
    });

    // Send Fujinon commands
    device.setOISMode(PelcoD::FujinonOISMode::OisOn);
    device.setDefog(PelcoD::FujinonDefogLevel::Level2);
    device.setHeatHaze(PelcoD::FujinonHeatHazeLevel::Level1);
    device.setWDR(PelcoD::FujinonWDRLevel::Level3);
    device.setVLCFilter(true);
    device.setFocusPosition(0x3456U);

    // Fine image adjustments
    device.setBrightness(60U);
    device.setContrast(50U);
    device.setSaturation(40U);
    device.setSharpness(30U);
    device.setColorTemperature(PelcoD::FujinonColorTemp::K9000);
    device.setWhiteBalance(PelcoD::FujinonWBMode::Auto);
    device.setDigitalZoom(PelcoD::FujinonDigitalZoom::X2);
    device.setNoiseReduction(2U);

    // Day / Night & IR
    device.setDayNightMode(PelcoD::FujinonDayNightMode::Night);
    device.setIRWavelength(PelcoD::FujinonIRWavelength::W850nm);

    // AF & Optics
    device.onePushAF();
    device.setAFSensitivity(2U);
    device.setAFArea(1U);

    // Manual Exposure
    device.setManualIris(10U);
    device.setManualShutter(20U);
    device.setManualISO(15U);

    // OSD Overlays
    device.setTimeDisplay(true);
    device.setTimePosition(PelcoD::FujinonOSDPosition::TopRight);
    device.setTitleDisplay(true);
    device.setTitlePosition(PelcoD::FujinonOSDPosition::BottomLeft);
    device.setIdDisplay(false);
    device.setIdPosition(PelcoD::FujinonOSDPosition::BottomRight);
    device.setReticleDisplay(true);

    // System Config & Media
    device.setVideoMode(PelcoD::FujinonVideoMode::PAL);
    device.setHDFormat(PelcoD::FujinonHDFormat::F1080p30);
    device.setRS485Termination(true);
    device.recordLiveView(true);
    device.playMovie(false);
    device.setMovieMode(true);
    device.menuOk();
    device.menuDirection(PelcoD::FujinonMenuDirection::Down);
    device.reboot();

    // Telemetry Queries
    device.queryFujinonFocus();
    device.queryFujinonZoom();
    device.querySerialNumber();
    device.queryFirmwareVersion();
    device.queryLensStatus();
    device.queryPhotoSettings();
    device.queryImageQuality();
    device.queryManualSettings();

    // Inject a 7-byte focus position response: FF 01 00 81 34 56 CKSM
    const auto focusResp = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x81U, 0x34U, 0x56U);
    mock->injectRxData(focusResp);

    // Wait briefly for callback dispatch
    const auto start = std::chrono::steady_clock::now();
    while (!statusReceived.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(500)) {
            FAIL() << "Timeout waiting for Fujinon status callback!";
        }
    }

    EXPECT_EQ(reportedFocus.load(), 0x3456U);
    EXPECT_EQ(device.getFujinonStatus().absoluteFocusPosition, 0x3456U);

    // Inject an OIS setting response: FF 01 F0 1F 13 02 CKSM
    const auto oisResp = PelcoD::PelcoDFrame::createFrame(1U, 0xF0U, 0x1FU, 0x13U, 0x02U);
    mock->injectRxData(oisResp);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(device.getFujinonStatus().oisMode, PelcoD::FujinonOISMode::OisOn);

    // Inject a firmware version response: FF 01 00 8B 02 51 CKSM
    const auto fwResp = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x8BU, 0x02U, 0x51U);
    mock->injectRxData(fwResp);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(device.getFujinonStatus().firmwareVersion, "v2.51");

    // Inject 18-byte Serial Number response
    std::vector<std::uint8_t> serialFrame { 0xFFU, 1U, 'S', 'X', '8', '0', '0', '0', '0', '1', 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U };
    std::uint32_t cksm { 0U };
    for (std::size_t i = 1; i < serialFrame.size(); ++i) {
        cksm += serialFrame[i];
    }
    serialFrame.push_back(static_cast<std::uint8_t>(cksm % 256U));
    mock->injectRxData(serialFrame);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(device.getFujinonStatus().serialNumber, "SX800001");

    // v2.12.0 Controls
    device.setBrightnessFine(80U);
    device.setContrastFine(70U);
    device.setSaturationFine(60U);
    device.setSharpnessFine(50U);
    device.setWBShiftRedFine(55U);
    device.setWBShiftBlueFine(45U);
    device.queryImageQualityFine(0xEBU);

    device.setDayNightModeEx(PelcoD::FujinonDayNightModeEx::Scheduled);
    device.setDayToNightThreshold(100U);
    device.setNightToDayThreshold(50U);
    device.setDayNightAutoDelay(10U);
    device.setDayStartTime(6U, 0U);
    device.setNightStartTime(19U, 0U);
    device.setOpticalFilterDay(false);
    device.setOpticalFilterNight(true);
    device.queryDayNightEx(0x01U);

    device.setZoomSpeedEx(4U);
    device.setFocusSpeedEx(2U);
    device.setDigitalZoomMode(PelcoD::FujinonDigitalZoomMode::CropMode);
    device.queryZoomFocusEx(0x25U);

    device.setAntialiasing(true);
    device.menuBack();
    device.formatSDCard();
    device.factoryReset();
    device.setLanguage(PelcoD::FujinonLanguage::English);

    device.setRTCSecond(30U);
    device.setRTCHourMinute(12U, 0U);
    device.setRTCMonthDay(9U, 14U);
    device.setRTCYear(2026U);
    device.queryRTC(0x01U);
    device.queryZoomStandard();

    // Inject 0x5D Standard Zoom Response (0x4000 = 800mm tele)
    const auto stdZoomResp = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x5DU, 0x40U, 0x00U);
    mock->injectRxData(stdZoomResp);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(device.getFujinonStatus().absoluteZoomPosition, 0x4000U);
    EXPECT_DOUBLE_EQ(device.getFujinonStatus().focalLengthMm, 800.0);

    // Inject Fine Image Setting Response (0xF0 0xFF)
    const auto fineResp = PelcoD::PelcoDFrame::createFrame(1U, 0xF0U, 0xFFU, 0xEBU, 80U);
    mock->injectRxData(fineResp);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(device.getFujinonStatus().fineImageSettings.brightness, 80U);

    // Inject Antialiasing Response (0xF0 0x55)
    const auto aaResp = PelcoD::PelcoDFrame::createFrame(1U, 0xF0U, 0x55U, 0x00U, 0x01U);
    mock->injectRxData(aaResp);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_TRUE(device.getFujinonStatus().antialiasing);

    device.stop();
    EXPECT_FALSE(device.isConnected());
}

} // namespace
