/// @file FujinonBuilder.cpp
/// @brief Implementation of frame encoder for Fujinon SX800 / SX801 extended commands.

#include "FujinonBuilder.h"

#include <cstdint>

#include <algorithm>

namespace PelcoD {

StandardFrame FujinonBuilder::buildSetOIS(std::uint8_t address, FujinonOISMode mode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x13U, 0x00U, static_cast<std::uint8_t>(mode));
}

StandardFrame FujinonBuilder::buildSetDefog(std::uint8_t address, FujinonDefogLevel level) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x29U, 0x00U, static_cast<std::uint8_t>(level));
}

StandardFrame FujinonBuilder::buildSetHeatHaze(std::uint8_t address, FujinonHeatHazeLevel level) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x27U, 0x00U, static_cast<std::uint8_t>(level));
}

StandardFrame FujinonBuilder::buildSetWDR(std::uint8_t address, FujinonWDRLevel level) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x23U, 0x00U, static_cast<std::uint8_t>(level));
}

StandardFrame FujinonBuilder::buildSetVLCFilter(std::uint8_t address, bool enable) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x21U, 0x00U, enable ? 0x01U : 0x00U);
}

StandardFrame FujinonBuilder::buildSetBrightness(std::uint8_t address, std::uint8_t level) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(level, 1U, 21U) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x2BU, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetContrast(std::uint8_t address, std::uint8_t level) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(level, 1U, 5U) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x2DU, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetSaturation(std::uint8_t address, std::uint8_t level) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(level, 1U, 5U) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x2FU, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetSharpness(std::uint8_t address, std::uint8_t level) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(level, 1U, 5U) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x31U, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetColorTemperature(std::uint8_t address, FujinonColorTemp temp) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x33U, 0x00U, static_cast<std::uint8_t>(temp));
}

StandardFrame FujinonBuilder::buildSetWhiteBalance(std::uint8_t address, FujinonWBMode mode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x35U, 0x00U, static_cast<std::uint8_t>(mode));
}

StandardFrame FujinonBuilder::buildSetDigitalZoom(std::uint8_t address, FujinonDigitalZoom zoom) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x37U, 0x00U, static_cast<std::uint8_t>(zoom));
}

StandardFrame FujinonBuilder::buildSetNoiseReduction(std::uint8_t address, std::uint8_t level) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(level, 1U, 3U) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x39U, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetDayNight(std::uint8_t address, FujinonDayNightMode mode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x0FU, 0x00U, static_cast<std::uint8_t>(mode));
}

StandardFrame FujinonBuilder::buildSetIRWavelength(std::uint8_t address, FujinonIRWavelength wavelength) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x11U, 0x00U, static_cast<std::uint8_t>(wavelength));
}

StandardFrame FujinonBuilder::buildSetFocusPosition(std::uint8_t address, std::uint16_t focusPos) noexcept
{
    const std::uint8_t msb { static_cast<std::uint8_t>((focusPos >> 8U) & 0xFFU) };
    const std::uint8_t lsb { static_cast<std::uint8_t>(focusPos & 0xFFU) };
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x8FU, msb, lsb);
}

StandardFrame FujinonBuilder::buildOnePushAF(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x07U, 0x00U, 0x00U);
}

StandardFrame FujinonBuilder::buildSetAFSensitivity(std::uint8_t address, std::uint8_t sensitivity) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(sensitivity, 1U, 3U) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x05U, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetAFArea(std::uint8_t address, std::uint8_t area) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(area, 0U, 2U) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x03U, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetManualIris(std::uint8_t address, std::uint8_t irisVal) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x91U, 0x00U, irisVal);
}

StandardFrame FujinonBuilder::buildSetManualShutter(std::uint8_t address, std::uint8_t shutterVal) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x95U, 0x00U, shutterVal);
}

StandardFrame FujinonBuilder::buildSetManualISO(std::uint8_t address, std::uint8_t isoVal) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x99U, 0x00U, isoVal);
}

StandardFrame FujinonBuilder::buildSetTimeDisplay(std::uint8_t address, bool enable) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x43U, 0x00U, enable ? 0x01U : 0x00U);
}

StandardFrame FujinonBuilder::buildSetTimePosition(std::uint8_t address, FujinonOSDPosition pos) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x45U, 0x00U, static_cast<std::uint8_t>(pos));
}

StandardFrame FujinonBuilder::buildSetTitleDisplay(std::uint8_t address, bool enable) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x47U, 0x00U, enable ? 0x01U : 0x00U);
}

StandardFrame FujinonBuilder::buildSetTitlePosition(std::uint8_t address, FujinonOSDPosition pos) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x4BU, 0x00U, static_cast<std::uint8_t>(pos));
}

StandardFrame FujinonBuilder::buildSetIdDisplay(std::uint8_t address, bool enable) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x4DU, 0x00U, enable ? 0x01U : 0x00U);
}

StandardFrame FujinonBuilder::buildSetIdPosition(std::uint8_t address, FujinonOSDPosition pos) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x4FU, 0x00U, static_cast<std::uint8_t>(pos));
}

StandardFrame FujinonBuilder::buildSetReticleDisplay(std::uint8_t address, bool enable) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x51U, 0x00U, enable ? 0x01U : 0x00U);
}

StandardFrame FujinonBuilder::buildSetVideoMode(std::uint8_t address, FujinonVideoMode mode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x67U, 0x00U, static_cast<std::uint8_t>(mode));
}

StandardFrame FujinonBuilder::buildSetHDFormat(std::uint8_t address, FujinonHDFormat format) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x69U, 0x00U, static_cast<std::uint8_t>(format));
}

StandardFrame FujinonBuilder::buildSetRS485Termination(std::uint8_t address, bool enable) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x73U, 0x00U, enable ? 0x01U : 0x00U);
}

StandardFrame FujinonBuilder::buildReboot(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x83U, 0x00U, 0x01U);
}

StandardFrame FujinonBuilder::buildRecordLiveView(std::uint8_t address, bool start) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x93U, 0x00U, start ? 0x00U : 0x01U);
}

StandardFrame FujinonBuilder::buildPlayMovie(std::uint8_t address, bool start) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x97U, 0x00U, start ? 0x02U : 0x00U);
}

StandardFrame FujinonBuilder::buildSetMovieMode(std::uint8_t address, bool isPlayMode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x9BU, 0x00U, isPlayMode ? 0x02U : 0x01U);
}

StandardFrame FujinonBuilder::buildMenuOk(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xA7U, 0x00U, 0x01U);
}

StandardFrame FujinonBuilder::buildMenuDirection(std::uint8_t address, FujinonMenuDirection dir) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xA9U, 0x00U, static_cast<std::uint8_t>(dir));
}

StandardFrame FujinonBuilder::buildQueryFocus(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x81U, 0x00U, 0x00U);
}

StandardFrame FujinonBuilder::buildQueryZoom(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x83U, 0x00U, 0x00U);
}

StandardFrame FujinonBuilder::buildQuerySerialNumber(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x89U, 0x00U, 0x00U);
}

StandardFrame FujinonBuilder::buildQueryFirmwareVersion(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x8BU, 0x00U, 0x00U);
}

StandardFrame FujinonBuilder::buildQueryLensStatus(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x8DU, 0x00U, 0x00U);
}

StandardFrame FujinonBuilder::buildQueryPhotoSettings(std::uint8_t address, std::uint8_t target) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x1DU, target, 0x00U);
}

StandardFrame FujinonBuilder::buildQueryImageQuality(std::uint8_t address, std::uint8_t target) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x3DU, target, 0x00U);
}

StandardFrame FujinonBuilder::buildQueryManualSettings(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0xADU, 0x00U, 0x00U);
}

StandardFrame FujinonBuilder::buildSetBrightnessFine(std::uint8_t address, std::uint8_t level) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(level, 1U, 100U) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xEBU, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetContrastFine(std::uint8_t address, std::uint8_t level) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(level, 1U, 100U) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xEDU, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetSaturationFine(std::uint8_t address, std::uint8_t level) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(level, 1U, 100U) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xEFU, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetSharpnessFine(std::uint8_t address, std::uint8_t level) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(level, 1U, 100U) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xF1U, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetWBShiftRedFine(std::uint8_t address, std::uint8_t level) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(level, 1U, 100U) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xF5U, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetWBShiftBlueFine(std::uint8_t address, std::uint8_t level) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(level, 1U, 100U) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xF7U, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildQueryImageQualityFine(std::uint8_t address, std::uint8_t target) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xFDU, target, 0x00U);
}

StandardFrame FujinonBuilder::buildSetDayNightModeEx(std::uint8_t address, FujinonDayNightModeEx mode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF1U, 0x01U, 0x00U, static_cast<std::uint8_t>(mode));
}

StandardFrame FujinonBuilder::buildSetDayToNightThreshold(std::uint8_t address, std::uint8_t threshold) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF1U, 0x03U, 0x00U, threshold);
}

StandardFrame FujinonBuilder::buildSetNightToDayThreshold(std::uint8_t address, std::uint8_t threshold) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF1U, 0x05U, 0x00U, threshold);
}

StandardFrame FujinonBuilder::buildSetDayNightAutoDelay(std::uint8_t address, std::uint8_t delaySeconds) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF1U, 0x07U, 0x00U, delaySeconds);
}

StandardFrame FujinonBuilder::buildSetDayStartTime(
    std::uint8_t address, std::uint8_t hour, std::uint8_t minute) noexcept
{
    const std::uint8_t clampedHour { std::clamp<std::uint8_t>(hour, 0U, 23U) };
    const std::uint8_t clampedMinute { std::clamp<std::uint8_t>(minute, 0U, 59U) };
    return PelcoDFrame::createStandardFrame(address, 0xF1U, 0x09U, clampedHour, clampedMinute);
}

StandardFrame FujinonBuilder::buildSetNightStartTime(
    std::uint8_t address, std::uint8_t hour, std::uint8_t minute) noexcept
{
    const std::uint8_t clampedHour { std::clamp<std::uint8_t>(hour, 0U, 23U) };
    const std::uint8_t clampedMinute { std::clamp<std::uint8_t>(minute, 0U, 59U) };
    return PelcoDFrame::createStandardFrame(address, 0xF1U, 0x0BU, clampedHour, clampedMinute);
}

StandardFrame FujinonBuilder::buildSetOpticalFilterDay(std::uint8_t address, bool irPass) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF1U, 0x0DU, 0x00U, irPass ? 0x01U : 0x00U);
}

StandardFrame FujinonBuilder::buildSetOpticalFilterNight(std::uint8_t address, bool irPass) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF1U, 0x0FU, 0x00U, irPass ? 0x01U : 0x00U);
}

StandardFrame FujinonBuilder::buildQueryDayNightEx(std::uint8_t address, std::uint8_t target) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF1U, 0x1DU, target, 0x00U);
}

StandardFrame FujinonBuilder::buildSetZoomSpeedEx(std::uint8_t address, std::uint8_t speed) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(speed, 1U, 8U) };
    return PelcoDFrame::createStandardFrame(address, 0xF1U, 0x25U, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetFocusSpeedEx(std::uint8_t address, std::uint8_t speed) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(speed, 1U, 5U) };
    return PelcoDFrame::createStandardFrame(address, 0xF1U, 0x27U, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetDigitalZoomMode(std::uint8_t address, FujinonDigitalZoomMode mode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x37U, 0x00U, static_cast<std::uint8_t>(mode));
}

StandardFrame FujinonBuilder::buildQueryZoomFocusEx(std::uint8_t address, std::uint8_t target) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF1U, 0x2DU, target, 0x00U);
}

StandardFrame FujinonBuilder::buildSetAntialiasing(std::uint8_t address, bool enable) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x55U, 0x00U, enable ? 0x01U : 0x00U);
}

StandardFrame FujinonBuilder::buildMenuBack(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xABU, 0x00U, 0x01U);
}

StandardFrame FujinonBuilder::buildFormatSDCard(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x7BU, 0x00U, 0x01U);
}

StandardFrame FujinonBuilder::buildFactoryReset(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x81U, 0x00U, 0x01U);
}

StandardFrame FujinonBuilder::buildSetLanguage(std::uint8_t address, FujinonLanguage lang) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x85U, 0x00U, static_cast<std::uint8_t>(lang));
}

StandardFrame FujinonBuilder::buildSetRTCSecond(std::uint8_t address, std::uint8_t second) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(second, 0U, 59U) };
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x3BU, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetRTCHourMinute(
    std::uint8_t address, std::uint8_t hour, std::uint8_t minute) noexcept
{
    const std::uint8_t clampedHour { std::clamp<std::uint8_t>(hour, 0U, 23U) };
    const std::uint8_t clampedMinute { std::clamp<std::uint8_t>(minute, 0U, 59U) };
    return PelcoDFrame::createStandardFrame(address, 0x02U, 0x3BU, clampedHour, clampedMinute);
}

StandardFrame FujinonBuilder::buildSetRTCMonthDay(std::uint8_t address, std::uint8_t month, std::uint8_t day) noexcept
{
    const std::uint8_t clampedMonth { std::clamp<std::uint8_t>(month, 1U, 12U) };
    const std::uint8_t clampedDay { std::clamp<std::uint8_t>(day, 1U, 31U) };
    return PelcoDFrame::createStandardFrame(address, 0x04U, 0x3BU, clampedMonth, clampedDay);
}

StandardFrame FujinonBuilder::buildSetRTCYear(std::uint8_t address, std::uint16_t year) noexcept
{
    return PelcoDFrame::createStandardFrame(
        address, 0x06U, 0x3BU, static_cast<std::uint8_t>((year >> 8) & 0xFFU), static_cast<std::uint8_t>(year & 0xFFU));
}

StandardFrame FujinonBuilder::buildQueryRTC(std::uint8_t address, std::uint8_t subOpCode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, subOpCode, 0x3BU, 0x00U, 0x00U);
}

StandardFrame FujinonBuilder::buildQueryZoomStandard(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x55U, 0x00U, 0x00U);
}

StandardFrame FujinonBuilder::buildSetZoomPosition(std::uint8_t address, std::uint16_t zoomPos) noexcept
{
    const std::uint8_t msb { static_cast<std::uint8_t>((zoomPos >> 8U) & 0xFFU) };
    const std::uint8_t lsb { static_cast<std::uint8_t>(zoomPos & 0xFFU) };
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x4FU, msb, lsb);
}

StandardFrame FujinonBuilder::buildSetZoomSpeed(std::uint8_t address, std::uint8_t speed) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(speed, 0U, 3U) };
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x25U, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetFocusSpeed(std::uint8_t address, std::uint8_t speed) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(speed, 0U, 3U) };
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x27U, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetAutoFocus(std::uint8_t address, FujinonAutoFocusMode mode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x2BU, 0x00U, static_cast<std::uint8_t>(mode));
}

StandardFrame FujinonBuilder::buildSetAutoIris(std::uint8_t address, FujinonAutoIrisMode mode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x2DU, 0x00U, static_cast<std::uint8_t>(mode));
}

StandardFrame FujinonBuilder::buildSetAGC(std::uint8_t address, FujinonAGCMode mode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x2FU, 0x00U, static_cast<std::uint8_t>(mode));
}

StandardFrame FujinonBuilder::buildSetBLC(std::uint8_t address, FujinonBLCMode mode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x31U, 0x00U, static_cast<std::uint8_t>(mode));
}

StandardFrame FujinonBuilder::buildSetBaudRate(std::uint8_t address, std::uint8_t baudRateIndex) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x67U, 0x00U, baudRateIndex);
}

StandardFrame FujinonBuilder::buildSetShutterLimitAuto(std::uint8_t address, FujinonShutterLimitAuto limit) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x93U, 0x00U, static_cast<std::uint8_t>(limit));
}

StandardFrame FujinonBuilder::buildQueryManualShutter(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x97U, 0x00U, 0x00U);
}

StandardFrame FujinonBuilder::buildQueryManualISO(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x9BU, 0x00U, 0x00U);
}

StandardFrame FujinonBuilder::buildQueryManualIris(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0x00U, 0x9DU, 0x00U, 0x00U);
}

StandardFrame FujinonBuilder::buildSetPhotoModePreset(std::uint8_t address, FujinonPhotoModePreset preset) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x17U, 0x00U, static_cast<std::uint8_t>(preset));
}

StandardFrame FujinonBuilder::buildSetExternalDayNight(std::uint8_t address, FujinonExternalDayNight mode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x19U, 0x00U, static_cast<std::uint8_t>(mode));
}

StandardFrame FujinonBuilder::buildQueryDisplay(std::uint8_t address, std::uint8_t target) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x5DU, target, 0x00U);
}

StandardFrame FujinonBuilder::buildSetTimeFormat(std::uint8_t address, FujinonTimeFormat fmt) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x61U, 0x00U, static_cast<std::uint8_t>(fmt));
}

StandardFrame FujinonBuilder::buildSetDateFormat(std::uint8_t address, FujinonDateFormat fmt) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x63U, 0x00U, static_cast<std::uint8_t>(fmt));
}

StandardFrame FujinonBuilder::buildSetVideoDisplay(std::uint8_t address, FujinonVideoDisplayMode mode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x6BU, 0x00U, static_cast<std::uint8_t>(mode));
}

StandardFrame FujinonBuilder::buildSetRS485Address(std::uint8_t address, std::uint8_t newId) noexcept
{
    const std::uint8_t clamped { std::clamp<std::uint8_t>(newId, 1U, 31U) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x6FU, 0x00U, clamped);
}

StandardFrame FujinonBuilder::buildSetSDRecordingMode(std::uint8_t address, FujinonSDRecordMode mode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x77U, 0x00U, static_cast<std::uint8_t>(mode));
}

StandardFrame FujinonBuilder::buildSetSDCapacityDisp(std::uint8_t address, bool enable) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x79U, 0x00U, enable ? 0x01U : 0x02U);
}

StandardFrame FujinonBuilder::buildRecordLogData(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x7FU, 0x00U, 0x01U);
}

StandardFrame FujinonBuilder::buildSetEncodeMode(std::uint8_t address, FujinonVideoEncoding enc) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x91U, 0x00U, static_cast<std::uint8_t>(enc));
}

StandardFrame FujinonBuilder::buildSetFirstMovieFile(std::uint8_t address, std::uint16_t fileIndex) noexcept
{
    const std::uint8_t msb { static_cast<std::uint8_t>((fileIndex >> 8U) & 0xFFU) };
    const std::uint8_t lsb { static_cast<std::uint8_t>(fileIndex & 0xFFU) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x95U, msb, lsb);
}

StandardFrame FujinonBuilder::buildPlayMovieMode(std::uint8_t address, FujinonMoviePlayMode mode) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x97U, 0x00U, static_cast<std::uint8_t>(mode));
}

StandardFrame FujinonBuilder::buildDeleteMovie(std::uint8_t address, std::uint16_t fileIndex) noexcept
{
    const std::uint8_t msb { static_cast<std::uint8_t>((fileIndex >> 8U) & 0xFFU) };
    const std::uint8_t lsb { static_cast<std::uint8_t>(fileIndex & 0xFFU) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0x99U, msb, lsb);
}

StandardFrame FujinonBuilder::buildQueryKeySetting(std::uint8_t address, std::uint8_t target) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xADU, target, 0x00U);
}

StandardFrame FujinonBuilder::buildQueryMovieCount(std::uint8_t address) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xB1U, 0xB3U, 0x00U);
}

StandardFrame FujinonBuilder::buildQueryMovieYear(std::uint8_t address, std::uint16_t fileIndex) noexcept
{
    const std::uint8_t msb { static_cast<std::uint8_t>((fileIndex >> 8U) & 0xFFU) };
    const std::uint8_t lsb { static_cast<std::uint8_t>(fileIndex & 0xFFU) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xB5U, msb, lsb);
}

StandardFrame FujinonBuilder::buildQueryMovieDate(std::uint8_t address, std::uint16_t fileIndex) noexcept
{
    const std::uint8_t msb { static_cast<std::uint8_t>((fileIndex >> 8U) & 0xFFU) };
    const std::uint8_t lsb { static_cast<std::uint8_t>(fileIndex & 0xFFU) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xB9U, msb, lsb);
}

StandardFrame FujinonBuilder::buildQueryMovieTime(std::uint8_t address, std::uint16_t fileIndex) noexcept
{
    const std::uint8_t msb { static_cast<std::uint8_t>((fileIndex >> 8U) & 0xFFU) };
    const std::uint8_t lsb { static_cast<std::uint8_t>(fileIndex & 0xFFU) };
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xBDU, msb, lsb);
}

StandardFrame FujinonBuilder::buildQueryLogData(std::uint8_t address, FujinonLogType logType) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF0U, 0xD1U, 0xD3U, static_cast<std::uint8_t>(logType));
}

StandardFrame FujinonBuilder::buildSetDigitalZoomEx(std::uint8_t address, std::uint8_t magIndex) noexcept
{
    return PelcoDFrame::createStandardFrame(address, 0xF1U, 0x21U, 0x00U, magIndex);
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
