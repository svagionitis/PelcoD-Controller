/// @file FujinonSX800Device.cpp
/// @brief Implementation of Fujinon SX800 / SX801 specialized device profile.

#include "FujinonSX800Device.h"

#include <cstdint>

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <glog/logging.h>

#include "CallbackGate.h"

namespace PelcoD {

/// @class FujinonSX800Device::FujinonFrameExt
/// @brief Shared-owned Fujinon RX extension: telemetry model, subscribers, and query matching.
/// @details Co-owned by FujinonSX800Device and its PelcoDDevice base, so it outlives every in-flight
///          RX callback even while the derived device's members are being destroyed (review C3a).
/// @note Thread-safe: status and subscriber list are guarded by separate mutexes; subscriber
///       callbacks are invoked without holding any lock (copy-on-write snapshot).
class FujinonSX800Device::FujinonFrameExt final : public IFrameExtension {
public:
    explicit FujinonFrameExt(std::uint8_t address = 1U) noexcept
        : m_address { address }
    {
    }

    /// @brief Updates active RS-485 bus address for incoming frame filtration.
    /// @param[in] address Device bus address.
    void setAddress(std::uint8_t address) noexcept
    {
        m_address.store(address, std::memory_order_relaxed);
    }

    void setFocusPositionTarget(std::uint16_t focusPos) noexcept
    {
        std::scoped_lock lock { m_statusMutex };
        m_status.absoluteFocusPosition = focusPos;
    }

    /// @brief Returns a snapshot of the Fujinon status model (baseStatus left default).
    /// @return Copy of the current Fujinon status.
    [[nodiscard]] FujinonStatus snapshot() const
    {
        std::scoped_lock lock { m_statusMutex };
        return m_status;
    }

    /// @brief Registers a Fujinon status subscriber.
    /// @param[in] cb Non-empty callback.
    /// @return Identifier used by removeCallback().
    [[nodiscard]] CallbackId addCallback(FujinonStatusCallback cb)
    {
        const CallbackId id { m_nextId.fetch_add(1U, std::memory_order_relaxed) };
        auto gate = std::make_shared<CallbackGate>();
        std::scoped_lock lock { m_cbMutex };
        auto nextList = std::make_shared<std::vector<Entry>>(*m_callbacks);
        nextList->push_back(Entry { id, std::move(cb), std::move(gate) });
        m_callbacks = std::move(nextList);
        return id;
    }

    /// @brief Removes a subscriber and drains any active callback.
    /// @param[in] id Identifier returned by addCallback().
    /// @return True if found and removed.
    [[nodiscard]] bool removeCallback(CallbackId id)
    {
        std::shared_ptr<CallbackGate> gateToClose;
        {
            std::scoped_lock lock { m_cbMutex };
            const auto& current = *m_callbacks;
            const auto it = std::find_if(current.begin(), current.end(), [id](const Entry& e) { return e.id == id; });
            if (it == current.end()) {
                return false;
            }
            gateToClose = it->gate;
            auto nextList = std::make_shared<std::vector<Entry>>();
            nextList->reserve(current.size() - 1U);
            for (const auto& entry : current) {
                if (entry.id != id) {
                    nextList->push_back(entry);
                }
            }
            m_callbacks = std::move(nextList);
        }
        if (gateToClose) {
            gateToClose->close();
        }
        return true;
    }

    /// @brief Removes every subscriber and drains all active callbacks.
    void clearCallbacks()
    {
        std::shared_ptr<const std::vector<Entry>> oldList;
        {
            std::scoped_lock lock { m_cbMutex };
            oldList = m_callbacks;
            m_callbacks = std::make_shared<const std::vector<Entry>>();
        }
        if (oldList) {
            for (const auto& entry : *oldList) {
                if (entry.gate) {
                    entry.gate->close();
                }
            }
        }
    }

    [[nodiscard]] ExtMatch matchQuery(
        const std::string& queryTag, const std::vector<std::uint8_t>& frame) const noexcept override;

    [[nodiscard]] bool onFrame(const std::vector<std::uint8_t>& frame) override
    {
        if (frame.size() < 2U || frame[1] != m_address.load(std::memory_order_relaxed)) {
            return false;
        }
        std::scoped_lock lock { m_statusMutex };
        return FujinonParser::updateFujinonStatus(frame, m_status);
    }

    void publish(const DeviceStatus& base) override
    {
        std::shared_ptr<const std::vector<Entry>> callbacks {};
        {
            std::scoped_lock lock { m_cbMutex };
            callbacks = m_callbacks;
        }
        FujinonStatus current { snapshot() };
        current.baseStatus = base;
        for (const auto& entry : *callbacks) {
            if (entry.cb && entry.gate) {
                const CallbackGate::Pass pass { *entry.gate };
                if (pass) {
                    try {
                        entry.cb(current);
                    } catch (const std::exception& ex) {
                        LOG(ERROR) << "FujinonStatusCallback threw an unhandled std::exception: " << ex.what();
                    } catch (...) {
                        LOG(ERROR) << "FujinonStatusCallback threw an unknown non-standard exception";
                    }
                }
            }
        }
    }

private:
    struct Entry {
        CallbackId id { 0U };
        FujinonStatusCallback cb {};
        std::shared_ptr<CallbackGate> gate {};
    };

    std::atomic<std::uint8_t> m_address { 1U };
    mutable std::mutex m_statusMutex {};
    FujinonStatus m_status {};

    mutable std::mutex m_cbMutex {};
    std::atomic<CallbackId> m_nextId { 1U };
    std::shared_ptr<const std::vector<Entry>> m_callbacks { std::make_shared<const std::vector<Entry>>() };
};

FujinonSX800Device::FujinonSX800Device(std::shared_ptr<ITransport> transport, std::uint8_t address)
    : PelcoDDevice(std::move(transport), address)
    , m_ext { std::make_shared<FujinonFrameExt>(address) }
{
    // No session can be active inside the constructor, so installation cannot be refused.
    static_cast<void>(setFrameExt(m_ext));
}

FujinonSX800Device::~FujinonSX800Device() = default;

void FujinonSX800Device::setAddress(std::uint8_t address)
{
    PelcoDDevice::setAddress(address);
    if (m_ext) {
        m_ext->setAddress(address);
    }
}

FujinonStatus FujinonSX800Device::getFujinonStatus() const
{
    FujinonStatus status { m_ext->snapshot() };
    status.baseStatus = getStatus();
    return status;
}

Connection FujinonSX800Device::addFujinonStatusCallback(FujinonStatusCallback cb)
{
    if (!cb) {
        return Connection {};
    }
    const CallbackId id { m_ext->addCallback(std::move(cb)) };
    std::weak_ptr<FujinonFrameExt> weakExt { m_ext };
    return Connection([weakExt, id]() {
        if (auto ext = weakExt.lock()) {
            static_cast<void>(ext->removeCallback(id));
        }
    });
}

bool FujinonSX800Device::removeFujinonStatusCallback(CallbackId id)
{
    return m_ext->removeCallback(id);
}

void FujinonSX800Device::clearCallbacks()
{
    PelcoDDevice::clearCallbacks();
    m_ext->clearCallbacks();
}

SendResult FujinonSX800Device::setOISMode(FujinonOISMode mode)
{
    return enqueueCommand(FujinonBuilder::buildSetOIS(getAddress(), mode));
}

SendResult FujinonSX800Device::setDefog(FujinonDefogLevel level)
{
    return enqueueCommand(FujinonBuilder::buildSetDefog(getAddress(), level));
}

SendResult FujinonSX800Device::setHeatHaze(FujinonHeatHazeLevel level)
{
    return enqueueCommand(FujinonBuilder::buildSetHeatHaze(getAddress(), level));
}

SendResult FujinonSX800Device::setWDR(FujinonWDRLevel level)
{
    return enqueueCommand(FujinonBuilder::buildSetWDR(getAddress(), level));
}

SendResult FujinonSX800Device::setVLCFilter(bool enable)
{
    return enqueueCommand(FujinonBuilder::buildSetVLCFilter(getAddress(), enable));
}

SendResult FujinonSX800Device::setBrightness(std::uint8_t level)
{
    return enqueueCommand(FujinonBuilder::buildSetBrightness(getAddress(), level));
}

SendResult FujinonSX800Device::setContrast(std::uint8_t level)
{
    return enqueueCommand(FujinonBuilder::buildSetContrast(getAddress(), level));
}

SendResult FujinonSX800Device::setSaturation(std::uint8_t level)
{
    return enqueueCommand(FujinonBuilder::buildSetSaturation(getAddress(), level));
}

SendResult FujinonSX800Device::setSharpness(std::uint8_t level)
{
    return enqueueCommand(FujinonBuilder::buildSetSharpness(getAddress(), level));
}

SendResult FujinonSX800Device::setColorTemperature(FujinonColorTemp temp)
{
    return enqueueCommand(FujinonBuilder::buildSetColorTemperature(getAddress(), temp));
}

SendResult FujinonSX800Device::setWhiteBalance(FujinonWBMode mode)
{
    return enqueueCommand(FujinonBuilder::buildSetWhiteBalance(getAddress(), mode));
}

SendResult FujinonSX800Device::setDigitalZoom(FujinonDigitalZoom zoom)
{
    return enqueueCommand(FujinonBuilder::buildSetDigitalZoom(getAddress(), zoom));
}

SendResult FujinonSX800Device::setNoiseReduction(std::uint8_t level)
{
    return enqueueCommand(FujinonBuilder::buildSetNoiseReduction(getAddress(), level));
}

SendResult FujinonSX800Device::setDayNightMode(FujinonDayNightMode mode)
{
    return enqueueCommand(FujinonBuilder::buildSetDayNight(getAddress(), mode));
}

SendResult FujinonSX800Device::setIRWavelength(FujinonIRWavelength wavelength)
{
    return enqueueCommand(FujinonBuilder::buildSetIRWavelength(getAddress(), wavelength));
}

SendResult FujinonSX800Device::setFocusPosition(std::uint16_t focusPos)
{
    const auto result = enqueueCommand(FujinonBuilder::buildSetFocusPosition(getAddress(), focusPos));
    if (result == SendResult::Success && m_ext) {
        m_ext->setFocusPositionTarget(focusPos);
    }
    return result;
}

SendResult FujinonSX800Device::onePushAF()
{
    static_cast<void>(setAutoFocus(FujinonAutoFocusMode::AFOff));
    return enqueueCommand(FujinonBuilder::buildOnePushAF(getAddress()));
}

SendResult FujinonSX800Device::setAFSensitivity(std::uint8_t sensitivity)
{
    return enqueueCommand(FujinonBuilder::buildSetAFSensitivity(getAddress(), sensitivity));
}

SendResult FujinonSX800Device::setAFArea(std::uint8_t area)
{
    return enqueueCommand(FujinonBuilder::buildSetAFArea(getAddress(), area));
}

SendResult FujinonSX800Device::setManualIris(std::uint8_t irisVal)
{
    return enqueueCommand(FujinonBuilder::buildSetManualIris(getAddress(), irisVal));
}

SendResult FujinonSX800Device::setManualShutter(std::uint8_t shutterVal)
{
    return enqueueCommand(FujinonBuilder::buildSetManualShutter(getAddress(), shutterVal));
}

SendResult FujinonSX800Device::setManualISO(std::uint8_t isoVal)
{
    return enqueueCommand(FujinonBuilder::buildSetManualISO(getAddress(), isoVal));
}

SendResult FujinonSX800Device::setTimeDisplay(bool enable)
{
    return enqueueCommand(FujinonBuilder::buildSetTimeDisplay(getAddress(), enable));
}

SendResult FujinonSX800Device::setTimePosition(FujinonOSDPosition pos)
{
    return enqueueCommand(FujinonBuilder::buildSetTimePosition(getAddress(), pos));
}

SendResult FujinonSX800Device::setTitleDisplay(bool enable)
{
    return enqueueCommand(FujinonBuilder::buildSetTitleDisplay(getAddress(), enable));
}

SendResult FujinonSX800Device::setTitlePosition(FujinonOSDPosition pos)
{
    return enqueueCommand(FujinonBuilder::buildSetTitlePosition(getAddress(), pos));
}

SendResult FujinonSX800Device::setIdDisplay(bool enable)
{
    return enqueueCommand(FujinonBuilder::buildSetIdDisplay(getAddress(), enable));
}

SendResult FujinonSX800Device::setIdPosition(FujinonOSDPosition pos)
{
    return enqueueCommand(FujinonBuilder::buildSetIdPosition(getAddress(), pos));
}

SendResult FujinonSX800Device::setReticleDisplay(bool enable)
{
    return enqueueCommand(FujinonBuilder::buildSetReticleDisplay(getAddress(), enable));
}

SendResult FujinonSX800Device::setVideoMode(FujinonVideoMode mode)
{
    return enqueueCommand(FujinonBuilder::buildSetVideoMode(getAddress(), mode));
}

SendResult FujinonSX800Device::setHDFormat(FujinonHDFormat format)
{
    return enqueueCommand(FujinonBuilder::buildSetHDFormat(getAddress(), format));
}

SendResult FujinonSX800Device::setRS485Termination(bool enable)
{
    return enqueueCommand(FujinonBuilder::buildSetRS485Termination(getAddress(), enable));
}

SendResult FujinonSX800Device::reboot()
{
    return enqueueCommand(FujinonBuilder::buildReboot(getAddress()));
}

SendResult FujinonSX800Device::recordLiveView(bool start)
{
    return enqueueCommand(FujinonBuilder::buildRecordLiveView(getAddress(), start));
}

SendResult FujinonSX800Device::playMovie(bool start)
{
    return enqueueCommand(FujinonBuilder::buildPlayMovie(getAddress(), start));
}

SendResult FujinonSX800Device::setMovieMode(bool isPlayMode)
{
    return enqueueCommand(FujinonBuilder::buildSetMovieMode(getAddress(), isPlayMode));
}

SendResult FujinonSX800Device::menuOk()
{
    return enqueueCommand(FujinonBuilder::buildMenuOk(getAddress()));
}

SendResult FujinonSX800Device::menuDirection(FujinonMenuDirection dir)
{
    return enqueueCommand(FujinonBuilder::buildMenuDirection(getAddress(), dir));
}

SendResult FujinonSX800Device::queryFujinonFocus()
{
    return sendQueryFrame(FujinonBuilder::buildQueryFocus(getAddress()), "FujinonQueryFocus");
}

SendResult FujinonSX800Device::queryFujinonZoom()
{
    return sendQueryFrame(FujinonBuilder::buildQueryZoom(getAddress()), "FujinonQueryZoom");
}

SendResult FujinonSX800Device::querySerialNumber()
{
    return sendQueryFrame(FujinonBuilder::buildQuerySerialNumber(getAddress()), "FujinonQuerySerial");
}

SendResult FujinonSX800Device::queryFirmwareVersion()
{
    return sendQueryFrame(FujinonBuilder::buildQueryFirmwareVersion(getAddress()), "FujinonQueryFirmware");
}

SendResult FujinonSX800Device::queryLensStatus()
{
    return sendQueryFrame(FujinonBuilder::buildQueryLensStatus(getAddress()), "FujinonQueryLens");
}

SendResult FujinonSX800Device::queryPhotoSettings(std::uint8_t target)
{
    return sendQueryFrame(FujinonBuilder::buildQueryPhotoSettings(getAddress(), target), "FujinonQueryPhoto");
}

SendResult FujinonSX800Device::queryImageQuality(std::uint8_t target)
{
    return sendQueryFrame(FujinonBuilder::buildQueryImageQuality(getAddress(), target), "FujinonQueryImageQuality");
}

SendResult FujinonSX800Device::queryManualSettings()
{
    return sendQueryFrame(FujinonBuilder::buildQueryManualSettings(getAddress()), "FujinonQueryManual");
}

SendResult FujinonSX800Device::setBrightnessFine(std::uint8_t level)
{
    return enqueueCommand(FujinonBuilder::buildSetBrightnessFine(getAddress(), level));
}

SendResult FujinonSX800Device::setContrastFine(std::uint8_t level)
{
    return enqueueCommand(FujinonBuilder::buildSetContrastFine(getAddress(), level));
}

SendResult FujinonSX800Device::setSaturationFine(std::uint8_t level)
{
    return enqueueCommand(FujinonBuilder::buildSetSaturationFine(getAddress(), level));
}

SendResult FujinonSX800Device::setSharpnessFine(std::uint8_t level)
{
    return enqueueCommand(FujinonBuilder::buildSetSharpnessFine(getAddress(), level));
}

SendResult FujinonSX800Device::setWBShiftRedFine(std::uint8_t level)
{
    return enqueueCommand(FujinonBuilder::buildSetWBShiftRedFine(getAddress(), level));
}

SendResult FujinonSX800Device::setWBShiftBlueFine(std::uint8_t level)
{
    return enqueueCommand(FujinonBuilder::buildSetWBShiftBlueFine(getAddress(), level));
}

SendResult FujinonSX800Device::queryImageQualityFine(std::uint8_t target)
{
    return sendQueryFrame(
        FujinonBuilder::buildQueryImageQualityFine(getAddress(), target), "FujinonQueryImageQualityFine");
}

SendResult FujinonSX800Device::setDayNightModeEx(FujinonDayNightModeEx mode)
{
    return enqueueCommand(FujinonBuilder::buildSetDayNightModeEx(getAddress(), mode));
}

SendResult FujinonSX800Device::setDayToNightThreshold(std::uint8_t threshold)
{
    return enqueueCommand(FujinonBuilder::buildSetDayToNightThreshold(getAddress(), threshold));
}

SendResult FujinonSX800Device::setNightToDayThreshold(std::uint8_t threshold)
{
    return enqueueCommand(FujinonBuilder::buildSetNightToDayThreshold(getAddress(), threshold));
}

SendResult FujinonSX800Device::setDayNightAutoDelay(std::uint8_t delaySeconds)
{
    return enqueueCommand(FujinonBuilder::buildSetDayNightAutoDelay(getAddress(), delaySeconds));
}

SendResult FujinonSX800Device::setDayStartTime(std::uint8_t hour, std::uint8_t minute)
{
    return enqueueCommand(FujinonBuilder::buildSetDayStartTime(getAddress(), hour, minute));
}

SendResult FujinonSX800Device::setNightStartTime(std::uint8_t hour, std::uint8_t minute)
{
    return enqueueCommand(FujinonBuilder::buildSetNightStartTime(getAddress(), hour, minute));
}

SendResult FujinonSX800Device::setOpticalFilterDay(bool irPass)
{
    return enqueueCommand(FujinonBuilder::buildSetOpticalFilterDay(getAddress(), irPass));
}

SendResult FujinonSX800Device::setOpticalFilterNight(bool irPass)
{
    return enqueueCommand(FujinonBuilder::buildSetOpticalFilterNight(getAddress(), irPass));
}

SendResult FujinonSX800Device::queryDayNightEx(std::uint8_t target)
{
    return sendQueryFrame(FujinonBuilder::buildQueryDayNightEx(getAddress(), target), "FujinonQueryDayNightEx");
}

SendResult FujinonSX800Device::setZoomSpeedEx(std::uint8_t speed)
{
    return enqueueCommand(FujinonBuilder::buildSetZoomSpeedEx(getAddress(), speed));
}

SendResult FujinonSX800Device::setFocusSpeedEx(std::uint8_t speed)
{
    return enqueueCommand(FujinonBuilder::buildSetFocusSpeedEx(getAddress(), speed));
}

SendResult FujinonSX800Device::setDigitalZoomMode(FujinonDigitalZoomMode mode)
{
    return enqueueCommand(FujinonBuilder::buildSetDigitalZoomMode(getAddress(), mode));
}

SendResult FujinonSX800Device::queryZoomFocusEx(std::uint8_t target)
{
    return sendQueryFrame(FujinonBuilder::buildQueryZoomFocusEx(getAddress(), target), "FujinonQueryZoomFocusEx");
}

SendResult FujinonSX800Device::setAntialiasing(bool enable)
{
    return enqueueCommand(FujinonBuilder::buildSetAntialiasing(getAddress(), enable));
}

SendResult FujinonSX800Device::menuBack()
{
    return enqueueCommand(FujinonBuilder::buildMenuBack(getAddress()));
}

SendResult FujinonSX800Device::formatSDCard()
{
    return enqueueCommand(FujinonBuilder::buildFormatSDCard(getAddress()));
}

SendResult FujinonSX800Device::factoryReset()
{
    return enqueueCommand(FujinonBuilder::buildFactoryReset(getAddress()));
}

SendResult FujinonSX800Device::setLanguage(FujinonLanguage lang)
{
    return enqueueCommand(FujinonBuilder::buildSetLanguage(getAddress(), lang));
}

SendResult FujinonSX800Device::setRTCSecond(std::uint8_t second)
{
    return enqueueCommand(FujinonBuilder::buildSetRTCSecond(getAddress(), second));
}

SendResult FujinonSX800Device::setRTCHourMinute(std::uint8_t hour, std::uint8_t minute)
{
    return enqueueCommand(FujinonBuilder::buildSetRTCHourMinute(getAddress(), hour, minute));
}

SendResult FujinonSX800Device::setRTCMonthDay(std::uint8_t month, std::uint8_t day)
{
    return enqueueCommand(FujinonBuilder::buildSetRTCMonthDay(getAddress(), month, day));
}

SendResult FujinonSX800Device::setRTCYear(std::uint16_t year)
{
    return enqueueCommand(FujinonBuilder::buildSetRTCYear(getAddress(), year));
}

SendResult FujinonSX800Device::queryRTC(std::uint8_t subOpCode)
{
    return sendQueryFrame(FujinonBuilder::buildQueryRTC(getAddress(), subOpCode), "FujinonQueryRTC");
}

SendResult FujinonSX800Device::queryZoomStandard()
{
    return sendQueryFrame(FujinonBuilder::buildQueryZoomStandard(getAddress()), "FujinonQueryZoomStd");
}

SendResult FujinonSX800Device::setZoomPosition(std::uint16_t zoomPos)
{
    return PtzCommands::setZoomPosition(zoomPos);
}

SendResult FujinonSX800Device::setZoomSpeed(std::uint8_t speed)
{
    const std::uint8_t satSpeed { std::min(speed, static_cast<std::uint8_t>(0x03U)) };
    return PtzCommands::setZoomSpeed(satSpeed);
}

SendResult FujinonSX800Device::setFocusSpeed(std::uint8_t speed)
{
    const std::uint8_t satSpeed { std::min(speed, static_cast<std::uint8_t>(0x03U)) };
    return PtzCommands::setFocusSpeed(satSpeed);
}

SendResult FujinonSX800Device::setAutoFocus(AutoMode mode)
{
    const auto fMode = (mode == AutoMode::Off) ? FujinonAutoFocusMode::AFOff : FujinonAutoFocusMode::AFOn;
    return setAutoFocus(fMode);
}

SendResult FujinonSX800Device::setAutoFocus(FujinonAutoFocusMode mode)
{
    return enqueueCommand(FujinonBuilder::buildSetAutoFocus(getAddress(), mode));
}

SendResult FujinonSX800Device::setAutoIris(FujinonAutoIrisMode mode)
{
    return enqueueCommand(FujinonBuilder::buildSetAutoIris(getAddress(), mode));
}

SendResult FujinonSX800Device::setAGC(FujinonAGCMode mode)
{
    return enqueueCommand(FujinonBuilder::buildSetAGC(getAddress(), mode));
}

SendResult FujinonSX800Device::setBLC(FujinonBLCMode mode)
{
    return enqueueCommand(FujinonBuilder::buildSetBLC(getAddress(), mode));
}

SendResult FujinonSX800Device::setBaudRate(std::uint8_t baudRateIndex)
{
    return enqueueCommand(FujinonBuilder::buildSetBaudRate(getAddress(), baudRateIndex));
}

SendResult FujinonSX800Device::setShutterLimitAuto(FujinonShutterLimitAuto limit)
{
    return enqueueCommand(FujinonBuilder::buildSetShutterLimitAuto(getAddress(), limit));
}

SendResult FujinonSX800Device::queryManualShutter()
{
    return sendQueryFrame(FujinonBuilder::buildQueryManualShutter(getAddress()), "FujinonQueryManualShutter");
}

SendResult FujinonSX800Device::queryManualISO()
{
    return sendQueryFrame(FujinonBuilder::buildQueryManualISO(getAddress()), "FujinonQueryManualISO");
}

SendResult FujinonSX800Device::queryManualIris()
{
    return sendQueryFrame(FujinonBuilder::buildQueryManualIris(getAddress()), "FujinonQueryManualIris");
}

SendResult FujinonSX800Device::setPhotoModePreset(FujinonPhotoModePreset preset)
{
    return enqueueCommand(FujinonBuilder::buildSetPhotoModePreset(getAddress(), preset));
}

SendResult FujinonSX800Device::setExternalDayNight(FujinonExternalDayNight mode)
{
    return enqueueCommand(FujinonBuilder::buildSetExternalDayNight(getAddress(), mode));
}

SendResult FujinonSX800Device::queryDisplay(std::uint8_t target)
{
    return sendQueryFrame(FujinonBuilder::buildQueryDisplay(getAddress(), target), "FujinonQueryDisplay");
}

SendResult FujinonSX800Device::setTimeFormat(FujinonTimeFormat fmt)
{
    return enqueueCommand(FujinonBuilder::buildSetTimeFormat(getAddress(), fmt));
}

SendResult FujinonSX800Device::setDateFormat(FujinonDateFormat fmt)
{
    return enqueueCommand(FujinonBuilder::buildSetDateFormat(getAddress(), fmt));
}

SendResult FujinonSX800Device::setVideoDisplay(FujinonVideoDisplayMode mode)
{
    return enqueueCommand(FujinonBuilder::buildSetVideoDisplay(getAddress(), mode));
}

SendResult FujinonSX800Device::setRS485Address(std::uint8_t newId)
{
    return enqueueCommand(FujinonBuilder::buildSetRS485Address(getAddress(), newId));
}

SendResult FujinonSX800Device::setSDRecordingMode(FujinonSDRecordMode mode)
{
    return enqueueCommand(FujinonBuilder::buildSetSDRecordingMode(getAddress(), mode));
}

SendResult FujinonSX800Device::setSDCapacityDisp(bool enable)
{
    return enqueueCommand(FujinonBuilder::buildSetSDCapacityDisp(getAddress(), enable));
}

SendResult FujinonSX800Device::recordLogData()
{
    return enqueueCommand(FujinonBuilder::buildRecordLogData(getAddress()));
}

SendResult FujinonSX800Device::setEncodeMode(FujinonVideoEncoding enc)
{
    return enqueueCommand(FujinonBuilder::buildSetEncodeMode(getAddress(), enc));
}

SendResult FujinonSX800Device::setFirstMovieFile(std::uint16_t fileIndex)
{
    return enqueueCommand(FujinonBuilder::buildSetFirstMovieFile(getAddress(), fileIndex));
}

SendResult FujinonSX800Device::playMovieMode(FujinonMoviePlayMode mode)
{
    return enqueueCommand(FujinonBuilder::buildPlayMovieMode(getAddress(), mode));
}

SendResult FujinonSX800Device::deleteMovie(std::uint16_t fileIndex)
{
    return enqueueCommand(FujinonBuilder::buildDeleteMovie(getAddress(), fileIndex));
}

SendResult FujinonSX800Device::queryKeySetting(std::uint8_t target)
{
    return sendQueryFrame(FujinonBuilder::buildQueryKeySetting(getAddress(), target), "FujinonQueryKeySetting");
}

SendResult FujinonSX800Device::queryMovieCount()
{
    return sendQueryFrame(FujinonBuilder::buildQueryMovieCount(getAddress()), "FujinonQueryMovieCount");
}

SendResult FujinonSX800Device::queryMovieYear(std::uint16_t fileIndex)
{
    return sendQueryFrame(FujinonBuilder::buildQueryMovieYear(getAddress(), fileIndex), "FujinonQueryMovieYear");
}

SendResult FujinonSX800Device::queryMovieDate(std::uint16_t fileIndex)
{
    return sendQueryFrame(FujinonBuilder::buildQueryMovieDate(getAddress(), fileIndex), "FujinonQueryMovieDate");
}

SendResult FujinonSX800Device::queryMovieTime(std::uint16_t fileIndex)
{
    return sendQueryFrame(FujinonBuilder::buildQueryMovieTime(getAddress(), fileIndex), "FujinonQueryMovieTime");
}

SendResult FujinonSX800Device::queryLogData(FujinonLogType logType)
{
    return sendQueryFrame(FujinonBuilder::buildQueryLogData(getAddress(), logType), "FujinonQueryLogData");
}

SendResult FujinonSX800Device::setDigitalZoomEx(std::uint8_t magIndex)
{
    return enqueueCommand(FujinonBuilder::buildSetDigitalZoomEx(getAddress(), magIndex));
}

namespace {

    /// @brief Maps a parser verdict for an extension-owned query tag onto ExtMatch.
    /// @param[in] parsed True if the frame parsed as the expected response.
    /// @return ExtMatch::Matched or ExtMatch::Rejected.
    [[nodiscard]] constexpr ExtMatch toMatch(bool parsed) noexcept
    {
        return parsed ? ExtMatch::Matched : ExtMatch::Rejected;
    }

} // namespace

ExtMatch FujinonSX800Device::FujinonFrameExt::matchQuery(
    const std::string& queryTag, const std::vector<std::uint8_t>& frame) const noexcept
{
    if (queryTag == "FujinonQueryFocus") {
        std::uint16_t val { 0U };
        return toMatch(FujinonParser::parseQueryFocus(frame, val));
    }
    if (queryTag == "FujinonQueryZoom") {
        std::uint16_t val { 0U };
        return toMatch(FujinonParser::parseQueryZoom(frame, val));
    }
    if (queryTag == "FujinonQuerySerial") {
        std::string s {};
        return toMatch(FujinonParser::parseQuerySerialNumber(frame, s));
    }
    if (queryTag == "FujinonQueryFirmware") {
        std::string fw {};
        return toMatch(FujinonParser::parseQueryFirmwareVersion(frame, fw));
    }
    if (queryTag == "FujinonQueryLens") {
        std::uint8_t st { 0U };
        return toMatch(FujinonParser::parseQueryLensStatus(frame, st));
    }
    if (queryTag == "FujinonQueryPhoto") {
        FujinonPhotoSettings photo {};
        return toMatch(FujinonParser::parsePhotoSettings(frame, photo));
    }
    if (queryTag == "FujinonQueryImageQuality") {
        FujinonImageQualitySettings img {};
        return toMatch(FujinonParser::parseImageQualitySettings(frame, img));
    }
    if (queryTag == "FujinonQueryManual") {
        FujinonManualSettings man {};
        return toMatch(FujinonParser::parseManualSettings(frame, man));
    }
    if (queryTag == "FujinonQueryImageQualityFine") {
        FujinonFineImageSettings fine {};
        return toMatch(FujinonParser::parseFineImageSettings(frame, fine));
    }
    if (queryTag == "FujinonQueryDayNightEx") {
        FujinonDayNightExSettings dn {};
        return toMatch(FujinonParser::parseDayNightExSettings(frame, dn));
    }
    if (queryTag == "FujinonQueryZoomFocusEx") {
        FujinonZoomFocusExSettings zf {};
        return toMatch(FujinonParser::parseZoomFocusExSettings(frame, zf));
    }
    if (queryTag == "FujinonQueryRTC") {
        std::uint8_t d1 { 0U };
        std::uint8_t d2 { 0U };
        return toMatch(FujinonParser::parseQueryRTC(frame, d1, d2));
    }
    if (queryTag == "FujinonQueryZoomStd") {
        std::uint16_t val { 0U };
        return toMatch(FujinonParser::parseQueryZoomStandard(frame, val));
    }
    if (queryTag == "FujinonQueryManualShutter") {
        std::uint8_t val { 0U };
        return toMatch(FujinonParser::parseQueryManualShutter(frame, val));
    }
    if (queryTag == "FujinonQueryManualISO") {
        std::uint8_t val { 0U };
        return toMatch(FujinonParser::parseQueryManualISO(frame, val));
    }
    if (queryTag == "FujinonQueryManualIris") {
        std::uint8_t val { 0U };
        return toMatch(FujinonParser::parseQueryManualIris(frame, val));
    }
    if (queryTag == "FujinonQueryDisplay") {
        std::uint8_t target { 0U };
        std::uint8_t val { 0U };
        return toMatch(FujinonParser::parseQueryDisplay(frame, target, val));
    }
    if (queryTag == "FujinonQueryKeySetting") {
        std::uint8_t target { 0U };
        std::uint8_t val { 0U };
        return toMatch(FujinonParser::parseQueryKeySetting(frame, target, val));
    }
    if (queryTag == "FujinonQueryMovieCount") {
        std::uint16_t count { 0U };
        return toMatch(FujinonParser::parseQueryMovieCount(frame, count));
    }
    if (queryTag == "FujinonQueryMovieYear") {
        std::uint16_t year { 0U };
        return toMatch(FujinonParser::parseQueryMovieYear(frame, year));
    }
    if (queryTag == "FujinonQueryMovieDate") {
        std::uint8_t month { 0U };
        std::uint8_t day { 0U };
        return toMatch(FujinonParser::parseQueryMovieDate(frame, month, day));
    }
    if (queryTag == "FujinonQueryMovieTime") {
        std::uint8_t hour { 0U };
        std::uint8_t min { 0U };
        return toMatch(FujinonParser::parseQueryMovieTime(frame, hour, min));
    }
    if (queryTag == "FujinonQueryLogData") {
        std::uint8_t d1 { 0U };
        std::uint8_t d2 { 0U };
        return toMatch(FujinonParser::parseQueryLogData(frame, d1, d2));
    }

    return ExtMatch::NotHandled;
}

} // namespace PelcoD
