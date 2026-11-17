/// @file ParfocalFeedforwardController.cpp
#include "ParfocalFeedforwardController.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "LensTypes.h"
#include "MotionControlTypes.h"
#include "ZoomTrackFocusEngine.h"

namespace LensControl {

ParfocalFeedforwardController::ParfocalFeedforwardController(const FeedforwardConfig& config) noexcept
    : m_config { config }
{
}

void ParfocalFeedforwardController::reset() noexcept
{
    m_prevZoom = 0.0F;
    m_filteredZoomVel = 0.0F;
    m_lastFfTerm = 0.0F;
    m_lastFbTerm = 0.0F;
    m_isInitialized = false;
}

void ParfocalFeedforwardController::setConfig(const FeedforwardConfig& config) noexcept
{
    m_config = config;
}

const FeedforwardConfig& ParfocalFeedforwardController::config() const noexcept
{
    return m_config;
}

float ParfocalFeedforwardController::update(MotorPosition currentZoom, MotorPosition currentFocus,
    const ZoomTrackFocusEngine& trackEngine, DeltaTimeSec dt) noexcept
{
    const float curZ = static_cast<float>(currentZoom.value());

    if (!m_isInitialized) {
        m_prevZoom = curZ;
        m_filteredZoomVel = 0.0F;
        m_lastFfTerm = 0.0F;
        m_lastFbTerm = 0.0F;
        m_isInitialized = true;
        return 0.0F;
    }

    const float deltaZ = curZ - m_prevZoom;
    m_prevZoom = curZ;

    const float dtVal = dt.value();
    const float rawVel = deltaZ / dtVal;

    const float alpha = std::clamp(m_config.filterAlpha, 0.0F, 1.0F);
    m_filteredZoomVel = (alpha * rawVel) + ((1.0F - alpha) * m_filteredZoomVel);

    float activeZoomVel = m_filteredZoomVel;
    if (std::abs(activeZoomVel) < m_config.deadbandZoomVelocity) {
        activeZoomVel = 0.0F;
    }

    const float dfDz = trackEngine.evaluateDerivative(currentZoom);
    const float targetF = static_cast<float>(trackEngine.evaluateFocus(currentZoom).value());
    const float curF = static_cast<float>(currentFocus.value());
    const float errF = targetF - curF;

    m_lastFfTerm = dfDz * activeZoomVel;
    m_lastFbTerm = m_config.kp * errF;

    const float totalCmd = m_lastFfTerm + m_lastFbTerm;
    const float maxVel = (m_config.maxFocusVelocity > 0.0F) ? m_config.maxFocusVelocity : 255.0F;

    return std::clamp(totalCmd, -maxVel, maxVel);
}

VelocityTicksPerSec ParfocalFeedforwardController::zoomVelocity() const noexcept
{
    return VelocityTicksPerSec { m_filteredZoomVel };
}

float ParfocalFeedforwardController::lastFeedforwardTerm() const noexcept
{
    return m_lastFfTerm;
}

float ParfocalFeedforwardController::lastFeedbackTerm() const noexcept
{
    return m_lastFbTerm;
}

} // namespace LensControl
