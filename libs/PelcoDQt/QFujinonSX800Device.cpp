/// @file QFujinonSX800Device.cpp
/// @brief Implementation of QFujinonSX800Device Qt adapter.

#include "QFujinonSX800Device.h"

#include <QMetaObject>
#include <utility>

namespace PelcoDQt {

QFujinonSX800Device::QFujinonSX800Device(std::shared_ptr<PelcoD::FujinonSX800Device> device, QObject* parent)
    : QObject(parent)
    , m_device(std::move(device))
{
    if (m_device) {
        m_device->addFujinonStatusCallback([this](const PelcoD::FujinonStatus& status) {
            const QString serial = QString::fromStdString(status.serialNumber);
            const QString fw = QString::fromStdString(status.firmwareVersion);
            const int lensSt = static_cast<int>(status.lensStatus);
            QMetaObject::invokeMethod(
                this,
                [this, status, serial, fw, lensSt] {
                    emit fujinonStatusChanged(status);
                    emit oisModeChanged(status.oisMode);
                    emit defogChanged(status.defogLevel);
                    emit dayNightChanged(status.dayNightMode);
                    emit dayNightExChanged(status.dayNightExSettings.mode);
                    emit antialiasingChanged(status.antialiasing);
                    emit focalLengthChanged(status.focalLengthMm);
                    if (!serial.isEmpty()) {
                        emit serialNumberReceived(serial);
                    }
                    if (!fw.isEmpty()) {
                        emit firmwareVersionReceived(fw);
                    }
                    emit lensStatusReceived(lensSt);
                },
                Qt::QueuedConnection);
        });
    }
}

PelcoD::FujinonStatus QFujinonSX800Device::currentStatus() const
{
    return m_device ? m_device->getFujinonStatus() : PelcoD::FujinonStatus {};
}

std::shared_ptr<PelcoD::FujinonSX800Device> QFujinonSX800Device::coreDevice() const noexcept
{
    return m_device;
}

void QFujinonSX800Device::setOISMode(PelcoD::FujinonOISMode mode)
{
    if (m_device) {
        static_cast<void>(m_device->setOISMode(mode));
    }
}

void QFujinonSX800Device::setDefog(PelcoD::FujinonDefogLevel level)
{
    if (m_device) {
        static_cast<void>(m_device->setDefog(level));
    }
}

void QFujinonSX800Device::setHeatHaze(PelcoD::FujinonHeatHazeLevel level)
{
    if (m_device) {
        static_cast<void>(m_device->setHeatHaze(level));
    }
}

void QFujinonSX800Device::setWDR(PelcoD::FujinonWDRLevel level)
{
    if (m_device) {
        static_cast<void>(m_device->setWDR(level));
    }
}

void QFujinonSX800Device::setVLCFilter(bool enable)
{
    if (m_device) {
        static_cast<void>(m_device->setVLCFilter(enable));
    }
}

void QFujinonSX800Device::setBrightness(int level)
{
    if (m_device && level >= 1 && level <= 21) {
        static_cast<void>(m_device->setBrightness(static_cast<std::uint8_t>(level)));
    }
}

void QFujinonSX800Device::setContrast(int level)
{
    if (m_device && level >= 1 && level <= 5) {
        static_cast<void>(m_device->setContrast(static_cast<std::uint8_t>(level)));
    }
}

void QFujinonSX800Device::setSaturation(int level)
{
    if (m_device && level >= 1 && level <= 5) {
        static_cast<void>(m_device->setSaturation(static_cast<std::uint8_t>(level)));
    }
}

void QFujinonSX800Device::setSharpness(int level)
{
    if (m_device && level >= 1 && level <= 5) {
        static_cast<void>(m_device->setSharpness(static_cast<std::uint8_t>(level)));
    }
}

void QFujinonSX800Device::setColorTemperature(PelcoD::FujinonColorTemp temp)
{
    if (m_device) {
        static_cast<void>(m_device->setColorTemperature(temp));
    }
}

void QFujinonSX800Device::setWhiteBalance(PelcoD::FujinonWBMode mode)
{
    if (m_device) {
        static_cast<void>(m_device->setWhiteBalance(mode));
    }
}

void QFujinonSX800Device::setDigitalZoom(PelcoD::FujinonDigitalZoom zoom)
{
    if (m_device) {
        static_cast<void>(m_device->setDigitalZoom(zoom));
    }
}

void QFujinonSX800Device::setNoiseReduction(int level)
{
    if (m_device && level >= 1 && level <= 5) {
        static_cast<void>(m_device->setNoiseReduction(static_cast<std::uint8_t>(level)));
    }
}

void QFujinonSX800Device::setDayNightMode(PelcoD::FujinonDayNightMode mode)
{
    if (m_device) {
        static_cast<void>(m_device->setDayNightMode(mode));
    }
}

void QFujinonSX800Device::setIRWavelength(PelcoD::FujinonIRWavelength wavelength)
{
    if (m_device) {
        static_cast<void>(m_device->setIRWavelength(wavelength));
    }
}

void QFujinonSX800Device::setFocusPosition(int focusPos)
{
    if (m_device && focusPos >= 0) {
        static_cast<void>(m_device->setFocusPosition(static_cast<std::uint16_t>(focusPos)));
    }
}

void QFujinonSX800Device::onePushAF()
{
    if (m_device) {
        static_cast<void>(m_device->onePushAF());
    }
}

void QFujinonSX800Device::setAFSensitivity(int sensitivity)
{
    if (m_device && sensitivity >= 0) {
        static_cast<void>(m_device->setAFSensitivity(static_cast<std::uint8_t>(sensitivity)));
    }
}

void QFujinonSX800Device::setAFArea(int area)
{
    if (m_device && area >= 0) {
        static_cast<void>(m_device->setAFArea(static_cast<std::uint8_t>(area)));
    }
}

void QFujinonSX800Device::setManualIris(int irisVal)
{
    if (m_device && irisVal >= 0) {
        static_cast<void>(m_device->setManualIris(static_cast<std::uint8_t>(irisVal)));
    }
}

void QFujinonSX800Device::setManualShutter(int shutterVal)
{
    if (m_device && shutterVal >= 0) {
        static_cast<void>(m_device->setManualShutter(static_cast<std::uint8_t>(shutterVal)));
    }
}

void QFujinonSX800Device::setManualISO(int isoVal)
{
    if (m_device && isoVal >= 0) {
        static_cast<void>(m_device->setManualISO(static_cast<std::uint8_t>(isoVal)));
    }
}

void QFujinonSX800Device::setTimeDisplay(bool enable)
{
    if (m_device) {
        static_cast<void>(m_device->setTimeDisplay(enable));
    }
}

void QFujinonSX800Device::setTimePosition(PelcoD::FujinonOSDPosition pos)
{
    if (m_device) {
        static_cast<void>(m_device->setTimePosition(pos));
    }
}

void QFujinonSX800Device::setTitleDisplay(bool enable)
{
    if (m_device) {
        static_cast<void>(m_device->setTitleDisplay(enable));
    }
}

void QFujinonSX800Device::setTitlePosition(PelcoD::FujinonOSDPosition pos)
{
    if (m_device) {
        static_cast<void>(m_device->setTitlePosition(pos));
    }
}

void QFujinonSX800Device::setIdDisplay(bool enable)
{
    if (m_device) {
        static_cast<void>(m_device->setIdDisplay(enable));
    }
}

void QFujinonSX800Device::setIdPosition(PelcoD::FujinonOSDPosition pos)
{
    if (m_device) {
        static_cast<void>(m_device->setIdPosition(pos));
    }
}

void QFujinonSX800Device::setReticleDisplay(bool enable)
{
    if (m_device) {
        static_cast<void>(m_device->setReticleDisplay(enable));
    }
}

void QFujinonSX800Device::setVideoMode(PelcoD::FujinonVideoMode mode)
{
    if (m_device) {
        static_cast<void>(m_device->setVideoMode(mode));
    }
}

void QFujinonSX800Device::setHDFormat(PelcoD::FujinonHDFormat format)
{
    if (m_device) {
        static_cast<void>(m_device->setHDFormat(format));
    }
}

void QFujinonSX800Device::setRS485Termination(bool enable)
{
    if (m_device) {
        static_cast<void>(m_device->setRS485Termination(enable));
    }
}

void QFujinonSX800Device::reboot()
{
    if (m_device) {
        static_cast<void>(m_device->reboot());
    }
}

void QFujinonSX800Device::recordLiveView(bool start)
{
    if (m_device) {
        static_cast<void>(m_device->recordLiveView(start));
    }
}

void QFujinonSX800Device::playMovie(bool start)
{
    if (m_device) {
        static_cast<void>(m_device->playMovie(start));
    }
}

void QFujinonSX800Device::setMovieMode(bool isPlayMode)
{
    if (m_device) {
        static_cast<void>(m_device->setMovieMode(isPlayMode));
    }
}

void QFujinonSX800Device::menuOk()
{
    if (m_device) {
        static_cast<void>(m_device->menuOk());
    }
}

void QFujinonSX800Device::menuDirection(PelcoD::FujinonMenuDirection dir)
{
    if (m_device) {
        static_cast<void>(m_device->menuDirection(dir));
    }
}

void QFujinonSX800Device::queryPhotoSettings()
{
    if (m_device) {
        static_cast<void>(m_device->queryPhotoSettings());
    }
}

void QFujinonSX800Device::queryImageQuality()
{
    if (m_device) {
        static_cast<void>(m_device->queryImageQuality());
    }
}

void QFujinonSX800Device::queryManualSettings()
{
    if (m_device) {
        static_cast<void>(m_device->queryManualSettings());
    }
}

void QFujinonSX800Device::querySerialNumber()
{
    if (m_device) {
        static_cast<void>(m_device->querySerialNumber());
    }
}

void QFujinonSX800Device::queryFirmwareVersion()
{
    if (m_device) {
        static_cast<void>(m_device->queryFirmwareVersion());
    }
}

void QFujinonSX800Device::queryLensStatus()
{
    if (m_device) {
        static_cast<void>(m_device->queryLensStatus());
    }
}

void QFujinonSX800Device::setBrightnessFine(int level)
{
    if (m_device && level >= 1 && level <= 100) {
        static_cast<void>(m_device->setBrightnessFine(static_cast<std::uint8_t>(level)));
    }
}

void QFujinonSX800Device::setContrastFine(int level)
{
    if (m_device && level >= 1 && level <= 100) {
        static_cast<void>(m_device->setContrastFine(static_cast<std::uint8_t>(level)));
    }
}

void QFujinonSX800Device::setSaturationFine(int level)
{
    if (m_device && level >= 1 && level <= 100) {
        static_cast<void>(m_device->setSaturationFine(static_cast<std::uint8_t>(level)));
    }
}

void QFujinonSX800Device::setSharpnessFine(int level)
{
    if (m_device && level >= 1 && level <= 100) {
        static_cast<void>(m_device->setSharpnessFine(static_cast<std::uint8_t>(level)));
    }
}

void QFujinonSX800Device::setWBShiftRedFine(int level)
{
    if (m_device && level >= 1 && level <= 100) {
        static_cast<void>(m_device->setWBShiftRedFine(static_cast<std::uint8_t>(level)));
    }
}

void QFujinonSX800Device::setWBShiftBlueFine(int level)
{
    if (m_device && level >= 1 && level <= 100) {
        static_cast<void>(m_device->setWBShiftBlueFine(static_cast<std::uint8_t>(level)));
    }
}

void QFujinonSX800Device::queryImageQualityFine(int target)
{
    if (m_device) {
        static_cast<void>(m_device->queryImageQualityFine(static_cast<std::uint8_t>(target)));
    }
}

void QFujinonSX800Device::setDayNightModeEx(PelcoD::FujinonDayNightModeEx mode)
{
    if (m_device) {
        static_cast<void>(m_device->setDayNightModeEx(mode));
    }
}

void QFujinonSX800Device::setDayToNightThreshold(int threshold)
{
    if (m_device && threshold >= 0 && threshold <= 255) {
        static_cast<void>(m_device->setDayToNightThreshold(static_cast<std::uint8_t>(threshold)));
    }
}

void QFujinonSX800Device::setNightToDayThreshold(int threshold)
{
    if (m_device && threshold >= 0 && threshold <= 255) {
        static_cast<void>(m_device->setNightToDayThreshold(static_cast<std::uint8_t>(threshold)));
    }
}

void QFujinonSX800Device::setDayNightAutoDelay(int delaySeconds)
{
    if (m_device && delaySeconds >= 0 && delaySeconds <= 60) {
        static_cast<void>(m_device->setDayNightAutoDelay(static_cast<std::uint8_t>(delaySeconds)));
    }
}

void QFujinonSX800Device::setDayStartTime(int hour, int minute)
{
    if (m_device && hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
        static_cast<void>(
            m_device->setDayStartTime(static_cast<std::uint8_t>(hour), static_cast<std::uint8_t>(minute)));
    }
}

void QFujinonSX800Device::setNightStartTime(int hour, int minute)
{
    if (m_device && hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
        static_cast<void>(
            m_device->setNightStartTime(static_cast<std::uint8_t>(hour), static_cast<std::uint8_t>(minute)));
    }
}

void QFujinonSX800Device::setOpticalFilterDay(bool irPass)
{
    if (m_device) {
        static_cast<void>(m_device->setOpticalFilterDay(irPass));
    }
}

void QFujinonSX800Device::setOpticalFilterNight(bool irPass)
{
    if (m_device) {
        static_cast<void>(m_device->setOpticalFilterNight(irPass));
    }
}

void QFujinonSX800Device::queryDayNightEx(int target)
{
    if (m_device) {
        static_cast<void>(m_device->queryDayNightEx(static_cast<std::uint8_t>(target)));
    }
}

void QFujinonSX800Device::setZoomSpeedEx(int speed)
{
    if (m_device && speed >= 1 && speed <= 8) {
        static_cast<void>(m_device->setZoomSpeedEx(static_cast<std::uint8_t>(speed)));
    }
}

void QFujinonSX800Device::setFocusSpeedEx(int speed)
{
    if (m_device && speed >= 1 && speed <= 5) {
        static_cast<void>(m_device->setFocusSpeedEx(static_cast<std::uint8_t>(speed)));
    }
}

void QFujinonSX800Device::setDigitalZoomMode(PelcoD::FujinonDigitalZoomMode mode)
{
    if (m_device) {
        static_cast<void>(m_device->setDigitalZoomMode(mode));
    }
}

void QFujinonSX800Device::queryZoomFocusEx(int target)
{
    if (m_device) {
        static_cast<void>(m_device->queryZoomFocusEx(static_cast<std::uint8_t>(target)));
    }
}

void QFujinonSX800Device::setAntialiasing(bool enable)
{
    if (m_device) {
        static_cast<void>(m_device->setAntialiasing(enable));
    }
}

void QFujinonSX800Device::menuBack()
{
    if (m_device) {
        static_cast<void>(m_device->menuBack());
    }
}

void QFujinonSX800Device::formatSDCard()
{
    if (m_device) {
        static_cast<void>(m_device->formatSDCard());
    }
}

void QFujinonSX800Device::factoryReset()
{
    if (m_device) {
        static_cast<void>(m_device->factoryReset());
    }
}

void QFujinonSX800Device::setLanguage(PelcoD::FujinonLanguage lang)
{
    if (m_device) {
        static_cast<void>(m_device->setLanguage(lang));
    }
}

void QFujinonSX800Device::setRTCSecond(int second)
{
    if (m_device && second >= 0 && second <= 59) {
        static_cast<void>(m_device->setRTCSecond(static_cast<std::uint8_t>(second)));
    }
}

void QFujinonSX800Device::setRTCHourMinute(int hour, int minute)
{
    if (m_device && hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
        static_cast<void>(
            m_device->setRTCHourMinute(static_cast<std::uint8_t>(hour), static_cast<std::uint8_t>(minute)));
    }
}

void QFujinonSX800Device::setRTCMonthDay(int month, int day)
{
    if (m_device && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
        static_cast<void>(m_device->setRTCMonthDay(static_cast<std::uint8_t>(month), static_cast<std::uint8_t>(day)));
    }
}

void QFujinonSX800Device::setRTCYear(int year)
{
    if (m_device && year >= 2000 && year <= 2099) {
        static_cast<void>(m_device->setRTCYear(static_cast<std::uint16_t>(year)));
    }
}

void QFujinonSX800Device::queryRTC(int subOpCode)
{
    if (m_device) {
        static_cast<void>(m_device->queryRTC(static_cast<std::uint8_t>(subOpCode)));
    }
}

void QFujinonSX800Device::queryZoomStandard()
{
    if (m_device) {
        static_cast<void>(m_device->queryZoomStandard());
    }
}

void QFujinonSX800Device::queryZoom()
{
    if (m_device) {
        static_cast<void>(m_device->queryFujinonZoom());
        static_cast<void>(m_device->queryZoomStandard());
    }
}

void QFujinonSX800Device::queryFocus()
{
    if (m_device) {
        static_cast<void>(m_device->queryFujinonFocus());
    }
}

void QFujinonSX800Device::setZoomPosition(int zoomPos)
{
    if (m_device && zoomPos >= 0 && zoomPos <= 65535) {
        static_cast<void>(m_device->setZoomPosition(static_cast<std::uint16_t>(zoomPos)));
    }
}

void QFujinonSX800Device::setZoomSpeed(int speed)
{
    if (m_device && speed >= 0 && speed <= 255) {
        static_cast<void>(m_device->setZoomSpeed(static_cast<std::uint8_t>(speed)));
    }
}

void QFujinonSX800Device::setFocusSpeed(int speed)
{
    if (m_device && speed >= 0 && speed <= 255) {
        static_cast<void>(m_device->setFocusSpeed(static_cast<std::uint8_t>(speed)));
    }
}

void QFujinonSX800Device::setAutoFocus(PelcoD::FujinonAutoFocusMode mode)
{
    if (m_device) {
        static_cast<void>(m_device->setAutoFocus(mode));
    }
}

void QFujinonSX800Device::setAutoIris(PelcoD::FujinonAutoIrisMode mode)
{
    if (m_device) {
        static_cast<void>(m_device->setAutoIris(mode));
    }
}

void QFujinonSX800Device::setAGC(PelcoD::FujinonAGCMode mode)
{
    if (m_device) {
        static_cast<void>(m_device->setAGC(mode));
    }
}

void QFujinonSX800Device::setBLC(PelcoD::FujinonBLCMode mode)
{
    if (m_device) {
        static_cast<void>(m_device->setBLC(mode));
    }
}

void QFujinonSX800Device::setBaudRate(int baudRateIndex)
{
    if (m_device && baudRateIndex >= 0 && baudRateIndex <= 255) {
        static_cast<void>(m_device->setBaudRate(static_cast<std::uint8_t>(baudRateIndex)));
    }
}

void QFujinonSX800Device::setShutterLimitAuto(PelcoD::FujinonShutterLimitAuto limit)
{
    if (m_device) {
        static_cast<void>(m_device->setShutterLimitAuto(limit));
    }
}

void QFujinonSX800Device::queryManualShutter()
{
    if (m_device) {
        static_cast<void>(m_device->queryManualShutter());
    }
}

void QFujinonSX800Device::queryManualISO()
{
    if (m_device) {
        static_cast<void>(m_device->queryManualISO());
    }
}

void QFujinonSX800Device::queryManualIris()
{
    if (m_device) {
        static_cast<void>(m_device->queryManualIris());
    }
}

void QFujinonSX800Device::setPhotoModePreset(PelcoD::FujinonPhotoModePreset preset)
{
    if (m_device) {
        static_cast<void>(m_device->setPhotoModePreset(preset));
    }
}

void QFujinonSX800Device::setExternalDayNight(PelcoD::FujinonExternalDayNight mode)
{
    if (m_device) {
        static_cast<void>(m_device->setExternalDayNight(mode));
    }
}

void QFujinonSX800Device::queryDisplay(int target)
{
    if (m_device) {
        static_cast<void>(m_device->queryDisplay(static_cast<std::uint8_t>(target)));
    }
}

void QFujinonSX800Device::setTimeFormat(PelcoD::FujinonTimeFormat fmt)
{
    if (m_device) {
        static_cast<void>(m_device->setTimeFormat(fmt));
    }
}

void QFujinonSX800Device::setDateFormat(PelcoD::FujinonDateFormat fmt)
{
    if (m_device) {
        static_cast<void>(m_device->setDateFormat(fmt));
    }
}

void QFujinonSX800Device::setVideoDisplay(PelcoD::FujinonVideoDisplayMode mode)
{
    if (m_device) {
        static_cast<void>(m_device->setVideoDisplay(mode));
    }
}

void QFujinonSX800Device::setRS485Address(int newId)
{
    if (m_device && newId >= 1 && newId <= 31) {
        static_cast<void>(m_device->setRS485Address(static_cast<std::uint8_t>(newId)));
    }
}

void QFujinonSX800Device::setSDRecordingMode(PelcoD::FujinonSDRecordMode mode)
{
    if (m_device) {
        static_cast<void>(m_device->setSDRecordingMode(mode));
    }
}

void QFujinonSX800Device::setSDCapacityDisp(bool enable)
{
    if (m_device) {
        static_cast<void>(m_device->setSDCapacityDisp(enable));
    }
}

void QFujinonSX800Device::recordLogData()
{
    if (m_device) {
        static_cast<void>(m_device->recordLogData());
    }
}

void QFujinonSX800Device::setEncodeMode(PelcoD::FujinonVideoEncoding enc)
{
    if (m_device) {
        static_cast<void>(m_device->setEncodeMode(enc));
    }
}

void QFujinonSX800Device::setFirstMovieFile(int fileIndex)
{
    if (m_device && fileIndex >= 0 && fileIndex <= 65535) {
        static_cast<void>(m_device->setFirstMovieFile(static_cast<std::uint16_t>(fileIndex)));
    }
}

void QFujinonSX800Device::playMovieMode(PelcoD::FujinonMoviePlayMode mode)
{
    if (m_device) {
        static_cast<void>(m_device->playMovieMode(mode));
    }
}

void QFujinonSX800Device::deleteMovie(int fileIndex)
{
    if (m_device && fileIndex >= 0 && fileIndex <= 65535) {
        static_cast<void>(m_device->deleteMovie(static_cast<std::uint16_t>(fileIndex)));
    }
}

void QFujinonSX800Device::queryKeySetting(int target)
{
    if (m_device) {
        static_cast<void>(m_device->queryKeySetting(static_cast<std::uint8_t>(target)));
    }
}

void QFujinonSX800Device::queryMovieCount()
{
    if (m_device) {
        static_cast<void>(m_device->queryMovieCount());
    }
}

void QFujinonSX800Device::queryMovieYear(int fileIndex)
{
    if (m_device && fileIndex >= 0 && fileIndex <= 65535) {
        static_cast<void>(m_device->queryMovieYear(static_cast<std::uint16_t>(fileIndex)));
    }
}

void QFujinonSX800Device::queryMovieDate(int fileIndex)
{
    if (m_device && fileIndex >= 0 && fileIndex <= 65535) {
        static_cast<void>(m_device->queryMovieDate(static_cast<std::uint16_t>(fileIndex)));
    }
}

void QFujinonSX800Device::queryMovieTime(int fileIndex)
{
    if (m_device && fileIndex >= 0 && fileIndex <= 65535) {
        static_cast<void>(m_device->queryMovieTime(static_cast<std::uint16_t>(fileIndex)));
    }
}

void QFujinonSX800Device::queryLogData(PelcoD::FujinonLogType logType)
{
    if (m_device) {
        static_cast<void>(m_device->queryLogData(logType));
    }
}

void QFujinonSX800Device::setDigitalZoomEx(int magIndex)
{
    if (m_device && magIndex >= 0 && magIndex <= 3) {
        static_cast<void>(m_device->setDigitalZoomEx(static_cast<std::uint8_t>(magIndex)));
    }
}

} // namespace PelcoDQt
