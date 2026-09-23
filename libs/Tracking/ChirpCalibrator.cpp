/// @file ChirpCalibrator.cpp
/// @brief Implementation of active swept-sine PTZ plant identification orchestrator.

#include "ChirpCalibrator.h"

#include <algorithm>
#include <cmath>

namespace Tracking {

ChirpCalibrator::ChirpCalibrator(CommandCallback cmdCb, ChirpConfig config)
    : m_cmdCb(std::move(cmdCb))
    , m_identifier(std::move(config))
{
}

ChirpCalibrator::~ChirpCalibrator()
{
    cancel();
}

void ChirpCalibrator::setCommandCallback(CommandCallback cmdCb)
{
    std::scoped_lock lock(m_mutex);
    m_cmdCb = std::move(cmdCb);
}

double ChirpCalibrator::getMonotonicNowSeconds() noexcept
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool ChirpCalibrator::start(CalibrationAxis axis, int maxSpeed, double nowSec)
{
    std::scoped_lock lock(m_mutex);

    if (m_state != ChirpCalibratorState::Idle && m_state != ChirpCalibratorState::Completed
        && m_state != ChirpCalibratorState::Failed) {
        return false;
    }

    m_axis = axis;
    m_maxSpeed = std::clamp(maxSpeed, 1, 63);
    const double currentSec = (nowSec > 0.0) ? nowSec : getMonotonicNowSeconds();
    m_phaseStartTime = currentSec;
    m_sweepStartTime = currentSec;
    m_progress = 0.0;
    m_lastCommandVal = 0.0;
    m_result = PlantIdentificationResult {};

    m_identifier.reset();
    dispatchStopLocked();
    m_state = ChirpCalibratorState::PreSettle;
    return true;
}

void ChirpCalibrator::cancel()
{
    std::scoped_lock lock(m_mutex);
    if (m_state != ChirpCalibratorState::Idle && m_state != ChirpCalibratorState::Completed
        && m_state != ChirpCalibratorState::Failed) {
        dispatchStopLocked();
        m_state = ChirpCalibratorState::Idle;
        m_progress = 0.0;
        m_lastCommandVal = 0.0;
    }
}

void ChirpCalibrator::dispatchStopLocked()
{
    if (m_cmdCb) {
        m_cmdCb(0, 0, 0, 0);
    }
}

void ChirpCalibrator::dispatchCommandLocked(int panDir, int panSpeed, int tiltDir, int tiltSpeed)
{
    if (m_cmdCb) {
        m_cmdCb(panDir, panSpeed, tiltDir, tiltSpeed);
    }
}

void ChirpCalibrator::update(double nowSec)
{
    std::scoped_lock lock(m_mutex);

    if (m_state == ChirpCalibratorState::Idle || m_state == ChirpCalibratorState::Completed
        || m_state == ChirpCalibratorState::Failed) {
        return;
    }

    const double dt = nowSec - m_phaseStartTime;

    switch (m_state) {
    case ChirpCalibratorState::PreSettle:
        dispatchStopLocked();
        if (dt >= PRE_SETTLE_DURATION) {
            m_state = ChirpCalibratorState::Sweeping;
            m_phaseStartTime = nowSec;
            m_sweepStartTime = nowSec;
        }
        break;

    case ChirpCalibratorState::Sweeping: {
        const double sweepElapsed = nowSec - m_sweepStartTime;
        const double duration = m_identifier.getConfig().durationSec;
        m_progress = std::clamp(sweepElapsed / duration, 0.0, 1.0);

        if (sweepElapsed >= duration) {
            dispatchStopLocked();
            m_lastCommandVal = 0.0;
            m_state = ChirpCalibratorState::PostSettle;
            m_phaseStartTime = nowSec;
        } else {
            const double normCmd = m_identifier.generateChirpSample(sweepElapsed, 1.0);
            m_lastCommandVal = normCmd;

            // Map continuous normalized command [-1.0, 1.0] into discrete Pelco-D direction & speed
            const int dir = (normCmd > 0.02) ? 1 : ((normCmd < -0.02) ? -1 : 0);
            int speed = static_cast<int>(std::round(std::abs(normCmd) * static_cast<double>(m_maxSpeed)));
            speed = std::clamp(speed, 0, m_maxSpeed);
            if (dir == 0) {
                speed = 0;
            }

            if (m_axis == CalibrationAxis::Pan) {
                dispatchCommandLocked(dir, speed, 0, 0);
            } else {
                dispatchCommandLocked(0, 0, dir, speed);
            }
        }
        break;
    }

    case ChirpCalibratorState::PostSettle:
        dispatchStopLocked();
        if (dt >= POST_SETTLE_DURATION) {
            m_state = ChirpCalibratorState::Analyzing;
            m_result = m_identifier.analyze();
            m_state = m_result.success ? ChirpCalibratorState::Completed : ChirpCalibratorState::Failed;
            m_progress = 1.0;
        }
        break;

    case ChirpCalibratorState::Analyzing:
    case ChirpCalibratorState::Idle:
    case ChirpCalibratorState::Completed:
    case ChirpCalibratorState::Failed:
        break;
    }
}

void ChirpCalibrator::ingestVisualMotion(double /*nowSec*/, double visualVelocityX, double visualVelocityY)
{
    std::scoped_lock lock(m_mutex);

    if (m_state != ChirpCalibratorState::Sweeping && m_state != ChirpCalibratorState::PostSettle) {
        return;
    }

    const double response = (m_axis == CalibrationAxis::Pan) ? visualVelocityX : visualVelocityY;
    m_identifier.addSample(m_lastCommandVal, response);
}

bool ChirpCalibrator::isRunning() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return (m_state == ChirpCalibratorState::PreSettle || m_state == ChirpCalibratorState::Sweeping
        || m_state == ChirpCalibratorState::PostSettle || m_state == ChirpCalibratorState::Analyzing);
}

ChirpCalibratorState ChirpCalibrator::getState() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_state;
}

double ChirpCalibrator::getProgress() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_progress;
}

CalibrationAxis ChirpCalibrator::getAxis() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_axis;
}

PlantIdentificationResult ChirpCalibrator::getResult() const
{
    std::scoped_lock lock(m_mutex);
    return m_result;
}

PlantIdentifier& ChirpCalibrator::getIdentifier() noexcept
{
    return m_identifier;
}

const PlantIdentifier& ChirpCalibrator::getIdentifier() const noexcept
{
    return m_identifier;
}

} // namespace Tracking
