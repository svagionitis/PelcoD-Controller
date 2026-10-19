/// @file TestFujinonProtocol.cpp
/// @brief Unit tests for Fujinon SX800 / SX801 protocol frame builder and response parser.

#include <cstddef>
#include <cstdint>

#include <array>
#include <iostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "FujinonBuilder.h"
#include "FujinonParser.h"
#include "PelcoDFrame.h"

TEST(FujinonProtocolTest, BuilderCommands)
{
    const std::uint8_t addr = 1U;

    // OIS Auto (0x01), OisOn (0x02), EisOn (0x03), Off (0x04)
    const auto oisFrame = PelcoD::FujinonBuilder::buildSetOIS(addr, PelcoD::FujinonOISMode::OisOn);
    EXPECT_TRUE(oisFrame.size() == 7U);
    EXPECT_TRUE(oisFrame[0] == 0xFFU);
    EXPECT_TRUE(oisFrame[1] == addr);
    EXPECT_TRUE(oisFrame[2] == 0xF0U);
    EXPECT_TRUE(oisFrame[3] == 0x13U);
    EXPECT_TRUE(oisFrame[4] == 0x00U);
    EXPECT_TRUE(oisFrame[5] == 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(oisFrame));

    // Defog Level 2
    const auto defogFrame = PelcoD::FujinonBuilder::buildSetDefog(addr, PelcoD::FujinonDefogLevel::Level2);
    EXPECT_TRUE(defogFrame.size() == 7U);
    EXPECT_TRUE(defogFrame[2] == 0xF0U);
    EXPECT_TRUE(defogFrame[3] == 0x29U);
    EXPECT_TRUE(defogFrame[5] == 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(defogFrame));

    // Heat Haze Level 1
    const auto hazeFrame = PelcoD::FujinonBuilder::buildSetHeatHaze(addr, PelcoD::FujinonHeatHazeLevel::Level1);
    EXPECT_TRUE(hazeFrame.size() == 7U);
    EXPECT_TRUE(hazeFrame[2] == 0xF0U);
    EXPECT_TRUE(hazeFrame[3] == 0x27U);
    EXPECT_TRUE(hazeFrame[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(hazeFrame));

    // WDR Level 3
    const auto wdrFrame = PelcoD::FujinonBuilder::buildSetWDR(addr, PelcoD::FujinonWDRLevel::Level3);
    EXPECT_TRUE(wdrFrame.size() == 7U);
    EXPECT_TRUE(wdrFrame[2] == 0xF0U);
    EXPECT_TRUE(wdrFrame[3] == 0x23U);
    EXPECT_TRUE(wdrFrame[5] == 0x03U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(wdrFrame));

    // VLC Filter On
    const auto vlcFrame = PelcoD::FujinonBuilder::buildSetVLCFilter(addr, true);
    EXPECT_TRUE(vlcFrame.size() == 7U);
    EXPECT_TRUE(vlcFrame[2] == 0xF0U);
    EXPECT_TRUE(vlcFrame[3] == 0x21U);
    EXPECT_TRUE(vlcFrame[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(vlcFrame));

    // Day / Night (Night = 0x03)
    const auto dnFrame = PelcoD::FujinonBuilder::buildSetDayNight(addr, PelcoD::FujinonDayNightMode::Night);
    EXPECT_TRUE(dnFrame.size() == 7U);
    EXPECT_TRUE(dnFrame[2] == 0xF0U);
    EXPECT_TRUE(dnFrame[3] == 0x0FU);
    EXPECT_TRUE(dnFrame[5] == 0x03U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(dnFrame));

    // IR Wavelength (850nm = 0x03)
    const auto irFrame = PelcoD::FujinonBuilder::buildSetIRWavelength(addr, PelcoD::FujinonIRWavelength::W850nm);
    EXPECT_TRUE(irFrame.size() == 7U);
    EXPECT_TRUE(irFrame[2] == 0xF0U);
    EXPECT_TRUE(irFrame[3] == 0x11U);
    EXPECT_TRUE(irFrame[5] == 0x03U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(irFrame));

    // Focus Position 0x1234
    const auto focusFrame = PelcoD::FujinonBuilder::buildSetFocusPosition(addr, 0x1234U);
    EXPECT_TRUE(focusFrame.size() == 7U);
    EXPECT_TRUE(focusFrame[2] == 0x00U);
    EXPECT_TRUE(focusFrame[3] == 0x8FU);
    EXPECT_TRUE(focusFrame[4] == 0x12U);
    EXPECT_TRUE(focusFrame[5] == 0x34U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(focusFrame));

    // One push AF
    const auto afFrame = PelcoD::FujinonBuilder::buildOnePushAF(addr);
    EXPECT_TRUE(afFrame.size() == 7U && afFrame[2] == 0xF0U && afFrame[3] == 0x07U);
    EXPECT_EQ(afFrame[5], 0x00U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(afFrame));

    // AF Sensitivity
    const auto afSensFrame = PelcoD::FujinonBuilder::buildSetAFSensitivity(addr, 2U);
    EXPECT_TRUE(afSensFrame.size() == 7U && afSensFrame[3] == 0x05U && afSensFrame[5] == 2U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(afSensFrame));

    // AF Area
    const auto afAreaFrame = PelcoD::FujinonBuilder::buildSetAFArea(addr, 1U);
    EXPECT_TRUE(afAreaFrame.size() == 7U && afAreaFrame[3] == 0x03U && afAreaFrame[5] == 1U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(afAreaFrame));

    // Image adjustments: Brightness (1..21), Contrast (1..5), Saturation (1..5), Sharpness (1..5)
    const auto brightFrame = PelcoD::FujinonBuilder::buildSetBrightness(addr, 15U);
    EXPECT_TRUE(brightFrame.size() == 7U && brightFrame[3] == 0x2BU && brightFrame[5] == 15U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(brightFrame));

    const auto contFrame = PelcoD::FujinonBuilder::buildSetContrast(addr, 4U);
    EXPECT_TRUE(contFrame.size() == 7U && contFrame[3] == 0x2DU && contFrame[5] == 4U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(contFrame));

    const auto satFrame = PelcoD::FujinonBuilder::buildSetSaturation(addr, 3U);
    EXPECT_TRUE(satFrame.size() == 7U && satFrame[3] == 0x2FU && satFrame[5] == 3U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(satFrame));

    const auto sharpFrame = PelcoD::FujinonBuilder::buildSetSharpness(addr, 4U);
    EXPECT_TRUE(sharpFrame.size() == 7U && sharpFrame[3] == 0x31U && sharpFrame[5] == 4U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(sharpFrame));

    // Color Temp & White Balance
    const auto ctFrame = PelcoD::FujinonBuilder::buildSetColorTemperature(addr, PelcoD::FujinonColorTemp::K9000);
    EXPECT_TRUE(ctFrame.size() == 7U && ctFrame[3] == 0x33U && ctFrame[5] == 0x03U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(ctFrame));

    const auto wbFrame = PelcoD::FujinonBuilder::buildSetWhiteBalance(addr, PelcoD::FujinonWBMode::Auto);
    EXPECT_TRUE(wbFrame.size() == 7U && wbFrame[3] == 0x35U && wbFrame[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(wbFrame));

    // Digital Zoom & Noise Reduction
    const auto dzFrame = PelcoD::FujinonBuilder::buildSetDigitalZoom(addr, PelcoD::FujinonDigitalZoom::X2);
    EXPECT_TRUE(dzFrame.size() == 7U && dzFrame[3] == 0x37U && dzFrame[5] == 0x04U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(dzFrame));

    const auto nrFrame = PelcoD::FujinonBuilder::buildSetNoiseReduction(addr, 2U);
    EXPECT_TRUE(nrFrame.size() == 7U && nrFrame[3] == 0x39U && nrFrame[5] == 2U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(nrFrame));

    // Exposure Manual Overrides
    const auto irisFrame = PelcoD::FujinonBuilder::buildSetManualIris(addr, 15U);
    EXPECT_TRUE(irisFrame.size() == 7U && irisFrame[3] == 0x91U && irisFrame[5] == 15U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(irisFrame));

    const auto shutterFrame = PelcoD::FujinonBuilder::buildSetManualShutter(addr, 25U);
    EXPECT_TRUE(shutterFrame.size() == 7U && shutterFrame[3] == 0x95U && shutterFrame[5] == 25U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(shutterFrame));

    const auto isoFrame = PelcoD::FujinonBuilder::buildSetManualISO(addr, 10U);
    EXPECT_TRUE(isoFrame.size() == 7U && isoFrame[3] == 0x99U && isoFrame[5] == 10U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(isoFrame));

    // OSD Overlays
    const auto timeDis = PelcoD::FujinonBuilder::buildSetTimeDisplay(addr, true);
    EXPECT_TRUE(timeDis.size() == 7U && timeDis[3] == 0x43U && timeDis[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(timeDis));

    const auto timePos = PelcoD::FujinonBuilder::buildSetTimePosition(addr, PelcoD::FujinonOSDPosition::TopRight);
    EXPECT_TRUE(timePos.size() == 7U && timePos[3] == 0x45U && timePos[5] == 0x03U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(timePos));

    const auto titleDis = PelcoD::FujinonBuilder::buildSetTitleDisplay(addr, true);
    EXPECT_TRUE(titleDis.size() == 7U && titleDis[3] == 0x47U && titleDis[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(titleDis));

    const auto titlePos = PelcoD::FujinonBuilder::buildSetTitlePosition(addr, PelcoD::FujinonOSDPosition::BottomLeft);
    EXPECT_TRUE(titlePos.size() == 7U && titlePos[3] == 0x4BU && titlePos[5] == 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(titlePos));

    const auto idDis = PelcoD::FujinonBuilder::buildSetIdDisplay(addr, false);
    EXPECT_TRUE(idDis.size() == 7U && idDis[3] == 0x4DU && idDis[5] == 0x00U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(idDis));

    const auto idPos = PelcoD::FujinonBuilder::buildSetIdPosition(addr, PelcoD::FujinonOSDPosition::BottomRight);
    EXPECT_TRUE(idPos.size() == 7U && idPos[3] == 0x4FU && idPos[5] == 0x04U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(idPos));

    const auto reticleDis = PelcoD::FujinonBuilder::buildSetReticleDisplay(addr, true);
    EXPECT_TRUE(reticleDis.size() == 7U && reticleDis[3] == 0x51U && reticleDis[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(reticleDis));

    // Video Mode, HD Format, RS-485 termination, Reboot
    const auto vmFrame = PelcoD::FujinonBuilder::buildSetVideoMode(addr, PelcoD::FujinonVideoMode::PAL);
    EXPECT_TRUE(vmFrame.size() == 7U && vmFrame[3] == 0x67U && vmFrame[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(vmFrame));

    const auto hdFrame = PelcoD::FujinonBuilder::buildSetHDFormat(addr, PelcoD::FujinonHDFormat::F1080p30);
    EXPECT_TRUE(hdFrame.size() == 7U && hdFrame[3] == 0x69U && hdFrame[5] == 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(hdFrame));

    const auto termFrame = PelcoD::FujinonBuilder::buildSetRS485Termination(addr, true);
    EXPECT_TRUE(termFrame.size() == 7U && termFrame[3] == 0x73U && termFrame[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(termFrame));

    const auto rebootFrame = PelcoD::FujinonBuilder::buildReboot(addr);
    EXPECT_TRUE(rebootFrame.size() == 7U && rebootFrame[3] == 0x83U && rebootFrame[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(rebootFrame));

    // Media & OSD Menu Navigation
    const auto recFrame = PelcoD::FujinonBuilder::buildRecordLiveView(addr, true);
    EXPECT_TRUE(recFrame.size() == 7U && recFrame[3] == 0x93U && recFrame[5] == 0x00U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(recFrame));

    const auto playFrame = PelcoD::FujinonBuilder::buildPlayMovie(addr, false);
    EXPECT_TRUE(playFrame.size() == 7U && playFrame[3] == 0x97U && playFrame[5] == 0x00U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(playFrame));

    const auto movieModeFrame = PelcoD::FujinonBuilder::buildSetMovieMode(addr, true);
    EXPECT_TRUE(movieModeFrame.size() == 7U && movieModeFrame[3] == 0x9BU && movieModeFrame[5] == 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(movieModeFrame));

    const auto menuOkFrame = PelcoD::FujinonBuilder::buildMenuOk(addr);
    EXPECT_TRUE(menuOkFrame.size() == 7U && menuOkFrame[3] == 0xA7U && menuOkFrame[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(menuOkFrame));

    const auto menuDirFrame = PelcoD::FujinonBuilder::buildMenuDirection(addr, PelcoD::FujinonMenuDirection::Down);
    EXPECT_TRUE(menuDirFrame.size() == 7U && menuDirFrame[3] == 0xA9U && menuDirFrame[5] == 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(menuDirFrame));

    // Query Focus, Zoom, Serial Number, Firmware Version, Lens Status
    const auto qFocus = PelcoD::FujinonBuilder::buildQueryFocus(addr);
    EXPECT_TRUE(qFocus[2] == 0x00U && qFocus[3] == 0x81U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qFocus));

    const auto qZoom = PelcoD::FujinonBuilder::buildQueryZoom(addr);
    EXPECT_TRUE(qZoom[2] == 0x00U && qZoom[3] == 0x83U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qZoom));

    const auto qSerial = PelcoD::FujinonBuilder::buildQuerySerialNumber(addr);
    EXPECT_TRUE(qSerial[2] == 0x00U && qSerial[3] == 0x89U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qSerial));

    const auto qFw = PelcoD::FujinonBuilder::buildQueryFirmwareVersion(addr);
    EXPECT_TRUE(qFw[2] == 0x00U && qFw[3] == 0x8BU);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qFw));

    const auto qLens = PelcoD::FujinonBuilder::buildQueryLensStatus(addr);
    EXPECT_TRUE(qLens[2] == 0x00U && qLens[3] == 0x8DU);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qLens));

    const auto qPhoto = PelcoD::FujinonBuilder::buildQueryPhotoSettings(addr, 0x13U);
    EXPECT_TRUE(qPhoto[2] == 0xF0U && qPhoto[3] == 0x1DU && qPhoto[4] == 0x13U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qPhoto));

    const auto qImg = PelcoD::FujinonBuilder::buildQueryImageQuality(addr, 0x29U);
    EXPECT_TRUE(qImg[2] == 0xF0U && qImg[3] == 0x3DU && qImg[4] == 0x29U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qImg));

    const auto qManual = PelcoD::FujinonBuilder::buildQueryManualSettings(addr);
    EXPECT_TRUE(qManual[2] == 0x00U && qManual[3] == 0xADU);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qManual));

    std::cout << "  testBuilderCommands: PASSED\n";
}

TEST(FujinonProtocolTest, ParserResponses)
{
    const std::uint8_t addr = 1U;

    // Focus response: FF 01 00 81 20 50 CKSM
    const auto focusFrame = PelcoD::PelcoDFrame::createFrame(addr, 0x00U, 0x81U, 0x20U, 0x50U);
    EXPECT_TRUE(PelcoD::FujinonParser::isFujinonResponse(focusFrame));

    std::uint16_t focusVal { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryFocus(focusFrame, focusVal));
    EXPECT_TRUE(focusVal == 0x2050U);

    // Zoom response: FF 01 00 83 40 10 CKSM
    const auto zoomFrame = PelcoD::PelcoDFrame::createFrame(addr, 0x00U, 0x83U, 0x40U, 0x10U);
    EXPECT_TRUE(PelcoD::FujinonParser::isFujinonResponse(zoomFrame));

    std::uint16_t zoomVal { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryZoom(zoomFrame, zoomVal));
    EXPECT_TRUE(zoomVal == 0x4010U);

    // Firmware Version response: FF 01 00 8B 02 51 CKSM (v2.51)
    const auto fwFrame = PelcoD::PelcoDFrame::createFrame(addr, 0x00U, 0x8BU, 0x02U, 0x51U);
    EXPECT_TRUE(PelcoD::FujinonParser::isFujinonResponse(fwFrame));
    std::string fwStr;
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryFirmwareVersion(fwFrame, fwStr));
    EXPECT_TRUE(fwStr == "v2.51");

    // Lens Status response: FF 01 00 8D 07 00 CKSM (DATA1 = lens status, DATA2 = 0x00)
    const auto lensFrame = PelcoD::PelcoDFrame::createFrame(addr, 0x00U, 0x8DU, 0x07U, 0x00U);
    EXPECT_TRUE(PelcoD::FujinonParser::isFujinonResponse(lensFrame));
    std::uint8_t lensStatus { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryLensStatus(lensFrame, lensStatus));
    EXPECT_TRUE(lensStatus == 0x07U);

    // 18-byte Serial Number response: FF 01 'S''X''8''0''0''0''0''1' 0 0 0 0 0 0 0 CKSM
    std::vector<std::uint8_t> serialFrame { 0xFFU, addr, 'S', 'X', '8', '0', '0', '0', '0', '1', 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U };
    std::uint32_t cksm { 0U };
    for (std::size_t i = 1; i < serialFrame.size(); ++i) {
        cksm += serialFrame[i];
    }
    serialFrame.push_back(static_cast<std::uint8_t>(cksm % 256U));
    EXPECT_TRUE(PelcoD::FujinonParser::isFujinonResponse(serialFrame));
    std::string serialStr;
    EXPECT_TRUE(PelcoD::FujinonParser::parseQuerySerialNumber(serialFrame, serialStr));
    EXPECT_TRUE(serialStr == "SX800001");

    // 7-byte Photo Setting response for OIS: FF 01 F0 1F 13 02 CKSM
    const auto oisResp = PelcoD::PelcoDFrame::createFrame(addr, 0xF0U, 0x1FU, 0x13U, 0x02U);
    PelcoD::FujinonPhotoSettings photo;
    EXPECT_TRUE(PelcoD::FujinonParser::parsePhotoSettings(oisResp, photo));
    EXPECT_TRUE(photo.oisMode == PelcoD::FujinonOISMode::OisOn);

    // 18-byte Photo Setting response
    std::vector<std::uint8_t> fullPhotoFrame(18U, 0x00U);
    fullPhotoFrame[0] = 0xFFU;
    fullPhotoFrame[1] = addr;
    fullPhotoFrame[2] = 0x01U; // afArea
    fullPhotoFrame[3] = 0x02U; // afSensitivity
    fullPhotoFrame[4] = 0x03U; // dayNight Night
    fullPhotoFrame[5] = 0x02U; // ir 940nm
    fullPhotoFrame[6] = 0x02U; // ois OisOn
    std::uint32_t photoCksm { 0U };
    for (std::size_t i = 1; i < 17U; ++i) {
        photoCksm += fullPhotoFrame[i];
    }
    fullPhotoFrame[17] = static_cast<std::uint8_t>(photoCksm % 256U);
    PelcoD::FujinonPhotoSettings fullPhoto;
    EXPECT_TRUE(PelcoD::FujinonParser::parsePhotoSettings(fullPhotoFrame, fullPhoto));
    EXPECT_TRUE(fullPhoto.afArea == 0x01U);
    EXPECT_TRUE(fullPhoto.afSensitivity == 0x02U);
    EXPECT_TRUE(fullPhoto.dayNightMode == PelcoD::FujinonDayNightMode::Night);
    EXPECT_TRUE(fullPhoto.irWavelength == PelcoD::FujinonIRWavelength::W940nm);
    EXPECT_TRUE(fullPhoto.oisMode == PelcoD::FujinonOISMode::OisOn);

    // 7-byte Image Quality response for Defog: FF 01 F0 2B 29 03 CKSM
    const auto defogResp = PelcoD::PelcoDFrame::createFrame(addr, 0xF0U, 0x2BU, 0x29U, 0x03U);
    PelcoD::FujinonImageQualitySettings img;
    EXPECT_TRUE(PelcoD::FujinonParser::parseImageQualitySettings(defogResp, img));
    EXPECT_TRUE(img.defog == PelcoD::FujinonDefogLevel::Level3);

    // 18-byte Image Quality response
    std::vector<std::uint8_t> fullImgFrame(18U, 0x00U);
    fullImgFrame[0] = 0xFFU;
    fullImgFrame[1] = addr;
    fullImgFrame[2] = 0x01U; // vlcFilter true
    fullImgFrame[3] = 0x03U; // wdr Level3
    fullImgFrame[4] = 0x01U; // heatHaze Level1
    fullImgFrame[5] = 0x02U; // defog Level2
    fullImgFrame[6] = 50U; // brightness
    fullImgFrame[7] = 40U; // contrast
    fullImgFrame[8] = 30U; // saturation
    fullImgFrame[9] = 20U; // sharpness
    fullImgFrame[10] = 0x04U; // digitalZoom X2
    fullImgFrame[11] = 0x03U; // noiseReduction 3
    std::uint32_t imgCksm { 0U };
    for (std::size_t i = 1; i < 17U; ++i) {
        imgCksm += fullImgFrame[i];
    }
    fullImgFrame[17] = static_cast<std::uint8_t>(imgCksm % 256U);
    PelcoD::FujinonImageQualitySettings fullImg;
    EXPECT_TRUE(PelcoD::FujinonParser::parseImageQualitySettings(fullImgFrame, fullImg));
    EXPECT_TRUE(fullImg.vlcFilter);
    EXPECT_TRUE(fullImg.wdr == PelcoD::FujinonWDRLevel::Level3);
    EXPECT_TRUE(fullImg.heatHaze == PelcoD::FujinonHeatHazeLevel::Level1);
    EXPECT_TRUE(fullImg.defog == PelcoD::FujinonDefogLevel::Level2);
    EXPECT_TRUE(fullImg.brightness == 50U);
    EXPECT_TRUE(fullImg.contrast == 40U);
    EXPECT_TRUE(fullImg.saturation == 30U);
    EXPECT_TRUE(fullImg.sharpness == 20U);
    EXPECT_TRUE(fullImg.digitalZoom == PelcoD::FujinonDigitalZoom::X2);
    EXPECT_TRUE(fullImg.noiseReduction == 3U);

    // 7-byte Manual Setting response: FF 01 00 8F 0A 14 CKSM
    const auto manual7Resp = PelcoD::PelcoDFrame::createFrame(addr, 0x00U, 0x8FU, 0x0AU, 0x14U);
    PelcoD::FujinonManualSettings manual7;
    EXPECT_TRUE(PelcoD::FujinonParser::parseManualSettings(manual7Resp, manual7));
    EXPECT_TRUE(manual7.manualIris == 0x0AU);
    EXPECT_TRUE(manual7.manualShutter == 0x14U);

    // 18-byte Manual Setting response
    std::vector<std::uint8_t> fullManFrame(18U, 0x00U);
    fullManFrame[0] = 0xFFU;
    fullManFrame[1] = addr;
    fullManFrame[2] = 0x0AU; // iris
    fullManFrame[3] = 0x14U; // shutter
    fullManFrame[4] = 0x1EU; // iso
    std::uint32_t manCksm { 0U };
    for (std::size_t i = 1; i < 17U; ++i) {
        manCksm += fullManFrame[i];
    }
    fullManFrame[17] = static_cast<std::uint8_t>(manCksm % 256U);
    PelcoD::FujinonManualSettings fullMan;
    EXPECT_TRUE(PelcoD::FujinonParser::parseManualSettings(fullManFrame, fullMan));
    EXPECT_TRUE(fullMan.manualIris == 0x0AU);
    EXPECT_TRUE(fullMan.manualShutter == 0x14U);
    EXPECT_TRUE(fullMan.manualISO == 0x1EU);

    // Status updates
    PelcoD::FujinonStatus status;
    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(focusFrame, status));
    EXPECT_TRUE(status.absoluteFocusPosition == 0x2050U);

    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(oisResp, status));
    EXPECT_TRUE(status.oisMode == PelcoD::FujinonOISMode::OisOn);

    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(defogResp, status));
    EXPECT_TRUE(status.defogLevel == PelcoD::FujinonDefogLevel::Level3);

    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(fwFrame, status));
    EXPECT_TRUE(status.firmwareVersion == "v2.51");

    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(lensFrame, status));
    EXPECT_TRUE(status.lensStatus == 0x07U);

    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(serialFrame, status));
    EXPECT_TRUE(status.serialNumber == "SX800001");

    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(manual7Resp, status));
    EXPECT_TRUE(status.manualIris == 0x0AU);
    EXPECT_TRUE(status.manualShutter == 0x14U);

    std::cout << "  testParserResponses: PASSED\n";
}

TEST(FujinonProtocolTest, V2120Extensions)
{
    const std::uint8_t addr = 1U;

    // 1. Fine Image Quality Builder
    const auto bFine = PelcoD::FujinonBuilder::buildSetBrightnessFine(addr, 75U);
    EXPECT_TRUE(bFine.size() == 7U && bFine[2] == 0xF0U && bFine[3] == 0xEBU && bFine[5] == 75U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(bFine));

    const auto cFine = PelcoD::FujinonBuilder::buildSetContrastFine(addr, 65U);
    EXPECT_TRUE(cFine.size() == 7U && cFine[2] == 0xF0U && cFine[3] == 0xEDU && cFine[5] == 65U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(cFine));

    const auto sFine = PelcoD::FujinonBuilder::buildSetSaturationFine(addr, 55U);
    EXPECT_TRUE(sFine.size() == 7U && sFine[2] == 0xF0U && sFine[3] == 0xEFU && sFine[5] == 55U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(sFine));

    const auto shFine = PelcoD::FujinonBuilder::buildSetSharpnessFine(addr, 45U);
    EXPECT_TRUE(shFine.size() == 7U && shFine[2] == 0xF0U && shFine[3] == 0xF1U && shFine[5] == 45U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(shFine));

    const auto rFine = PelcoD::FujinonBuilder::buildSetWBShiftRedFine(addr, 80U);
    EXPECT_TRUE(rFine.size() == 7U && rFine[2] == 0xF0U && rFine[3] == 0xF5U && rFine[5] == 80U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(rFine));

    const auto blFine = PelcoD::FujinonBuilder::buildSetWBShiftBlueFine(addr, 40U);
    EXPECT_TRUE(blFine.size() == 7U && blFine[2] == 0xF0U && blFine[3] == 0xF7U && blFine[5] == 40U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(blFine));

    const auto qFine = PelcoD::FujinonBuilder::buildQueryImageQualityFine(addr, 0xEBU);
    EXPECT_TRUE(qFine.size() == 7U && qFine[2] == 0xF0U && qFine[3] == 0xFDU && qFine[4] == 0xEBU);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qFine));

    // 2. DayNight Ex Builder
    const auto dnEx = PelcoD::FujinonBuilder::buildSetDayNightModeEx(addr, PelcoD::FujinonDayNightModeEx::Scheduled);
    EXPECT_TRUE(dnEx.size() == 7U && dnEx[2] == 0xF1U && dnEx[3] == 0x01U && dnEx[5] == 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(dnEx));

    const auto d2n = PelcoD::FujinonBuilder::buildSetDayToNightThreshold(addr, 128U);
    EXPECT_TRUE(d2n.size() == 7U && d2n[2] == 0xF1U && d2n[3] == 0x03U && d2n[5] == 128U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(d2n));

    const auto n2d = PelcoD::FujinonBuilder::buildSetNightToDayThreshold(addr, 64U);
    EXPECT_TRUE(n2d.size() == 7U && n2d[2] == 0xF1U && n2d[3] == 0x05U && n2d[5] == 64U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(n2d));

    const auto delay = PelcoD::FujinonBuilder::buildSetDayNightAutoDelay(addr, 15U);
    EXPECT_TRUE(delay.size() == 7U && delay[2] == 0xF1U && delay[3] == 0x07U && delay[5] == 15U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(delay));

    const auto dTime = PelcoD::FujinonBuilder::buildSetDayStartTime(addr, 6U, 30U);
    EXPECT_TRUE(dTime.size() == 7U && dTime[2] == 0xF1U && dTime[3] == 0x09U && dTime[4] == 6U && dTime[5] == 30U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(dTime));

    const auto nTime = PelcoD::FujinonBuilder::buildSetNightStartTime(addr, 18U, 45U);
    EXPECT_TRUE(nTime.size() == 7U && nTime[2] == 0xF1U && nTime[3] == 0x0BU && nTime[4] == 18U && nTime[5] == 45U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(nTime));

    const auto optDay = PelcoD::FujinonBuilder::buildSetOpticalFilterDay(addr, false);
    EXPECT_TRUE(optDay.size() == 7U && optDay[2] == 0xF1U && optDay[3] == 0x0DU && optDay[5] == 0x00U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(optDay));

    const auto optNight = PelcoD::FujinonBuilder::buildSetOpticalFilterNight(addr, true);
    EXPECT_TRUE(optNight.size() == 7U && optNight[2] == 0xF1U && optNight[3] == 0x0FU && optNight[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(optNight));

    const auto qDnEx = PelcoD::FujinonBuilder::buildQueryDayNightEx(addr, 0x01U);
    EXPECT_TRUE(qDnEx.size() == 7U && qDnEx[2] == 0xF1U && qDnEx[3] == 0x1DU && qDnEx[4] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qDnEx));

    // 3. ZoomFocus Ex Builder
    const auto zSpeed = PelcoD::FujinonBuilder::buildSetZoomSpeedEx(addr, 5U);
    EXPECT_TRUE(zSpeed.size() == 7U && zSpeed[2] == 0xF1U && zSpeed[3] == 0x25U && zSpeed[5] == 5U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(zSpeed));

    const auto fSpeed = PelcoD::FujinonBuilder::buildSetFocusSpeedEx(addr, 3U);
    EXPECT_TRUE(fSpeed.size() == 7U && fSpeed[2] == 0xF1U && fSpeed[3] == 0x27U && fSpeed[5] == 3U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(fSpeed));

    const auto dzMode = PelcoD::FujinonBuilder::buildSetDigitalZoomMode(addr, PelcoD::FujinonDigitalZoomMode::CropMode);
    EXPECT_TRUE(dzMode.size() == 7U && dzMode[2] == 0xF0U && dzMode[3] == 0x37U && dzMode[5] == 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(dzMode));

    const auto qZfEx = PelcoD::FujinonBuilder::buildQueryZoomFocusEx(addr, 0x25U);
    EXPECT_TRUE(qZfEx.size() == 7U && qZfEx[2] == 0xF1U && qZfEx[3] == 0x2DU && qZfEx[4] == 0x25U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qZfEx));

    // 4. Extended Commands
    const auto aaOn = PelcoD::FujinonBuilder::buildSetAntialiasing(addr, true);
    EXPECT_TRUE(aaOn.size() == 7U && aaOn[2] == 0xF0U && aaOn[3] == 0x55U && aaOn[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(aaOn));

    const auto mBack = PelcoD::FujinonBuilder::buildMenuBack(addr);
    EXPECT_TRUE(mBack.size() == 7U && mBack[2] == 0xF0U && mBack[3] == 0xABU && mBack[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(mBack));

    const auto fmtSD = PelcoD::FujinonBuilder::buildFormatSDCard(addr);
    EXPECT_TRUE(fmtSD.size() == 7U && fmtSD[2] == 0xF0U && fmtSD[3] == 0x7BU && fmtSD[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(fmtSD));

    const auto fReset = PelcoD::FujinonBuilder::buildFactoryReset(addr);
    EXPECT_TRUE(fReset.size() == 7U && fReset[2] == 0xF0U && fReset[3] == 0x81U && fReset[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(fReset));

    const auto lang = PelcoD::FujinonBuilder::buildSetLanguage(addr, PelcoD::FujinonLanguage::Japanese);
    EXPECT_TRUE(lang.size() == 7U && lang[2] == 0xF0U && lang[3] == 0x85U && lang[5] == 0x03U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(lang));

    // 5. RTC Commands
    const auto rtcSec = PelcoD::FujinonBuilder::buildSetRTCSecond(addr, 45U);
    EXPECT_TRUE(rtcSec.size() == 7U && rtcSec[2] == 0x00U && rtcSec[3] == 0x3BU && rtcSec[5] == 45U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(rtcSec));

    const auto rtcHm = PelcoD::FujinonBuilder::buildSetRTCHourMinute(addr, 14U, 30U);
    EXPECT_TRUE(rtcHm.size() == 7U && rtcHm[2] == 0x02U && rtcHm[3] == 0x3BU && rtcHm[4] == 14U && rtcHm[5] == 30U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(rtcHm));

    const auto rtcMd = PelcoD::FujinonBuilder::buildSetRTCMonthDay(addr, 9U, 14U);
    EXPECT_TRUE(rtcMd.size() == 7U && rtcMd[2] == 0x04U && rtcMd[3] == 0x3BU && rtcMd[4] == 9U && rtcMd[5] == 14U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(rtcMd));

    const auto rtcYr = PelcoD::FujinonBuilder::buildSetRTCYear(addr, 2026U);
    EXPECT_TRUE(rtcYr.size() == 7U && rtcYr[2] == 0x06U && rtcYr[3] == 0x3BU && rtcYr[4] == 0x07U && rtcYr[5] == 0xEAU);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(rtcYr));

    const auto qRtc = PelcoD::FujinonBuilder::buildQueryRTC(addr, 0x01U);
    EXPECT_TRUE(qRtc.size() == 7U && qRtc[2] == 0x01U && qRtc[3] == 0x3BU);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qRtc));

    // 6. Standard Pelco-D Zoom Position Query
    const auto qZoomStd = PelcoD::FujinonBuilder::buildQueryZoomStandard(addr);
    EXPECT_TRUE(qZoomStd.size() == 7U && qZoomStd[2] == 0x00U && qZoomStd[3] == 0x55U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qZoomStd));

    // 7. Focal Length Calculation
    EXPECT_TRUE(PelcoD::FujinonBuilder::rawZoomToFocalLengthMm(0x0000U) == 20.0);
    EXPECT_TRUE(PelcoD::FujinonBuilder::rawZoomToFocalLengthMm(0x4000U) == 800.0);
    EXPECT_TRUE(PelcoD::FujinonBuilder::rawZoomToFocalLengthMm(0x2000U) == 410.0); // halfway: 20 + 0.5*780 = 410
    EXPECT_TRUE(PelcoD::FujinonBuilder::rawZoomToFocalLengthMm(0x4000U, PelcoD::FujinonDigitalZoom::X2) == 1600.0);

    // 8. Parser for Zoom Standard (0x00 0x5D)
    const auto respZoomStd = PelcoD::PelcoDFrame::createFrame(addr, 0x00U, 0x5DU, 0x20U, 0x00U);
    EXPECT_TRUE(PelcoD::FujinonParser::isFujinonResponse(respZoomStd));
    std::uint16_t zPosStd { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryZoomStandard(respZoomStd, zPosStd));
    EXPECT_TRUE(zPosStd == 0x2000U);

    // 9. Parser for Fine Image Quality (0xF0 0xFF)
    const auto respFine = PelcoD::PelcoDFrame::createFrame(addr, 0xF0U, 0xFFU, 0xEBU, 75U);
    EXPECT_TRUE(PelcoD::FujinonParser::isFujinonResponse(respFine));
    PelcoD::FujinonFineImageSettings fineSettings;
    EXPECT_TRUE(PelcoD::FujinonParser::parseFineImageSettings(respFine, fineSettings));
    EXPECT_TRUE(fineSettings.brightness == 75U);

    // 10. Parser for DayNightEx (0xF1 0x1F)
    const auto respDnEx = PelcoD::PelcoDFrame::createFrame(addr, 0xF1U, 0x1FU, 0x00U, 0x02U);
    EXPECT_TRUE(PelcoD::FujinonParser::isFujinonResponse(respDnEx));
    PelcoD::FujinonDayNightExSettings dnExSettings;
    EXPECT_TRUE(PelcoD::FujinonParser::parseDayNightExSettings(respDnEx, dnExSettings, 0x01U));
    EXPECT_TRUE(dnExSettings.mode == PelcoD::FujinonDayNightModeEx::Scheduled);

    // 11. Parser for ZoomFocusEx (0xF1 0x2F)
    const auto respZfEx = PelcoD::PelcoDFrame::createFrame(addr, 0xF1U, 0x2FU, 0x00U, 0x05U);
    EXPECT_TRUE(PelcoD::FujinonParser::isFujinonResponse(respZfEx));
    PelcoD::FujinonZoomFocusExSettings zfExSettings;
    EXPECT_TRUE(PelcoD::FujinonParser::parseZoomFocusExSettings(respZfEx, zfExSettings, 0x25U));
    EXPECT_TRUE(zfExSettings.zoomSpeedEx == 5U);

    // 12. Parser for RTC (0x00 0x3B)
    const auto respRTC = PelcoD::PelcoDFrame::createFrame(addr, 0x00U, 0x3BU, 14U, 30U);
    EXPECT_TRUE(PelcoD::FujinonParser::isFujinonResponse(respRTC));
    std::uint8_t rtcD1 { 0U }, rtcD2 { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryRTC(respRTC, rtcD1, rtcD2));
    EXPECT_TRUE(rtcD1 == 14U && rtcD2 == 30U);

    // 13. Update FujinonStatus with v2.12.0 telemetry
    PelcoD::FujinonStatus st;
    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(respZoomStd, st));
    EXPECT_TRUE(st.absoluteZoomPosition == 0x2000U);
    EXPECT_TRUE(st.focalLengthMm == 410.0);

    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(respFine, st));
    EXPECT_TRUE(st.fineImageSettings.brightness == 75U);

    const auto aaResp = PelcoD::PelcoDFrame::createFrame(addr, 0xF0U, 0x55U, 0x00U, 0x01U);
    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(aaResp, st));
    EXPECT_TRUE(st.antialiasing);

    const auto langResp = PelcoD::PelcoDFrame::createFrame(addr, 0xF0U, 0x85U, 0x00U, 0x03U);
    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(langResp, st));
    EXPECT_TRUE(st.language == PelcoD::FujinonLanguage::Japanese);

    std::cout << "  testV2120Extensions: PASSED\n";
}

TEST(FujinonProtocolTest, AdditionalV2120Commands)
{
    const std::uint8_t addr = 1U;

    // Zoom Position
    const auto zp = PelcoD::FujinonBuilder::buildSetZoomPosition(addr, 0x4000U);
    EXPECT_TRUE(zp.size() == 7U && zp[2] == 0x00U && zp[3] == 0x4FU && zp[4] == 0x40U && zp[5] == 0x00U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(zp));

    // Standard Zoom / Focus Speed (0..3)
    const auto zs = PelcoD::FujinonBuilder::buildSetZoomSpeed(addr, 2U);
    EXPECT_TRUE(zs.size() == 7U && zs[2] == 0x00U && zs[3] == 0x25U && zs[5] == 2U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(zs));

    const auto fs = PelcoD::FujinonBuilder::buildSetFocusSpeed(addr, 1U);
    EXPECT_TRUE(fs.size() == 7U && fs[2] == 0x00U && fs[3] == 0x27U && fs[5] == 1U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(fs));

    // Auto Focus mode
    const auto afm = PelcoD::FujinonBuilder::buildSetAutoFocus(addr, PelcoD::FujinonAutoFocusMode::QuickAF);
    EXPECT_TRUE(afm.size() == 7U && afm[2] == 0x00U && afm[3] == 0x2BU && afm[5] == 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(afm));

    // Auto Iris mode
    const auto aim = PelcoD::FujinonBuilder::buildSetAutoIris(addr, PelcoD::FujinonAutoIrisMode::Manual);
    EXPECT_TRUE(aim.size() == 7U && aim[2] == 0x00U && aim[3] == 0x2DU && aim[5] == 0x00U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(aim));

    // AGC
    const auto agc = PelcoD::FujinonBuilder::buildSetAGC(addr, PelcoD::FujinonAGCMode::Hyper);
    EXPECT_TRUE(agc.size() == 7U && agc[2] == 0x00U && agc[3] == 0x2FU && agc[5] == 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(agc));

    // BLC
    const auto blc = PelcoD::FujinonBuilder::buildSetBLC(addr, PelcoD::FujinonBLCMode::Soft);
    EXPECT_TRUE(blc.size() == 7U && blc[2] == 0x00U && blc[3] == 0x31U && blc[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(blc));

    // Photo Mode Preset
    const auto pmp
        = PelcoD::FujinonBuilder::buildSetPhotoModePreset(addr, PelcoD::FujinonPhotoModePreset::Surveillance);
    EXPECT_TRUE(pmp.size() == 7U && pmp[2] == 0xF0U && pmp[3] == 0x17U && pmp[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(pmp));

    // External DayNight
    const auto edn = PelcoD::FujinonBuilder::buildSetExternalDayNight(addr, PelcoD::FujinonExternalDayNight::OnClear);
    EXPECT_TRUE(edn.size() == 7U && edn[2] == 0xF0U && edn[3] == 0x19U && edn[5] == 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(edn));

    // Time & Date format
    const auto tf = PelcoD::FujinonBuilder::buildSetTimeFormat(addr, PelcoD::FujinonTimeFormat::Mode24H);
    EXPECT_TRUE(tf.size() == 7U && tf[2] == 0xF0U && tf[3] == 0x61U && tf[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(tf));

    const auto df = PelcoD::FujinonBuilder::buildSetDateFormat(addr, PelcoD::FujinonDateFormat::DMY_Hyphen);
    EXPECT_TRUE(df.size() == 7U && df[2] == 0xF0U && df[3] == 0x63U && df[5] == 0x03U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(df));

    // Video Display Mode
    const auto vd = PelcoD::FujinonBuilder::buildSetVideoDisplay(addr, PelcoD::FujinonVideoDisplayMode::Squeeze);
    EXPECT_TRUE(vd.size() == 7U && vd[2] == 0xF0U && vd[3] == 0x6BU && vd[5] == 0x03U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(vd));

    // SD Mode & Capacity
    const auto sdm = PelcoD::FujinonBuilder::buildSetSDRecordingMode(addr, PelcoD::FujinonSDRecordMode::Overwrite);
    EXPECT_TRUE(sdm.size() == 7U && sdm[2] == 0xF0U && sdm[3] == 0x77U && sdm[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(sdm));

    const auto sdc = PelcoD::FujinonBuilder::buildSetSDCapacityDisp(addr, true);
    EXPECT_TRUE(sdc.size() == 7U && sdc[2] == 0xF0U && sdc[3] == 0x79U && sdc[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(sdc));

    // Video Encode format
    const auto enc = PelcoD::FujinonBuilder::buildSetEncodeMode(addr, PelcoD::FujinonVideoEncoding::H264);
    EXPECT_TRUE(enc.size() == 7U && enc[2] == 0xF0U && enc[3] == 0x91U && enc[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(enc));

    // Log recording
    const auto logRec = PelcoD::FujinonBuilder::buildRecordLogData(addr);
    EXPECT_TRUE(logRec.size() == 7U && logRec[2] == 0xF0U && logRec[3] == 0x7FU && logRec[5] == 0x01U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(logRec));

    const auto qLog = PelcoD::FujinonBuilder::buildQueryLogData(addr, PelcoD::FujinonLogType::Focus);
    EXPECT_TRUE(qLog.size() == 7U && qLog[2] == 0xF0U && qLog[3] == 0xD1U && qLog[4] == 0xD3U && qLog[5] == 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qLog));

    // Movie controls
    const auto firstMov = PelcoD::FujinonBuilder::buildSetFirstMovieFile(addr, 42U);
    EXPECT_TRUE(firstMov.size() == 7U && firstMov[2] == 0xF0U && firstMov[3] == 0x95U && firstMov[5] == 42U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(firstMov));

    const auto playMMode = PelcoD::FujinonBuilder::buildPlayMovieMode(addr, PelcoD::FujinonMoviePlayMode::Continuous);
    EXPECT_TRUE(playMMode.size() == 7U && playMMode[2] == 0xF0U && playMMode[3] == 0x97U && playMMode[5] == 0x02U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(playMMode));

    const auto delMov = PelcoD::FujinonBuilder::buildDeleteMovie(addr, 10U);
    EXPECT_TRUE(delMov.size() == 7U && delMov[2] == 0xF0U && delMov[3] == 0x99U && delMov[5] == 10U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(delMov));

    // Query Commands
    const auto qDisp = PelcoD::FujinonBuilder::buildQueryDisplay(addr, 0x43U);
    EXPECT_TRUE(qDisp.size() == 7U && qDisp[2] == 0xF0U && qDisp[3] == 0x5DU && qDisp[4] == 0x43U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qDisp));

    const auto qKey = PelcoD::FujinonBuilder::buildQueryKeySetting(addr, 0x73U);
    EXPECT_TRUE(qKey.size() == 7U && qKey[2] == 0xF0U && qKey[3] == 0xADU && qKey[4] == 0x73U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qKey));

    const auto qMovCnt = PelcoD::FujinonBuilder::buildQueryMovieCount(addr);
    EXPECT_TRUE(qMovCnt.size() == 7U && qMovCnt[2] == 0xF0U && qMovCnt[3] == 0xB1U && qMovCnt[4] == 0xB3U);
    EXPECT_TRUE(PelcoD::PelcoDFrame::isValidFrame(qMovCnt));

    std::cout << "  testAdditionalV2120Commands: PASSED\n";
}

TEST(FujinonProtocolTest, ParserDecodesManualExposureResponses)
{
    const std::uint8_t addr = 1U;

    // Manual Shutter response: [FF, addr, 0x00, 0x97, 0x00, 10, Checksum]
    const auto shutterResp = PelcoD::PelcoDFrame::createFrame(addr, 0x00U, 0x97U, 0x00U, 10U);
    std::uint8_t shutterVal { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryManualShutter(shutterResp, shutterVal));
    EXPECT_EQ(shutterVal, 10U);

    // Manual ISO response: [FF, addr, 0x00, 0x9B, 0x00, 25, Checksum]
    const auto isoResp = PelcoD::PelcoDFrame::createFrame(addr, 0x00U, 0x9BU, 0x00U, 25U);
    std::uint8_t isoVal { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryManualISO(isoResp, isoVal));
    EXPECT_EQ(isoVal, 25U);

    // Manual Iris response: [FF, addr, 0x00, 0x9D, 0x00, 7, Checksum]
    const auto irisResp = PelcoD::PelcoDFrame::createFrame(addr, 0x00U, 0x9DU, 0x00U, 7U);
    std::uint8_t irisVal { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryManualIris(irisResp, irisVal));
    EXPECT_EQ(irisVal, 7U);

    // Status update check
    PelcoD::FujinonStatus status {};
    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(shutterResp, status));
    EXPECT_EQ(status.manualShutter, 10U);

    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(isoResp, status));
    EXPECT_EQ(status.manualISO, 25U);

    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(irisResp, status));
    EXPECT_EQ(status.manualIris, 7U);
}

TEST(FujinonProtocolTest, ParserDecodesMovieAndDisplayQueries)
{
    const std::uint8_t addr = 1U;

    // Movie count response: [FF, addr, 0xF0, 0xB3, 0x01, 0x2C, Checksum] -> 300 files
    const auto countResp = PelcoD::PelcoDFrame::createFrame(addr, 0xF0U, 0xB3U, 0x01U, 0x2CU);
    std::uint16_t movieCount { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryMovieCount(countResp, movieCount));
    EXPECT_EQ(movieCount, 300U);

    // Movie year response: [FF, addr, 0xF0, 0xB7, 0x07, 0xEAU, Checksum] -> 2026
    const auto yearResp = PelcoD::PelcoDFrame::createFrame(addr, 0xF0U, 0xB7U, 0x07U, 0xEAU);
    std::uint16_t movieYear { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryMovieYear(yearResp, movieYear));
    EXPECT_EQ(movieYear, 2026U);

    // Movie date response: [FF, addr, 0xF0, 0xBB, 10, 16, Checksum] -> Oct 16
    const auto dateResp = PelcoD::PelcoDFrame::createFrame(addr, 0xF0U, 0xBBU, 10U, 16U);
    std::uint8_t month { 0U };
    std::uint8_t day { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryMovieDate(dateResp, month, day));
    EXPECT_EQ(month, 10U);
    EXPECT_EQ(day, 16U);

    // Movie time response: [FF, addr, 0xF0, 0xBF, 14, 30, Checksum] -> 14:30
    const auto timeResp = PelcoD::PelcoDFrame::createFrame(addr, 0xF0U, 0xBFU, 14U, 30U);
    std::uint8_t hour { 0U };
    std::uint8_t min { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryMovieTime(timeResp, hour, min));
    EXPECT_EQ(hour, 14U);
    EXPECT_EQ(min, 30U);

    // Display query response: [FF, addr, 0xF0, 0x5F, 0x43, 0x01, Checksum] -> timeDisplay true
    const auto dispResp = PelcoD::PelcoDFrame::createFrame(addr, 0xF0U, 0x5FU, 0x43U, 0x01U);
    std::uint8_t target { 0U };
    std::uint8_t val { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryDisplay(dispResp, target, val));
    EXPECT_EQ(target, 0x43U);
    EXPECT_EQ(val, 0x01U);

    // Key setting query response: [FF, addr, 0xF0, 0xAF, 0x73, 0x02, Checksum]
    const auto keyResp = PelcoD::PelcoDFrame::createFrame(addr, 0xF0U, 0xAFU, 0x73U, 0x02U);
    std::uint8_t keyTarget { 0U };
    std::uint8_t keyVal { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryKeySetting(keyResp, keyTarget, keyVal));
    EXPECT_EQ(keyTarget, 0x73U);
    EXPECT_EQ(keyVal, 0x02U);

    // Log data response: [FF, addr, 0xF0, 0xD3, 0xAA, 0xBB, Checksum]
    const auto logResp = PelcoD::PelcoDFrame::createFrame(addr, 0xF0U, 0xD3U, 0xAAU, 0xBBU);
    std::uint8_t logD1 { 0U };
    std::uint8_t logD2 { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryLogData(logResp, logD1, logD2));
    EXPECT_EQ(logD1, 0xAAU);
    EXPECT_EQ(logD2, 0xBBU);

    // Status update checks
    PelcoD::FujinonStatus status {};
    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(countResp, status));
    EXPECT_EQ(status.movieCount, 300U);

    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(yearResp, status));
    EXPECT_EQ(status.lastMovieYear, 2026U);

    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(dateResp, status));
    EXPECT_EQ(status.lastMovieMonth, 10U);
    EXPECT_EQ(status.lastMovieDay, 16U);

    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(timeResp, status));
    EXPECT_EQ(status.lastMovieHour, 14U);
    EXPECT_EQ(status.lastMovieMinute, 30U);

    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(dispResp, status));
    EXPECT_TRUE(status.timeDisplay);
}

TEST(FujinonProtocolTest, ParserRejectsCorruptedChecksumFrames)
{
    const std::uint8_t addr { 0x01U };

    // 1. Valid focus response: FF 01 00 81 12 34 87 (0x01+0x00+0x81+0x12+0x34 = 0xC8)
    const auto validFocus = PelcoD::PelcoDFrame::createFrame(addr, 0x00U, 0x81U, 0x12U, 0x34U);
    std::uint16_t focusVal { 0U };
    EXPECT_TRUE(PelcoD::FujinonParser::parseQueryFocus(validFocus, focusVal));
    EXPECT_EQ(focusVal, 0x1234U);

    // Corrupted checksum at byte 6
    auto badFocus = validFocus;
    badFocus[6] ^= 0x55U;
    EXPECT_FALSE(PelcoD::FujinonParser::isFujinonResponse(badFocus));
    EXPECT_FALSE(PelcoD::FujinonParser::parseQueryFocus(badFocus, focusVal));

    // Corrupted checksum on 18-byte query response
    std::vector<std::uint8_t> validSerial(18U, 0x00U);
    validSerial[0] = 0xFFU;
    validSerial[1] = addr;
    for (std::size_t i { 2U }; i <= 9U; ++i) {
        validSerial[i] = static_cast<std::uint8_t>('0' + i);
    }
    validSerial[17] = PelcoD::PelcoDFrame::calculateChecksum(&validSerial[1], 16U);
    std::string serialStr;
    EXPECT_TRUE(PelcoD::FujinonParser::parseQuerySerialNumber(validSerial, serialStr));

    auto badSerial = validSerial;
    badSerial[17] ^= 0xAAU;
    EXPECT_FALSE(PelcoD::FujinonParser::isFujinonResponse(badSerial));
    EXPECT_FALSE(PelcoD::FujinonParser::parseQuerySerialNumber(badSerial, serialStr));
}

TEST(FujinonProtocolTest, EnumValidatorsRejectOutOfRangeValues)
{
    // Test toFujinonOISMode
    EXPECT_EQ(PelcoD::toFujinonOISMode(0x01U), PelcoD::FujinonOISMode::Auto);
    EXPECT_EQ(PelcoD::toFujinonOISMode(0x02U), PelcoD::FujinonOISMode::OisOn);
    EXPECT_EQ(PelcoD::toFujinonOISMode(0x03U), PelcoD::FujinonOISMode::EisOn);
    EXPECT_EQ(PelcoD::toFujinonOISMode(0x04U), PelcoD::FujinonOISMode::Off);
    EXPECT_FALSE(PelcoD::toFujinonOISMode(0x00U).has_value());
    EXPECT_FALSE(PelcoD::toFujinonOISMode(0x05U).has_value());
    EXPECT_FALSE(PelcoD::toFujinonOISMode(0xFFU).has_value());

    // Test toFujinonDayNightMode
    EXPECT_EQ(PelcoD::toFujinonDayNightMode(0x01U), PelcoD::FujinonDayNightMode::Auto);
    EXPECT_EQ(PelcoD::toFujinonDayNightMode(0x04U), PelcoD::FujinonDayNightMode::External);
    EXPECT_FALSE(PelcoD::toFujinonDayNightMode(0x00U).has_value());
    EXPECT_FALSE(PelcoD::toFujinonDayNightMode(0x05U).has_value());

    // Test toFujinonIRWavelength
    EXPECT_EQ(PelcoD::toFujinonIRWavelength(0x00U), PelcoD::FujinonIRWavelength::Visible);
    EXPECT_EQ(PelcoD::toFujinonIRWavelength(0x04U), PelcoD::FujinonIRWavelength::W808nm);
    EXPECT_FALSE(PelcoD::toFujinonIRWavelength(0x05U).has_value());

    // Test toFujinonDefogLevel
    EXPECT_EQ(PelcoD::toFujinonDefogLevel(0x00U), PelcoD::FujinonDefogLevel::Off);
    EXPECT_EQ(PelcoD::toFujinonDefogLevel(0x03U), PelcoD::FujinonDefogLevel::Level3);
    EXPECT_FALSE(PelcoD::toFujinonDefogLevel(0x04U).has_value());

    // Test toFujinonHeatHazeLevel
    EXPECT_EQ(PelcoD::toFujinonHeatHazeLevel(0x00U), PelcoD::FujinonHeatHazeLevel::Off);
    EXPECT_EQ(PelcoD::toFujinonHeatHazeLevel(0x02U), PelcoD::FujinonHeatHazeLevel::Level2);
    EXPECT_FALSE(PelcoD::toFujinonHeatHazeLevel(0x03U).has_value());

    // Test toFujinonWDRLevel
    EXPECT_EQ(PelcoD::toFujinonWDRLevel(0x00U), PelcoD::FujinonWDRLevel::Off);
    EXPECT_EQ(PelcoD::toFujinonWDRLevel(0x03U), PelcoD::FujinonWDRLevel::Level3);
    EXPECT_FALSE(PelcoD::toFujinonWDRLevel(0x04U).has_value());

    // Test toFujinonDigitalZoom
    EXPECT_EQ(PelcoD::toFujinonDigitalZoom(0x00U), PelcoD::FujinonDigitalZoom::Off);
    EXPECT_EQ(PelcoD::toFujinonDigitalZoom(0x04U), PelcoD::FujinonDigitalZoom::X2);
    EXPECT_FALSE(PelcoD::toFujinonDigitalZoom(0x05U).has_value());

    // Test toFujinonOSDPosition
    EXPECT_EQ(PelcoD::toFujinonOSDPosition(0x01U), PelcoD::FujinonOSDPosition::TopLeft);
    EXPECT_EQ(PelcoD::toFujinonOSDPosition(0x04U), PelcoD::FujinonOSDPosition::BottomRight);
    EXPECT_FALSE(PelcoD::toFujinonOSDPosition(0x00U).has_value());
    EXPECT_FALSE(PelcoD::toFujinonOSDPosition(0x05U).has_value());

    // Test toFujinonDigitalZoomMode
    EXPECT_EQ(PelcoD::toFujinonDigitalZoomMode(0x00U), PelcoD::FujinonDigitalZoomMode::Off);
    EXPECT_EQ(PelcoD::toFujinonDigitalZoomMode(0x02U), PelcoD::FujinonDigitalZoomMode::CropMode);
    EXPECT_FALSE(PelcoD::toFujinonDigitalZoomMode(0x03U).has_value());

    // Test toFujinonDayNightModeEx
    EXPECT_EQ(PelcoD::toFujinonDayNightModeEx(0x00U), PelcoD::FujinonDayNightModeEx::Auto);
    EXPECT_EQ(PelcoD::toFujinonDayNightModeEx(0x04U), PelcoD::FujinonDayNightModeEx::Night);
    EXPECT_FALSE(PelcoD::toFujinonDayNightModeEx(0x05U).has_value());

    // Test toFujinonLanguage
    EXPECT_EQ(PelcoD::toFujinonLanguage(0x00U), PelcoD::FujinonLanguage::English);
    EXPECT_EQ(PelcoD::toFujinonLanguage(0x01U), PelcoD::FujinonLanguage::French);
    EXPECT_EQ(PelcoD::toFujinonLanguage(0x03U), PelcoD::FujinonLanguage::Japanese);
    EXPECT_FALSE(PelcoD::toFujinonLanguage(0x02U).has_value()); // 0x02 is unassigned in spec
    EXPECT_FALSE(PelcoD::toFujinonLanguage(0x04U).has_value());

    // Test toFujinonAutoFocusMode
    EXPECT_EQ(PelcoD::toFujinonAutoFocusMode(0x00U), PelcoD::FujinonAutoFocusMode::AFOn);
    EXPECT_EQ(PelcoD::toFujinonAutoFocusMode(0x02U), PelcoD::FujinonAutoFocusMode::QuickAF);
    EXPECT_FALSE(PelcoD::toFujinonAutoFocusMode(0x03U).has_value());
}

TEST(FujinonProtocolTest, ParserRejectsMalformedEnumPayloads)
{
    const std::uint8_t addr { 0x01U };

    // Construct frame with invalid OIS mode 0x99: FF 01 F0 1F 13 99 CKSM
    const auto badOisFrame = PelcoD::PelcoDFrame::createFrame(addr, 0xF0U, 0x1FU, 0x13U, 0x99U);
    PelcoD::FujinonPhotoSettings photo {};
    EXPECT_FALSE(PelcoD::FujinonParser::parsePhotoSettings(badOisFrame, photo));

    // Construct frame with invalid DayNight mode 0xFE: FF 01 F1 1F 01 FE CKSM
    const auto badDayNightFrame = PelcoD::PelcoDFrame::createFrame(addr, 0xF1U, 0x1FU, 0x01U, 0xFEU);
    PelcoD::FujinonDayNightExSettings dnEx {};
    EXPECT_FALSE(PelcoD::FujinonParser::parseDayNightExSettings(badDayNightFrame, dnEx));

    // Construct frame with invalid DigitalZoomMode 0x55: FF 01 F1 2F 21 55 CKSM
    const auto badZoomModeFrame = PelcoD::PelcoDFrame::createFrame(addr, 0xF1U, 0x2FU, 0x21U, 0x55U);
    PelcoD::FujinonZoomFocusExSettings zfEx {};
    EXPECT_FALSE(PelcoD::FujinonParser::parseZoomFocusExSettings(badZoomModeFrame, zfEx));

    // Construct status frame with invalid AutoFocusMode 0x99: FF 01 00 2B 00 99 CKSM
    const auto badAfFrame = PelcoD::PelcoDFrame::createFrame(addr, 0x00U, 0x2BU, 0x00U, 0x99U);
    PelcoD::FujinonStatus status {};
    EXPECT_FALSE(PelcoD::FujinonParser::updateFujinonStatus(badAfFrame, status));

    // Construct status frame with invalid Language 0x02: FF 01 F0 85 00 02 CKSM
    const auto badLangFrame = PelcoD::PelcoDFrame::createFrame(addr, 0xF0U, 0x85U, 0x00U, 0x02U);
    EXPECT_FALSE(PelcoD::FujinonParser::updateFujinonStatus(badLangFrame, status));
}

TEST(FujinonProtocolTest, ZeroAllocationBuildersTest)
{
    // Verify static type is StandardFrame (std::array<std::uint8_t, 7>)
    static_assert(std::is_same_v<decltype(PelcoD::FujinonBuilder::buildSetOIS(1U, PelcoD::FujinonOISMode::Auto)),
        PelcoD::StandardFrame>);
    static_assert(
        std::is_same_v<decltype(PelcoD::FujinonBuilder::buildSetFocusPosition(1U, 0x1000U)), PelcoD::StandardFrame>);
    static_assert(std::is_same_v<decltype(PelcoD::FujinonBuilder::buildSetZoomSpeed(1U, 2U)), PelcoD::StandardFrame>);

    // Verify sizeof is strictly 7 bytes (zero dynamic pointer overhead)
    static_assert(sizeof(PelcoD::FujinonBuilder::buildSetOIS(1U, PelcoD::FujinonOISMode::Auto)) == 7U);
    static_assert(sizeof(PelcoD::FujinonBuilder::buildSetBrightness(1U, 11U)) == 7U);

    // Verify noexcept guarantees
    static_assert(noexcept(PelcoD::FujinonBuilder::buildSetOIS(1U, PelcoD::FujinonOISMode::Auto)));
    static_assert(noexcept(PelcoD::FujinonBuilder::buildSetFocusPosition(1U, 0x1000U)));
    static_assert(noexcept(PelcoD::FujinonBuilder::buildSetZoomSpeed(1U, 2U)));
    static_assert(noexcept(PelcoD::FujinonBuilder::buildSetRTCSecond(1U, 30U)));
}

TEST(FujinonProtocolTest, ParameterClampingTest)
{
    const std::uint8_t addr { 1U };

    // Brightness: 1..21
    const auto lowBright = PelcoD::FujinonBuilder::buildSetBrightness(addr, 0U);
    EXPECT_EQ(lowBright[5], 1U);
    const auto highBright = PelcoD::FujinonBuilder::buildSetBrightness(addr, 255U);
    EXPECT_EQ(highBright[5], 21U);

    // Contrast: 1..5
    const auto lowCont = PelcoD::FujinonBuilder::buildSetContrast(addr, 0U);
    EXPECT_EQ(lowCont[5], 1U);
    const auto highCont = PelcoD::FujinonBuilder::buildSetContrast(addr, 10U);
    EXPECT_EQ(highCont[5], 5U);

    // Saturation: 1..5
    const auto lowSat = PelcoD::FujinonBuilder::buildSetSaturation(addr, 0U);
    EXPECT_EQ(lowSat[5], 1U);
    const auto highSat = PelcoD::FujinonBuilder::buildSetSaturation(addr, 99U);
    EXPECT_EQ(highSat[5], 5U);

    // Sharpness: 1..5
    const auto lowSharp = PelcoD::FujinonBuilder::buildSetSharpness(addr, 0U);
    EXPECT_EQ(lowSharp[5], 1U);
    const auto highSharp = PelcoD::FujinonBuilder::buildSetSharpness(addr, 40U);
    EXPECT_EQ(highSharp[5], 5U);

    // Noise reduction: 1..3
    const auto lowNr = PelcoD::FujinonBuilder::buildSetNoiseReduction(addr, 0U);
    EXPECT_EQ(lowNr[5], 1U);
    const auto highNr = PelcoD::FujinonBuilder::buildSetNoiseReduction(addr, 5U);
    EXPECT_EQ(highNr[5], 3U);

    // Fine adjustments: 1..100
    const auto lowFine = PelcoD::FujinonBuilder::buildSetBrightnessFine(addr, 0U);
    EXPECT_EQ(lowFine[5], 1U);
    const auto highFine = PelcoD::FujinonBuilder::buildSetBrightnessFine(addr, 150U);
    EXPECT_EQ(highFine[5], 100U);

    // Zoom speed: 0..3
    const auto highZs = PelcoD::FujinonBuilder::buildSetZoomSpeed(addr, 7U);
    EXPECT_EQ(highZs[5], 3U);

    // Focus speed: 0..3
    const auto highFs = PelcoD::FujinonBuilder::buildSetFocusSpeed(addr, 9U);
    EXPECT_EQ(highFs[5], 3U);

    // Extended zoom speed: 1..8
    const auto lowZsEx = PelcoD::FujinonBuilder::buildSetZoomSpeedEx(addr, 0U);
    EXPECT_EQ(lowZsEx[5], 1U);
    const auto highZsEx = PelcoD::FujinonBuilder::buildSetZoomSpeedEx(addr, 15U);
    EXPECT_EQ(highZsEx[5], 8U);

    // Extended focus speed: 1..5
    const auto lowFsEx = PelcoD::FujinonBuilder::buildSetFocusSpeedEx(addr, 0U);
    EXPECT_EQ(lowFsEx[5], 1U);
    const auto highFsEx = PelcoD::FujinonBuilder::buildSetFocusSpeedEx(addr, 20U);
    EXPECT_EQ(highFsEx[5], 5U);

    // RTC Seconds: 0..59
    const auto highSec = PelcoD::FujinonBuilder::buildSetRTCSecond(addr, 85U);
    EXPECT_EQ(highSec[5], 59U);

    // RTC Hour & Minute: 0..23, 0..59
    const auto highHm = PelcoD::FujinonBuilder::buildSetRTCHourMinute(addr, 30U, 99U);
    EXPECT_EQ(highHm[4], 23U);
    EXPECT_EQ(highHm[5], 59U);

    // RTC Month & Day: 1..12, 1..31
    const auto lowMd = PelcoD::FujinonBuilder::buildSetRTCMonthDay(addr, 0U, 0U);
    EXPECT_EQ(lowMd[4], 1U);
    EXPECT_EQ(lowMd[5], 1U);
    const auto highMd = PelcoD::FujinonBuilder::buildSetRTCMonthDay(addr, 15U, 40U);
    EXPECT_EQ(highMd[4], 12U);
    EXPECT_EQ(highMd[5], 31U);

    // RS-485 Address: 1..31
    const auto lowAddr = PelcoD::FujinonBuilder::buildSetRS485Address(addr, 0U);
    EXPECT_EQ(lowAddr[5], 1U);
    const auto highAddr = PelcoD::FujinonBuilder::buildSetRS485Address(addr, 40U);
    EXPECT_EQ(highAddr[5], 31U);
}

TEST(FujinonProtocolTest, RTCTelemetryStatusTests)
{
    const std::uint8_t addr { 1U };
    PelcoD::FujinonStatus st {};

    // 1. Second telemetry (subOp 0x01)
    const auto respSec = PelcoD::PelcoDFrame::createFrame(addr, 0x01U, 0x3BU, 0x00U, 45U);
    EXPECT_TRUE(PelcoD::FujinonParser::isFujinonResponse(respSec));
    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(respSec, st));
    EXPECT_EQ(st.baseStatus.deviceTime.second, 45U);

    // 2. Hour / Minute telemetry (subOp 0x03)
    const auto respHm = PelcoD::PelcoDFrame::createFrame(addr, 0x03U, 0x3BU, 14U, 30U);
    EXPECT_TRUE(PelcoD::FujinonParser::isFujinonResponse(respHm));
    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(respHm, st));
    EXPECT_EQ(st.baseStatus.deviceTime.hour, 14U);
    EXPECT_EQ(st.baseStatus.deviceTime.minute, 30U);

    // 3. Month / Day telemetry (subOp 0x05)
    const auto respMd = PelcoD::PelcoDFrame::createFrame(addr, 0x05U, 0x3BU, 10U, 19U);
    EXPECT_TRUE(PelcoD::FujinonParser::isFujinonResponse(respMd));
    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(respMd, st));
    EXPECT_EQ(st.baseStatus.deviceTime.month, 10U);
    EXPECT_EQ(st.baseStatus.deviceTime.day, 19U);

    // 4. Year telemetry (subOp 0x07: 2026 = 0x07EA)
    const auto respYr = PelcoD::PelcoDFrame::createFrame(addr, 0x07U, 0x3BU, 0x07U, 0xEAU);
    EXPECT_TRUE(PelcoD::FujinonParser::isFujinonResponse(respYr));
    EXPECT_TRUE(PelcoD::FujinonParser::updateFujinonStatus(respYr, st));
    EXPECT_EQ(st.baseStatus.deviceTime.year, 2026U);
}
