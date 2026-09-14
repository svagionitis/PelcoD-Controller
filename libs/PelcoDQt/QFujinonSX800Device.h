#pragma once

/// @file QFujinonSX800Device.h
/// @brief Qt adapter providing signals and slots for PelcoD::FujinonSX800Device.

#include "FujinonSX800Device.h"
#include "FujinonTypes.h"

#include <QObject>
#include <QString>
#include <memory>

namespace PelcoDQt {

/// @class QFujinonSX800Device
/// @brief QObject wrapper for FujinonSX800Device providing thread-safe Qt signals and slots.
class QFujinonSX800Device : public QObject {
    Q_OBJECT

public:
    /// @brief Construct a Qt adapter for FujinonSX800Device.
    /// @param[in] device Shared pointer to underlying core FujinonSX800Device.
    /// @param[in] parent Optional QObject parent for ownership tree.
    explicit QFujinonSX800Device(std::shared_ptr<PelcoD::FujinonSX800Device> device, QObject* parent = nullptr);
    ~QFujinonSX800Device() override = default;

    /// @brief Retrieve latest snapshot of Fujinon telemetry.
    /// @return Current FujinonStatus struct.
    [[nodiscard]] PelcoD::FujinonStatus currentStatus() const;

    /// @brief Get access to the underlying core device.
    /// @return Shared pointer to PelcoD::FujinonSX800Device.
    [[nodiscard]] std::shared_ptr<PelcoD::FujinonSX800Device> coreDevice() const noexcept;

public slots:
    // Stabilization & Image Enhancement
    /// @brief Set optical/electronic image stabilization mode.
    void setOISMode(PelcoD::FujinonOISMode mode);
    /// @brief Set optical/digital defog level.
    void setDefog(PelcoD::FujinonDefogLevel level);
    /// @brief Set de-heat haze level.
    void setHeatHaze(PelcoD::FujinonHeatHazeLevel level);
    /// @brief Set Wide Dynamic Range level.
    void setWDR(PelcoD::FujinonWDRLevel level);
    /// @brief Toggle visible-light-cut filter.
    void setVLCFilter(bool enable);

    // Fine Image Adjustments
    /// @brief Set image brightness level (1 to 21).
    void setBrightness(int level);
    /// @brief Set image contrast level (1 to 5).
    void setContrast(int level);
    /// @brief Set image color saturation level (1 to 5).
    void setSaturation(int level);
    /// @brief Set image sharpness level (1 to 5).
    void setSharpness(int level);
    /// @brief Set manual color temperature preset.
    void setColorTemperature(PelcoD::FujinonColorTemp temp);
    /// @brief Set white balance mode.
    void setWhiteBalance(PelcoD::FujinonWBMode mode);
    /// @brief Set digital zoom magnification factor.
    void setDigitalZoom(PelcoD::FujinonDigitalZoom zoom);
    /// @brief Set 2D/3D noise reduction intensity.
    void setNoiseReduction(int level);

    // Day / Night & Infrared Filters
    /// @brief Set Day/Night IR cut filter mode.
    void setDayNightMode(PelcoD::FujinonDayNightMode mode);
    /// @brief Set infrared bandpass wavelength filter.
    void setIRWavelength(PelcoD::FujinonIRWavelength wavelength);

    // Optics & Focus Control
    /// @brief Drive focus to absolute 16-bit position.
    void setFocusPosition(int focusPos);
    /// @brief Trigger One-Push auto-focus execution.
    void onePushAF();
    /// @brief Set auto-focus tracking sensitivity.
    void setAFSensitivity(int sensitivity);
    /// @brief Select auto-focus measurement area.
    void setAFArea(int area);

    // Exposure & Sensor Controls
    /// @brief Set manual aperture iris aperture level.
    void setManualIris(int irisVal);
    /// @brief Set manual electronic shutter speed index.
    void setManualShutter(int shutterVal);
    /// @brief Set manual sensor gain / ISO sensitivity index.
    void setManualISO(int isoVal);

    // OSD Screen Overlays
    /// @brief Toggle on-screen time display overlay.
    void setTimeDisplay(bool enable);
    /// @brief Set on-screen time display position.
    void setTimePosition(PelcoD::FujinonOSDPosition pos);
    /// @brief Toggle on-screen title text overlay.
    void setTitleDisplay(bool enable);
    /// @brief Set on-screen title text position.
    void setTitlePosition(PelcoD::FujinonOSDPosition pos);
    /// @brief Toggle on-screen camera ID overlay.
    void setIdDisplay(bool enable);
    /// @brief Set on-screen camera ID position.
    void setIdPosition(PelcoD::FujinonOSDPosition pos);
    /// @brief Toggle on-screen crosshair reticle overlay.
    void setReticleDisplay(bool enable);

    // Operation & System
    /// @brief Set base video standard (NTSC/PAL).
    void setVideoMode(PelcoD::FujinonVideoMode mode);
    /// @brief Set Full HD SDI / HDMI output format.
    void setHDFormat(PelcoD::FujinonHDFormat format);
    /// @brief Engage or release internal 120-ohm RS-485 termination.
    void setRS485Termination(bool enable);
    /// @brief Trigger software reboot.
    void reboot();

    // Media & Menu
    /// @brief Start or stop live video recording to SD card.
    void recordLiveView(bool start);
    /// @brief Start or pause recorded movie playback.
    void playMovie(bool start);
    /// @brief Switch between live monitoring and movie playback mode.
    void setMovieMode(bool isPlayMode);
    /// @brief Send OSD menu Enter / Confirm keypress.
    void menuOk();
    /// @brief Send OSD menu directional navigation keypress.
    void menuDirection(PelcoD::FujinonMenuDirection dir);

    // Telemetry Queries
    /// @brief Send query for photo / exposure settings.
    void queryPhotoSettings();
    /// @brief Send query for image quality and enhancement settings.
    void queryImageQuality();
    /// @brief Send query for manual exposure overrides.
    void queryManualSettings();
    /// @brief Send query for hardware serial number.
    void querySerialNumber();
    /// @brief Send query for firmware version string.
    void queryFirmwareVersion();
    /// @brief Send query for lens diagnostic status flags.
    void queryLensStatus();

    // -------------------------------------------------------------------------
    // Fujinon SX800 v2.12.0 Extended Slots
    // -------------------------------------------------------------------------

    /// @brief Set fine brightness level (1 to 100).
    void setBrightnessFine(int level);
    /// @brief Set fine contrast level (1 to 100).
    void setContrastFine(int level);
    /// @brief Set fine color saturation level (1 to 100).
    void setSaturationFine(int level);
    /// @brief Set fine sharpness level (1 to 100).
    void setSharpnessFine(int level);
    /// @brief Set fine white balance red shift (1 to 100).
    void setWBShiftRedFine(int level);
    /// @brief Set fine white balance blue shift (1 to 100).
    void setWBShiftBlueFine(int level);
    /// @brief Send query for fine image quality setting.
    void queryImageQualityFine(int target);

    /// @brief Set extended Day/Night mode.
    void setDayNightModeEx(PelcoD::FujinonDayNightModeEx mode);
    /// @brief Set Day to Night switching threshold (0..255).
    void setDayToNightThreshold(int threshold);
    /// @brief Set Night to Day switching threshold (0..255).
    void setNightToDayThreshold(int threshold);
    /// @brief Set Day/Night auto-switching delay seconds (0..60).
    void setDayNightAutoDelay(int delaySeconds);
    /// @brief Set scheduled Day start time.
    void setDayStartTime(int hour, int minute);
    /// @brief Set scheduled Night start time.
    void setNightStartTime(int hour, int minute);
    /// @brief Set Day optical filter state.
    void setOpticalFilterDay(bool irPass);
    /// @brief Set Night optical filter state.
    void setOpticalFilterNight(bool irPass);
    /// @brief Send query for extended Day/Night settings.
    void queryDayNightEx(int target = 0);

    /// @brief Set extended zoom speed (1..8).
    void setZoomSpeedEx(int speed);
    /// @brief Set extended focus speed (1..5).
    void setFocusSpeedEx(int speed);
    /// @brief Set digital zoom operation mode.
    void setDigitalZoomMode(PelcoD::FujinonDigitalZoomMode mode);
    /// @brief Send query for extended zoom/focus settings.
    void queryZoomFocusEx(int target = 0);

    /// @brief Toggle anti-aliasing filter.
    void setAntialiasing(bool enable);
    /// @brief Send OSD menu Back keypress.
    void menuBack();
    /// @brief Format internal SD card storage.
    void formatSDCard();
    /// @brief Execute factory reset to restore default parameters.
    void factoryReset();
    /// @brief Set OSD language.
    void setLanguage(PelcoD::FujinonLanguage lang);

    /// @brief Set real-time clock second and synchronize.
    void setRTCSecond(int second);
    /// @brief Set real-time clock hour and minute.
    void setRTCHourMinute(int hour, int minute);
    /// @brief Set real-time clock month and day.
    void setRTCMonthDay(int month, int day);
    /// @brief Set real-time clock year.
    void setRTCYear(int year);
    /// @brief Send query for real-time clock field.
    void queryRTC(int subOpCode);
    /// @brief Send standard Pelco-D query for zoom position.
    void queryZoomStandard();

signals:
    /// @brief Emitted when any Fujinon status parameter changes.
    void fujinonStatusChanged(const PelcoD::FujinonStatus& status);
    /// @brief Emitted when stabilization mode changes.
    void oisModeChanged(PelcoD::FujinonOISMode mode);
    /// @brief Emitted when defog level changes.
    void defogChanged(PelcoD::FujinonDefogLevel level);
    /// @brief Emitted when Day/Night mode changes.
    void dayNightChanged(PelcoD::FujinonDayNightMode mode);
    /// @brief Emitted when extended Day/Night mode changes.
    void dayNightExChanged(PelcoD::FujinonDayNightModeEx mode);
    /// @brief Emitted when anti-aliasing filter changes.
    void antialiasingChanged(bool enabled);
    /// @brief Emitted when optical focal length is recalculated.
    void focalLengthChanged(double focalLengthMm);
    /// @brief Emitted when camera serial number is received.
    void serialNumberReceived(const QString& serial);
    /// @brief Emitted when camera firmware version string is received.
    void firmwareVersionReceived(const QString& fw);
    /// @brief Emitted when lens diagnostic status is received.
    void lensStatusReceived(int status);

private:
    std::shared_ptr<PelcoD::FujinonSX800Device> m_device;
};

} // namespace PelcoDQt
