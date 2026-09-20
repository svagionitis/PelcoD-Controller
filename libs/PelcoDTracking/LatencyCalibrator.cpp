/// @file LatencyCalibrator.cpp
/// @brief Implementation of active doublet pulse latency calibration controller.

#include "LatencyCalibrator.h"

#include <algorithm>
#include <chrono>

namespace PelcoD {

LatencyCalibrator::LatencyCalibrator(CommandCallback cmdCb, LatencyEstimatorConfig config)
    : m_cmdCb(std::move(cmdCb))
    , m_estimator(config)
{
    // Configure default polarity to Negative (command + pan -> negative optical flow)
    LatencyEstimatorConfig activeConfig = m_estimator.getConfig();
    activeConfig.polarity = PeakPolarity::Negative;
    activeConfig.confidenceThreshold = 0.5;
    activeConfig.smoothingAlpha = 1.0; // Immediate latch for calibration
    activeConfig.bufferCapacity = 256U;
    activeConfig.sampleRateHz = 50.0;
    activeConfig.minLagMs = 0.0;
    activeConfig.maxLagMs = 400.0;
    m_estimator.setConfig(activeConfig);
}

LatencyCalibrator::~LatencyCalibrator()
{
    cancel();
}

void LatencyCalibrator::setCommandCallback(CommandCallback cmdCb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cmdCb = std::move(cmdCb);
}

double LatencyCalibrator::getMonotonicNowSeconds() noexcept
{
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now.time_since_epoch()).count();
}

bool LatencyCalibrator::start(int panPulseSpeed, int tiltPulseSpeed, double nowSec)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != CalibrationState::Idle && m_state != CalibrationState::Completed
        && m_state != CalibrationState::Failed) {
        return false;
    }

    if (nowSec <= 0.0) {
        nowSec = getMonotonicNowSeconds();
    }

    m_pulsePanSpeed = std::clamp(panPulseSpeed, 1, 63);
    m_pulseTiltSpeed = std::clamp(tiltPulseSpeed, 0, 63);
    m_phaseStartTime = nowSec;
    m_state = CalibrationState::PreSettle;
    m_result = CalibrationResult {};
    m_lastCommandValue = 0.0;

    m_estimator.reset();
    dispatchStopLocked();
    return true;
}

void LatencyCalibrator::cancel()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != CalibrationState::Idle) {
        m_state = CalibrationState::Idle;
        dispatchStopLocked();
    }
}

void LatencyCalibrator::dispatchStopLocked()
{
    m_lastCommandValue = 0.0;
    if (m_cmdCb) {
        m_cmdCb(0, 0, 0, 0);
    }
}

void LatencyCalibrator::dispatchCommandLocked(int panDir, int panSpeed, int tiltDir, int tiltSpeed)
{
    m_lastCommandValue = static_cast<double>(panDir * panSpeed);
    if (m_cmdCb) {
        m_cmdCb(panDir, panSpeed, tiltDir, tiltSpeed);
    }
}

void LatencyCalibrator::update(double nowSec)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == CalibrationState::Idle || m_state == CalibrationState::Completed
        || m_state == CalibrationState::Failed) {
        return;
    }

    if (nowSec <= 0.0) {
        nowSec = getMonotonicNowSeconds();
    }

    const double elapsedInPhase = nowSec - m_phaseStartTime;

    switch (m_state) {
    case CalibrationState::PreSettle:
        if (elapsedInPhase >= PRE_SETTLE_DURATION) {
            m_state = CalibrationState::PositivePulse;
            m_phaseStartTime = nowSec;
            // Command Pan Right (+1)
            dispatchCommandLocked(1, m_pulsePanSpeed, (m_pulseTiltSpeed > 0 ? 1 : 0), m_pulseTiltSpeed);
        }
        break;

    case CalibrationState::PositivePulse:
        if (elapsedInPhase >= PULSE_DURATION) {
            m_state = CalibrationState::InterDwell;
            m_phaseStartTime = nowSec;
            dispatchStopLocked();
        }
        break;

    case CalibrationState::InterDwell:
        if (elapsedInPhase >= INTER_DWELL_DURATION) {
            m_state = CalibrationState::NegativePulse;
            m_phaseStartTime = nowSec;
            // Command Pan Left (-1)
            dispatchCommandLocked(-1, m_pulsePanSpeed, (m_pulseTiltSpeed > 0 ? -1 : 0), m_pulseTiltSpeed);
        }
        break;

    case CalibrationState::NegativePulse:
        if (elapsedInPhase >= PULSE_DURATION) {
            m_state = CalibrationState::PostCollect;
            m_phaseStartTime = nowSec;
            dispatchStopLocked();
        }
        break;

    case CalibrationState::PostCollect:
        if (elapsedInPhase >= POST_COLLECT_DURATION) {
            dispatchStopLocked();
            m_estimator.update();

            if (m_estimator.isConfident()) {
                m_state = CalibrationState::Completed;
                m_result.success = true;
                m_result.latencyMs = m_estimator.getEstimatedLatencyMs();
                m_result.latencySeconds = m_estimator.getEstimatedLatencySeconds();
                m_result.correlation = m_estimator.getPeakCorrelation();
                m_result.message = "Calibration Successful";
            } else {
                m_state = CalibrationState::Failed;
                m_result.success = false;
                m_result.latencyMs = 0.0;
                m_result.latencySeconds = 0.0;
                m_result.correlation = m_estimator.getPeakCorrelation();
                m_result.message = "Low correlation / insufficient visual motion";
            }
        }
        break;

    default:
        break;
    }

    // Record reference sample at current timestamp reflecting active command
    m_estimator.addTimestampedReference(nowSec, m_lastCommandValue);
}

void LatencyCalibrator::ingestVisualMotion(double nowSec, double visualVelocityX, double /*visualVelocityY*/)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != CalibrationState::Idle) {
        if (nowSec <= 0.0) {
            nowSec = getMonotonicNowSeconds();
        }
        m_estimator.addTimestampedResponse(nowSec, visualVelocityX);
    }
}

bool LatencyCalibrator::isRunning() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return (m_state != CalibrationState::Idle && m_state != CalibrationState::Completed
        && m_state != CalibrationState::Failed);
}

CalibrationState LatencyCalibrator::getState() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

CalibrationResult LatencyCalibrator::getResult() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_result;
}

LatencyEstimator& LatencyCalibrator::getEstimator() noexcept
{
    return m_estimator;
}

const LatencyEstimator& LatencyCalibrator::getEstimator() const noexcept
{
    return m_estimator;
}

} // namespace PelcoD
