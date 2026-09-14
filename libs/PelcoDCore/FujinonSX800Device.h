#pragma once

/// @file FujinonSX800Device.h
/// @brief Device controller profile for Fujinon SX800 / SX801 long-range surveillance zoom cameras.

#include "FujinonBuilder.h"
#include "FujinonParser.h"
#include "FujinonTypes.h"
#include "PelcoDDevice.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace PelcoD {

/// @class FujinonSX800Device
/// @brief Specialized PelcoDDevice profile extending standard PTZ with Fujinon optics, OIS, and filter controls.
class FujinonSX800Device : public PelcoDDevice {
public:
    using FujinonStatusCallback = std::function<void(const FujinonStatus& status)>;

    /// @brief Construct a FujinonSX800Device profile with transport and bus address.
    /// @param[in] transport Underlying communication interface.
    /// @param[in] address RS-485 device bus address (1 - 31).
    explicit FujinonSX800Device(std::shared_ptr<ITransport> transport, std::uint8_t address = 1U);
    ~FujinonSX800Device() override = default;

    /// @brief Retrieves the latest snapshot of Fujinon extended status and telemetry.
    [[nodiscard]] FujinonStatus getFujinonStatus() const;

    /// @brief Register a callback for extended Fujinon telemetry updates.
    /// @param[in] cb Function receiving FujinonStatus snapshot.
    void addFujinonStatusCallback(FujinonStatusCallback cb);

    // Stabilization & Image Enhancement
    /// @brief Select Optical/Electronic image stabilization mode.
    /// @param[in] mode Stabilization mode.
    void setOISMode(FujinonOISMode mode);

    /// @brief Set defogging intensity level.
    /// @param[in] level Defog level (Off, Level1, Level2, Level3).
    void setDefog(FujinonDefogLevel level);

    /// @brief Set de-heat haze intensity level.
    /// @param[in] level Heat haze level (Off, Level1, Level2).
    void setHeatHaze(FujinonHeatHazeLevel level);

    /// @brief Set Wide Dynamic Range intensity level.
    /// @param[in] level WDR level (Off, Level1, Level2, Level3).
    void setWDR(FujinonWDRLevel level);

    /// @brief Engage or retract visible-light-cut (VLC) filter.
    /// @param[in] enable True to engage VLC filter, false to retract.
    void setVLCFilter(bool enable);

    // Fine Image Adjustments
    /// @brief Set image brightness level.
    /// @param[in] level Brightness level (1 to 21, default 11).
    void setBrightness(std::uint8_t level);

    /// @brief Set image contrast level.
    /// @param[in] level Contrast level (1 to 5, default 3).
    void setContrast(std::uint8_t level);

    /// @brief Set color saturation level.
    /// @param[in] level Saturation level (1 to 5, default 3).
    void setSaturation(std::uint8_t level);

    /// @brief Set sharpness level.
    /// @param[in] level Sharpness level (1 to 5, default 4).
    void setSharpness(std::uint8_t level);

    /// @brief Set color temperature preset when white balance is ColorTemp.
    /// @param[in] temp Color temperature preset (3000K, 5000K, 9000K).
    void setColorTemperature(FujinonColorTemp temp);

    /// @brief Set white balance operating mode.
    /// @param[in] mode White balance mode.
    void setWhiteBalance(FujinonWBMode mode);

    /// @brief Set digital zoom magnification factor.
    /// @param[in] zoom Digital zoom level.
    void setDigitalZoom(FujinonDigitalZoom zoom);

    /// @brief Set noise reduction level.
    /// @param[in] level 2D/3D noise reduction intensity (1 to 3).
    void setNoiseReduction(std::uint8_t level);

    // Day / Night & Infrared Filters
    /// @brief Set Day / Night IR cut filter mode.
    /// @param[in] mode Day/night mode.
    void setDayNightMode(FujinonDayNightMode mode);

    /// @brief Select infrared bandpass wavelength for night surveillance.
    /// @param[in] wavelength Target IR wavelength filter.
    void setIRWavelength(FujinonIRWavelength wavelength);

    // Optics & Focus Control
    /// @brief Drive lens focus to absolute 16-bit position.
    /// @param[in] focusPos Target focus position.
    void setFocusPosition(std::uint16_t focusPos);

    /// @brief Trigger One-Push auto-focus execution.
    void onePushAF();

    /// @brief Set auto-focus tracking sensitivity.
    /// @param[in] sensitivity Sensitivity level (1 to 3).
    void setAFSensitivity(std::uint8_t sensitivity);

    /// @brief Select auto-focus measurement area.
    /// @param[in] area Target AF area selector.
    void setAFArea(std::uint8_t area);

    // Exposure & Sensor Manual Controls
    /// @brief Set manual aperture iris value.
    /// @param[in] irisVal Iris aperture level.
    void setManualIris(std::uint8_t irisVal);

    /// @brief Set manual electronic shutter speed.
    /// @param[in] shutterVal Shutter speed index.
    void setManualShutter(std::uint8_t shutterVal);

    /// @brief Set manual sensor gain / ISO sensitivity.
    /// @param[in] isoVal ISO gain index.
    void setManualISO(std::uint8_t isoVal);

    // OSD Screen Display Overlays
    /// @brief Toggle on-screen time display overlay.
    /// @param[in] enable True to display, false to hide.
    void setTimeDisplay(bool enable);

    /// @brief Set on-screen time display corner position.
    /// @param[in] pos Target corner position.
    void setTimePosition(FujinonOSDPosition pos);

    /// @brief Toggle on-screen title text overlay.
    /// @param[in] enable True to display, false to hide.
    void setTitleDisplay(bool enable);

    /// @brief Set on-screen title corner position.
    /// @param[in] pos Target corner position.
    void setTitlePosition(FujinonOSDPosition pos);

    /// @brief Toggle camera ID display overlay.
    /// @param[in] enable True to display, false to hide.
    void setIdDisplay(bool enable);

    /// @brief Set camera ID corner position.
    /// @param[in] pos Target corner position.
    void setIdPosition(FujinonOSDPosition pos);

    /// @brief Toggle crosshair reticle overlay.
    /// @param[in] enable True to display crosshair, false to hide.
    void setReticleDisplay(bool enable);

    // Operation & System Config
    /// @brief Configure base analog/broadcast video standard.
    /// @param[in] mode Video standard (NTSC / PAL).
    void setVideoMode(FujinonVideoMode mode);

    /// @brief Set Full HD SDI / HDMI output format and framerate.
    /// @param[in] format Target Full HD output format.
    void setHDFormat(FujinonHDFormat format);

    /// @brief Engage or release internal 120-ohm RS-485 termination resistor.
    /// @param[in] enable True to engage internal bus termination.
    void setRS485Termination(bool enable);

    /// @brief Execute camera software reboot.
    void reboot();

    // Media & OSD Menu Navigation
    /// @brief Start or stop live video recording to SD card.
    /// @param[in] start True to start recording, false to stop.
    void recordLiveView(bool start);

    /// @brief Start or pause recorded movie playback.
    /// @param[in] start True to start playback, false to stop.
    void playMovie(bool start);

    /// @brief Switch between live monitoring and movie playback mode.
    /// @param[in] isPlayMode True for movie playback mode, false for live monitoring.
    void setMovieMode(bool isPlayMode);

    /// @brief Send OSD menu Enter / Confirm keypress.
    void menuOk();

    /// @brief Send OSD menu directional navigation keypress.
    /// @param[in] dir Direction (Up, Down, Left, Right).
    void menuDirection(FujinonMenuDirection dir);

    // Telemetry Queries
    /// @brief Send query for current absolute focus position.
    void queryFujinonFocus();

    /// @brief Send query for current optical zoom magnification.
    void queryFujinonZoom();

    /// @brief Send query for camera hardware serial number.
    void querySerialNumber();

    /// @brief Send query for camera firmware version string.
    void queryFirmwareVersion();

    /// @brief Send query for lens diagnostic status flags.
    void queryLensStatus();

    /// @brief Send query for current photo / exposure mode settings.
    /// @param[in] target Specific setting opcode or 0x00 for general query.
    void queryPhotoSettings(std::uint8_t target = 0x00U);

    /// @brief Send query for current image quality and filter settings.
    /// @param[in] target Specific setting opcode or 0x00 for general query.
    void queryImageQuality(std::uint8_t target = 0x00U);

    /// @brief Send query for manual exposure override parameters.
    void queryManualSettings();

    // -------------------------------------------------------------------------
    // Fujinon SX800 v2.12.0 Extended Controls
    // -------------------------------------------------------------------------

    /// @brief Set fine brightness level (1..100, default 50).
    /// @param[in] level Brightness level (1 to 100).
    void setBrightnessFine(std::uint8_t level);

    /// @brief Set fine contrast level (1..100, default 50).
    /// @param[in] level Contrast level (1 to 100).
    void setContrastFine(std::uint8_t level);

    /// @brief Set fine color saturation level (1..100, default 50).
    /// @param[in] level Saturation level (1 to 100).
    void setSaturationFine(std::uint8_t level);

    /// @brief Set fine sharpness level (1..100, default 50).
    /// @param[in] level Sharpness level (1 to 100).
    void setSharpnessFine(std::uint8_t level);

    /// @brief Set fine white balance red shift (1..100, default 50).
    /// @param[in] level WB shift red level (1 to 100).
    void setWBShiftRedFine(std::uint8_t level);

    /// @brief Set fine white balance blue shift (1..100, default 50).
    /// @param[in] level WB shift blue level (1 to 100).
    void setWBShiftBlueFine(std::uint8_t level);

    /// @brief Send query for fine image quality setting.
    /// @param[in] target Target opcode (e.g. 0xEB, 0xED, etc.).
    void queryImageQualityFine(std::uint8_t target);

    /// @brief Set extended Day/Night mode.
    /// @param[in] mode Extended day/night mode preset.
    void setDayNightModeEx(FujinonDayNightModeEx mode);

    /// @brief Set Day to Night switching threshold (0..255).
    /// @param[in] threshold Luminance threshold.
    void setDayToNightThreshold(std::uint8_t threshold);

    /// @brief Set Night to Day switching threshold (0..255).
    /// @param[in] threshold Luminance threshold.
    void setNightToDayThreshold(std::uint8_t threshold);

    /// @brief Set Day/Night auto-switching delay seconds (0..60s).
    /// @param[in] delaySeconds Delay time in seconds.
    void setDayNightAutoDelay(std::uint8_t delaySeconds);

    /// @brief Set scheduled Day start time.
    /// @param[in] hour Hour (0..23).
    /// @param[in] minute Minute (0..59).
    void setDayStartTime(std::uint8_t hour, std::uint8_t minute);

    /// @brief Set scheduled Night start time.
    /// @param[in] hour Hour (0..23).
    /// @param[in] minute Minute (0..59).
    void setNightStartTime(std::uint8_t hour, std::uint8_t minute);

    /// @brief Set Day optical filter state.
    /// @param[in] irPass False for IR cut (Day), true for IR pass.
    void setOpticalFilterDay(bool irPass);

    /// @brief Set Night optical filter state.
    /// @param[in] irPass False for IR cut, true for IR pass (Night).
    void setOpticalFilterNight(bool irPass);

    /// @brief Send query for extended Day/Night settings.
    /// @param[in] target Target opcode or 0x00 for general query.
    void queryDayNightEx(std::uint8_t target = 0x00U);

    /// @brief Set extended zoom speed (1..8).
    /// @param[in] speed Speed level (1: 4s to 8: 60s).
    void setZoomSpeedEx(std::uint8_t speed);

    /// @brief Set extended focus speed (1..5).
    /// @param[in] speed Focus speed level (1 to 5).
    void setFocusSpeedEx(std::uint8_t speed);

    /// @brief Set digital zoom operation mode.
    /// @param[in] mode Digital zoom mode (Off, Digital Zoom, Crop Mode).
    void setDigitalZoomMode(FujinonDigitalZoomMode mode);

    /// @brief Send query for extended zoom/focus settings.
    /// @param[in] target Target opcode or 0x00 for general query.
    void queryZoomFocusEx(std::uint8_t target = 0x00U);

    /// @brief Toggle anti-aliasing filter.
    /// @param[in] enable True to enable antialiasing, false to disable.
    void setAntialiasing(bool enable);

    /// @brief Send OSD menu Back keypress.
    void menuBack();

    /// @brief Format internal SD card storage.
    void formatSDCard();

    /// @brief Execute factory reset to restore default parameters.
    void factoryReset();

    /// @brief Set OSD language.
    /// @param[in] lang Target language.
    void setLanguage(FujinonLanguage lang);

    /// @brief Set real-time clock second and synchronize.
    /// @param[in] second Second (0..59).
    void setRTCSecond(std::uint8_t second);

    /// @brief Set real-time clock hour and minute.
    /// @param[in] hour Hour (0..23).
    /// @param[in] minute Minute (0..59).
    void setRTCHourMinute(std::uint8_t hour, std::uint8_t minute);

    /// @brief Set real-time clock month and day.
    /// @param[in] month Month (1..12).
    /// @param[in] day Day (1..31).
    void setRTCMonthDay(std::uint8_t month, std::uint8_t day);

    /// @brief Set real-time clock year.
    /// @param[in] year 4-digit Gregorian year (e.g. 2026).
    void setRTCYear(std::uint16_t year);

    /// @brief Send query for real-time clock field.
    /// @param[in] subOpCode RTC query subcommand (0x01: second, 0x03: hour/minute, 0x05: month/day, 0x07: year).
    void queryRTC(std::uint8_t subOpCode);

    /// @brief Send standard Pelco-D query for optical zoom position (0x00 0x55).
    void queryZoomStandard();

protected:
    void dispatchFrame(const std::vector<std::uint8_t>& frame) override;
    [[nodiscard]] bool isResponseMatchingQuery(
        const std::string& queryTag, const std::vector<std::uint8_t>& frame) const noexcept override;

private:
    mutable std::mutex m_fujinonMutex;
    FujinonStatus m_fujinonStatus {};
    std::shared_ptr<const std::vector<FujinonStatusCallback>> m_fujinonCallbacks {
        std::make_shared<const std::vector<FujinonStatusCallback>>()
    };
};

} // namespace PelcoD
