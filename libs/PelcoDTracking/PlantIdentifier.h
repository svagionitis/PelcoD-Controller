#pragma once

/// @file PlantIdentifier.h
/// @brief Empirical plant frequency response estimation (Bode plot) and PID auto-tuning.

#include "Fft.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PelcoD {

/// @enum ChirpType
/// @brief Mathematical modulation law for frequency swept excitation.
enum class ChirpType : std::uint8_t {
    Linear, ///< Linear frequency increase: f(t) = f0 + (f1 - f0) * (t / T).
    Logarithmic ///< Exponential logarithmic sweep: spends more dwell cycles at lower frequencies.
};

/// @enum TuningRule
/// @brief Closed-loop PID tuning algorithms based on frequency response or FOPDT plant models.
enum class TuningRule : std::uint8_t {
    TyreusLuyben, ///< Recommended for PTZ heads: conservative, anti-jitter, high phase margin.
    ZieglerNichols, ///< Classic ultimate frequency response method: fast, aggressive response.
    Amigo, ///< Åström-Hägglund approximate M-constrained integral gain optimization.
    Imc ///< Internal Model Control (robust, zero-overshoot).
};

/// @struct ChirpConfig
/// @brief Parameters defining the swept-sine excitation waveform and spectral analysis.
struct ChirpConfig {
    double startFreqHz { 0.2 }; ///< Initial sweep frequency in Hertz.
    double endFreqHz { 12.0 }; ///< Final sweep frequency in Hertz (<= Nyquist f_s / 2).
    double durationSec { 6.0 }; ///< Active sweep duration in seconds.
    double sampleRateHz { 50.0 }; ///< Sampling frequency in Hertz.
    double amplitude { 1.0 }; ///< Normalized peak excitation amplitude [0.0, 1.0].
    ChirpType type { ChirpType::Logarithmic }; ///< Frequency sweep progression.
    std::size_t segmentSize { 128U }; ///< Welch FFT segment window size (power of 2: 64, 128, 256).
    double overlapRatio { 0.5 }; ///< Fractional segment overlap (e.g. 0.5 for 50%).
    Math::WindowType windowType { Math::WindowType::Hann }; ///< Spectral leakage suppression window.
    double taperRatio { 0.05 }; ///< Fraction of duration tapered at start/end to avoid step shocks.
};

/// @struct BodeData
/// @brief Empirical frequency response data vectors across the evaluated frequency spectrum.
struct BodeData {
    std::vector<double> frequenciesHz {}; ///< Frequency bins in Hertz.
    std::vector<double> magnitudeDb {}; ///< Empirical plant magnitude response in decibels (20*log10|H|).
    std::vector<double> phaseDeg {}; ///< Unwrapped phase response in degrees.
    std::vector<double> coherence {}; ///< Coherence function [0.0, 1.0] indicating SNR and linearity.
};

/// @struct StabilityMargins
/// @brief Closed-loop stability metrics and critical crossover frequencies.
struct StabilityMargins {
    double gainCrossoverFreqHz { 0.0 }; ///< Frequency where |H(f)| = 0 dB (gain crossover).
    double phaseMarginDeg { 0.0 }; ///< Phase margin Pm = 180 + phase(f_gc) in degrees.
    double phaseCrossoverFreqHz { 0.0 }; ///< Frequency where phase = -180 deg (phase crossover).
    double gainMarginDb { 0.0 }; ///< Gain margin Gm = -magnitudeDb(f_180) in decibels.
    double ultimateGainKu { 0.0 }; ///< Critical ultimate gain Ku = 1 / |H(f_180)|.
    double ultimatePeriodTu { 0.0 }; ///< Critical ultimate oscillation period Tu = 1 / f_180 in seconds.
    bool hasGainCrossover { false }; ///< True if 0 dB crossing was found.
    bool hasPhaseCrossover { false }; ///< True if -180 deg crossing was found.
};

/// @struct ResonancePeak
/// @brief Detected physical structural or mechanical resonance mode.
struct ResonancePeak {
    double frequencyHz { 0.0 }; ///< Resonance peak center frequency in Hertz.
    double peakMagnitudeDb { 0.0 }; ///< Maximum peak magnitude in decibels.
    double qFactor { 0.0 }; ///< Quality factor (resonance sharpness: f_res / delta_f_-3dB).
    double coherence { 0.0 }; ///< Coherence value at resonance peak.
};

/// @struct FopdtModel
/// @brief Equivalent First-Order Plus Dead-Time parametric transfer function fit G(s) = K*exp(-td*s)/(tau*s + 1).
struct FopdtModel {
    double dcGainK { 1.0 }; ///< Steady-state low-frequency plant gain K.
    double timeConstantTauSec { 0.1 }; ///< Dominant plant inertia / electrical time constant in seconds.
    double deadTimeTdSec { 0.05 }; ///< Equivalent pure transport delay in seconds.
    double fitRmse { 0.0 }; ///< Root-mean-square fitting error.
};

/// @struct PidTuningResult
/// @brief Recommended controller gains derived from empirical frequency response.
struct PidTuningResult {
    double kp { 1.0 }; ///< Proportional gain.
    double ki { 0.0 }; ///< Integral gain.
    double kd { 0.0 }; ///< Derivative gain.
    double kff { 0.0 }; ///< Velocity feedforward gain.
    TuningRule rule { TuningRule::TyreusLuyben }; ///< Tuning rule used.
    std::string ruleName {}; ///< Human-readable rule title.
    double estimatedPhaseMarginDeg { 0.0 }; ///< Expected closed-loop phase margin.
};

/// @struct PlantIdentificationResult
/// @brief Complete synthesis of empirical plant identification and tuning recommendations.
struct PlantIdentificationResult {
    bool success { false }; ///< True if frequency response and margins were successfully estimated.
    BodeData bode {}; ///< Complete Bode plot vectors (magnitude, phase, coherence).
    StabilityMargins margins {}; ///< Stability margins (Gm, Pm, Ku, Tu).
    std::vector<ResonancePeak> resonancePeaks {}; ///< Identified structural resonance modes.
    FopdtModel fopdt {}; ///< Fitted FOPDT parametric transfer function model.
    PidTuningResult suggestedPid {}; ///< Recommended auto-tuned PID gains.
    std::string message {}; ///< Diagnostic summary message.
};

/// @class PlantIdentifier
/// @brief Pure C++17 DSP engine for swept-sine plant identification, Bode estimation, and PID tuning.
/// @details Generates chirp waveforms, evaluates Welch cross-power spectral density between plant input
///          and observed output, extracts stability margins, detects resonance modes, and auto-tunes PID.
class PlantIdentifier {
public:
    /// @brief Construct a plant identifier with given configuration.
    /// @param[in] config Chirp and spectral analysis parameters.
    explicit PlantIdentifier(ChirpConfig config = {});

    /// @brief Generate an instantaneous chirp excitation value at time tSec.
    /// @param[in] tSec Elapsed time in seconds from sweep start.
    /// @param[in] amplitude Normalized peak scaling factor (default from config).
    /// @return Excitation signal value in [-amplitude, +amplitude].
    [[nodiscard]] double generateChirpSample(double tSec, double amplitude = -1.0) const noexcept;

    /// @brief Ingest streaming real-time input command and observed response samples.
    /// @param[in] inputCommand Commanded motor velocity excitation u(t).
    /// @param[in] outputResponse Observed plant motion feedback y(t).
    void addSample(double inputCommand, double outputResponse);

    /// @brief Bulk process complete recorded time-series sequences.
    /// @param[in] inputCommands Vector of commanded excitation samples u.
    /// @param[in] outputResponses Vector of observed feedback samples y.
    /// @param[in] sampleRateHz Sampling rate in Hertz (or <= 0 to use config rate).
    /// @return Identification outcome including Bode data, margins, and recommended PID gains.
    PlantIdentificationResult analyze(const std::vector<double>& inputCommands,
        const std::vector<double>& outputResponses, double sampleRateHz = -1.0);

    /// @brief Analyze currently accumulated sample buffer.
    /// @return Identification outcome.
    PlantIdentificationResult analyze();

    /// @brief Compute PID gains for a specific tuning rule using the latest identification results.
    /// @param[in] rule Desired tuning method (TyreusLuyben, ZieglerNichols, Amigo, Imc).
    /// @param[in] imcFilterLambda Optional lambda parameter for IMC (in seconds, <= 0 for auto).
    /// @return Tuned PID gains.
    [[nodiscard]] PidTuningResult computePidGains(TuningRule rule, double imcFilterLambda = -1.0) const;

    /// @brief Reset sample buffers and previous estimation state.
    void reset() noexcept;

    /// @brief Configure the identifier parameters.
    /// @param[in] config New configuration settings.
    void setConfig(const ChirpConfig& config);

    /// @brief Access the active configuration.
    [[nodiscard]] const ChirpConfig& getConfig() const noexcept;

    /// @brief Retrieve the latest identification result.
    [[nodiscard]] const PlantIdentificationResult& getLatestResult() const noexcept;

    /// @brief Get total number of accumulated sample pairs in the buffer.
    [[nodiscard]] std::size_t getSampleCount() const noexcept;

private:
    void computeWelchEstimates(const std::vector<double>& u, const std::vector<double>& y,
        double fs, BodeData& bode);
    void extractStabilityMargins(const BodeData& bode, StabilityMargins& margins);
    void detectResonancePeaks(const BodeData& bode, std::vector<ResonancePeak>& peaks);
    void fitFopdtModel(const BodeData& bode, const StabilityMargins& margins, FopdtModel& model);

    ChirpConfig m_config {};
    std::vector<double> m_inputBuffer {};
    std::vector<double> m_outputBuffer {};
    PlantIdentificationResult m_latestResult {};
};

} // namespace PelcoD
