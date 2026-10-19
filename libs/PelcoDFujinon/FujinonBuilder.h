#pragma once

/// @file FujinonBuilder.h
/// @brief Frame encoder for Fujinon SX800 / SX801 extended Pelco-D commands.

#include "FujinonTypes.h"
#include "PelcoDFrame.h"

#include <cstdint>
#include <vector>

namespace PelcoD {

/// @class FujinonBuilder
/// @brief Factory methods constructing standard and proprietary Pelco-D packets for Fujinon SX800.
class FujinonBuilder {
public:
    FujinonBuilder() = delete;

    /// @brief Construct a command to configure Optical/Electronic Image Stabilization.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Stabilization mode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetOIS(std::uint8_t address, FujinonOISMode mode) noexcept;

    /// @brief Construct a command to set optical and digital defog level.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Defog intensity level.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetDefog(std::uint8_t address, FujinonDefogLevel level) noexcept;

    /// @brief Construct a command to set de-heat haze level.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Heat haze reduction level.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetHeatHaze(std::uint8_t address, FujinonHeatHazeLevel level) noexcept;

    /// @brief Construct a command to configure Wide Dynamic Range.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level WDR intensity level.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetWDR(std::uint8_t address, FujinonWDRLevel level) noexcept;

    /// @brief Construct a command to toggle visible-light-cut (VLC) filter.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to engage VLC filter, false to retract.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetVLCFilter(std::uint8_t address, bool enable) noexcept;

    /// @brief Construct a command to adjust image brightness.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Brightness level (1 to 21, default 11).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetBrightness(std::uint8_t address, std::uint8_t level) noexcept;

    /// @brief Construct a command to adjust image contrast.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Contrast level (1 to 5, default 3).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetContrast(std::uint8_t address, std::uint8_t level) noexcept;

    /// @brief Construct a command to adjust color saturation.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Saturation level (1 to 5, default 3).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetSaturation(std::uint8_t address, std::uint8_t level) noexcept;

    /// @brief Construct a command to adjust sharpness enhancement.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Sharpness level (1 to 5, default 4).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetSharpness(std::uint8_t address, std::uint8_t level) noexcept;

    /// @brief Construct a command to select manual color temperature preset.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] temp Color temperature preset (3000K, 5000K, 9000K).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetColorTemperature(std::uint8_t address, FujinonColorTemp temp) noexcept;

    /// @brief Construct a command to select white balance mode.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode White balance mode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetWhiteBalance(std::uint8_t address, FujinonWBMode mode) noexcept;

    /// @brief Construct a command to set digital zoom factor.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] zoom Digital magnification preset.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetDigitalZoom(std::uint8_t address, FujinonDigitalZoom zoom) noexcept;

    /// @brief Construct a command to set noise reduction level.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level 2D/3D noise reduction intensity (1 to 3).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetNoiseReduction(std::uint8_t address, std::uint8_t level) noexcept;

    /// @brief Construct a command to configure Day/Night IR cut filter mode.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Day/night mode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetDayNight(std::uint8_t address, FujinonDayNightMode mode) noexcept;

    /// @brief Construct a command to select night IR bandpass wavelength.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] wavelength Target IR wavelength filter.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetIRWavelength(
        std::uint8_t address, FujinonIRWavelength wavelength) noexcept;

    /// @brief Construct a command to position focus to absolute coordinate.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] focusPos 16-bit focus position value.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetFocusPosition(std::uint8_t address, std::uint16_t focusPos) noexcept;

    /// @brief Construct a command triggering One-Push auto-focus execution.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildOnePushAF(std::uint8_t address) noexcept;

    /// @brief Construct a command to configure auto-focus tracking sensitivity.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] sensitivity Sensitivity level (1 to 3).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetAFSensitivity(std::uint8_t address, std::uint8_t sensitivity) noexcept;

    /// @brief Construct a command to configure auto-focus measurement area.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] area Target AF area selector.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetAFArea(std::uint8_t address, std::uint8_t area) noexcept;

    /// @brief Construct a command to set manual iris aperture value.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] irisVal Iris aperture level.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetManualIris(std::uint8_t address, std::uint8_t irisVal) noexcept;

    /// @brief Construct a command to set manual electronic shutter speed.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] shutterVal Shutter speed index.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetManualShutter(std::uint8_t address, std::uint8_t shutterVal) noexcept;

    /// @brief Construct a command to set manual sensor gain / ISO sensitivity.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] isoVal ISO gain index.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetManualISO(std::uint8_t address, std::uint8_t isoVal) noexcept;

    /// @brief Construct a command to toggle on-screen time display overlay.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to display, false to hide.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetTimeDisplay(std::uint8_t address, bool enable) noexcept;

    /// @brief Construct a command to set on-screen time display corner position.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] pos Target corner position.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetTimePosition(std::uint8_t address, FujinonOSDPosition pos) noexcept;

    /// @brief Construct a command to toggle on-screen title text overlay.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to display, false to hide.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetTitleDisplay(std::uint8_t address, bool enable) noexcept;

    /// @brief Construct a command to set on-screen title corner position.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] pos Target corner position.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetTitlePosition(std::uint8_t address, FujinonOSDPosition pos) noexcept;

    /// @brief Construct a command to toggle camera ID overlay on screen.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to display, false to hide.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetIdDisplay(std::uint8_t address, bool enable) noexcept;

    /// @brief Construct a command to set camera ID corner position.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] pos Target corner position.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetIdPosition(std::uint8_t address, FujinonOSDPosition pos) noexcept;

    /// @brief Construct a command to toggle crosshair reticle overlay.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to display crosshair, false to hide.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetReticleDisplay(std::uint8_t address, bool enable) noexcept;

    /// @brief Construct a command to configure base analog/broadcast video standard.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Video standard (NTSC / PAL).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetVideoMode(std::uint8_t address, FujinonVideoMode mode) noexcept;

    /// @brief Construct a command to set HD SDI / HDMI output format and framerate.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] format Target Full HD output format.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetHDFormat(std::uint8_t address, FujinonHDFormat format) noexcept;

    /// @brief Construct a command to engage or release internal 120-ohm RS-485 termination resistor.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to engage internal bus termination.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetRS485Termination(std::uint8_t address, bool enable) noexcept;

    /// @brief Construct a system reboot command.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildReboot(std::uint8_t address) noexcept;

    /// @brief Construct a command to start or stop live video recording to SD card.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] start True to start recording, false to stop.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildRecordLiveView(std::uint8_t address, bool start) noexcept;

    /// @brief Construct a command to start or pause recorded movie playback.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] start True to start playback, false to stop.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildPlayMovie(std::uint8_t address, bool start) noexcept;

    /// @brief Construct a command switching between live monitoring and movie playback mode.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] isPlayMode True for movie playback mode, false for live monitoring.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetMovieMode(std::uint8_t address, bool isPlayMode) noexcept;

    /// @brief Construct an OSD menu Enter / Confirm keypress command.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildMenuOk(std::uint8_t address) noexcept;

    /// @brief Construct an OSD menu directional navigation keypress command.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] dir Direction (Up, Down, Left, Right).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildMenuDirection(std::uint8_t address, FujinonMenuDirection dir) noexcept;

    /// @brief Construct a telemetry query for current absolute focus position.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static StandardFrame buildQueryFocus(std::uint8_t address) noexcept;

    /// @brief Construct a telemetry query for current optical zoom magnification.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static StandardFrame buildQueryZoom(std::uint8_t address) noexcept;

    /// @brief Construct a query packet requesting device hardware serial number.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static StandardFrame buildQuerySerialNumber(std::uint8_t address) noexcept;

    /// @brief Construct a query packet requesting installed firmware version string.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static StandardFrame buildQueryFirmwareVersion(std::uint8_t address) noexcept;

    /// @brief Construct a query packet requesting lens diagnostic status flags.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static StandardFrame buildQueryLensStatus(std::uint8_t address) noexcept;

    /// @brief Construct a query packet requesting current photo / exposure mode settings.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] target Specific setting opcode or 0x00 for general query.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static StandardFrame buildQueryPhotoSettings(
        std::uint8_t address, std::uint8_t target = 0x00U) noexcept;

    /// @brief Construct a query packet requesting current image quality and filter settings.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] target Specific setting opcode or 0x00 for general query.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static StandardFrame buildQueryImageQuality(
        std::uint8_t address, std::uint8_t target = 0x00U) noexcept;

    /// @brief Construct a query packet requesting manual exposure override parameters.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static StandardFrame buildQueryManualSettings(std::uint8_t address) noexcept;

    // -------------------------------------------------------------------------
    // Fujinon SX800 v2.12.0 Extensions
    // -------------------------------------------------------------------------

    /// @brief Construct a command to adjust fine image brightness (1..100, default 50).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Fine brightness level (1 to 100).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetBrightnessFine(std::uint8_t address, std::uint8_t level) noexcept;

    /// @brief Construct a command to adjust fine image contrast (1..100, default 50).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Fine contrast level (1 to 100).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetContrastFine(std::uint8_t address, std::uint8_t level) noexcept;

    /// @brief Construct a command to adjust fine image saturation (1..100, default 50).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Fine saturation level (1 to 100).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetSaturationFine(std::uint8_t address, std::uint8_t level) noexcept;

    /// @brief Construct a command to adjust fine image sharpness (1..100, default 50).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Fine sharpness level (1 to 100).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetSharpnessFine(std::uint8_t address, std::uint8_t level) noexcept;

    /// @brief Construct a command to adjust fine white balance red shift (1..100, default 50).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Fine WB shift red level (1 to 100).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetWBShiftRedFine(std::uint8_t address, std::uint8_t level) noexcept;

    /// @brief Construct a command to adjust fine white balance blue shift (1..100, default 50).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Fine WB shift blue level (1 to 100).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetWBShiftBlueFine(std::uint8_t address, std::uint8_t level) noexcept;

    /// @brief Construct a query packet requesting fine image quality setting.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] target Target setting opcode (0xEB, 0xED, 0xEF, 0xF1, 0xF5, 0xF7).
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static StandardFrame buildQueryImageQualityFine(std::uint8_t address, std::uint8_t target) noexcept;

    /// @brief Construct a command to configure extended Day/Night mode.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Extended day/night mode preset.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetDayNightModeEx(
        std::uint8_t address, FujinonDayNightModeEx mode) noexcept;

    /// @brief Construct a command to configure Day to Night switching threshold (0..255).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] threshold Luminance threshold.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetDayToNightThreshold(
        std::uint8_t address, std::uint8_t threshold) noexcept;

    /// @brief Construct a command to configure Night to Day switching threshold (0..255).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] threshold Luminance threshold.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetNightToDayThreshold(
        std::uint8_t address, std::uint8_t threshold) noexcept;

    /// @brief Construct a command to configure Day/Night auto-switching delay seconds (0..60s).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] delaySeconds Delay time in seconds.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetDayNightAutoDelay(
        std::uint8_t address, std::uint8_t delaySeconds) noexcept;

    /// @brief Construct a command to configure scheduled Day start time.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] hour Hour (0..23).
    /// @param[in] minute Minute (0..59).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetDayStartTime(
        std::uint8_t address, std::uint8_t hour, std::uint8_t minute) noexcept;

    /// @brief Construct a command to configure scheduled Night start time.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] hour Hour (0..23).
    /// @param[in] minute Minute (0..59).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetNightStartTime(
        std::uint8_t address, std::uint8_t hour, std::uint8_t minute) noexcept;

    /// @brief Construct a command to configure Day optical filter state.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] irPass False for IR cut (Day), true for IR pass.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetOpticalFilterDay(std::uint8_t address, bool irPass) noexcept;

    /// @brief Construct a command to configure Night optical filter state.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] irPass False for IR cut, true for IR pass (Night).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetOpticalFilterNight(std::uint8_t address, bool irPass) noexcept;

    /// @brief Construct a query packet requesting extended Day/Night settings.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] target Target opcode or 0x00 for general query.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static StandardFrame buildQueryDayNightEx(std::uint8_t address, std::uint8_t target = 0x00U) noexcept;

    /// @brief Construct a command to configure extended zoom speed (1..8).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] speed Speed level (1: 4s, 2: 6s, 3: 8s, 4: 10s, 5: 15s, 6: 20s, 7: 30s, 8: 60s).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetZoomSpeedEx(std::uint8_t address, std::uint8_t speed) noexcept;

    /// @brief Construct a command to configure extended focus speed (1..5).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] speed Focus speed level (1 to 5).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetFocusSpeedEx(std::uint8_t address, std::uint8_t speed) noexcept;

    /// @brief Construct a command to set digital zoom operation mode (Off, Digital Zoom, Crop Mode).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Digital zoom mode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetDigitalZoomMode(
        std::uint8_t address, FujinonDigitalZoomMode mode) noexcept;

    /// @brief Construct a query packet requesting extended zoom/focus settings.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] target Target opcode or 0x00 for general query.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static StandardFrame buildQueryZoomFocusEx(
        std::uint8_t address, std::uint8_t target = 0x00U) noexcept;

    /// @brief Construct a command to toggle anti-aliasing filter.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to enable antialiasing, false to disable.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetAntialiasing(std::uint8_t address, bool enable) noexcept;

    /// @brief Construct an OSD menu Back keypress command.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildMenuBack(std::uint8_t address) noexcept;

    /// @brief Construct a command to format the SD card.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildFormatSDCard(std::uint8_t address) noexcept;

    /// @brief Construct a command to execute factory reset.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildFactoryReset(std::uint8_t address) noexcept;

    /// @brief Construct a command to set OSD language.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] lang Target language.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetLanguage(std::uint8_t address, FujinonLanguage lang) noexcept;

    /// @brief Construct a command to set real-time clock second and synchronize.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] second Second (0..59).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetRTCSecond(std::uint8_t address, std::uint8_t second) noexcept;

    /// @brief Construct a command to set real-time clock hour and minute.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] hour Hour (0..23).
    /// @param[in] minute Minute (0..59).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetRTCHourMinute(
        std::uint8_t address, std::uint8_t hour, std::uint8_t minute) noexcept;

    /// @brief Construct a command to set real-time clock month and day.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] month Month (1..12).
    /// @param[in] day Day (1..31).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetRTCMonthDay(
        std::uint8_t address, std::uint8_t month, std::uint8_t day) noexcept;

    /// @brief Construct a command to set real-time clock year.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] year 4-digit Gregorian year (e.g. 2026).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetRTCYear(std::uint8_t address, std::uint16_t year) noexcept;

    /// @brief Construct a query packet requesting real-time clock field.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] subOpCode RTC query subcommand (0x01: second, 0x03: hour/minute, 0x05: month/day, 0x07: year).
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static StandardFrame buildQueryRTC(std::uint8_t address, std::uint8_t subOpCode) noexcept;

    /// @brief Construct a standard Pelco-D query packet requesting zoom position (0x00 0x55).
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static StandardFrame buildQueryZoomStandard(std::uint8_t address) noexcept;

    /// @brief Construct a command to set direct zoom coordinate (0x00 0x4F).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] zoomPos 16-bit zoom coordinate.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetZoomPosition(std::uint8_t address, std::uint16_t zoomPos) noexcept;

    /// @brief Construct a command to configure standard zoom speed (0x00 0x25).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] speed Zoom speed (0..3).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetZoomSpeed(std::uint8_t address, std::uint8_t speed) noexcept;

    /// @brief Construct a command to configure standard focus speed (0x00 0x27).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] speed Focus speed (0..3).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetFocusSpeed(std::uint8_t address, std::uint8_t speed) noexcept;

    /// @brief Construct a command to configure auto-focus mode (0x00 0x2B).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Auto focus mode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetAutoFocus(std::uint8_t address, FujinonAutoFocusMode mode) noexcept;

    /// @brief Construct a command to configure auto-iris mode (0x00 0x2D).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Auto iris mode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetAutoIris(std::uint8_t address, FujinonAutoIrisMode mode) noexcept;

    /// @brief Construct a command to configure automatic gain control (0x00 0x2F).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode AGC mode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetAGC(std::uint8_t address, FujinonAGCMode mode) noexcept;

    /// @brief Construct a command to configure backlight compensation (0x00 0x31).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Backlight compensation mode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetBLC(std::uint8_t address, FujinonBLCMode mode) noexcept;

    /// @brief Construct a command to configure remote baud rate (0x00 0x67).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] baudRateIndex Baud rate index (0:2400 to 5:115200).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetBaudRate(std::uint8_t address, std::uint8_t baudRateIndex) noexcept;

    /// @brief Construct a command to set AE lower shutter limit (0x00 0x93).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] limit Auto shutter lower limit.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetShutterLimitAuto(
        std::uint8_t address, FujinonShutterLimitAuto limit) noexcept;

    /// @brief Construct a query for manual electronic shutter speed (0x00 0x97).
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildQueryManualShutter(std::uint8_t address) noexcept;

    /// @brief Construct a query for manual ISO gain sensitivity (0x00 0x9B).
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildQueryManualISO(std::uint8_t address) noexcept;

    /// @brief Construct a query for manual aperture iris (0x00 0x9D).
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildQueryManualIris(std::uint8_t address) noexcept;

    /// @brief Construct a command to select photo scene preset (0xF0 0x17).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] preset Scene preset.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetPhotoModePreset(
        std::uint8_t address, FujinonPhotoModePreset preset) noexcept;

    /// @brief Construct a command to configure external Day/Night trigger (0xF0 0x19).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode External contact trigger mode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetExternalDayNight(
        std::uint8_t address, FujinonExternalDayNight mode) noexcept;

    /// @brief Construct a query for OSD display settings (0xF0 0x5D).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] target Query target opcode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildQueryDisplay(std::uint8_t address, std::uint8_t target = 0x00U) noexcept;

    /// @brief Construct a command to set OSD time display format (0xF0 0x61).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] fmt Time format (12h or 24h).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetTimeFormat(std::uint8_t address, FujinonTimeFormat fmt) noexcept;

    /// @brief Construct a command to set OSD calendar date format (0xF0 0x63).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] fmt Date format.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetDateFormat(std::uint8_t address, FujinonDateFormat fmt) noexcept;

    /// @brief Construct a command to set video aspect display mode (0xF0 0x6B).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Aspect display mode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetVideoDisplay(
        std::uint8_t address, FujinonVideoDisplayMode mode) noexcept;

    /// @brief Construct a command to reprogram RS-485 bus address (0xF0 0x6F).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] newId Target bus address (1..31).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetRS485Address(std::uint8_t address, std::uint8_t newId) noexcept;

    /// @brief Construct a command to set SD card recording mode (0xF0 0x77).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Full-card behavior (overwrite or stop).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetSDRecordingMode(std::uint8_t address, FujinonSDRecordMode mode) noexcept;

    /// @brief Construct a command to toggle SD card capacity remaining display (0xF0 0x79).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to display capacity on OSD.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetSDCapacityDisp(std::uint8_t address, bool enable) noexcept;

    /// @brief Construct a command to record diagnostic log data to SD card (0xF0 0x7F).
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildRecordLogData(std::uint8_t address) noexcept;

    /// @brief Construct a command to configure video encoding codec (0xF0 0x91).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enc Video encoding format.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetEncodeMode(std::uint8_t address, FujinonVideoEncoding enc) noexcept;

    /// @brief Construct a command to select first file for movie playback (0xF0 0x95).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] fileIndex 1-based movie file index.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetFirstMovieFile(std::uint8_t address, std::uint16_t fileIndex) noexcept;

    /// @brief Construct a command to play movie with play mode (0xF0 0x97).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Play mode (Stop, Step, Continuous).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildPlayMovieMode(std::uint8_t address, FujinonMoviePlayMode mode) noexcept;

    /// @brief Construct a command to delete movie file from SD card (0xF0 0x99).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] fileIndex File index or 0x0000 to delete all.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildDeleteMovie(std::uint8_t address, std::uint16_t fileIndex) noexcept;

    /// @brief Construct a query for key setting (0xF0 0xAD).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] target Target opcode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildQueryKeySetting(std::uint8_t address, std::uint8_t target = 0x00U) noexcept;

    /// @brief Construct a query for total number of recorded movies on SD card (0xF0 0xB1).
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildQueryMovieCount(std::uint8_t address) noexcept;

    /// @brief Construct a query for movie recording year (0xF0 0xB5).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] fileIndex Movie file index.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildQueryMovieYear(std::uint8_t address, std::uint16_t fileIndex) noexcept;

    /// @brief Construct a query for movie recording month/day (0xF0 0xB9).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] fileIndex Movie file index.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildQueryMovieDate(std::uint8_t address, std::uint16_t fileIndex) noexcept;

    /// @brief Construct a query for movie recording hour/minute (0xF0 0xBD).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] fileIndex Movie file index.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildQueryMovieTime(std::uint8_t address, std::uint16_t fileIndex) noexcept;

    /// @brief Construct a query for internal diagnostic log data (0xF0 0xD1).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] logType Category of log data.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildQueryLogData(std::uint8_t address, FujinonLogType logType) noexcept;

    /// @brief Construct a command to set digital zoom magnification ratio (0xF1 0x21).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] magIndex 0:1.25x, 1:1.50x, 2:1.75x, 3:2.00x.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static StandardFrame buildSetDigitalZoomEx(std::uint8_t address, std::uint8_t magIndex) noexcept;

    /// @brief Convert raw optical zoom position to physical focal length in millimeters.
    /// @param[in] rawZoom Raw 16-bit zoom coordinate (0x0000 = 20mm to 0x4000 = 800mm).
    /// @param[in] digitalZoom Optional digital zoom magnification factor.
    /// @return Calculated focal length in millimeters.
    [[nodiscard]] static double rawZoomToFocalLengthMm(
        std::uint16_t rawZoom, FujinonDigitalZoom digitalZoom = FujinonDigitalZoom::Off);
    /// @brief Converts a StandardFrame to a std::vector for legacy API compatibility.
    /// @param[in] frame Standard 7-byte protocol array.
    /// @return Byte vector containing the 7 frame bytes.
    [[nodiscard]] static inline std::vector<std::uint8_t> toVector(const StandardFrame& frame)
    {
        return PelcoDFrame::toVector(frame);
    }
};

} // namespace PelcoD
