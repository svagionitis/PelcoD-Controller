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
    [[nodiscard]] static std::vector<std::uint8_t> buildSetOIS(std::uint8_t address, FujinonOISMode mode);

    /// @brief Construct a command to set optical and digital defog level.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Defog intensity level.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDefog(std::uint8_t address, FujinonDefogLevel level);

    /// @brief Construct a command to set de-heat haze level.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Heat haze reduction level.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetHeatHaze(std::uint8_t address, FujinonHeatHazeLevel level);

    /// @brief Construct a command to configure Wide Dynamic Range.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level WDR intensity level.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetWDR(std::uint8_t address, FujinonWDRLevel level);

    /// @brief Construct a command to toggle visible-light-cut (VLC) filter.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to engage VLC filter, false to retract.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVLCFilter(std::uint8_t address, bool enable);

    /// @brief Construct a command to adjust image brightness.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Brightness level (1 to 21, default 11).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetBrightness(std::uint8_t address, std::uint8_t level);

    /// @brief Construct a command to adjust image contrast.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Contrast level (1 to 5, default 3).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetContrast(std::uint8_t address, std::uint8_t level);

    /// @brief Construct a command to adjust color saturation.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Saturation level (1 to 5, default 3).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetSaturation(std::uint8_t address, std::uint8_t level);

    /// @brief Construct a command to adjust sharpness enhancement.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Sharpness level (1 to 5, default 4).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetSharpness(std::uint8_t address, std::uint8_t level);

    /// @brief Construct a command to select manual color temperature preset.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] temp Color temperature preset (3000K, 5000K, 9000K).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetColorTemperature(
        std::uint8_t address, FujinonColorTemp temp);

    /// @brief Construct a command to select white balance mode.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode White balance mode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetWhiteBalance(std::uint8_t address, FujinonWBMode mode);

    /// @brief Construct a command to set digital zoom factor.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] zoom Digital magnification preset.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDigitalZoom(std::uint8_t address, FujinonDigitalZoom zoom);

    /// @brief Construct a command to set noise reduction level.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level 2D/3D noise reduction intensity (1 to 3).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetNoiseReduction(std::uint8_t address, std::uint8_t level);

    /// @brief Construct a command to configure Day/Night IR cut filter mode.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Day/night mode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDayNight(std::uint8_t address, FujinonDayNightMode mode);

    /// @brief Construct a command to select night IR bandpass wavelength.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] wavelength Target IR wavelength filter.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetIRWavelength(
        std::uint8_t address, FujinonIRWavelength wavelength);

    /// @brief Construct a command to position focus to absolute coordinate.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] focusPos 16-bit focus position value.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetFocusPosition(std::uint8_t address, std::uint16_t focusPos);

    /// @brief Construct a command triggering One-Push auto-focus execution.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildOnePushAF(std::uint8_t address);

    /// @brief Construct a command to configure auto-focus tracking sensitivity.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] sensitivity Sensitivity level (1 to 3).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetAFSensitivity(
        std::uint8_t address, std::uint8_t sensitivity);

    /// @brief Construct a command to configure auto-focus measurement area.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] area Target AF area selector.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetAFArea(std::uint8_t address, std::uint8_t area);

    /// @brief Construct a command to set manual iris aperture value.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] irisVal Iris aperture level.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetManualIris(std::uint8_t address, std::uint8_t irisVal);

    /// @brief Construct a command to set manual electronic shutter speed.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] shutterVal Shutter speed index.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetManualShutter(std::uint8_t address, std::uint8_t shutterVal);

    /// @brief Construct a command to set manual sensor gain / ISO sensitivity.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] isoVal ISO gain index.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetManualISO(std::uint8_t address, std::uint8_t isoVal);

    /// @brief Construct a command to toggle on-screen time display overlay.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to display, false to hide.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTimeDisplay(std::uint8_t address, bool enable);

    /// @brief Construct a command to set on-screen time display corner position.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] pos Target corner position.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTimePosition(std::uint8_t address, FujinonOSDPosition pos);

    /// @brief Construct a command to toggle on-screen title text overlay.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to display, false to hide.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTitleDisplay(std::uint8_t address, bool enable);

    /// @brief Construct a command to set on-screen title corner position.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] pos Target corner position.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetTitlePosition(std::uint8_t address, FujinonOSDPosition pos);

    /// @brief Construct a command to toggle camera ID overlay on screen.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to display, false to hide.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetIdDisplay(std::uint8_t address, bool enable);

    /// @brief Construct a command to set camera ID corner position.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] pos Target corner position.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetIdPosition(std::uint8_t address, FujinonOSDPosition pos);

    /// @brief Construct a command to toggle crosshair reticle overlay.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to display crosshair, false to hide.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetReticleDisplay(std::uint8_t address, bool enable);

    /// @brief Construct a command to configure base analog/broadcast video standard.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Video standard (NTSC / PAL).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetVideoMode(std::uint8_t address, FujinonVideoMode mode);

    /// @brief Construct a command to set HD SDI / HDMI output format and framerate.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] format Target Full HD output format.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetHDFormat(std::uint8_t address, FujinonHDFormat format);

    /// @brief Construct a command to engage or release internal 120-ohm RS-485 termination resistor.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to engage internal bus termination.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetRS485Termination(std::uint8_t address, bool enable);

    /// @brief Construct a system reboot command.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildReboot(std::uint8_t address);

    /// @brief Construct a command to start or stop live video recording to SD card.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] start True to start recording, false to stop.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildRecordLiveView(std::uint8_t address, bool start);

    /// @brief Construct a command to start or pause recorded movie playback.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] start True to start playback, false to stop.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildPlayMovie(std::uint8_t address, bool start);

    /// @brief Construct a command switching between live monitoring and movie playback mode.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] isPlayMode True for movie playback mode, false for live monitoring.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetMovieMode(std::uint8_t address, bool isPlayMode);

    /// @brief Construct an OSD menu Enter / Confirm keypress command.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildMenuOk(std::uint8_t address);

    /// @brief Construct an OSD menu directional navigation keypress command.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] dir Direction (Up, Down, Left, Right).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildMenuDirection(std::uint8_t address, FujinonMenuDirection dir);

    /// @brief Construct a telemetry query for current absolute focus position.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryFocus(std::uint8_t address);

    /// @brief Construct a telemetry query for current optical zoom magnification.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryZoom(std::uint8_t address);

    /// @brief Construct a query packet requesting device hardware serial number.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildQuerySerialNumber(std::uint8_t address);

    /// @brief Construct a query packet requesting installed firmware version string.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryFirmwareVersion(std::uint8_t address);

    /// @brief Construct a query packet requesting lens diagnostic status flags.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryLensStatus(std::uint8_t address);

    /// @brief Construct a query packet requesting current photo / exposure mode settings.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] target Specific setting opcode or 0x00 for general query.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryPhotoSettings(
        std::uint8_t address, std::uint8_t target = 0x00U);

    /// @brief Construct a query packet requesting current image quality and filter settings.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] target Specific setting opcode or 0x00 for general query.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryImageQuality(
        std::uint8_t address, std::uint8_t target = 0x00U);

    /// @brief Construct a query packet requesting manual exposure override parameters.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryManualSettings(std::uint8_t address);

    // -------------------------------------------------------------------------
    // Fujinon SX800 v2.12.0 Extensions
    // -------------------------------------------------------------------------

    /// @brief Construct a command to adjust fine image brightness (1..100, default 50).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Fine brightness level (1 to 100).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetBrightnessFine(std::uint8_t address, std::uint8_t level);

    /// @brief Construct a command to adjust fine image contrast (1..100, default 50).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Fine contrast level (1 to 100).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetContrastFine(std::uint8_t address, std::uint8_t level);

    /// @brief Construct a command to adjust fine image saturation (1..100, default 50).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Fine saturation level (1 to 100).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetSaturationFine(std::uint8_t address, std::uint8_t level);

    /// @brief Construct a command to adjust fine image sharpness (1..100, default 50).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Fine sharpness level (1 to 100).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetSharpnessFine(std::uint8_t address, std::uint8_t level);

    /// @brief Construct a command to adjust fine white balance red shift (1..100, default 50).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Fine WB shift red level (1 to 100).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetWBShiftRedFine(std::uint8_t address, std::uint8_t level);

    /// @brief Construct a command to adjust fine white balance blue shift (1..100, default 50).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] level Fine WB shift blue level (1 to 100).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetWBShiftBlueFine(std::uint8_t address, std::uint8_t level);

    /// @brief Construct a query packet requesting fine image quality setting.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] target Target setting opcode (0xEB, 0xED, 0xEF, 0xF1, 0xF5, 0xF7).
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryImageQualityFine(
        std::uint8_t address, std::uint8_t target);

    /// @brief Construct a command to configure extended Day/Night mode.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Extended day/night mode preset.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDayNightModeEx(
        std::uint8_t address, FujinonDayNightModeEx mode);

    /// @brief Construct a command to configure Day to Night switching threshold (0..255).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] threshold Luminance threshold.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDayToNightThreshold(
        std::uint8_t address, std::uint8_t threshold);

    /// @brief Construct a command to configure Night to Day switching threshold (0..255).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] threshold Luminance threshold.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetNightToDayThreshold(
        std::uint8_t address, std::uint8_t threshold);

    /// @brief Construct a command to configure Day/Night auto-switching delay seconds (0..60s).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] delaySeconds Delay time in seconds.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDayNightAutoDelay(
        std::uint8_t address, std::uint8_t delaySeconds);

    /// @brief Construct a command to configure scheduled Day start time.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] hour Hour (0..23).
    /// @param[in] minute Minute (0..59).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDayStartTime(
        std::uint8_t address, std::uint8_t hour, std::uint8_t minute);

    /// @brief Construct a command to configure scheduled Night start time.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] hour Hour (0..23).
    /// @param[in] minute Minute (0..59).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetNightStartTime(
        std::uint8_t address, std::uint8_t hour, std::uint8_t minute);

    /// @brief Construct a command to configure Day optical filter state.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] irPass False for IR cut (Day), true for IR pass.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetOpticalFilterDay(std::uint8_t address, bool irPass);

    /// @brief Construct a command to configure Night optical filter state.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] irPass False for IR cut, true for IR pass (Night).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetOpticalFilterNight(std::uint8_t address, bool irPass);

    /// @brief Construct a query packet requesting extended Day/Night settings.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] target Target opcode or 0x00 for general query.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryDayNightEx(
        std::uint8_t address, std::uint8_t target = 0x00U);

    /// @brief Construct a command to configure extended zoom speed (1..8).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] speed Speed level (1: 4s, 2: 6s, 3: 8s, 4: 10s, 5: 15s, 6: 20s, 7: 30s, 8: 60s).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetZoomSpeedEx(std::uint8_t address, std::uint8_t speed);

    /// @brief Construct a command to configure extended focus speed (1..5).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] speed Focus speed level (1 to 5).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetFocusSpeedEx(std::uint8_t address, std::uint8_t speed);

    /// @brief Construct a command to set digital zoom operation mode (Off, Digital Zoom, Crop Mode).
    /// @param[in] address RS-485 device bus address.
    /// @param[in] mode Digital zoom mode.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetDigitalZoomMode(
        std::uint8_t address, FujinonDigitalZoomMode mode);

    /// @brief Construct a query packet requesting extended zoom/focus settings.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] target Target opcode or 0x00 for general query.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryZoomFocusEx(
        std::uint8_t address, std::uint8_t target = 0x00U);

    /// @brief Construct a command to toggle anti-aliasing filter.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] enable True to enable antialiasing, false to disable.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetAntialiasing(std::uint8_t address, bool enable);

    /// @brief Construct an OSD menu Back keypress command.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildMenuBack(std::uint8_t address);

    /// @brief Construct a command to format the SD card.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildFormatSDCard(std::uint8_t address);

    /// @brief Construct a command to execute factory reset.
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildFactoryReset(std::uint8_t address);

    /// @brief Construct a command to set OSD language.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] lang Target language.
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetLanguage(std::uint8_t address, FujinonLanguage lang);

    /// @brief Construct a command to set real-time clock second and synchronize.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] second Second (0..59).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetRTCSecond(std::uint8_t address, std::uint8_t second);

    /// @brief Construct a command to set real-time clock hour and minute.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] hour Hour (0..23).
    /// @param[in] minute Minute (0..59).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetRTCHourMinute(
        std::uint8_t address, std::uint8_t hour, std::uint8_t minute);

    /// @brief Construct a command to set real-time clock month and day.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] month Month (1..12).
    /// @param[in] day Day (1..31).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetRTCMonthDay(
        std::uint8_t address, std::uint8_t month, std::uint8_t day);

    /// @brief Construct a command to set real-time clock year.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] year 4-digit Gregorian year (e.g. 2026).
    /// @return 7-byte Pelco-D command packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildSetRTCYear(std::uint8_t address, std::uint16_t year);

    /// @brief Construct a query packet requesting real-time clock field.
    /// @param[in] address RS-485 device bus address.
    /// @param[in] subOpCode RTC query subcommand (0x01: second, 0x03: hour/minute, 0x05: month/day, 0x07: year).
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryRTC(std::uint8_t address, std::uint8_t subOpCode);

    /// @brief Construct a standard Pelco-D query packet requesting zoom position (0x00 0x55).
    /// @param[in] address RS-485 device bus address.
    /// @return 7-byte Pelco-D query packet.
    [[nodiscard]] static std::vector<std::uint8_t> buildQueryZoomStandard(std::uint8_t address);

    /// @brief Convert raw optical zoom position to physical focal length in millimeters.
    /// @param[in] rawZoom Raw 16-bit zoom coordinate (0x0000 = 20mm to 0x4000 = 800mm).
    /// @param[in] digitalZoom Optional digital zoom magnification factor.
    /// @return Calculated focal length in millimeters.
    [[nodiscard]] static double rawZoomToFocalLengthMm(
        std::uint16_t rawZoom, FujinonDigitalZoom digitalZoom = FujinonDigitalZoom::Off);
};

} // namespace PelcoD
