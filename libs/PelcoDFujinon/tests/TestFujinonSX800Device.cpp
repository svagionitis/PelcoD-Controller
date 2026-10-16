/// @file TestFujinonSX800Device.cpp
/// @brief Integration tests for PelcoD::FujinonSX800Device profile with mock transport.

#include "FujinonBuilder.h"
#include "FujinonSX800Device.h"
#include "MockPelcoDDevice.h"
#include "PelcoDFrame.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

namespace {

TEST(FujinonSX800DeviceTest, DeviceLifecycleAndCommands)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
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
    EXPECT_EQ(device.setOISMode(PelcoD::FujinonOISMode::OisOn), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setDefog(PelcoD::FujinonDefogLevel::Level2), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setHeatHaze(PelcoD::FujinonHeatHazeLevel::Level1), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setWDR(PelcoD::FujinonWDRLevel::Level3), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setVLCFilter(true), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setFocusPosition(0x3456U), PelcoD::SendResult::Success);

    // Fine image adjustments
    EXPECT_EQ(device.setBrightness(60U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setContrast(50U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setSaturation(40U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setSharpness(30U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setColorTemperature(PelcoD::FujinonColorTemp::K9000), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setWhiteBalance(PelcoD::FujinonWBMode::Auto), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setDigitalZoom(PelcoD::FujinonDigitalZoom::X2), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setNoiseReduction(2U), PelcoD::SendResult::Success);

    // Day / Night & IR
    EXPECT_EQ(device.setDayNightMode(PelcoD::FujinonDayNightMode::Night), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setIRWavelength(PelcoD::FujinonIRWavelength::W850nm), PelcoD::SendResult::Success);

    // AF & Optics
    EXPECT_EQ(device.onePushAF(), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setAFSensitivity(2U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setAFArea(1U), PelcoD::SendResult::Success);

    // Manual Exposure
    EXPECT_EQ(device.setManualIris(10U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setManualShutter(20U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setManualISO(15U), PelcoD::SendResult::Success);

    // OSD Overlays
    EXPECT_EQ(device.setTimeDisplay(true), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setTimePosition(PelcoD::FujinonOSDPosition::TopRight), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setTitleDisplay(true), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setTitlePosition(PelcoD::FujinonOSDPosition::BottomLeft), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setIdDisplay(false), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setIdPosition(PelcoD::FujinonOSDPosition::BottomRight), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setReticleDisplay(true), PelcoD::SendResult::Success);

    // System Config & Media
    EXPECT_EQ(device.setVideoMode(PelcoD::FujinonVideoMode::PAL), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setHDFormat(PelcoD::FujinonHDFormat::F1080p30), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setRS485Termination(true), PelcoD::SendResult::Success);
    EXPECT_EQ(device.recordLiveView(true), PelcoD::SendResult::Success);
    EXPECT_EQ(device.playMovie(false), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setMovieMode(true), PelcoD::SendResult::Success);
    EXPECT_EQ(device.menuOk(), PelcoD::SendResult::Success);
    EXPECT_EQ(device.menuDirection(PelcoD::FujinonMenuDirection::Down), PelcoD::SendResult::Success);
    EXPECT_EQ(device.reboot(), PelcoD::SendResult::Success);

    // Telemetry Queries
    EXPECT_EQ(device.queryFujinonFocus(), PelcoD::SendResult::Success);
    EXPECT_EQ(device.queryFujinonZoom(), PelcoD::SendResult::Success);
    EXPECT_EQ(device.querySerialNumber(), PelcoD::SendResult::Success);
    EXPECT_EQ(device.queryFirmwareVersion(), PelcoD::SendResult::Success);
    EXPECT_EQ(device.queryLensStatus(), PelcoD::SendResult::Success);
    EXPECT_EQ(device.queryPhotoSettings(), PelcoD::SendResult::Success);
    EXPECT_EQ(device.queryImageQuality(), PelcoD::SendResult::Success);
    EXPECT_EQ(device.queryManualSettings(), PelcoD::SendResult::Success);

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
    EXPECT_EQ(device.setBrightnessFine(80U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setContrastFine(70U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setSaturationFine(60U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setSharpnessFine(50U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setWBShiftRedFine(55U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setWBShiftBlueFine(45U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.queryImageQualityFine(0xEBU), PelcoD::SendResult::Success);

    EXPECT_EQ(device.setDayNightModeEx(PelcoD::FujinonDayNightModeEx::Scheduled), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setDayToNightThreshold(100U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setNightToDayThreshold(50U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setDayNightAutoDelay(10U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setDayStartTime(6U, 0U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setNightStartTime(19U, 0U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setOpticalFilterDay(false), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setOpticalFilterNight(true), PelcoD::SendResult::Success);
    EXPECT_EQ(device.queryDayNightEx(0x01U), PelcoD::SendResult::Success);

    EXPECT_EQ(device.setZoomSpeedEx(4U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setFocusSpeedEx(2U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setDigitalZoomMode(PelcoD::FujinonDigitalZoomMode::CropMode), PelcoD::SendResult::Success);
    EXPECT_EQ(device.queryZoomFocusEx(0x25U), PelcoD::SendResult::Success);

    EXPECT_EQ(device.setAntialiasing(true), PelcoD::SendResult::Success);
    EXPECT_EQ(device.menuBack(), PelcoD::SendResult::Success);
    EXPECT_EQ(device.formatSDCard(), PelcoD::SendResult::Success);
    EXPECT_EQ(device.factoryReset(), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setLanguage(PelcoD::FujinonLanguage::English), PelcoD::SendResult::Success);

    EXPECT_EQ(device.setRTCSecond(30U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setRTCHourMinute(12U, 0U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setRTCMonthDay(9U, 14U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.setRTCYear(2026U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.queryRTC(0x01U), PelcoD::SendResult::Success);
    EXPECT_EQ(device.queryZoomStandard(), PelcoD::SendResult::Success);

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

/// @brief Destroying a FujinonSX800Device must wait for an in-flight RX callback (review C3a).
/// @details A Fujinon status callback blocks on the RX thread while another thread destroys the device.
///          Before the fix, ~FujinonSX800Device destroyed the derived members (mutex, status, callback
///          list) while the RX thread was still executing the overridden dispatchFrame(), and the
///          base destructor returned without waiting for it.
TEST(FujinonSX800DeviceTest, FujinonDtorWaitsForRx)
{
    using namespace std::chrono_literals;

    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
    auto device = std::make_unique<PelcoD::FujinonSX800Device>(mock, std::uint8_t { 1U });
    ASSERT_TRUE(device->start());

    std::promise<void> enteredPromise {};
    auto entered = enteredPromise.get_future();
    std::promise<void> releasePromise {};
    const std::shared_future<void> release { releasePromise.get_future().share() };
    std::atomic<bool> first { true };

    static_cast<void>(device->addFujinonStatusCallback([&](const PelcoD::FujinonStatus&) {
        if (first.exchange(false)) {
            enteredPromise.set_value();
            static_cast<void>(release.wait_for(5s));
        }
    }));

    const auto focusResp = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x81U, 0x34U, 0x56U);
    std::thread rxThread { [&mock, &focusResp] { mock->injectRxData(focusResp); } };
    if (entered.wait_for(2s) != std::future_status::ready) {
        releasePromise.set_value();
        rxThread.join();
        FAIL() << "Fujinon RX callback was never entered";
    }

    std::atomic<bool> destroyed { false };
    std::thread killer { [&device, &destroyed] {
        device.reset();
        destroyed.store(true);
    } };

    std::this_thread::sleep_for(150ms);
    EXPECT_FALSE(destroyed.load()) << "Destructor returned while a Fujinon RX callback was in flight";

    releasePromise.set_value();
    rxThread.join();
    killer.join();
    EXPECT_TRUE(destroyed.load());
}

/// @brief Telemetry from other bus addresses must NOT mutate device status (Finding F1).
TEST(FujinonSX800DeviceTest, MultiDropAddressFilteringIgnoresForeignFrames)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
    PelcoD::FujinonSX800Device device1(mock, 1U);
    ASSERT_TRUE(device1.start());

    std::atomic<bool> cbTriggered { false };
    static_cast<void>(device1.addFujinonStatusCallback([&](const PelcoD::FujinonStatus&) { cbTriggered.store(true); }));

    // Inject focus telemetry addressed to Device 2 (0x02)
    const auto foreignFocusResp = PelcoD::PelcoDFrame::createFrame(2U, 0x00U, 0x81U, 0x55U, 0x66U);
    mock->injectRxData(foreignFocusResp);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Device 1 must NOT have its focus mutated by Device 2's frame
    EXPECT_EQ(device1.getFujinonStatus().absoluteFocusPosition, 0U);
    EXPECT_FALSE(cbTriggered.load());

    // Inject focus telemetry addressed to Device 1 (0x01)
    const auto myFocusResp = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x81U, 0x12U, 0x34U);
    mock->injectRxData(myFocusResp);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(device1.getFujinonStatus().absoluteFocusPosition, 0x1234U);
    EXPECT_TRUE(cbTriggered.load());

    device1.stop();
}

/// @brief Runtime address changes must synchronize into the frame extension (Finding F1).
TEST(FujinonSX800DeviceTest, DynamicAddressReconfigurationFiltersCorrectly)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
    PelcoD::FujinonSX800Device device(mock, 1U);
    ASSERT_TRUE(device.start());

    // Switch device address to 5
    device.setAddress(5U);
    EXPECT_EQ(device.getAddress(), 5U);

    // Old address 1 should now be rejected
    const auto oldAddrResp = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x81U, 0x11U, 0x11U);
    mock->injectRxData(oldAddrResp);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(device.getFujinonStatus().absoluteFocusPosition, 0U);

    // New address 5 should be accepted
    const auto newAddrResp = PelcoD::PelcoDFrame::createFrame(5U, 0x00U, 0x81U, 0x55U, 0x55U);
    mock->injectRxData(newAddrResp);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(device.getFujinonStatus().absoluteFocusPosition, 0x5555U);

    device.stop();
}

/// @brief Subscriber exceptions must NOT escape the RX thread or invoke std::terminate (Finding F2).
TEST(FujinonSX800DeviceTest, ThrowingSubscriberDoesNotCrashRxThread)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
    PelcoD::FujinonSX800Device device(mock, 1U);
    ASSERT_TRUE(device.start());

    std::atomic<bool> throwingCbCalled { false };
    std::atomic<bool> secondCbCalled { false };

    // Register a subscriber that throws an exception
    static_cast<void>(device.addFujinonStatusCallback([&](const PelcoD::FujinonStatus&) {
        throwingCbCalled.store(true);
        throw std::runtime_error("Simulated subscriber UI crash");
    }));

    // Register a second subscriber to verify execution continues
    static_cast<void>(
        device.addFujinonStatusCallback([&](const PelcoD::FujinonStatus&) { secondCbCalled.store(true); }));

    const auto focusResp = PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x81U, 0x44U, 0x44U);
    EXPECT_NO_THROW({ mock->injectRxData(focusResp); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_TRUE(throwingCbCalled.load());
    EXPECT_TRUE(secondCbCalled.load());
    EXPECT_TRUE(device.isConnected());

    device.stop();
}

/// @brief Queries with formerly dead tags must resolve and populate status (Finding F3).
TEST(FujinonSX800DeviceTest, PreviouslyDeadQueriesResolvePromptly)
{
    auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(std::uint8_t { 1U });
    PelcoD::FujinonSX800Device device(mock, 1U);
    ASSERT_TRUE(device.start());

    // 1. Manual Shutter
    EXPECT_EQ(device.queryManualShutter(), PelcoD::SendResult::Success);
    mock->injectRxData(PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x97U, 0x00U, 12U));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(device.getFujinonStatus().manualShutter, 12U);

    // 2. Manual ISO
    EXPECT_EQ(device.queryManualISO(), PelcoD::SendResult::Success);
    mock->injectRxData(PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x9BU, 0x00U, 30U));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(device.getFujinonStatus().manualISO, 30U);

    // 3. Manual Iris
    EXPECT_EQ(device.queryManualIris(), PelcoD::SendResult::Success);
    mock->injectRxData(PelcoD::PelcoDFrame::createFrame(1U, 0x00U, 0x9DU, 0x00U, 8U));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(device.getFujinonStatus().manualIris, 8U);

    // 4. Movie Count
    EXPECT_EQ(device.queryMovieCount(), PelcoD::SendResult::Success);
    mock->injectRxData(PelcoD::PelcoDFrame::createFrame(1U, 0xF0U, 0xB3U, 0x02U, 0x05U));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(device.getFujinonStatus().movieCount, 517U);

    // 5. Movie Year
    EXPECT_EQ(device.queryMovieYear(1U), PelcoD::SendResult::Success);
    mock->injectRxData(PelcoD::PelcoDFrame::createFrame(1U, 0xF0U, 0xB7U, 0x07U, 0xEAU));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(device.getFujinonStatus().lastMovieYear, 2026U);

    // 6. Movie Date
    EXPECT_EQ(device.queryMovieDate(1U), PelcoD::SendResult::Success);
    mock->injectRxData(PelcoD::PelcoDFrame::createFrame(1U, 0xF0U, 0xBBU, 11U, 20U));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(device.getFujinonStatus().lastMovieMonth, 11U);
    EXPECT_EQ(device.getFujinonStatus().lastMovieDay, 20U);

    // 7. Movie Time
    EXPECT_EQ(device.queryMovieTime(1U), PelcoD::SendResult::Success);
    mock->injectRxData(PelcoD::PelcoDFrame::createFrame(1U, 0xF0U, 0xBFU, 18U, 45U));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(device.getFujinonStatus().lastMovieHour, 18U);
    EXPECT_EQ(device.getFujinonStatus().lastMovieMinute, 45U);

    // 8. Display Query (Time display enable)
    EXPECT_EQ(device.queryDisplay(0x43U), PelcoD::SendResult::Success);
    mock->injectRxData(PelcoD::PelcoDFrame::createFrame(1U, 0xF0U, 0x5FU, 0x43U, 0x01U));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_TRUE(device.getFujinonStatus().timeDisplay);

    // 9. Key Setting
    EXPECT_EQ(device.queryKeySetting(0x73U), PelcoD::SendResult::Success);
    mock->injectRxData(PelcoD::PelcoDFrame::createFrame(1U, 0xF0U, 0xAFU, 0x73U, 0x01U));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // 10. Log Data
    EXPECT_EQ(device.queryLogData(PelcoD::FujinonLogType::Focus), PelcoD::SendResult::Success);
    mock->injectRxData(PelcoD::PelcoDFrame::createFrame(1U, 0xF0U, 0xD3U, 0x01U, 0x02U));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    device.stop();
}

} // namespace
