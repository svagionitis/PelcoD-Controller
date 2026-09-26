/// @file SensorFusionManager.cpp
/// @brief Implementation of Optical Sensor Switching, Digital Match-Zoom & Fusion Manager.

#include "SensorFusionManager.h"

#include <algorithm>
#include <cmath>

namespace PayloadHal {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kWideHfovDeg = 60.0;
constexpr double kMaxOpticalZoom = 30.0; // 30x optical zoom ratio
constexpr double kEpsilon = 1e-6;

double degToRad(double deg) noexcept
{
    return deg * kDegToRad;
}

double radToDeg(double rad) noexcept
{
    return rad * kRadToDeg;
}

} // namespace

SensorFusionManager::SensorFusionManager() = default;

SensorFusionManager::SensorFusionManager(
    std::shared_ptr<ICameraPayload> primaryCam,
    std::shared_ptr<ICameraPayload> secondaryCam)
    : m_primaryCam(std::move(primaryCam))
    , m_secondaryCam(std::move(secondaryCam))
{
}

SensorFusionManager::~SensorFusionManager() = default;

void SensorFusionManager::setCameras(
    std::shared_ptr<ICameraPayload> primaryCam,
    std::shared_ptr<ICameraPayload> secondaryCam) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_primaryCam = std::move(primaryCam);
    m_secondaryCam = std::move(secondaryCam);
}

std::shared_ptr<ICameraPayload> SensorFusionManager::primaryCamera() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_primaryCam;
}

std::shared_ptr<ICameraPayload> SensorFusionManager::secondaryCamera() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_secondaryCam;
}

std::shared_ptr<ICameraPayload> SensorFusionManager::activeCamera() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return getCameraForChannelLocked(m_activeChannel);
}

std::shared_ptr<ICameraPayload> SensorFusionManager::getCameraForChannelLocked(OpticalChannel channel) const noexcept
{
    switch (channel) {
    case OpticalChannel::Primary:
        return m_primaryCam;
    case OpticalChannel::Secondary:
        return m_secondaryCam;
    case OpticalChannel::Auxiliary:
        return m_secondaryCam ? m_secondaryCam : m_primaryCam;
    }
    return nullptr;
}

bool SensorFusionManager::setActiveChannel(OpticalChannel channel, bool applyMatchZoomFlag)
{
    std::shared_ptr<ICameraPayload> targetCam;
    std::shared_ptr<ICameraPayload> srcCam;
    OpticalChannel prevChannel;
    ChannelSwitchCallback callback;
    bool needMatchZoom = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (channel == m_activeChannel) {
            return true;
        }

        targetCam = getCameraForChannelLocked(channel);
        if (!targetCam) {
            return false;
        }

        prevChannel = m_activeChannel;
        srcCam = getCameraForChannelLocked(prevChannel);
        needMatchZoom = (applyMatchZoomFlag && m_matchZoomEnabled && srcCam != nullptr);

        m_activeChannel = channel;
        callback = m_switchCallback;
    }

    if (needMatchZoom) {
        const auto res = computeMatchZoomInternal(srcCam, targetCam);
        targetCam->setZoomNormalized(res.targetOpticalZoom01);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentDigitalCropFactor = res.digitalCropFactor;
    } else {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentDigitalCropFactor = 1.0;
    }

    if (callback) {
        callback(prevChannel, channel, "Operator manual switch");
    }
    return true;
}

OpticalChannel SensorFusionManager::activeChannel() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeChannel;
}

void SensorFusionManager::setMatchZoomEnabled(bool enabled) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_matchZoomEnabled = enabled;
}

bool SensorFusionManager::isMatchZoomEnabled() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_matchZoomEnabled;
}

MatchZoomResult SensorFusionManager::computeMatchZoom(
    OpticalChannel sourceChannel,
    OpticalChannel targetChannel) const
{
    std::shared_ptr<ICameraPayload> srcCam;
    std::shared_ptr<ICameraPayload> tgtCam;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        srcCam = getCameraForChannelLocked(sourceChannel);
        tgtCam = getCameraForChannelLocked(targetChannel);
    }
    return computeMatchZoomInternal(srcCam, tgtCam);
}

MatchZoomResult SensorFusionManager::computeMatchZoomInternal(
    const std::shared_ptr<ICameraPayload>& srcCam,
    const std::shared_ptr<ICameraPayload>& tgtCam)
{
    MatchZoomResult result {};
    if (!srcCam || !tgtCam) {
        return result;
    }

    const auto srcTelem = srcCam->currentTelemetry();
    const double srcHfovDeg = std::max(0.1, srcTelem.horizontalFovDeg);

    // Analytical model of camera zoom vs HFOV:
    // HFOV(zoom) = 2.0 * atan(tan(wideHfov / 2.0) / zoomFactor)
    // where zoomFactor = 1.0 + normalizedZoom * (kMaxOpticalZoom - 1.0)
    const double wideHfovRad = degToRad(kWideHfovDeg);
    const double halfWideTan = std::tan(wideHfovRad / 2.0);

    const double srcHfovRad = degToRad(srcHfovDeg);
    const double halfSrcTan = std::tan(srcHfovRad / 2.0);

    if (halfSrcTan <= kEpsilon) {
        result.targetOpticalZoom01 = 1.0;
        result.digitalCropFactor = 1.0;
        result.achievedHfovDeg = srcHfovDeg;
        return result;
    }

    // Desired optical zoom factor to match HFOV
    const double requiredOpticalZoom = halfWideTan / halfSrcTan;

    if (requiredOpticalZoom <= 1.0) {
        // Wider than or equal to wide limit
        result.targetOpticalZoom01 = 0.0;
        result.digitalCropFactor = 1.0;
        result.achievedHfovDeg = kWideHfovDeg;
        result.fovDeltaDeg = kWideHfovDeg - srcHfovDeg;
        result.isClamped = (requiredOpticalZoom < 1.0 - kEpsilon);
    } else if (requiredOpticalZoom <= kMaxOpticalZoom) {
        // Perfectly within optical zoom range
        result.targetOpticalZoom01 = (requiredOpticalZoom - 1.0) / (kMaxOpticalZoom - 1.0);
        result.targetOpticalZoom01 = std::clamp(result.targetOpticalZoom01, 0.0, 1.0);
        result.digitalCropFactor = 1.0;
        result.achievedHfovDeg = srcHfovDeg;
        result.fovDeltaDeg = 0.0;
        result.isClamped = false;
    } else {
        // Beyond maximum optical telephoto limit: apply digital zoom crop factor
        result.targetOpticalZoom01 = 1.0; // Max optical telephoto
        const double teleHfovRad = 2.0 * std::atan(halfWideTan / kMaxOpticalZoom);

        // Digital crop factor = tan(teleHfov/2) / tan(srcHfov/2)
        const double digitalFactor = std::tan(teleHfovRad / 2.0) / halfSrcTan;
        result.digitalCropFactor = std::max(1.0, digitalFactor);
        result.achievedHfovDeg = srcHfovDeg;
        result.fovDeltaDeg = 0.0;
        result.isClamped = false;
    }

    return result;
}

bool SensorFusionManager::applyMatchZoom(OpticalChannel sourceChannel, OpticalChannel targetChannel)
{
    std::shared_ptr<ICameraPayload> srcCam;
    std::shared_ptr<ICameraPayload> tgtCam;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        srcCam = getCameraForChannelLocked(sourceChannel);
        tgtCam = getCameraForChannelLocked(targetChannel);
    }

    if (!srcCam || !tgtCam) {
        return false;
    }

    const auto res = computeMatchZoomInternal(srcCam, tgtCam);
    tgtCam->setZoomNormalized(res.targetOpticalZoom01);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentDigitalCropFactor = res.digitalCropFactor;
    return true;
}

double SensorFusionManager::currentDigitalCropFactor() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentDigitalCropFactor;
}

bool SensorFusionManager::setPalette(OpticalPalette palette)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_palette = palette;

    if (m_secondaryCam) {
        switch (palette) {
        case OpticalPalette::WhiteHot:
            m_secondaryCam->setThermalPolarity(ThermalPolarity::WhiteHot);
            break;
        case OpticalPalette::BlackHot:
            m_secondaryCam->setThermalPolarity(ThermalPolarity::BlackHot);
            break;
        case OpticalPalette::Ironbow:
        case OpticalPalette::Sepia:
        case OpticalPalette::Arctic:
            m_secondaryCam->setThermalPolarity(ThermalPolarity::FusionColor);
            break;
        case OpticalPalette::Rainbow:
            m_secondaryCam->setThermalPolarity(ThermalPolarity::Rainbow);
            break;
        default:
            break;
        }
    }

    if (m_primaryCam) {
        if (palette == OpticalPalette::HazePenetration) {
            m_primaryCam->setDefog(true);
        } else if (palette == OpticalPalette::DaylightMonochrome) {
            m_primaryCam->setDayNightIcr(true);
        } else if (palette == OpticalPalette::DaylightColor) {
            m_primaryCam->setDayNightIcr(false);
            m_primaryCam->setDefog(false);
        }
    }

    return true;
}

OpticalPalette SensorFusionManager::palette() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_palette;
}

void SensorFusionManager::setIsothermHighlight(bool enabled, double minTempC, double maxTempC) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isothermEnabled = enabled;
    m_isothermMinTempC = minTempC;
    m_isothermMaxTempC = maxTempC;
}

bool SensorFusionManager::isIsothermEnabled() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isothermEnabled;
}

void SensorFusionManager::getIsothermRange(double& minTempC, double& maxTempC) const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    minTempC = m_isothermMinTempC;
    maxTempC = m_isothermMaxTempC;
}

void SensorFusionManager::setAutoSwitchConfig(const AutoSwitchConfig& config) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoConfig = config;
    m_hysteresisTimerSec = 0.0;
    m_cooldownTimerSec = 0.0;
}

AutoSwitchConfig SensorFusionManager::autoSwitchConfig() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_autoConfig;
}

void SensorFusionManager::updateSceneMetrics(const EnvironmentalSceneMetrics& metrics)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sceneMetrics = metrics;
}

EnvironmentalSceneMetrics SensorFusionManager::sceneMetrics() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sceneMetrics;
}

void SensorFusionManager::setFusionLayoutMode(FusionLayoutMode mode) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_layoutMode = mode;
}

FusionLayoutMode SensorFusionManager::fusionLayoutMode() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_layoutMode;
}

void SensorFusionManager::setAlphaBlendWeight(double visibleWeight01) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_alphaBlendWeight = std::clamp(visibleWeight01, 0.0, 1.0);
}

double SensorFusionManager::alphaBlendWeight() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_alphaBlendWeight;
}

void SensorFusionManager::setPipConfig(const PipWindowConfig& config) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pipConfig = config;
}

PipWindowConfig SensorFusionManager::pipConfig() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pipConfig;
}

void SensorFusionManager::update(double dtSeconds)
{
    std::shared_ptr<ICameraPayload> srcCam;
    std::shared_ptr<ICameraPayload> tgtCam;
    OpticalChannel prevChannel {};
    OpticalChannel newChannel {};
    std::string switchReason;
    ChannelSwitchCallback callback;
    bool triggered = false;
    bool matchZoomNeeded = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_autoConfig.enabled) {
            return;
        }

        if (m_cooldownTimerSec > 0.0) {
            m_cooldownTimerSec = std::max(0.0, m_cooldownTimerSec - dtSeconds);
            return;
        }

        if (m_activeChannel == OpticalChannel::Primary) {
            // Check for transition to Thermal (night / low contrast / smoke)
            const bool isLowLight = (m_sceneMetrics.ambientIlluminanceLux < m_autoConfig.lowLightThresholdLux);
            const bool isLowContrast = (m_sceneMetrics.sceneContrastRatio < m_autoConfig.lowContrastThreshold ||
                                       m_sceneMetrics.isObscuredBySmokeHaze);

            if (isLowLight || isLowContrast) {
                if (m_pendingSwitchChannel != OpticalChannel::Secondary) {
                    m_pendingSwitchChannel = OpticalChannel::Secondary;
                    m_hysteresisTimerSec = 0.0;
                }

                m_hysteresisTimerSec += dtSeconds;
                if (m_hysteresisTimerSec >= m_autoConfig.hysteresisTimeSec) {
                    if (m_secondaryCam) {
                        prevChannel = m_activeChannel;
                        newChannel = OpticalChannel::Secondary;
                        srcCam = m_primaryCam;
                        tgtCam = m_secondaryCam;
                        matchZoomNeeded = m_matchZoomEnabled;

                        m_activeChannel = OpticalChannel::Secondary;
                        m_cooldownTimerSec = m_autoConfig.cooldownTimeSec;
                        m_hysteresisTimerSec = 0.0;
                        triggered = true;
                        switchReason = "Environmental low-light/low-contrast auto-switch";
                        callback = m_switchCallback;
                    }
                }
            } else {
                m_hysteresisTimerSec = 0.0;
                m_pendingSwitchChannel = OpticalChannel::Primary;
            }
        } else if (m_activeChannel == OpticalChannel::Secondary) {
            // Check for return to Daylight Visible
            const bool isDaylightRestored = (m_sceneMetrics.ambientIlluminanceLux >= m_autoConfig.daylightReturnThresholdLux &&
                                            m_sceneMetrics.sceneContrastRatio >= m_autoConfig.lowContrastThreshold &&
                                            !m_sceneMetrics.isObscuredBySmokeHaze);

            if (isDaylightRestored) {
                if (m_pendingSwitchChannel != OpticalChannel::Primary) {
                    m_pendingSwitchChannel = OpticalChannel::Primary;
                    m_hysteresisTimerSec = 0.0;
                }

                m_hysteresisTimerSec += dtSeconds;
                if (m_hysteresisTimerSec >= m_autoConfig.hysteresisTimeSec) {
                    if (m_primaryCam) {
                        prevChannel = m_activeChannel;
                        newChannel = OpticalChannel::Primary;
                        srcCam = m_secondaryCam;
                        tgtCam = m_primaryCam;
                        matchZoomNeeded = m_matchZoomEnabled;

                        m_activeChannel = OpticalChannel::Primary;
                        m_cooldownTimerSec = m_autoConfig.cooldownTimeSec;
                        m_hysteresisTimerSec = 0.0;
                        triggered = true;
                        switchReason = "Environmental daylight restored auto-switch";
                        callback = m_switchCallback;
                    }
                }
            } else {
                m_hysteresisTimerSec = 0.0;
                m_pendingSwitchChannel = OpticalChannel::Secondary;
            }
        }
    }

    if (triggered) {
        if (matchZoomNeeded && srcCam && tgtCam) {
            const auto res = computeMatchZoomInternal(srcCam, tgtCam);
            tgtCam->setZoomNormalized(res.targetOpticalZoom01);
            std::lock_guard<std::mutex> lock(m_mutex);
            m_currentDigitalCropFactor = res.digitalCropFactor;
        }
        if (callback) {
            callback(prevChannel, newChannel, switchReason);
        }
    }
}

void SensorFusionManager::setChannelSwitchCallback(ChannelSwitchCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_switchCallback = std::move(callback);
}

} // namespace PayloadHal
