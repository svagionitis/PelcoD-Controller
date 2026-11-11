/// @file LensController.cpp
/// @brief Master multi-channel motorized optical controller implementation.

#include "LensController.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <mutex>
#include <string_view>

#include "AutoFocusEngine.h"
#include "FocusEvaluator.h"
#include "ILensDriver.h"
#include "LensDriverLoader.h"
#include "LensTypes.h"
#include "ZoomTrackFocusEngine.h"

namespace LensControl {

LensController::LensController() noexcept
{
    for (std::size_t i = 0U; i < kMaxChannels; ++i) {
        Channel& ch = m_channels[i];
        ch.zoomTrackEngine = std::make_unique<ZoomTrackFocusEngine>();
        ch.status.camIdx = CameraIndex { static_cast<std::uint8_t>(i) };
    }
}

LensController::~LensController() noexcept
{
    for (std::size_t i = 0U; i < kMaxChannels; ++i) {
        Channel& ch = m_channels[i];
        std::lock_guard<std::mutex> lock { ch.mutex };
        if (ch.afEngine) {
            ch.afEngine->abort();
        }
        if (ch.driver) {
            ch.driver->stopMotion();
        }
    }
}

bool LensController::isValidChannel(CameraIndex cam) const noexcept
{
    return cam.value() < kMaxChannels;
}

void LensController::refreshChannelEnginesLocked(Channel& ch) noexcept
{
    if (ch.driver) {
        ch.afEngine = std::make_unique<AutoFocusEngine>(*ch.driver, ch.evaluator);
        const LensRanges drvRanges = ch.driver->lensRanges();
        if (drvRanges.isValid()) {
            ch.ranges = drvRanges;
        }
        ch.lastZoomPos = ch.driver->currentZoom();
    } else {
        ch.afEngine.reset();
    }
    if (!ch.zoomTrackEngine) {
        ch.zoomTrackEngine = std::make_unique<ZoomTrackFocusEngine>();
    }
}

void LensController::setLensMode(CameraIndex cam, LensMode mode, std::uint16_t data) noexcept
{
    if (!isValidChannel(cam)) {
        return;
    }
    (void)data;
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };

    ch.mode = mode;
    ch.status.mode = mode;

    if (mode == LensMode::AutoFocusOnce) {
        if (ch.afEngine) {
            ch.afEngine->trigger(AutoFocusMethod::ScanThreePhase);
        } else if (ch.driver && ch.driver->hasBuiltinAutofocus()) {
            ch.driver->triggerBuiltinAutofocus();
        }
    } else if (mode == LensMode::AutoFocusContinuous) {
        if (ch.afEngine) {
            ch.afEngine->setCafEnabled(true);
            ch.afEngine->trigger(AutoFocusMethod::AdaptiveMomentum);
        }
    } else {
        if (ch.afEngine && ch.afEngine->isRunning()) {
            ch.afEngine->abort();
        }
        if (ch.afEngine) {
            ch.afEngine->setCafEnabled(false);
        }
    }
}

void LensController::setLensRanges(CameraIndex cam, const LensRanges& ranges) noexcept
{
    if (!isValidChannel(cam) || !ranges.isValid()) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    ch.ranges = ranges;
}

void LensController::setZoomSpeed(CameraIndex cam, MotorSpeed speed) noexcept
{
    if (!isValidChannel(cam)) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    ch.zoomSpeed = speed;
}

void LensController::setFocusSpeed(CameraIndex cam, MotorSpeed speed) noexcept
{
    if (!isValidChannel(cam)) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    ch.focusSpeed = speed;
}

void LensController::moveFocusStep(CameraIndex cam, std::int16_t steps) noexcept
{
    if (!isValidChannel(cam) || steps == 0) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    if (!ch.driver) {
        return;
    }

    const std::uint8_t limits = ch.driver->limitFlags();
    if (steps < 0 && hasFlag(limits, LensLimitFlags::NearLimit)) {
        return;
    }
    if (steps > 0 && hasFlag(limits, LensLimitFlags::FarLimit)) {
        return;
    }

    ch.driver->moveFocus(steps, ch.focusSpeed);
    ch.status.focusPos = ch.driver->currentFocus();
    ch.status.focusLimits = ch.driver->limitFlags();
}

void LensController::moveZoomStep(CameraIndex cam, std::int16_t steps) noexcept
{
    if (!isValidChannel(cam) || steps == 0) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    if (!ch.driver) {
        return;
    }

    const std::uint8_t limits = ch.driver->limitFlags();
    if (steps < 0 && hasFlag(limits, LensLimitFlags::WideLimit)) {
        return;
    }
    if (steps > 0 && hasFlag(limits, LensLimitFlags::TeleLimit)) {
        return;
    }

    ch.driver->moveZoom(steps, ch.zoomSpeed);
    ch.status.zoomPos = ch.driver->currentZoom();
    ch.status.zoomLimits = ch.driver->limitFlags();
}

void LensController::setFocusTarget(CameraIndex cam, MotorPosition pos) noexcept
{
    if (!isValidChannel(cam)) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    if (!ch.driver) {
        return;
    }

    const MotorPosition clamped = ch.ranges.clampFocus(pos);
    ch.driver->setFocusPosition(clamped, ch.focusSpeed);
    ch.status.focusPos = ch.driver->currentFocus();
    ch.status.focusLimits = ch.driver->limitFlags();
}

void LensController::setZoomTarget(CameraIndex cam, MotorPosition pos) noexcept
{
    if (!isValidChannel(cam)) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    if (!ch.driver) {
        return;
    }

    const MotorPosition clamped = ch.ranges.clampZoom(pos);
    ch.driver->setZoomPosition(clamped, ch.zoomSpeed);
    ch.status.zoomPos = ch.driver->currentZoom();
    ch.status.zoomLimits = ch.driver->limitFlags();
}

void LensController::stopMotion(CameraIndex cam) noexcept
{
    if (!isValidChannel(cam)) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    if (ch.afEngine && ch.afEngine->isRunning()) {
        ch.afEngine->abort();
    }
    if (ch.driver) {
        ch.driver->stopMotion();
        ch.status.focusPos = ch.driver->currentFocus();
        ch.status.zoomPos = ch.driver->currentZoom();
        ch.status.focusLimits = ch.driver->limitFlags();
        ch.status.zoomLimits = ch.driver->limitFlags();
    }
}

void LensController::triggerAutoFocus(CameraIndex cam, AutoFocusMethod method) noexcept
{
    if (!isValidChannel(cam)) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };

    if (method == AutoFocusMethod::BuiltInDriver && ch.driver && ch.driver->hasBuiltinAutofocus()) {
        ch.mode = LensMode::AutoFocusOnce;
        ch.status.mode = LensMode::AutoFocusOnce;
        ch.driver->triggerBuiltinAutofocus();
        return;
    }

    if (ch.afEngine) {
        ch.mode = LensMode::AutoFocusOnce;
        ch.status.mode = LensMode::AutoFocusOnce;
        ch.afEngine->trigger(method);
    }
}

void LensController::stopAutoFocus(CameraIndex cam) noexcept
{
    if (!isValidChannel(cam)) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    if (ch.afEngine) {
        ch.afEngine->abort();
    }
    ch.mode = LensMode::ManualJog;
    ch.status.mode = LensMode::ManualJog;
    ch.status.isAfActive = false;
}

void LensController::setZoomTrackFocus(CameraIndex cam, bool enable) noexcept
{
    if (!isValidChannel(cam)) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    ch.zoomTrackEnabled = enable;
    if (ch.driver) {
        ch.lastZoomPos = ch.driver->currentZoom();
        if (ch.driver->hasBuiltinZoomTracking()) {
            ch.driver->setBuiltinZoomTracking(enable);
        }
    }
}

bool LensController::loadTrackCurves(CameraIndex cam, std::string_view filename) noexcept
{
    if (!isValidChannel(cam)) {
        return false;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    if (!ch.zoomTrackEngine) {
        ch.zoomTrackEngine = std::make_unique<ZoomTrackFocusEngine>();
    }
    const TrackCurveError err = ch.zoomTrackEngine->parseFile(filename);
    return err == TrackCurveError::None;
}

void LensController::setTrackerRoi(
    CameraIndex cam, float normX, float normY, float normW, float normH, float margin) noexcept
{
    if (!isValidChannel(cam)) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    if (ch.afEngine) {
        ch.afEngine->setTrackerRoi(normX, normY, normW, normH, margin);
    }
}

void LensController::clearTrackerRoi(CameraIndex cam) noexcept
{
    if (!isValidChannel(cam)) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    if (ch.afEngine) {
        ch.afEngine->clearTrackerRoi();
    }
}

void LensController::processFrame(CameraIndex cam, const std::uint8_t* luma, std::size_t width, std::size_t height,
    std::uint64_t timestampNs, std::uint32_t frameId) noexcept
{
    if (!isValidChannel(cam) || !luma || width == 0U || height == 0U) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    if (ch.afEngine) {
        ch.afEngine->processFrame(luma, width, height, 0U, timestampNs, frameId);
        ch.lastScore = ch.afEngine->latestScore();
    }
}

void LensController::update(CameraIndex cam) noexcept
{
    if (!isValidChannel(cam)) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    if (!ch.driver) {
        return;
    }

    const MotorPosition curFocus = ch.driver->currentFocus();
    const MotorPosition curZoom = ch.driver->currentZoom();
    const std::uint8_t limits = ch.driver->limitFlags();

    ch.status.focusPos = curFocus;
    ch.status.zoomPos = curZoom;
    ch.status.focusLimits = limits;
    ch.status.zoomLimits = limits;
    ch.status.camIdx = cam;

    if (ch.afEngine && ch.afEngine->isRunning()) {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const auto nowNs
            = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
        ch.afEngine->step(curFocus, nowNs);

        if (!ch.afEngine->isRunning()) {
            if (ch.mode == LensMode::AutoFocusOnce) {
                ch.mode = LensMode::ManualJog;
                ch.status.mode = LensMode::ManualJog;
            }
        }
    }

    const bool afActive = (ch.afEngine && ch.afEngine->isRunning());
    ch.status.isAfActive = afActive;

    if (ch.zoomTrackEnabled && ch.zoomTrackEngine && !afActive) {
        if (curZoom != ch.lastZoomPos) {
            const MotorPosition targetFocus = ch.zoomTrackEngine->evaluateFocus(curZoom);
            const MotorPosition clampedFocus = ch.ranges.clampFocus(targetFocus);
            ch.driver->setFocusPosition(clampedFocus, ch.focusSpeed);
            ch.lastZoomPos = curZoom;
        }
    }
}

LensStatus LensController::getLensStatus(CameraIndex cam) const noexcept
{
    if (!isValidChannel(cam)) {
        return LensStatus { };
    }
    const Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    return ch.status;
}

LensRanges LensController::getLensRanges(CameraIndex cam) const noexcept
{
    if (!isValidChannel(cam)) {
        return LensRanges { };
    }
    const Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    return ch.ranges;
}

bool LensController::isAutoFocusRunning(CameraIndex cam) const noexcept
{
    if (!isValidChannel(cam)) {
        return false;
    }
    const Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    return ch.afEngine && ch.afEngine->isRunning();
}

FocusMetricScore LensController::getLastMetricScore(CameraIndex cam) const noexcept
{
    if (!isValidChannel(cam)) {
        return FocusMetricScore { };
    }
    const Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    return ch.lastScore;
}

void LensController::setDriver(CameraIndex cam, std::unique_ptr<ILensDriver> driver) noexcept
{
    if (!isValidChannel(cam)) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    std::lock_guard<std::mutex> lock { ch.mutex };
    if (ch.afEngine) {
        ch.afEngine->abort();
    }
    if (ch.driver) {
        ch.driver->stopMotion();
    }
    ch.driver = std::move(driver);
    refreshChannelEnginesLocked(ch);
    if (ch.driver) {
        ch.status.focusPos = ch.driver->currentFocus();
        ch.status.zoomPos = ch.driver->currentZoom();
        ch.status.focusLimits = ch.driver->limitFlags();
        ch.status.zoomLimits = ch.driver->limitFlags();
    }
}

LensLoadResult LensController::loadLensDll(
    CameraIndex cam, std::string_view dllName, std::uint8_t port, std::uint32_t baud) noexcept
{
    if (!isValidChannel(cam)) {
        return LensLoadResult::InitFailed;
    }
    std::unique_ptr<ILensDriver> loadedDriver { nullptr };
    const LensLoadResult res = m_driverLoader.loadDriver(cam, dllName, port, baud, loadedDriver);
    if (res == LensLoadResult::Success && loadedDriver) {
        setDriver(cam, std::move(loadedDriver));
    }
    return res;
}

void LensController::unloadLensDll(CameraIndex cam) noexcept
{
    if (!isValidChannel(cam)) {
        return;
    }
    Channel& ch = m_channels[cam.value()];
    {
        std::lock_guard<std::mutex> lock { ch.mutex };
        if (ch.afEngine) {
            ch.afEngine->abort();
        }
        if (ch.driver) {
            ch.driver->stopMotion();
        }
        ch.driver.reset();
        refreshChannelEnginesLocked(ch);
    }
    m_driverLoader.unloadDriver(cam);
}

} // namespace LensControl
