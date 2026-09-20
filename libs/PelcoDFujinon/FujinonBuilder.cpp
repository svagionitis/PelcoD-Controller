/// @file FujinonBuilder.cpp
/// @brief Implementation of frame encoder for Fujinon SX800 / SX801 extended commands.

#include "FujinonBuilder.h"

namespace PelcoD {

std::vector<std::uint8_t> FujinonBuilder::buildSetOIS(std::uint8_t address, FujinonOISMode mode)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x13U, 0x00U, static_cast<std::uint8_t>(mode));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetDefog(std::uint8_t address, FujinonDefogLevel level)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x29U, 0x00U, static_cast<std::uint8_t>(level));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetHeatHaze(std::uint8_t address, FujinonHeatHazeLevel level)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x27U, 0x00U, static_cast<std::uint8_t>(level));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetWDR(std::uint8_t address, FujinonWDRLevel level)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x23U, 0x00U, static_cast<std::uint8_t>(level));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetVLCFilter(std::uint8_t address, bool enable)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x21U, 0x00U, enable ? 0x01U : 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetBrightness(std::uint8_t address, std::uint8_t level)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x2BU, 0x00U, level);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetContrast(std::uint8_t address, std::uint8_t level)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x2DU, 0x00U, level);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetSaturation(std::uint8_t address, std::uint8_t level)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x2FU, 0x00U, level);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetSharpness(std::uint8_t address, std::uint8_t level)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x31U, 0x00U, level);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetColorTemperature(std::uint8_t address, FujinonColorTemp temp)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x33U, 0x00U, static_cast<std::uint8_t>(temp));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetWhiteBalance(std::uint8_t address, FujinonWBMode mode)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x35U, 0x00U, static_cast<std::uint8_t>(mode));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetDigitalZoom(std::uint8_t address, FujinonDigitalZoom zoom)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x37U, 0x00U, static_cast<std::uint8_t>(zoom));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetNoiseReduction(std::uint8_t address, std::uint8_t level)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x39U, 0x00U, level);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetDayNight(std::uint8_t address, FujinonDayNightMode mode)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x0FU, 0x00U, static_cast<std::uint8_t>(mode));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetIRWavelength(std::uint8_t address, FujinonIRWavelength wavelength)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x11U, 0x00U, static_cast<std::uint8_t>(wavelength));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetFocusPosition(std::uint8_t address, std::uint16_t focusPos)
{
    const auto msb = static_cast<std::uint8_t>((focusPos >> 8U) & 0xFFU);
    const auto lsb = static_cast<std::uint8_t>(focusPos & 0xFFU);
    return PelcoDFrame::createFrame(address, 0x00U, 0x4BU, msb, lsb);
}

std::vector<std::uint8_t> FujinonBuilder::buildOnePushAF(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x07U, 0x00U, 0x01U);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetAFSensitivity(std::uint8_t address, std::uint8_t sensitivity)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x05U, 0x00U, sensitivity);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetAFArea(std::uint8_t address, std::uint8_t area)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x03U, 0x00U, area);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetManualIris(std::uint8_t address, std::uint8_t irisVal)
{
    return PelcoDFrame::createFrame(address, 0x00U, 0x4DU, 0x00U, irisVal);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetManualShutter(std::uint8_t address, std::uint8_t shutterVal)
{
    return PelcoDFrame::createFrame(address, 0x00U, 0x51U, 0x00U, shutterVal);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetManualISO(std::uint8_t address, std::uint8_t isoVal)
{
    return PelcoDFrame::createFrame(address, 0x00U, 0x53U, 0x00U, isoVal);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetTimeDisplay(std::uint8_t address, bool enable)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x43U, 0x00U, enable ? 0x01U : 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetTimePosition(std::uint8_t address, FujinonOSDPosition pos)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x45U, 0x00U, static_cast<std::uint8_t>(pos));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetTitleDisplay(std::uint8_t address, bool enable)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x47U, 0x00U, enable ? 0x01U : 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetTitlePosition(std::uint8_t address, FujinonOSDPosition pos)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x4BU, 0x00U, static_cast<std::uint8_t>(pos));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetIdDisplay(std::uint8_t address, bool enable)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x4DU, 0x00U, enable ? 0x01U : 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetIdPosition(std::uint8_t address, FujinonOSDPosition pos)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x4FU, 0x00U, static_cast<std::uint8_t>(pos));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetReticleDisplay(std::uint8_t address, bool enable)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x51U, 0x00U, enable ? 0x01U : 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetVideoMode(std::uint8_t address, FujinonVideoMode mode)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x67U, 0x00U, static_cast<std::uint8_t>(mode));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetHDFormat(std::uint8_t address, FujinonHDFormat format)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x69U, 0x00U, static_cast<std::uint8_t>(format));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetRS485Termination(std::uint8_t address, bool enable)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x73U, 0x00U, enable ? 0x01U : 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildReboot(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x83U, 0x00U, 0x01U);
}

std::vector<std::uint8_t> FujinonBuilder::buildRecordLiveView(std::uint8_t address, bool start)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x87U, 0x00U, start ? 0x01U : 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildPlayMovie(std::uint8_t address, bool start)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x8BU, 0x00U, start ? 0x01U : 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetMovieMode(std::uint8_t address, bool isPlayMode)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x8FU, 0x00U, isPlayMode ? 0x01U : 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildMenuOk(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x9BU, 0x00U, 0x01U);
}

std::vector<std::uint8_t> FujinonBuilder::buildMenuDirection(std::uint8_t address, FujinonMenuDirection dir)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x9DU, 0x00U, static_cast<std::uint8_t>(dir));
}

std::vector<std::uint8_t> FujinonBuilder::buildQueryFocus(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0x00U, 0x81U, 0x00U, 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildQueryZoom(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0x00U, 0x83U, 0x00U, 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildQuerySerialNumber(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0x00U, 0x89U, 0x00U, 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildQueryFirmwareVersion(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0x00U, 0x8BU, 0x00U, 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildQueryLensStatus(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0x00U, 0x8DU, 0x00U, 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildQueryPhotoSettings(std::uint8_t address, std::uint8_t target)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x1DU, target, 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildQueryImageQuality(std::uint8_t address, std::uint8_t target)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x2BU, target, 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildQueryManualSettings(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0x00U, 0x8FU, 0x00U, 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetBrightnessFine(std::uint8_t address, std::uint8_t level)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0xEBU, 0x00U, level);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetContrastFine(std::uint8_t address, std::uint8_t level)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0xEDU, 0x00U, level);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetSaturationFine(std::uint8_t address, std::uint8_t level)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0xEFU, 0x00U, level);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetSharpnessFine(std::uint8_t address, std::uint8_t level)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0xF1U, 0x00U, level);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetWBShiftRedFine(std::uint8_t address, std::uint8_t level)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0xF5U, 0x00U, level);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetWBShiftBlueFine(std::uint8_t address, std::uint8_t level)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0xF7U, 0x00U, level);
}

std::vector<std::uint8_t> FujinonBuilder::buildQueryImageQualityFine(std::uint8_t address, std::uint8_t target)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0xFDU, target, 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetDayNightModeEx(std::uint8_t address, FujinonDayNightModeEx mode)
{
    return PelcoDFrame::createFrame(address, 0xF1U, 0x01U, 0x00U, static_cast<std::uint8_t>(mode));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetDayToNightThreshold(std::uint8_t address, std::uint8_t threshold)
{
    return PelcoDFrame::createFrame(address, 0xF1U, 0x03U, 0x00U, threshold);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetNightToDayThreshold(std::uint8_t address, std::uint8_t threshold)
{
    return PelcoDFrame::createFrame(address, 0xF1U, 0x05U, 0x00U, threshold);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetDayNightAutoDelay(std::uint8_t address, std::uint8_t delaySeconds)
{
    return PelcoDFrame::createFrame(address, 0xF1U, 0x07U, 0x00U, delaySeconds);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetDayStartTime(
    std::uint8_t address, std::uint8_t hour, std::uint8_t minute)
{
    return PelcoDFrame::createFrame(address, 0xF1U, 0x09U, hour, minute);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetNightStartTime(
    std::uint8_t address, std::uint8_t hour, std::uint8_t minute)
{
    return PelcoDFrame::createFrame(address, 0xF1U, 0x0BU, hour, minute);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetOpticalFilterDay(std::uint8_t address, bool irPass)
{
    return PelcoDFrame::createFrame(address, 0xF1U, 0x0DU, 0x00U, irPass ? 0x01U : 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetOpticalFilterNight(std::uint8_t address, bool irPass)
{
    return PelcoDFrame::createFrame(address, 0xF1U, 0x0FU, 0x00U, irPass ? 0x01U : 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildQueryDayNightEx(std::uint8_t address, std::uint8_t target)
{
    return PelcoDFrame::createFrame(address, 0xF1U, 0x1FU, target, 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetZoomSpeedEx(std::uint8_t address, std::uint8_t speed)
{
    return PelcoDFrame::createFrame(address, 0xF1U, 0x25U, 0x00U, speed);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetFocusSpeedEx(std::uint8_t address, std::uint8_t speed)
{
    return PelcoDFrame::createFrame(address, 0xF1U, 0x27U, 0x00U, speed);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetDigitalZoomMode(std::uint8_t address, FujinonDigitalZoomMode mode)
{
    return PelcoDFrame::createFrame(address, 0xF1U, 0x37U, 0x00U, static_cast<std::uint8_t>(mode));
}

std::vector<std::uint8_t> FujinonBuilder::buildQueryZoomFocusEx(std::uint8_t address, std::uint8_t target)
{
    return PelcoDFrame::createFrame(address, 0xF1U, 0x3DU, target, 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetAntialiasing(std::uint8_t address, bool enable)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x55U, 0x00U, enable ? 0x01U : 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildMenuBack(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0xABU, 0x00U, 0x01U);
}

std::vector<std::uint8_t> FujinonBuilder::buildFormatSDCard(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x7BU, 0x00U, 0x01U);
}

std::vector<std::uint8_t> FujinonBuilder::buildFactoryReset(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x81U, 0x00U, 0x01U);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetLanguage(std::uint8_t address, FujinonLanguage lang)
{
    return PelcoDFrame::createFrame(address, 0xF0U, 0x85U, 0x00U, static_cast<std::uint8_t>(lang));
}

std::vector<std::uint8_t> FujinonBuilder::buildSetRTCSecond(std::uint8_t address, std::uint8_t second)
{
    return PelcoDFrame::createFrame(address, 0x00U, 0x3BU, 0x00U, second);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetRTCHourMinute(
    std::uint8_t address, std::uint8_t hour, std::uint8_t minute)
{
    return PelcoDFrame::createFrame(address, 0x02U, 0x3BU, hour, minute);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetRTCMonthDay(
    std::uint8_t address, std::uint8_t month, std::uint8_t day)
{
    return PelcoDFrame::createFrame(address, 0x04U, 0x3BU, month, day);
}

std::vector<std::uint8_t> FujinonBuilder::buildSetRTCYear(std::uint8_t address, std::uint16_t year)
{
    return PelcoDFrame::createFrame(
        address, 0x06U, 0x3BU, static_cast<std::uint8_t>((year >> 8) & 0xFFU), static_cast<std::uint8_t>(year & 0xFFU));
}

std::vector<std::uint8_t> FujinonBuilder::buildQueryRTC(std::uint8_t address, std::uint8_t subOpCode)
{
    return PelcoDFrame::createFrame(address, subOpCode, 0x3BU, 0x00U, 0x00U);
}

std::vector<std::uint8_t> FujinonBuilder::buildQueryZoomStandard(std::uint8_t address)
{
    return PelcoDFrame::createFrame(address, 0x00U, 0x55U, 0x00U, 0x00U);
}

double FujinonBuilder::rawZoomToFocalLengthMm(std::uint16_t rawZoom, FujinonDigitalZoom digitalZoom)
{
    double multiplier { 1.0 };
    switch (digitalZoom) {
    case FujinonDigitalZoom::X1_25:
        multiplier = 1.25;
        break;
    case FujinonDigitalZoom::X1_5:
        multiplier = 1.5;
        break;
    case FujinonDigitalZoom::X1_75:
        multiplier = 1.75;
        break;
    case FujinonDigitalZoom::X2:
        multiplier = 2.0;
        break;
    case FujinonDigitalZoom::Off:
    default:
        multiplier = 1.0;
        break;
    }

    constexpr double wideMm { 20.0 };
    constexpr double teleMm { 800.0 };
    constexpr double maxRaw { 16384.0 };

    const double clamped = std::min(static_cast<double>(rawZoom), maxRaw);
    const double baseMm = wideMm + (clamped / maxRaw) * (teleMm - wideMm);
    return baseMm * multiplier;
}

} // namespace PelcoD
