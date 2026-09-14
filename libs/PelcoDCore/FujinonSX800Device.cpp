/// @file FujinonSX800Device.cpp
/// @brief Implementation of Fujinon SX800 / SX801 specialized device profile.

#include "FujinonSX800Device.h"

#include <utility>

namespace PelcoD {

FujinonSX800Device::FujinonSX800Device(std::shared_ptr<ITransport> transport, std::uint8_t address)
    : PelcoDDevice(std::move(transport), address)
{
}

FujinonStatus FujinonSX800Device::getFujinonStatus() const
{
    std::lock_guard<std::mutex> lock(m_fujinonMutex);
    auto status = m_fujinonStatus;
    status.baseStatus = getStatus();
    return status;
}

void FujinonSX800Device::addFujinonStatusCallback(FujinonStatusCallback cb)
{
    if (!cb) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_fujinonMutex);
    auto nextList = std::make_shared<std::vector<FujinonStatusCallback>>(*m_fujinonCallbacks);
    nextList->push_back(std::move(cb));
    m_fujinonCallbacks = nextList;
}

void FujinonSX800Device::setOISMode(FujinonOISMode mode)
{
    enqueueCommand(FujinonBuilder::buildSetOIS(getAddress(), mode));
}

void FujinonSX800Device::setDefog(FujinonDefogLevel level)
{
    enqueueCommand(FujinonBuilder::buildSetDefog(getAddress(), level));
}

void FujinonSX800Device::setHeatHaze(FujinonHeatHazeLevel level)
{
    enqueueCommand(FujinonBuilder::buildSetHeatHaze(getAddress(), level));
}

void FujinonSX800Device::setWDR(FujinonWDRLevel level)
{
    enqueueCommand(FujinonBuilder::buildSetWDR(getAddress(), level));
}

void FujinonSX800Device::setVLCFilter(bool enable)
{
    enqueueCommand(FujinonBuilder::buildSetVLCFilter(getAddress(), enable));
}

void FujinonSX800Device::setBrightness(std::uint8_t level)
{
    enqueueCommand(FujinonBuilder::buildSetBrightness(getAddress(), level));
}

void FujinonSX800Device::setContrast(std::uint8_t level)
{
    enqueueCommand(FujinonBuilder::buildSetContrast(getAddress(), level));
}

void FujinonSX800Device::setSaturation(std::uint8_t level)
{
    enqueueCommand(FujinonBuilder::buildSetSaturation(getAddress(), level));
}

void FujinonSX800Device::setSharpness(std::uint8_t level)
{
    enqueueCommand(FujinonBuilder::buildSetSharpness(getAddress(), level));
}

void FujinonSX800Device::setColorTemperature(FujinonColorTemp temp)
{
    enqueueCommand(FujinonBuilder::buildSetColorTemperature(getAddress(), temp));
}

void FujinonSX800Device::setWhiteBalance(FujinonWBMode mode)
{
    enqueueCommand(FujinonBuilder::buildSetWhiteBalance(getAddress(), mode));
}

void FujinonSX800Device::setDigitalZoom(FujinonDigitalZoom zoom)
{
    enqueueCommand(FujinonBuilder::buildSetDigitalZoom(getAddress(), zoom));
}

void FujinonSX800Device::setNoiseReduction(std::uint8_t level)
{
    enqueueCommand(FujinonBuilder::buildSetNoiseReduction(getAddress(), level));
}

void FujinonSX800Device::setDayNightMode(FujinonDayNightMode mode)
{
    enqueueCommand(FujinonBuilder::buildSetDayNight(getAddress(), mode));
}

void FujinonSX800Device::setIRWavelength(FujinonIRWavelength wavelength)
{
    enqueueCommand(FujinonBuilder::buildSetIRWavelength(getAddress(), wavelength));
}

void FujinonSX800Device::setFocusPosition(std::uint16_t focusPos)
{
    enqueueCommand(FujinonBuilder::buildSetFocusPosition(getAddress(), focusPos));
}

void FujinonSX800Device::onePushAF()
{
    enqueueCommand(FujinonBuilder::buildOnePushAF(getAddress()));
}

void FujinonSX800Device::setAFSensitivity(std::uint8_t sensitivity)
{
    enqueueCommand(FujinonBuilder::buildSetAFSensitivity(getAddress(), sensitivity));
}

void FujinonSX800Device::setAFArea(std::uint8_t area)
{
    enqueueCommand(FujinonBuilder::buildSetAFArea(getAddress(), area));
}

void FujinonSX800Device::setManualIris(std::uint8_t irisVal)
{
    enqueueCommand(FujinonBuilder::buildSetManualIris(getAddress(), irisVal));
}

void FujinonSX800Device::setManualShutter(std::uint8_t shutterVal)
{
    enqueueCommand(FujinonBuilder::buildSetManualShutter(getAddress(), shutterVal));
}

void FujinonSX800Device::setManualISO(std::uint8_t isoVal)
{
    enqueueCommand(FujinonBuilder::buildSetManualISO(getAddress(), isoVal));
}

void FujinonSX800Device::setTimeDisplay(bool enable)
{
    enqueueCommand(FujinonBuilder::buildSetTimeDisplay(getAddress(), enable));
}

void FujinonSX800Device::setTimePosition(FujinonOSDPosition pos)
{
    enqueueCommand(FujinonBuilder::buildSetTimePosition(getAddress(), pos));
}

void FujinonSX800Device::setTitleDisplay(bool enable)
{
    enqueueCommand(FujinonBuilder::buildSetTitleDisplay(getAddress(), enable));
}

void FujinonSX800Device::setTitlePosition(FujinonOSDPosition pos)
{
    enqueueCommand(FujinonBuilder::buildSetTitlePosition(getAddress(), pos));
}

void FujinonSX800Device::setIdDisplay(bool enable)
{
    enqueueCommand(FujinonBuilder::buildSetIdDisplay(getAddress(), enable));
}

void FujinonSX800Device::setIdPosition(FujinonOSDPosition pos)
{
    enqueueCommand(FujinonBuilder::buildSetIdPosition(getAddress(), pos));
}

void FujinonSX800Device::setReticleDisplay(bool enable)
{
    enqueueCommand(FujinonBuilder::buildSetReticleDisplay(getAddress(), enable));
}

void FujinonSX800Device::setVideoMode(FujinonVideoMode mode)
{
    enqueueCommand(FujinonBuilder::buildSetVideoMode(getAddress(), mode));
}

void FujinonSX800Device::setHDFormat(FujinonHDFormat format)
{
    enqueueCommand(FujinonBuilder::buildSetHDFormat(getAddress(), format));
}

void FujinonSX800Device::setRS485Termination(bool enable)
{
    enqueueCommand(FujinonBuilder::buildSetRS485Termination(getAddress(), enable));
}

void FujinonSX800Device::reboot()
{
    enqueueCommand(FujinonBuilder::buildReboot(getAddress()));
}

void FujinonSX800Device::recordLiveView(bool start)
{
    enqueueCommand(FujinonBuilder::buildRecordLiveView(getAddress(), start));
}

void FujinonSX800Device::playMovie(bool start)
{
    enqueueCommand(FujinonBuilder::buildPlayMovie(getAddress(), start));
}

void FujinonSX800Device::setMovieMode(bool isPlayMode)
{
    enqueueCommand(FujinonBuilder::buildSetMovieMode(getAddress(), isPlayMode));
}

void FujinonSX800Device::menuOk()
{
    enqueueCommand(FujinonBuilder::buildMenuOk(getAddress()));
}

void FujinonSX800Device::menuDirection(FujinonMenuDirection dir)
{
    enqueueCommand(FujinonBuilder::buildMenuDirection(getAddress(), dir));
}

void FujinonSX800Device::queryFujinonFocus()
{
    sendQueryFrame(FujinonBuilder::buildQueryFocus(getAddress()), "FujinonQueryFocus");
}

void FujinonSX800Device::queryFujinonZoom()
{
    sendQueryFrame(FujinonBuilder::buildQueryZoom(getAddress()), "FujinonQueryZoom");
}

void FujinonSX800Device::querySerialNumber()
{
    sendQueryFrame(FujinonBuilder::buildQuerySerialNumber(getAddress()), "FujinonQuerySerial");
}

void FujinonSX800Device::queryFirmwareVersion()
{
    sendQueryFrame(FujinonBuilder::buildQueryFirmwareVersion(getAddress()), "FujinonQueryFirmware");
}

void FujinonSX800Device::queryLensStatus()
{
    sendQueryFrame(FujinonBuilder::buildQueryLensStatus(getAddress()), "FujinonQueryLens");
}

void FujinonSX800Device::queryPhotoSettings(std::uint8_t target)
{
    sendQueryFrame(FujinonBuilder::buildQueryPhotoSettings(getAddress(), target), "FujinonQueryPhoto");
}

void FujinonSX800Device::queryImageQuality(std::uint8_t target)
{
    sendQueryFrame(FujinonBuilder::buildQueryImageQuality(getAddress(), target), "FujinonQueryImageQuality");
}

void FujinonSX800Device::queryManualSettings()
{
    sendQueryFrame(FujinonBuilder::buildQueryManualSettings(getAddress()), "FujinonQueryManual");
}

void FujinonSX800Device::setBrightnessFine(std::uint8_t level)
{
    enqueueCommand(FujinonBuilder::buildSetBrightnessFine(getAddress(), level));
}

void FujinonSX800Device::setContrastFine(std::uint8_t level)
{
    enqueueCommand(FujinonBuilder::buildSetContrastFine(getAddress(), level));
}

void FujinonSX800Device::setSaturationFine(std::uint8_t level)
{
    enqueueCommand(FujinonBuilder::buildSetSaturationFine(getAddress(), level));
}

void FujinonSX800Device::setSharpnessFine(std::uint8_t level)
{
    enqueueCommand(FujinonBuilder::buildSetSharpnessFine(getAddress(), level));
}

void FujinonSX800Device::setWBShiftRedFine(std::uint8_t level)
{
    enqueueCommand(FujinonBuilder::buildSetWBShiftRedFine(getAddress(), level));
}

void FujinonSX800Device::setWBShiftBlueFine(std::uint8_t level)
{
    enqueueCommand(FujinonBuilder::buildSetWBShiftBlueFine(getAddress(), level));
}

void FujinonSX800Device::queryImageQualityFine(std::uint8_t target)
{
    sendQueryFrame(FujinonBuilder::buildQueryImageQualityFine(getAddress(), target), "FujinonQueryImageQualityFine");
}

void FujinonSX800Device::setDayNightModeEx(FujinonDayNightModeEx mode)
{
    enqueueCommand(FujinonBuilder::buildSetDayNightModeEx(getAddress(), mode));
}

void FujinonSX800Device::setDayToNightThreshold(std::uint8_t threshold)
{
    enqueueCommand(FujinonBuilder::buildSetDayToNightThreshold(getAddress(), threshold));
}

void FujinonSX800Device::setNightToDayThreshold(std::uint8_t threshold)
{
    enqueueCommand(FujinonBuilder::buildSetNightToDayThreshold(getAddress(), threshold));
}

void FujinonSX800Device::setDayNightAutoDelay(std::uint8_t delaySeconds)
{
    enqueueCommand(FujinonBuilder::buildSetDayNightAutoDelay(getAddress(), delaySeconds));
}

void FujinonSX800Device::setDayStartTime(std::uint8_t hour, std::uint8_t minute)
{
    enqueueCommand(FujinonBuilder::buildSetDayStartTime(getAddress(), hour, minute));
}

void FujinonSX800Device::setNightStartTime(std::uint8_t hour, std::uint8_t minute)
{
    enqueueCommand(FujinonBuilder::buildSetNightStartTime(getAddress(), hour, minute));
}

void FujinonSX800Device::setOpticalFilterDay(bool irPass)
{
    enqueueCommand(FujinonBuilder::buildSetOpticalFilterDay(getAddress(), irPass));
}

void FujinonSX800Device::setOpticalFilterNight(bool irPass)
{
    enqueueCommand(FujinonBuilder::buildSetOpticalFilterNight(getAddress(), irPass));
}

void FujinonSX800Device::queryDayNightEx(std::uint8_t target)
{
    sendQueryFrame(FujinonBuilder::buildQueryDayNightEx(getAddress(), target), "FujinonQueryDayNightEx");
}

void FujinonSX800Device::setZoomSpeedEx(std::uint8_t speed)
{
    enqueueCommand(FujinonBuilder::buildSetZoomSpeedEx(getAddress(), speed));
}

void FujinonSX800Device::setFocusSpeedEx(std::uint8_t speed)
{
    enqueueCommand(FujinonBuilder::buildSetFocusSpeedEx(getAddress(), speed));
}

void FujinonSX800Device::setDigitalZoomMode(FujinonDigitalZoomMode mode)
{
    enqueueCommand(FujinonBuilder::buildSetDigitalZoomMode(getAddress(), mode));
}

void FujinonSX800Device::queryZoomFocusEx(std::uint8_t target)
{
    sendQueryFrame(FujinonBuilder::buildQueryZoomFocusEx(getAddress(), target), "FujinonQueryZoomFocusEx");
}

void FujinonSX800Device::setAntialiasing(bool enable)
{
    enqueueCommand(FujinonBuilder::buildSetAntialiasing(getAddress(), enable));
}

void FujinonSX800Device::menuBack()
{
    enqueueCommand(FujinonBuilder::buildMenuBack(getAddress()));
}

void FujinonSX800Device::formatSDCard()
{
    enqueueCommand(FujinonBuilder::buildFormatSDCard(getAddress()));
}

void FujinonSX800Device::factoryReset()
{
    enqueueCommand(FujinonBuilder::buildFactoryReset(getAddress()));
}

void FujinonSX800Device::setLanguage(FujinonLanguage lang)
{
    enqueueCommand(FujinonBuilder::buildSetLanguage(getAddress(), lang));
}

void FujinonSX800Device::setRTCSecond(std::uint8_t second)
{
    enqueueCommand(FujinonBuilder::buildSetRTCSecond(getAddress(), second));
}

void FujinonSX800Device::setRTCHourMinute(std::uint8_t hour, std::uint8_t minute)
{
    enqueueCommand(FujinonBuilder::buildSetRTCHourMinute(getAddress(), hour, minute));
}

void FujinonSX800Device::setRTCMonthDay(std::uint8_t month, std::uint8_t day)
{
    enqueueCommand(FujinonBuilder::buildSetRTCMonthDay(getAddress(), month, day));
}

void FujinonSX800Device::setRTCYear(std::uint16_t year)
{
    enqueueCommand(FujinonBuilder::buildSetRTCYear(getAddress(), year));
}

void FujinonSX800Device::queryRTC(std::uint8_t subOpCode)
{
    sendQueryFrame(FujinonBuilder::buildQueryRTC(getAddress(), subOpCode), "FujinonQueryRTC");
}

void FujinonSX800Device::queryZoomStandard()
{
    sendQueryFrame(FujinonBuilder::buildQueryZoomStandard(getAddress()), "FujinonQueryZoomStd");
}

bool FujinonSX800Device::isResponseMatchingQuery(
    const std::string& queryTag, const std::vector<std::uint8_t>& frame) const noexcept
{
    if (queryTag == "FujinonQueryFocus") {
        std::uint16_t val { 0U };
        return FujinonParser::parseQueryFocus(frame, val);
    }
    if (queryTag == "FujinonQueryZoom") {
        std::uint16_t val { 0U };
        return FujinonParser::parseQueryZoom(frame, val);
    }
    if (queryTag == "FujinonQuerySerial") {
        std::string s;
        return FujinonParser::parseQuerySerialNumber(frame, s);
    }
    if (queryTag == "FujinonQueryFirmware") {
        std::string fw;
        return FujinonParser::parseQueryFirmwareVersion(frame, fw);
    }
    if (queryTag == "FujinonQueryLens") {
        std::uint8_t st { 0U };
        return FujinonParser::parseQueryLensStatus(frame, st);
    }
    if (queryTag == "FujinonQueryPhoto") {
        FujinonPhotoSettings photo;
        return FujinonParser::parsePhotoSettings(frame, photo);
    }
    if (queryTag == "FujinonQueryImageQuality") {
        FujinonImageQualitySettings img;
        return FujinonParser::parseImageQualitySettings(frame, img);
    }
    if (queryTag == "FujinonQueryManual") {
        FujinonManualSettings man;
        return FujinonParser::parseManualSettings(frame, man);
    }
    if (queryTag == "FujinonQueryImageQualityFine") {
        FujinonFineImageSettings fine;
        return FujinonParser::parseFineImageSettings(frame, fine);
    }
    if (queryTag == "FujinonQueryDayNightEx") {
        FujinonDayNightExSettings dn;
        return FujinonParser::parseDayNightExSettings(frame, dn);
    }
    if (queryTag == "FujinonQueryZoomFocusEx") {
        FujinonZoomFocusExSettings zf;
        return FujinonParser::parseZoomFocusExSettings(frame, zf);
    }
    if (queryTag == "FujinonQueryRTC") {
        std::uint8_t d1 { 0U };
        std::uint8_t d2 { 0U };
        return FujinonParser::parseQueryRTC(frame, d1, d2);
    }
    if (queryTag == "FujinonQueryZoomStd") {
        std::uint16_t val { 0U };
        return FujinonParser::parseQueryZoomStandard(frame, val);
    }

    return PelcoDDevice::isResponseMatchingQuery(queryTag, frame);
}

void FujinonSX800Device::dispatchFrame(const std::vector<std::uint8_t>& frame)
{
    bool updated = false;
    FujinonStatus currentStatus;
    std::shared_ptr<const std::vector<FujinonStatusCallback>> callbacks;

    {
        std::lock_guard<std::mutex> lock(m_fujinonMutex);
        if (FujinonParser::updateFujinonStatus(frame, m_fujinonStatus)) {
            updated = true;
            currentStatus = m_fujinonStatus;
            callbacks = m_fujinonCallbacks;
        }
    }

    if (updated) {
        resolveQueryWait();
        if (callbacks) {
            currentStatus.baseStatus = getStatus();
            for (const auto& cb : *callbacks) {
                if (cb) {
                    cb(currentStatus);
                }
            }
        }
    }

    PelcoDDevice::dispatchFrame(frame);
}

} // namespace PelcoD
