/// @file MotionGatedAfManager.cpp
#include "MotionGatedAfManager.h"

#include <cmath>
#include <cstdint>

namespace LensControl {

MotionGatedAfManager::MotionGatedAfManager(const Config& config) noexcept
    : m_config { config }
    , m_state { AfGatingState::Active }
    , m_compositeRate { 0.0F }
    , m_holdoffTimerSec { 0.0F }
{
}

void MotionGatedAfManager::updateRates(
    AngularVelocityDegPerSec panRate, AngularVelocityDegPerSec tiltRate, float dtSec) noexcept
{
    const float dt = dtSec > 0.0F ? dtSec : 0.0F;
    const float p = panRate.value();
    const float t = tiltRate.value();
    const float rawRate = std::sqrt((p * p) + (t * t));

    const float deadband = m_config.deadband.value();
    const float filteredRate = rawRate < deadband ? 0.0F : rawRate;
    m_compositeRate = AngularVelocityDegPerSec { filteredRate };

    const float suppressThresh = m_config.suppressThreshold.value();
    const float releaseThresh = m_config.releaseThreshold.value();
    const float targetHoldoff = static_cast<float>(m_config.holdoffMs) * 0.001F;

    switch (m_state) {
    case AfGatingState::Active:
        if (filteredRate >= suppressThresh) {
            m_state = AfGatingState::Suppressed;
            m_holdoffTimerSec = 0.0F;
        }
        break;

    case AfGatingState::Suppressed:
        if (filteredRate < releaseThresh) {
            m_state = AfGatingState::SettlingHoldoff;
            m_holdoffTimerSec = dt;
            if (m_holdoffTimerSec >= targetHoldoff) {
                m_state = AfGatingState::Active;
                m_holdoffTimerSec = 0.0F;
            }
        }
        break;

    case AfGatingState::SettlingHoldoff:
        if (filteredRate >= releaseThresh) {
            m_state = AfGatingState::Suppressed;
            m_holdoffTimerSec = 0.0F;
        } else {
            m_holdoffTimerSec += dt;
            if (m_holdoffTimerSec >= targetHoldoff) {
                m_state = AfGatingState::Active;
                m_holdoffTimerSec = 0.0F;
            }
        }
        break;

    default:
        m_state = AfGatingState::Active;
        break;
    }
}

void MotionGatedAfManager::reset() noexcept
{
    m_state = AfGatingState::Active;
    m_compositeRate = AngularVelocityDegPerSec { 0.0F };
    m_holdoffTimerSec = 0.0F;
}

AfGatingState MotionGatedAfManager::gatingState() const noexcept
{
    return m_state;
}

bool MotionGatedAfManager::isSuppressed() const noexcept
{
    return m_state != AfGatingState::Active;
}

AngularVelocityDegPerSec MotionGatedAfManager::compositeRate() const noexcept
{
    return m_compositeRate;
}

void MotionGatedAfManager::setConfig(const Config& config) noexcept
{
    m_config = config;
}

MotionGatedAfManager::Config MotionGatedAfManager::config() const noexcept
{
    return m_config;
}

} // namespace LensControl
