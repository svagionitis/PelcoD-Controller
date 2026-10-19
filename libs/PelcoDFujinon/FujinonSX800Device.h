#pragma once

/// @file FujinonSX800Device.h
/// @brief Device controller profile for Fujinon SX800 / SX801 long-range surveillance zoom cameras.

#include <cstdint>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "FujinonBuilder.h"
#include "FujinonParser.h"
#include "FujinonTypes.h"
#include "PelcoDDevice.h"

namespace PelcoD {

/// @class FujinonSX800Device
/// @brief Specialized PelcoDDevice profile extending standard PTZ with Fujinon optics, OIS, and filter controls.
/// @details Fujinon telemetry decoding and query matching live in a private, shared-owned
///          IFrameExtension (FujinonFrameExt) that the base device consults on the RX thread. Because
///          the base co-owns it, destroying this object while frames are arriving is safe: no RX
///          thread ever dispatches into this partially destroyed object (review finding C3).
class FujinonSX800Device final : public PelcoDDevice {
public:
    using FujinonStatusCallback = std::function<void(const FujinonStatus& status)>;

    /// @brief Construct a FujinonSX800Device profile with transport and bus address.
    /// @param[in] transport Underlying communication interface.
    /// @param[in] address RS-485 device bus address (1 - 31).
    explicit FujinonSX800Device(std::shared_ptr<ITransport> transport, std::uint8_t address = 1U);

    /// @brief Destroys the profile; the base destructor drains in-flight RX callbacks.
    ~FujinonSX800Device() override;

    FujinonSX800Device(const FujinonSX800Device&) = delete;
    FujinonSX800Device& operator=(const FujinonSX800Device&) = delete;
    FujinonSX800Device(FujinonSX800Device&&) = delete;
    FujinonSX800Device& operator=(FujinonSX800Device&&) = delete;

    /// @brief Retrieves the latest snapshot of Fujinon extended status and telemetry.
    [[nodiscard]] FujinonStatus getFujinonStatus() const;

    /// @brief Configures device bus address and synchronizes RX frame extension filter.
    /// @param[in] address RS-485 device bus address (1 - 31).
    void setAddress(std::uint8_t address);
    using PelcoDDevice::getAddress;

    /// @brief Register a callback for extended Fujinon telemetry updates.
    /// @param[in] cb Function receiving FujinonStatus snapshot.
    /// @return Connection object to manage or disconnect the subscription.
    Connection addFujinonStatusCallback(FujinonStatusCallback cb);

    /// @brief Removes a Fujinon status callback by identifier.
    /// @param[in] id Callback identifier.
    /// @return True if callback was found and removed; false otherwise.
    [[nodiscard]] bool removeFujinonStatusCallback(CallbackId id);

    /// @brief Removes all base callbacks as well as Fujinon status callbacks.
    void clearCallbacks() override;

    // Stabilization & Image Enhancement
    /// @brief Select Optical/Electronic image stabilization mode.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode Stabilization mode.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setOISMode(FujinonOISMode mode);

    /// @brief Set defogging intensity level.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] level Defog level (Off, Level1, Level2, Level3).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setDefog(FujinonDefogLevel level);

    /// @brief Set de-heat haze intensity level.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] level Heat haze level (Off, Level1, Level2).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setHeatHaze(FujinonHeatHazeLevel level);

    /// @brief Set Wide Dynamic Range intensity level.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] level WDR level (Off, Level1, Level2, Level3).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setWDR(FujinonWDRLevel level);

    /// @brief Engage or retract visible-light-cut (VLC) filter.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] enable True to engage VLC filter, false to retract.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setVLCFilter(bool enable);

    // Fine Image Adjustments
    /// @brief Set image brightness level.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] level Brightness level (1 to 21, default 11).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setBrightness(std::uint8_t level);

    /// @brief Set image contrast level.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] level Contrast level (1 to 5, default 3).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setContrast(std::uint8_t level);

    /// @brief Set color saturation level.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] level Saturation level (1 to 5, default 3).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setSaturation(std::uint8_t level);

    /// @brief Set sharpness level.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] level Sharpness level (1 to 5, default 4).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setSharpness(std::uint8_t level);

    /// @brief Set color temperature preset when white balance is ColorTemp.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] temp Color temperature preset (3000K, 5000K, 9000K).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setColorTemperature(FujinonColorTemp temp);

    /// @brief Set white balance operating mode.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode White balance mode.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setWhiteBalance(FujinonWBMode mode);

    /// @brief Set digital zoom magnification factor.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] zoom Digital zoom level.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setDigitalZoom(FujinonDigitalZoom zoom);

    /// @brief Set noise reduction level.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] level 2D/3D noise reduction intensity (1 to 3).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setNoiseReduction(std::uint8_t level);

    // Day / Night & Infrared Filters
    /// @brief Set Day / Night IR cut filter mode.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode Day/night mode.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setDayNightMode(FujinonDayNightMode mode);

    /// @brief Select infrared bandpass wavelength for night surveillance.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] wavelength Target IR wavelength filter.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setIRWavelength(FujinonIRWavelength wavelength);

    // Optics & Focus Control
    /// @brief Drive lens focus to absolute 16-bit position.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] focusPos Target focus position.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setFocusPosition(std::uint16_t focusPos);

    /// @brief Trigger One-Push auto-focus execution.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult onePushAF();

    /// @brief Set auto-focus tracking sensitivity.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] sensitivity Sensitivity level (1 to 3).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setAFSensitivity(std::uint8_t sensitivity);

    /// @brief Select auto-focus measurement area.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] area Target AF area selector.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setAFArea(std::uint8_t area);

    // Exposure & Sensor Manual Controls
    /// @brief Set manual aperture iris value.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] irisVal Iris aperture level.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setManualIris(std::uint8_t irisVal);

    /// @brief Set manual electronic shutter speed.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] shutterVal Shutter speed index.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setManualShutter(std::uint8_t shutterVal);

    /// @brief Set manual sensor gain / ISO sensitivity.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] isoVal ISO gain index.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setManualISO(std::uint8_t isoVal);

    // OSD Screen Display Overlays
    /// @brief Toggle on-screen time display overlay.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] enable True to display, false to hide.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setTimeDisplay(bool enable);

    /// @brief Set on-screen time display corner position.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] pos Target corner position.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setTimePosition(FujinonOSDPosition pos);

    /// @brief Toggle on-screen title text overlay.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] enable True to display, false to hide.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setTitleDisplay(bool enable);

    /// @brief Set on-screen title corner position.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] pos Target corner position.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setTitlePosition(FujinonOSDPosition pos);

    /// @brief Toggle camera ID display overlay.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] enable True to display, false to hide.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setIdDisplay(bool enable);

    /// @brief Set camera ID corner position.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] pos Target corner position.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setIdPosition(FujinonOSDPosition pos);

    /// @brief Toggle crosshair reticle overlay.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] enable True to display crosshair, false to hide.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setReticleDisplay(bool enable);

    // Operation & System Config
    /// @brief Configure base analog/broadcast video standard.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode Video standard (NTSC / PAL).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setVideoMode(FujinonVideoMode mode);

    /// @brief Set Full HD SDI / HDMI output format and framerate.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] format Target Full HD output format.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setHDFormat(FujinonHDFormat format);

    /// @brief Engage or release internal 120-ohm RS-485 termination resistor.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] enable True to engage internal bus termination.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setRS485Termination(bool enable);

    /// @brief Execute camera software reboot.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult reboot();

    // Media & OSD Menu Navigation
    /// @brief Start or stop live video recording to SD card.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] start True to start recording, false to stop.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult recordLiveView(bool start);

    /// @brief Start or pause recorded movie playback.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] start True to start playback, false to stop.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult playMovie(bool start);

    /// @brief Switch between live monitoring and movie playback mode.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] isPlayMode True for movie playback mode, false for live monitoring.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setMovieMode(bool isPlayMode);

    /// @brief Send OSD menu Enter / Confirm keypress.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult menuOk();

    /// @brief Send OSD menu directional navigation keypress.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] dir Direction (Up, Down, Left, Right).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult menuDirection(FujinonMenuDirection dir);

    // Telemetry Queries
    /// @brief Send query for current absolute focus position.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryFujinonFocus();

    /// @brief Send query for current optical zoom magnification.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryFujinonZoom();

    /// @brief Send query for camera hardware serial number.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult querySerialNumber();

    /// @brief Send query for camera firmware version string.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryFirmwareVersion();

    /// @brief Send query for lens diagnostic status flags.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryLensStatus();

    /// @brief Send query for current photo / exposure mode settings.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] target Specific setting opcode or 0x00 for general query.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryPhotoSettings(std::uint8_t target = 0x00U);

    /// @brief Send query for current image quality and filter settings.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] target Specific setting opcode or 0x00 for general query.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryImageQuality(std::uint8_t target = 0x00U);

    /// @brief Send query for manual exposure override parameters.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryManualSettings();

    // -------------------------------------------------------------------------
    // Fujinon SX800 v2.12.0 Extended Controls
    // -------------------------------------------------------------------------

    /// @brief Set fine brightness level (1..100, default 50).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] level Brightness level (1 to 100).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setBrightnessFine(std::uint8_t level);

    /// @brief Set fine contrast level (1..100, default 50).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] level Contrast level (1 to 100).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setContrastFine(std::uint8_t level);

    /// @brief Set fine color saturation level (1..100, default 50).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] level Saturation level (1 to 100).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setSaturationFine(std::uint8_t level);

    /// @brief Set fine sharpness level (1..100, default 50).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] level Sharpness level (1 to 100).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setSharpnessFine(std::uint8_t level);

    /// @brief Set fine white balance red shift (1..100, default 50).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] level WB shift red level (1 to 100).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setWBShiftRedFine(std::uint8_t level);

    /// @brief Set fine white balance blue shift (1..100, default 50).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] level WB shift blue level (1 to 100).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setWBShiftBlueFine(std::uint8_t level);

    /// @brief Send query for fine image quality setting.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] target Target opcode (e.g. 0xEB, 0xED, etc.).
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryImageQualityFine(std::uint8_t target);

    /// @brief Set extended Day/Night mode.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode Extended day/night mode preset.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setDayNightModeEx(FujinonDayNightModeEx mode);

    /// @brief Set Day to Night switching threshold (0..255).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] threshold Luminance threshold.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setDayToNightThreshold(std::uint8_t threshold);

    /// @brief Set Night to Day switching threshold (0..255).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] threshold Luminance threshold.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setNightToDayThreshold(std::uint8_t threshold);

    /// @brief Set Day/Night auto-switching delay seconds (0..60s).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] delaySeconds Delay time in seconds.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setDayNightAutoDelay(std::uint8_t delaySeconds);

    /// @brief Set scheduled Day start time.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] hour Hour (0..23).
    /// @param[in] minute Minute (0..59).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setDayStartTime(std::uint8_t hour, std::uint8_t minute);

    /// @brief Set scheduled Night start time.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] hour Hour (0..23).
    /// @param[in] minute Minute (0..59).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setNightStartTime(std::uint8_t hour, std::uint8_t minute);

    /// @brief Set Day optical filter state.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] irPass False for IR cut (Day), true for IR pass.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setOpticalFilterDay(bool irPass);

    /// @brief Set Night optical filter state.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] irPass False for IR cut, true for IR pass (Night).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setOpticalFilterNight(bool irPass);

    /// @brief Send query for extended Day/Night settings.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] target Target opcode or 0x00 for general query.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryDayNightEx(std::uint8_t target = 0x00U);

    /// @brief Set extended zoom speed (1..8).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] speed Speed level (1: 4s to 8: 60s).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setZoomSpeedEx(std::uint8_t speed);

    /// @brief Set extended focus speed (1..5).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] speed Focus speed level (1 to 5).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setFocusSpeedEx(std::uint8_t speed);

    /// @brief Set digital zoom operation mode.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode Digital zoom mode (Off, Digital Zoom, Crop Mode).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setDigitalZoomMode(FujinonDigitalZoomMode mode);

    /// @brief Send query for extended zoom/focus settings.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] target Target opcode or 0x00 for general query.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryZoomFocusEx(std::uint8_t target = 0x00U);

    /// @brief Toggle anti-aliasing filter.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] enable True to enable antialiasing, false to disable.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setAntialiasing(bool enable);

    /// @brief Send OSD menu Back keypress.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult menuBack();

    /// @brief Format internal SD card storage.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult formatSDCard();

    /// @brief Execute factory reset to restore default parameters.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult factoryReset();

    /// @brief Set OSD language.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] lang Target language.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setLanguage(FujinonLanguage lang);

    /// @brief Set real-time clock second and synchronize.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] second Second (0..59).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setRTCSecond(std::uint8_t second);

    /// @brief Set real-time clock hour and minute.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] hour Hour (0..23).
    /// @param[in] minute Minute (0..59).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setRTCHourMinute(std::uint8_t hour, std::uint8_t minute);

    /// @brief Set real-time clock month and day.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] month Month (1..12).
    /// @param[in] day Day (1..31).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setRTCMonthDay(std::uint8_t month, std::uint8_t day);

    /// @brief Set real-time clock year.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] year 4-digit Gregorian year (e.g. 2026).
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setRTCYear(std::uint16_t year);

    /// @brief Send query for real-time clock field.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] subOpCode RTC query subcommand (0x01: second, 0x03: hour/minute, 0x05: month/day, 0x07: year).
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryRTC(std::uint8_t subOpCode);

    /// @brief Send standard Pelco-D query for optical zoom position (0x00 0x55).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryZoomStandard();

    /// @brief Set absolute optical zoom coordinate (0..65535).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] zoomPos Absolute 16-bit zoom coordinate.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setZoomPosition(std::uint16_t zoomPos);

    /// @brief Configure auto focus mode using standard Pelco-D AutoMode with Fujinon polarity.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode Off (manual), On, or Auto.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setAutoFocus(AutoMode mode);
    using PelcoDDevice::setAutoIris;
    using PelcoDDevice::setBaudRate;
    using PelcoDDevice::setFocusSpeed;
    using PelcoDDevice::setZoomPosition;
    using PelcoDDevice::setZoomSpeed;

    /// @brief Set standard zoom speed (0..3).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] speed Zoom speed.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setZoomSpeed(std::uint8_t speed);

    /// @brief Set standard focus speed (0..3).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] speed Focus speed.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setFocusSpeed(std::uint8_t speed);

    /// @brief Configure auto focus mode (AF on, AF off / manual, Quick AF).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode Auto focus mode.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setAutoFocus(FujinonAutoFocusMode mode);

    /// @brief Configure auto iris mode (manual or auto).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode Auto iris mode.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setAutoIris(FujinonAutoIrisMode mode);

    /// @brief Configure AGC mode (standard, off, hyper).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode AGC mode.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setAGC(FujinonAGCMode mode);

    /// @brief Configure backlight compensation mode (off, soft, hard).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode BLC mode.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setBLC(FujinonBLCMode mode);

    /// @brief Set remote baud rate index (0:2400 to 5:115200).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] baudRateIndex Baud rate index.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setBaudRate(std::uint8_t baudRateIndex);

    /// @brief Set auto shutter lowest limit.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] limit Lower limit.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setShutterLimitAuto(FujinonShutterLimitAuto limit);

    /// @brief Query manual electronic shutter speed setting.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryManualShutter();

    /// @brief Query manual sensor gain / ISO sensitivity setting.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryManualISO();

    /// @brief Query manual aperture iris setting.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryManualIris();

    /// @brief Select photo scene mode preset (Surveillance, Movie).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] preset Scene preset.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setPhotoModePreset(FujinonPhotoModePreset preset);

    /// @brief Configure external contact trigger Day/Night mode.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode External trigger mode.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setExternalDayNight(FujinonExternalDayNight mode);

    /// @brief Query OSD display setting.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] target Target opcode.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryDisplay(std::uint8_t target = 0x00U);

    /// @brief Set OSD time display format (12h or 24h).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] fmt Time format.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setTimeFormat(FujinonTimeFormat fmt);

    /// @brief Set OSD calendar date display format.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] fmt Date format.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setDateFormat(FujinonDateFormat fmt);

    /// @brief Set video display framing aspect mode.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode Aspect display mode.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setVideoDisplay(FujinonVideoDisplayMode mode);

    /// @brief Reprogram device RS-485 bus address (1..31).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] newId New bus address.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setRS485Address(std::uint8_t newId);

    /// @brief Set SD card recording mode on full card (overwrite or stop).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode SD recording mode.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setSDRecordingMode(FujinonSDRecordMode mode);

    /// @brief Toggle OSD display of remaining SD card capacity.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] enable True to display capacity on OSD.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setSDCapacityDisp(bool enable);

    /// @brief Copy memory diagnostic logs to SD card.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult recordLogData();

    /// @brief Set SD recording video codec format.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] enc Video encoding format.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setEncodeMode(FujinonVideoEncoding enc);

    /// @brief Set initial movie file index for playback.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] fileIndex 1-based movie index.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setFirstMovieFile(std::uint16_t fileIndex);

    /// @brief Set movie playback operating state (Stop, Step, Continuous).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] mode Movie play mode.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult playMovieMode(FujinonMoviePlayMode mode);

    /// @brief Delete movie from SD card by file index (or 0x0000 for all).
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] fileIndex 1-based index or 0 for all.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult deleteMovie(std::uint16_t fileIndex);

    /// @brief Query OSD menu key setting.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] target Target opcode.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryKeySetting(std::uint8_t target = 0x00U);

    /// @brief Query total count of recorded movies on SD card.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryMovieCount();

    /// @brief Query movie recording year.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] fileIndex 1-based movie index.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryMovieYear(std::uint16_t fileIndex);

    /// @brief Query movie recording month and day.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] fileIndex 1-based movie index.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryMovieDate(std::uint16_t fileIndex);

    /// @brief Query movie recording hour and minute.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] fileIndex 1-based movie index.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryMovieTime(std::uint16_t fileIndex);

    /// @brief Query internal diagnostic log entry.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] logType Category of log data.
    /// @return SendResult::Success if query was successfully transmitted; error code otherwise.
    [[nodiscard]] SendResult queryLogData(FujinonLogType logType);

    /// @brief Set digital zoom magnification ratio.
    /// @details Encodes and dispatches command via the paced command queue.
    /// @param[in] magIndex 0:1.25x, 1:1.50x, 2:1.75x, 3:2.00x.
    /// @return SendResult::Success if command was successfully enqueued; error code otherwise.
    [[nodiscard]] SendResult setDigitalZoomEx(std::uint8_t magIndex);

private:
    class FujinonFrameExt;

    /// Fujinon RX state and subscribers; co-owned by the base device (see class details).
    std::shared_ptr<FujinonFrameExt> m_ext;
};

} // namespace PelcoD
