/// @file FujinonSX800Device.cpp
/// @brief Implementation of Fujinon SX800 / SX801 specialized device profile.

#include "FujinonSX800Device.h"
#include "CallbackGate.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <utility>

namespace PelcoD {

/// @class FujinonSX800Device::FujinonFrameExt
/// @brief Shared-owned Fujinon RX extension: telemetry model, subscribers, and query matching.
/// @details Co-owned by FujinonSX800Device and its PelcoDDevice base, so it outlives every in-flight
///          RX callback even while the derived device's members are being destroyed (review C3a).
/// @note Thread-safe: status and subscriber list are guarded by separate mutexes; subscriber
///       callbacks are invoked without holding any lock (copy-on-write snapshot).
class FujinonSX800Device::FujinonFrameExt final : public IFrameExtension {
public:
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
    bool removeCallback(CallbackId id)
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
                    entry.cb(current);
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

    mutable std::mutex m_statusMutex {};
    FujinonStatus m_status {};

    mutable std::mutex m_cbMutex {};
    std::atomic<CallbackId> m_nextId { 1U };
    std::shared_ptr<const std::vector<Entry>> m_callbacks { std::make_shared<const std::vector<Entry>>() };
};

FujinonSX800Device::FujinonSX800Device(std::shared_ptr<ITransport> transport, std::uint8_t address)
    : PelcoDDevice(std::move(transport), address)
    , m_ext { std::make_shared<FujinonFrameExt>() }
{
    // No session can be active inside the constructor, so installation cannot be refused.
    static_cast<void>(setFrameExt(m_ext));
}

FujinonSX800Device::~FujinonSX800Device() = default;

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

    return ExtMatch::NotHandled;
}

} // namespace PelcoD
