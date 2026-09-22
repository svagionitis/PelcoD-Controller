/// @file PlantIdentifier.cpp
/// @brief Implementation of empirical plant frequency response estimation and PID auto-tuning.

#include "PlantIdentifier.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <numeric>

namespace Tracking {

namespace {

constexpr double PI { 3.14159265358979323846 };
constexpr double TWO_PI { 2.0 * PI };
constexpr double EPSILON { 1e-12 };

} // namespace

PlantIdentifier::PlantIdentifier(ChirpConfig config)
    : m_config(std::move(config))
{
}

double PlantIdentifier::generateChirpSample(double tSec, double amplitude) const noexcept
{
    if (tSec < 0.0 || tSec > m_config.durationSec) {
        return 0.0;
    }

    const double amp = (amplitude > 0.0) ? amplitude : m_config.amplitude;
    const double T = m_config.durationSec;
    const double f0 = std::max(0.01, m_config.startFreqHz);
    const double f1 = std::max(f0 + 0.01, m_config.endFreqHz);

    // Compute instantaneous excitation phase theta(t)
    double theta = 0.0;
    if (m_config.type == ChirpType::Linear) {
        const double deltaF = f1 - f0;
        theta = TWO_PI * (f0 * tSec + (deltaF / (2.0 * T)) * tSec * tSec);
    } else {
        // Logarithmic / exponential swept sine
        const double k = f1 / f0;
        theta = TWO_PI * f0 * (T / std::log(k)) * (std::pow(k, tSec / T) - 1.0);
    }

    // Apply smooth Tukey / raised-cosine taper to envelope edges to eliminate transient mechanical shocks
    double envelope = 1.0;
    const double taperDuration = T * std::clamp(m_config.taperRatio, 0.01, 0.49);
    if (tSec < taperDuration) {
        envelope = 0.5 * (1.0 - std::cos(PI * tSec / taperDuration));
    } else if (tSec > (T - taperDuration)) {
        envelope = 0.5 * (1.0 - std::cos(PI * (T - tSec) / taperDuration));
    }

    return amp * envelope * std::sin(theta);
}

void PlantIdentifier::addSample(double inputCommand, double outputResponse)
{
    m_inputBuffer.push_back(inputCommand);
    m_outputBuffer.push_back(outputResponse);
}

PlantIdentificationResult PlantIdentifier::analyze()
{
    return analyze(m_inputBuffer, m_outputBuffer, m_config.sampleRateHz);
}

PlantIdentificationResult PlantIdentifier::analyze(const std::vector<double>& inputCommands,
    const std::vector<double>& outputResponses, double sampleRateHz)
{
    PlantIdentificationResult result {};
    const double fs = (sampleRateHz > 0.0) ? sampleRateHz : m_config.sampleRateHz;

    if (inputCommands.size() < m_config.segmentSize || outputResponses.size() < m_config.segmentSize
        || inputCommands.size() != outputResponses.size()) {
        result.success = false;
        result.message = "Insufficient samples for spectral analysis (minimum segment size: "
            + std::to_string(m_config.segmentSize) + ")";
        m_latestResult = result;
        return result;
    }

    // Step 1: Compute Welch cross-spectral estimates (H(f), coherence, magnitude, phase)
    computeWelchEstimates(inputCommands, outputResponses, fs, result.bode);

    // Step 2: Extract stability margins (Gm, Pm, Ku, Tu)
    extractStabilityMargins(result.bode, result.margins);

    // Step 3: Identify structural resonance peaks
    detectResonancePeaks(result.bode, result.resonancePeaks);

    // Step 4: Fit equivalent FOPDT transfer function model G(s)
    fitFopdtModel(result.bode, result.margins, result.fopdt);

    // Step 5: Compute default recommended PID gains (Tyreus-Luyben)
    result.suggestedPid = computePidGains(TuningRule::TyreusLuyben);

    result.success = !result.bode.frequenciesHz.empty();
    result.message = result.success ? "Plant identification completed successfully." : "Spectral estimation failed.";

    m_latestResult = result;
    return result;
}

void PlantIdentifier::computeWelchEstimates(const std::vector<double>& u, const std::vector<double>& y,
    double fs, BodeData& bode)
{
    bode.frequenciesHz.clear();
    bode.magnitudeDb.clear();
    bode.phaseDeg.clear();
    bode.coherence.clear();

    const std::size_t L = Math::nextPowerOfTwo(m_config.segmentSize);
    const std::size_t numBins = (L / 2U) + 1U;
    const std::size_t N = u.size();

    const std::size_t step = std::max<std::size_t>(
        1U, static_cast<std::size_t>(static_cast<double>(L) * (1.0 - std::clamp(m_config.overlapRatio, 0.0, 0.9))));

    // Pre-calculate windowing coefficients and window energy S2
    std::vector<double> window(L, 1.0);
    Math::applyWindow(window, m_config.windowType);
    double s2 = 0.0;
    for (double w : window) {
        s2 += w * w;
    }
    if (s2 <= EPSILON) {
        s2 = 1.0;
    }

    std::vector<double> Suu(numBins, 0.0);
    std::vector<double> Syy(numBins, 0.0);
    std::vector<Math::Complex> Suy(numBins, Math::Complex(0.0, 0.0));
    std::size_t segmentCount = 0U;

    std::vector<double> uSeg(L);
    std::vector<double> ySeg(L);

    for (std::size_t offset = 0U; (offset + L) <= N; offset += step) {
        for (std::size_t i = 0U; i < L; ++i) {
            uSeg[i] = u[offset + i] * window[i];
            ySeg[i] = y[offset + i] * window[i];
        }

        const auto U = Math::rfft(uSeg);
        const auto Y = Math::rfft(ySeg);

        for (std::size_t k = 0U; k < numBins; ++k) {
            const double magU = std::abs(U[k]);
            const double magY = std::abs(Y[k]);
            Suu[k] += magU * magU;
            Syy[k] += magY * magY;
            Suy[k] += Y[k] * std::conj(U[k]);
        }
        ++segmentCount;
    }

    if (segmentCount == 0U) {
        return;
    }

    const double norm = static_cast<double>(segmentCount) * s2;
    bode.frequenciesHz.resize(numBins);
    bode.magnitudeDb.resize(numBins);
    bode.phaseDeg.resize(numBins);
    bode.coherence.resize(numBins);

    for (std::size_t k = 0U; k < numBins; ++k) {
        bode.frequenciesHz[k] = static_cast<double>(k) * (fs / static_cast<double>(L));

        const double suu = Suu[k] / norm;
        const double syy = Syy[k] / norm;
        const Math::Complex suy = Suy[k] / norm;

        // Transfer function H(f) = Suy(f) / Suu(f)
        const Math::Complex H = suy / (suu + EPSILON);
        const double mag = std::abs(H);
        bode.magnitudeDb[k] = 20.0 * std::log10(std::max(EPSILON, mag));
        bode.phaseDeg[k] = std::atan2(std::imag(H), std::real(H)) * (180.0 / PI);

        // Coherence gamma^2(f) = |Suy(f)|^2 / (Suu(f) * Syy(f))
        const double crossMag = std::abs(suy);
        const double coh = (crossMag * crossMag) / ((suu * syy) + EPSILON);
        bode.coherence[k] = std::clamp(coh, 0.0, 1.0);
    }

    // Unroll continuous phase curve to remove +/- 360 degree boundary jumps
    for (std::size_t k = 1U; k < numBins; ++k) {
        double delta = bode.phaseDeg[k] - bode.phaseDeg[k - 1U];
        while (delta > 180.0) {
            bode.phaseDeg[k] -= 360.0;
            delta -= 360.0;
        }
        while (delta < -180.0) {
            bode.phaseDeg[k] += 360.0;
            delta += 360.0;
        }
    }
}

void PlantIdentifier::extractStabilityMargins(const BodeData& bode, StabilityMargins& margins)
{
    margins = StabilityMargins {};
    const std::size_t n = bode.frequenciesHz.size();
    if (n < 2U) {
        return;
    }

    // 1. Detect Gain Crossover Frequency f_gc where |H(f)| = 0 dB
    for (std::size_t i = 1U; i < n; ++i) {
        const double mPrev = bode.magnitudeDb[i - 1U];
        const double mCurr = bode.magnitudeDb[i];
        if ((mPrev >= 0.0 && mCurr <= 0.0) || (mPrev <= 0.0 && mCurr >= 0.0)) {
            const double fraction = (0.0 - mPrev) / (mCurr - mPrev + EPSILON);
            margins.gainCrossoverFreqHz = bode.frequenciesHz[i - 1U]
                + fraction * (bode.frequenciesHz[i] - bode.frequenciesHz[i - 1U]);
            const double phaseAtGc = bode.phaseDeg[i - 1U]
                + fraction * (bode.phaseDeg[i] - bode.phaseDeg[i - 1U]);
            margins.phaseMarginDeg = 180.0 + phaseAtGc;
            margins.hasGainCrossover = true;
            break;
        }
    }

    // 2. Detect Phase Crossover Frequency f_180 where phase = -180 degrees
    for (std::size_t i = 1U; i < n; ++i) {
        const double pPrev = bode.phaseDeg[i - 1U];
        const double pCurr = bode.phaseDeg[i];
        if ((pPrev >= -180.0 && pCurr <= -180.0) || (pPrev <= -180.0 && pCurr >= -180.0)) {
            const double fraction = (-180.0 - pPrev) / (pCurr - pPrev + EPSILON);
            margins.phaseCrossoverFreqHz = bode.frequenciesHz[i - 1U]
                + fraction * (bode.frequenciesHz[i] - bode.frequenciesHz[i - 1U]);
            const double magAt180 = bode.magnitudeDb[i - 1U]
                + fraction * (bode.magnitudeDb[i] - bode.magnitudeDb[i - 1U]);
            margins.gainMarginDb = -magAt180;
            const double linMagAt180 = std::pow(10.0, magAt180 / 20.0);
            margins.ultimateGainKu = 1.0 / std::max(EPSILON, linMagAt180);
            margins.ultimatePeriodTu = (margins.phaseCrossoverFreqHz > EPSILON)
                ? (1.0 / margins.phaseCrossoverFreqHz)
                : 0.0;
            margins.hasPhaseCrossover = true;
            break;
        }
    }
}

void PlantIdentifier::detectResonancePeaks(const BodeData& bode, std::vector<ResonancePeak>& peaks)
{
    peaks.clear();
    const std::size_t n = bode.frequenciesHz.size();
    if (n < 5U) {
        return;
    }

    for (std::size_t i = 2U; i < n - 2U; ++i) {
        const double mag = bode.magnitudeDb[i];
        const double f = bode.frequenciesHz[i];
        const double coh = bode.coherence[i];

        // Must be a local maximum with reasonable coherence (> 0.35)
        if (mag > bode.magnitudeDb[i - 1U] && mag > bode.magnitudeDb[i + 1U]
            && mag > bode.magnitudeDb[i - 2U] && mag > bode.magnitudeDb[i + 2U]
            && coh >= 0.35) {
            
            // Measure prominence against adjacent troughs
            const double leftTrough = std::min(bode.magnitudeDb[i - 1U], bode.magnitudeDb[i - 2U]);
            const double rightTrough = std::min(bode.magnitudeDb[i + 1U], bode.magnitudeDb[i + 2U]);
            const double prominence = mag - std::max(leftTrough, rightTrough);

            if (prominence >= 1.5) { // Minimum 1.5 dB peak prominence
                // Estimate -3 dB bandwidth for Q-factor calculation
                const double halfPower = mag - 3.0;
                double fLow = f;
                for (std::size_t j = i; j > 0; --j) {
                    if (bode.magnitudeDb[j] <= halfPower) {
                        fLow = bode.frequenciesHz[j];
                        break;
                    }
                }
                double fHigh = f;
                for (std::size_t j = i; j < n; ++j) {
                    if (bode.magnitudeDb[j] <= halfPower) {
                        fHigh = bode.frequenciesHz[j];
                        break;
                    }
                }

                const double deltaF = std::max(0.1, fHigh - fLow);
                const double q = f / deltaF;

                peaks.push_back(ResonancePeak { f, mag, q, coh });
            }
        }
    }

    // Sort detected modes by descending peak magnitude
    std::sort(peaks.begin(), peaks.end(), [](const ResonancePeak& a, const ResonancePeak& b) {
        return a.peakMagnitudeDb > b.peakMagnitudeDb;
    });
}

void PlantIdentifier::fitFopdtModel(const BodeData& bode, const StabilityMargins& margins, FopdtModel& model)
{
    model = FopdtModel {};
    const std::size_t n = bode.frequenciesHz.size();
    if (n < 4U) {
        return;
    }

    // Estimate steady-state DC gain K from low-frequency asymptote (bins 1 to 3 above DC)
    double sumK = 0.0;
    std::size_t countK = 0U;
    for (std::size_t i = 1U; i < std::min<std::size_t>(n, 4U); ++i) {
        sumK += std::pow(10.0, bode.magnitudeDb[i] / 20.0);
        ++countK;
    }
    model.dcGainK = (countK > 0U) ? (sumK / static_cast<double>(countK)) : 1.0;

    // Estimate dominant time constant Tau from -3 dB cutoff frequency relative to DC gain
    const double targetMagDb = (20.0 * std::log10(std::max(EPSILON, model.dcGainK))) - 3.01;
    double cutoffFreqHz = 1.0;
    for (std::size_t i = 1U; i < n; ++i) {
        if (bode.magnitudeDb[i] <= targetMagDb) {
            cutoffFreqHz = bode.frequenciesHz[i];
            break;
        }
    }
    model.timeConstantTauSec = 1.0 / (TWO_PI * std::max(0.05, cutoffFreqHz));

    // Estimate effective dead time Td from high-frequency linear phase lag roll-off
    if (margins.hasPhaseCrossover && margins.phaseCrossoverFreqHz > 0.1) {
        // At f_180: phi = -180 deg = -atan(2*pi*f*tau) - (2*pi*f*td)
        const double w180 = TWO_PI * margins.phaseCrossoverFreqHz;
        const double lag1stOrderDeg = std::atan(w180 * model.timeConstantTauSec) * (180.0 / PI);
        const double remainingLagDeg = 180.0 - lag1stOrderDeg;
        model.deadTimeTdSec = std::max(0.005, (remainingLagDeg * (PI / 180.0)) / w180);
    } else {
        model.deadTimeTdSec = 0.05; // Fallback default
    }
}

PidTuningResult PlantIdentifier::computePidGains(TuningRule rule, double imcFilterLambda) const
{
    PidTuningResult result {};
    result.rule = rule;

    const auto& margins = m_latestResult.margins;
    const auto& fopdt = m_latestResult.fopdt;

    // Use estimated Ku / Tu if phase crossover was identified, otherwise synthesize from FOPDT model
    double Ku = margins.hasPhaseCrossover ? margins.ultimateGainKu : 0.0;
    double Tu = margins.hasPhaseCrossover ? margins.ultimatePeriodTu : 0.0;

    if (Ku <= EPSILON || Tu <= EPSILON) {
        // Synthesize equivalent ultimate critical parameters from FOPDT fit
        const double K = std::max(0.01, fopdt.dcGainK);
        const double tau = std::max(0.01, fopdt.timeConstantTauSec);
        const double td = std::max(0.005, fopdt.deadTimeTdSec);
        Ku = (2.0 * tau) / (K * td);
        Tu = 4.0 * td;
    }

    switch (rule) {
    case TuningRule::TyreusLuyben: {
        result.ruleName = "Tyreus-Luyben (Conservative Anti-Jitter)";
        result.kp = Ku / 2.2;
        const double Ti = 2.2 * Tu;
        const double Td = Tu / 6.3;
        result.ki = result.kp / Ti;
        result.kd = result.kp * Td;
        result.kff = 0.0;
        result.estimatedPhaseMarginDeg = 55.0;
        break;
    }
    case TuningRule::ZieglerNichols: {
        result.ruleName = "Ziegler-Nichols (Aggressive Frequency Response)";
        result.kp = 0.6 * Ku;
        const double Ti = 0.5 * Tu;
        const double Td = 0.125 * Tu;
        result.ki = result.kp / Ti;
        result.kd = result.kp * Td;
        result.kff = 0.0;
        result.estimatedPhaseMarginDeg = 30.0;
        break;
    }
    case TuningRule::Amigo: {
        result.ruleName = "AMIGO (Astrom-Hagglund M-Constrained)";
        const double K = std::max(0.01, fopdt.dcGainK);
        const double tau = std::max(0.01, fopdt.timeConstantTauSec);
        const double td = std::max(0.005, fopdt.deadTimeTdSec);

        result.kp = (1.0 / K) * (0.2 + 0.45 * (tau / td));
        const double Ti = ((0.4 * td + 0.8 * tau) / (td + 0.1 * tau)) * td;
        const double Td = (0.5 * tau * td) / (0.3 * td + tau);
        result.ki = result.kp / std::max(0.01, Ti);
        result.kd = result.kp * Td;
        result.kff = 0.0;
        result.estimatedPhaseMarginDeg = 60.0;
        break;
    }
    case TuningRule::Imc: {
        result.ruleName = "Internal Model Control (Zero-Overshoot)";
        const double K = std::max(0.01, fopdt.dcGainK);
        const double tau = std::max(0.01, fopdt.timeConstantTauSec);
        const double td = std::max(0.005, fopdt.deadTimeTdSec);
        const double lambda = (imcFilterLambda > 0.0) ? imcFilterLambda : std::max(td, 0.5 * tau);

        result.kp = (1.0 / K) * ((tau + 0.5 * td) / (lambda + 0.5 * td));
        const double Ti = tau + 0.5 * td;
        const double Td = (tau * td) / (2.0 * tau + td);
        result.ki = result.kp / std::max(0.01, Ti);
        result.kd = result.kp * Td;
        result.kff = 0.0;
        result.estimatedPhaseMarginDeg = 65.0;
        break;
    }
    }

    return result;
}

void PlantIdentifier::reset() noexcept
{
    m_inputBuffer.clear();
    m_outputBuffer.clear();
    m_latestResult = PlantIdentificationResult {};
}

void PlantIdentifier::setConfig(const ChirpConfig& config)
{
    m_config = config;
}

const ChirpConfig& PlantIdentifier::getConfig() const noexcept
{
    return m_config;
}

const PlantIdentificationResult& PlantIdentifier::getLatestResult() const noexcept
{
    return m_latestResult;
}

std::size_t PlantIdentifier::getSampleCount() const noexcept
{
    return m_inputBuffer.size();
}

} // namespace Tracking
