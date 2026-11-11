/// @file AutoFocusEngine.cpp
#include "AutoFocusEngine.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <cmath>

#include "AutoFocusTypes.h"
#include "FocusEvaluator.h"
#include "ILensDriver.h"
#include "LensTypes.h"
#include "PeakFitter.h"

namespace LensControl {

AutoFocusEngine::AutoFocusEngine(ILensDriver& driver, FocusEvaluator& evaluator) noexcept
    : m_driver { driver }
    , m_evaluator { evaluator }
{
}

void AutoFocusEngine::trigger(AutoFocusMethod method, const AutoFocusConfig& config) noexcept
{
    m_method = method;
    m_config = config;
    m_sampleCount = 0U;
    m_maxScoreSeen = 0U;
    m_prevScore = 0U;
    m_declines = 0U;
    m_sweepDirectionForward = true;
    m_seekReversed = false;
    m_backlashApproached = false;
    m_newScoreAvailable = false;
    m_lastPeak = PeakFitResult { };

    if (m_method == AutoFocusMethod::DctFrequency) {
        m_evaluator.setMetricType(FocusMetricType::DctHighFrequency);
    }

    if (m_method == AutoFocusMethod::BuiltInDriver) {
        if (m_driver.hasBuiltinAutofocus()) {
            (void)m_driver.triggerBuiltinAutofocus();
            m_state = AutoFocusState::CoarseSweep;
            return;
        }
        m_method = AutoFocusMethod::ScanThreePhase;
    }

    if (m_method == AutoFocusMethod::GoldenSection) {
        m_gssA = m_config.searchMin;
        m_gssB = m_config.searchMax;
        constexpr float phi = 0.6180339887F;
        const auto range = static_cast<float>(m_gssB - m_gssA);
        m_gssX1 = m_gssB - static_cast<std::uint16_t>(std::round(phi * range));
        m_gssX2 = m_gssA + static_cast<std::uint16_t>(std::round(phi * range));
        m_gssScore1 = 0U;
        m_gssScore2 = 0U;
        m_gssEvalX1 = true;
        m_gssInitialized = false;

        m_driver.setFocusPosition(MotorPosition { m_gssX1 }, m_config.coarseSpeed);
        m_state = AutoFocusState::CoarseSweep;
        return;
    }

    if (m_method == AutoFocusMethod::AdaptiveMomentum) {
        m_momentumVelocity = static_cast<float>(m_config.midStep);
        m_momentumLastPos = static_cast<std::int32_t>(m_driver.currentFocus().value());
        m_state = AutoFocusState::CoarseSweep;
        return;
    }

    m_state = AutoFocusState::CoarseSweep;
    const MotorPosition curPos = m_driver.currentFocus();
    if (curPos.value() >= m_config.searchMax) {
        m_sweepDirectionForward = false;
    }
}

void AutoFocusEngine::abort() noexcept
{
    m_state = AutoFocusState::Idle;
    m_driver.stopMotion();
}

void AutoFocusEngine::processFrame(const std::uint8_t* luma, std::size_t width, std::size_t height, std::size_t stride,
    std::uint64_t timestampNs, std::uint32_t frameId) noexcept
{
    if (luma == nullptr || width == 0U || height == 0U) {
        return;
    }

    m_latestScore = m_evaluator.evaluate(luma, width, height, stride, timestampNs, frameId);
    m_newScoreAvailable = true;

    // Continuous Autofocus (CAF) background defocus trigger
    if (m_cafEnabled && !isRunning() && m_cafPeakScore > 0U) {
        const auto threshold
            = static_cast<std::uint32_t>(static_cast<float>(m_cafPeakScore) * (1.0F - m_config.cafDefocusFrac));
        if (m_latestScore.metric < threshold) {
            trigger(m_method, m_config);
        }
    }
}

void AutoFocusEngine::step(MotorPosition currentFocusPos, std::uint64_t nowNs) noexcept
{
    m_lastFeedbackPos = currentFocusPos;

    // Timeout guard
    if (isRunning()) {
        if (m_startTimeNs == 0U) {
            m_startTimeNs = nowNs;
        } else if (nowNs > m_startTimeNs && m_config.timeoutMs > 0U) {
            const std::uint64_t elapsedMs = (nowNs - m_startTimeNs) / 1000000ULL;
            if (elapsedMs >= static_cast<std::uint64_t>(m_config.timeoutMs)) {
                m_state = AutoFocusState::Failed;
                m_driver.stopMotion();
                return;
            }
        }
    } else {
        m_startTimeNs = 0U;
    }

    // Delay ring buffer update for video latency compensation
    m_delayRing[m_delayHead] = currentFocusPos;
    m_delayHead = (m_delayHead + 1U) % kDelayRingSize;

    // SlewToPeak arrival check
    if (m_state == AutoFocusState::SlewToPeak) {
        if (m_backlashApproached) {
            const std::uint16_t underShoot = m_targetPos.value() > m_config.backlashSteps
                ? static_cast<std::uint16_t>(m_targetPos.value() - m_config.backlashSteps)
                : 0U;
            if (currentFocusPos.value() <= underShoot) {
                m_backlashApproached = false;
                m_driver.setFocusPosition(m_targetPos, m_config.fineSpeed);
            }
            return;
        }

        if (currentFocusPos == m_targetPos) {
            m_convergedPos = m_targetPos;
            m_cafPeakScore = m_maxScoreSeen;
            m_state = AutoFocusState::PeakConfirmed;
            m_driver.stopMotion();
        }
        return;
    }

    if (!m_newScoreAvailable || !isRunning()) {
        return;
    }

    // Determine latency-compensated position
    const std::size_t delayOffset
        = std::min(static_cast<std::size_t>(m_config.frameOffsetMult8 / 8U), kDelayRingSize - 1U);
    const std::size_t sampleRingIdx = (m_delayHead + kDelayRingSize - 1U - delayOffset) % kDelayRingSize;
    const MotorPosition samplePos = m_delayRing[sampleRingIdx];

    recordSample(samplePos, m_latestScore, nowNs);
    m_newScoreAvailable = false;

    switch (m_method) {
    case AutoFocusMethod::ScanThreePhase:
    case AutoFocusMethod::DctFrequency:
        stepScan(currentFocusPos);
        break;
    case AutoFocusMethod::SeekAdaptive:
        stepSeek(currentFocusPos);
        break;
    case AutoFocusMethod::GoldenSection:
        stepGss(currentFocusPos);
        break;
    case AutoFocusMethod::AdaptiveMomentum:
        stepMomentum(currentFocusPos);
        break;
    case AutoFocusMethod::BuiltInDriver:
        m_convergedPos = currentFocusPos;
        m_state = AutoFocusState::PeakConfirmed;
        break;
    }
}

void AutoFocusEngine::stepScan(MotorPosition pos) noexcept
{
    const std::uint32_t score = m_latestScore.metric;

    if (m_state == AutoFocusState::CoarseSweep) {
        if (m_maxScoreSeen == 0U) {
            m_maxScoreSeen = score;
        } else if (score > m_maxScoreSeen) {
            if (m_prevScore > 0U) {
                const float deriv = static_cast<float>(score - m_prevScore) / static_cast<float>(m_prevScore);
                if (deriv >= m_config.fineThresh) {
                    m_state = AutoFocusState::MidSweep;
                }
            }
            m_maxScoreSeen = score;
        } else if (m_prevScore > 0U && score < m_prevScore) {
            m_state = AutoFocusState::FineSweep;
            ++m_declines;
        }
    } else if (m_state == AutoFocusState::MidSweep) {
        if (score > m_maxScoreSeen) {
            if (m_prevScore > 0U) {
                const float deriv = static_cast<float>(score - m_prevScore) / static_cast<float>(m_prevScore);
                if (deriv >= m_config.fineThresh * 1.5F) {
                    m_state = AutoFocusState::FineSweep;
                }
            }
            m_maxScoreSeen = score;
        } else if (m_prevScore > 0U && score < m_prevScore) {
            m_state = AutoFocusState::FineSweep;
            ++m_declines;
        }
    } else if (m_state == AutoFocusState::FineSweep) {
        if (m_prevScore > 0U && score < m_prevScore) {
            ++m_declines;
            if (m_declines >= m_config.cdownMax) {
                finishConvergence(pos);
                return;
            }
        } else {
            m_declines = 0U;
            if (score > m_maxScoreSeen) {
                m_maxScoreSeen = score;
            }
        }
    }

    std::uint16_t stepSize = m_config.coarseStep;
    MotorSpeed speed = m_config.coarseSpeed;
    if (m_state == AutoFocusState::MidSweep) {
        stepSize = m_config.midStep;
        speed = m_config.midSpeed;
    } else if (m_state == AutoFocusState::FineSweep) {
        stepSize = m_config.fineStep;
        speed = m_config.fineSpeed;
    }

    std::uint16_t nextPos = pos.value();
    if (m_sweepDirectionForward) {
        if (static_cast<std::uint32_t>(pos.value()) + stepSize <= m_config.searchMax) {
            nextPos = static_cast<std::uint16_t>(pos.value() + stepSize);
        } else {
            if (m_sampleCount >= 3U) {
                finishConvergence(pos);
                return;
            }
            m_sweepDirectionForward = false;
            nextPos = pos.value() > stepSize ? static_cast<std::uint16_t>(pos.value() - stepSize) : 0U;
        }
    } else {
        if (pos.value() >= m_config.searchMin + stepSize) {
            nextPos = static_cast<std::uint16_t>(pos.value() - stepSize);
        } else {
            if (m_sampleCount >= 3U) {
                finishConvergence(pos);
                return;
            }
            m_state = AutoFocusState::Failed;
            m_driver.stopMotion();
            return;
        }
    }

    m_prevScore = score;
    m_driver.setFocusPosition(MotorPosition { nextPos }, speed);
}

void AutoFocusEngine::stepSeek(MotorPosition pos) noexcept
{
    const std::uint32_t score = m_latestScore.metric;

    if (m_prevScore > 0U && score < m_prevScore) {
        if (!m_seekReversed && m_sampleCount <= 2U) {
            m_sweepDirectionForward = !m_sweepDirectionForward;
            m_seekReversed = true;
            m_declines = 0U;
        } else {
            ++m_declines;
            if (m_declines >= m_config.cdownMax) {
                finishConvergence(pos);
                return;
            }
        }
    } else {
        m_declines = 0U;
        if (score > m_maxScoreSeen) {
            m_maxScoreSeen = score;
        }
    }

    const auto scaledStep = static_cast<std::uint16_t>(
        std::max(1.0F, std::round(static_cast<float>(m_config.midStep) * m_config.rateAdjust)));
    const MotorSpeed speed = m_seekReversed
        ? MotorSpeed { static_cast<std::uint8_t>(std::max(10U, m_config.midSpeed.value() / 2U)) }
        : m_config.midSpeed;

    std::uint16_t nextPos = pos.value();
    if (m_sweepDirectionForward) {
        if (static_cast<std::uint32_t>(pos.value()) + scaledStep <= m_config.searchMax) {
            nextPos = static_cast<std::uint16_t>(pos.value() + scaledStep);
        } else {
            finishConvergence(pos);
            return;
        }
    } else {
        if (pos.value() >= m_config.searchMin + scaledStep) {
            nextPos = static_cast<std::uint16_t>(pos.value() - scaledStep);
        } else {
            finishConvergence(pos);
            return;
        }
    }

    m_prevScore = score;
    m_driver.setFocusPosition(MotorPosition { nextPos }, speed);
}

void AutoFocusEngine::stepGss(MotorPosition /*pos*/) noexcept
{
    const std::uint32_t score = m_latestScore.metric;

    if (!m_gssInitialized) {
        m_gssScore1 = score;
        m_gssInitialized = true;
        m_gssEvalX1 = false;
        m_driver.setFocusPosition(MotorPosition { m_gssX2 }, m_config.coarseSpeed);
        return;
    }

    if (!m_gssEvalX1) {
        m_gssScore2 = score;
    } else {
        m_gssScore1 = score;
    }

    if (m_gssB - m_gssA <= m_config.gssTolerance) {
        const auto summit = static_cast<std::uint16_t>((m_gssA + m_gssB) / 2U);
        finishConvergence(MotorPosition { summit });
        return;
    }

    constexpr float phi = 0.6180339887F;

    if (m_gssScore1 > m_gssScore2) {
        m_gssB = m_gssX2;
        m_gssX2 = m_gssX1;
        m_gssScore2 = m_gssScore1;
        const auto range = static_cast<float>(m_gssB - m_gssA);
        m_gssX1 = m_gssB - static_cast<std::uint16_t>(std::round(phi * range));
        m_gssEvalX1 = true;
        m_driver.setFocusPosition(MotorPosition { m_gssX1 }, m_config.fineSpeed);
    } else {
        m_gssA = m_gssX1;
        m_gssX1 = m_gssX2;
        m_gssScore1 = m_gssScore2;
        const auto range = static_cast<float>(m_gssB - m_gssA);
        m_gssX2 = m_gssA + static_cast<std::uint16_t>(std::round(phi * range));
        m_gssEvalX1 = false;
        m_driver.setFocusPosition(MotorPosition { m_gssX2 }, m_config.fineSpeed);
    }
}

void AutoFocusEngine::stepMomentum(MotorPosition pos) noexcept
{
    const std::uint32_t score = m_latestScore.metric;
    const auto curPos = static_cast<std::int32_t>(pos.value());

    if (m_prevScore == 0U) {
        m_prevScore = score;
        m_momentumLastPos = curPos;
        const auto nextPos = static_cast<std::uint16_t>(std::clamp(curPos + static_cast<std::int32_t>(m_config.midStep),
            static_cast<std::int32_t>(m_config.searchMin), static_cast<std::int32_t>(m_config.searchMax)));
        m_driver.setFocusPosition(MotorPosition { nextPos }, m_config.midSpeed);
        return;
    }

    const float deltaM = static_cast<float>(score) - static_cast<float>(m_prevScore);
    float deltaZ = static_cast<float>(curPos - m_momentumLastPos);
    if (std::abs(deltaZ) < 1.0F) {
        deltaZ = 1.0F;
    }

    const float gradient = deltaM / deltaZ;
    constexpr float alpha = 0.5F;
    constexpr float eta = 0.5F;

    m_momentumVelocity = (alpha * m_momentumVelocity) + (eta * gradient);
    const auto maxStep = static_cast<float>(m_config.coarseStep);
    m_momentumVelocity = std::clamp(m_momentumVelocity, -maxStep, maxStep);

    if (score < m_prevScore) {
        ++m_declines;
        const auto threshold90 = static_cast<std::uint32_t>(static_cast<double>(m_maxScoreSeen) * 0.9);
        if (m_declines >= m_config.cdownMax || (m_maxScoreSeen > 0U && score < threshold90)) {
            finishConvergence(pos);
            return;
        }
    } else {
        m_declines = 0U;
        if (score > m_maxScoreSeen) {
            m_maxScoreSeen = score;
        }
    }

    auto stepOffset = static_cast<std::int32_t>(std::round(m_momentumVelocity));
    if (std::abs(stepOffset) < static_cast<std::int32_t>(m_config.midStep)) {
        stepOffset = (stepOffset >= 0) ? static_cast<std::int32_t>(m_config.midStep)
                                       : -static_cast<std::int32_t>(m_config.midStep);
    }

    const std::int32_t candPos = curPos + stepOffset;
    if (candPos < static_cast<std::int32_t>(m_config.searchMin)
        || candPos > static_cast<std::int32_t>(m_config.searchMax)) {
        finishConvergence(pos);
        return;
    }

    const auto clampedPos = static_cast<std::uint16_t>(candPos);
    m_momentumLastPos = curPos;
    m_prevScore = score;
    m_driver.setFocusPosition(MotorPosition { clampedPos }, m_config.midSpeed);
}

void AutoFocusEngine::recordSample(MotorPosition pos, FocusMetricScore score, std::uint64_t timeNs) noexcept
{
    if (m_sampleCount < kMaxSamples) {
        m_samples[m_sampleCount].pos = pos;
        m_samples[m_sampleCount].score = score;
        m_samples[m_sampleCount].timestampNs = timeNs;
        ++m_sampleCount;
    }
}

void AutoFocusEngine::slewToTarget(MotorPosition target, bool applyBacklash) noexcept
{
    m_targetPos = target;
    const MotorPosition curPos = m_driver.currentFocus();

    if (applyBacklash && curPos.value() > target.value() && m_config.backlashSteps > 0U) {
        const std::uint16_t underShoot = target.value() > m_config.backlashSteps
            ? static_cast<std::uint16_t>(target.value() - m_config.backlashSteps)
            : 0U;
        m_driver.setFocusPosition(MotorPosition { underShoot }, m_config.fineSpeed);
        m_backlashApproached = true;
    } else {
        m_driver.setFocusPosition(target, m_config.fineSpeed);
        m_backlashApproached = false;
    }

    m_state = AutoFocusState::SlewToPeak;
}

void AutoFocusEngine::finishConvergence(MotorPosition bestPos) noexcept
{
    m_lastPeak = PeakFitter::fitMultiPoint(m_samples.data(), m_sampleCount);
    MotorPosition finalSummit = bestPos;
    if (m_lastPeak.isValid) {
        finalSummit = m_lastPeak.vertex;
    } else if (m_sampleCount > 0U) {
        std::size_t maxIdx = 0U;
        for (std::size_t i = 1U; i < m_sampleCount; ++i) {
            if (m_samples[i].score.metric > m_samples[maxIdx].score.metric) {
                maxIdx = i;
            }
        }
        finalSummit = m_samples[maxIdx].pos;
    }
    slewToTarget(finalSummit, true);
}

void AutoFocusEngine::setTrackerRoi(float normX, float normY, float normW, float normH, float marginFrac) noexcept
{
    const float halfW = (normW * (1.0F + marginFrac)) * 0.5F;
    const float halfH = (normH * (1.0F + marginFrac)) * 0.5F;

    const float left = std::clamp(normX - halfW, 0.0F, 1.0F);
    const float top = std::clamp(normY - halfH, 0.0F, 1.0F);
    const float right = std::clamp(normX + halfW, 0.0F, 1.0F);
    const float bottom = std::clamp(normY + halfH, 0.0F, 1.0F);

    const float width = right - left;
    const float height = bottom - top;

    NormalizedRoi roi { };
    roi.colFrac = static_cast<std::uint8_t>(std::clamp(left * 255.0F, 0.0F, 255.0F));
    roi.rowFrac = static_cast<std::uint8_t>(std::clamp(top * 255.0F, 0.0F, 255.0F));

    const auto wFrac = static_cast<std::uint16_t>(std::clamp(width * 255.0F, 1.0F, 255.0F));
    const auto hFrac = static_cast<std::uint16_t>(std::clamp(height * 255.0F, 1.0F, 255.0F));

    const auto maxW = static_cast<std::uint16_t>(256U - roi.colFrac);
    const auto maxH = static_cast<std::uint16_t>(256U - roi.rowFrac);

    roi.widthFrac = static_cast<std::uint8_t>(std::min(wFrac, maxW));
    roi.highFrac = static_cast<std::uint8_t>(std::min(hFrac, maxH));

    if (roi.isValid()) {
        m_evaluator.setRoi(roi);
    }
}

void AutoFocusEngine::clearTrackerRoi() noexcept
{
    m_evaluator.setRoi(NormalizedRoi { });
}

void AutoFocusEngine::setCafEnabled(bool enable) noexcept
{
    m_cafEnabled = enable;
}

bool AutoFocusEngine::isCafEnabled() const noexcept
{
    return m_cafEnabled;
}

bool AutoFocusEngine::isRunning() const noexcept
{
    return m_state != AutoFocusState::Idle && m_state != AutoFocusState::PeakConfirmed
        && m_state != AutoFocusState::Failed;
}

AutoFocusState AutoFocusEngine::state() const noexcept
{
    return m_state;
}

MotorPosition AutoFocusEngine::convergedPosition() const noexcept
{
    return m_convergedPos;
}

PeakFitResult AutoFocusEngine::lastPeakFit() const noexcept
{
    return m_lastPeak;
}

std::size_t AutoFocusEngine::sampleCount() const noexcept
{
    return m_sampleCount;
}

SamplePoint AutoFocusEngine::getSample(std::size_t index) const noexcept
{
    if (index < m_sampleCount) {
        return m_samples[index];
    }
    return SamplePoint { };
}

const AutoFocusConfig& AutoFocusEngine::config() const noexcept
{
    return m_config;
}

} // namespace LensControl
