/// @file TestPlantIdentifier.cpp
/// @brief Automated unit test suite for swept-sine plant identification and empirical Bode estimation.

#include "ChirpCalibrator.h"
#include "PlantIdentifier.h"

#include <cmath>
#include <gtest/gtest.h>
#include <iostream>
#include <vector>

namespace {

using namespace Tracking;

constexpr double PI { 3.14159265358979323846 };
constexpr double TWO_PI { 2.0 * PI };

TEST(PlantIdentifierTest, ChirpGeneration)
{
    std::cout << "[Test] Chirp signal generation and envelope tapering...\n";

    ChirpConfig config {};
    config.startFreqHz = 0.5;
    config.endFreqHz = 10.0;
    config.durationSec = 4.0;
    config.sampleRateHz = 50.0;
    config.amplitude = 1.0;
    config.type = ChirpType::Linear;

    PlantIdentifier identifier(config);

    // Boundary conditions
    EXPECT_TRUE(identifier.generateChirpSample(-0.1) == 0.0);
    EXPECT_TRUE(identifier.generateChirpSample(4.1) == 0.0);

    // Verify envelope smoothly tapers up from zero
    const double valStart = identifier.generateChirpSample(0.0);
    EXPECT_TRUE(std::abs(valStart) < 1e-6);

    const double valEnd = identifier.generateChirpSample(4.0);
    EXPECT_TRUE(std::abs(valEnd) < 1e-6);

    // Logarithmic chirp
    config.type = ChirpType::Logarithmic;
    identifier.setConfig(config);

    bool hasPositive = false;
    bool hasNegative = false;
    const std::size_t numSamples = static_cast<std::size_t>(config.durationSec * config.sampleRateHz);
    for (std::size_t i = 0; i < numSamples; ++i) {
        const double t = static_cast<double>(i) / config.sampleRateHz;
        const double sample = identifier.generateChirpSample(t);
        EXPECT_TRUE(std::abs(sample) <= 1.0001); // Clamped within peak amplitude
        if (sample > 0.2) {
            hasPositive = true;
        }
        if (sample < -0.2) {
            hasNegative = true;
        }
    }
    EXPECT_TRUE(hasPositive && hasNegative);

    std::cout << "  -> Passed!\n";
}

TEST(PlantIdentifierTest, FirstOrderPlantIdentification)
{
    std::cout << "[Test] First-order physical plant identification (K=2.0, Tau=0.1s)...\n";

    ChirpConfig config {};
    config.startFreqHz = 0.2;
    config.endFreqHz = 10.0;
    config.durationSec = 8.0;
    config.sampleRateHz = 50.0;
    config.segmentSize = 128U;
    config.overlapRatio = 0.5;
    config.type = ChirpType::Logarithmic;

    PlantIdentifier identifier(config);

    const double dt = 1.0 / config.sampleRateHz;
    const std::size_t totalSamples = static_cast<std::size_t>(config.durationSec * config.sampleRateHz);

    // Discrete 1st-order simulation: y[k] = alpha * y[k-1] + (1 - alpha) * K * u[k]
    const double trueK = 2.0;
    const double trueTau = 0.10; // Cutoff ~ 1.59 Hz
    const double alpha = std::exp(-dt / trueTau);

    std::vector<double> u(totalSamples, 0.0);
    std::vector<double> y(totalSamples, 0.0);
    double plantState = 0.0;

    for (std::size_t k = 0; k < totalSamples; ++k) {
        const double t = static_cast<double>(k) * dt;
        u[k] = identifier.generateChirpSample(t);
        plantState = alpha * plantState + (1.0 - alpha) * trueK * u[k];
        y[k] = plantState;
    }

    const auto result = identifier.analyze(u, y);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(!result.bode.frequenciesHz.empty());

    // DC gain K should be close to 2.0 (within 20% empirical tolerance)
    std::cout << "  -> Identified DC Gain K: " << result.fopdt.dcGainK << " (True: " << trueK << ")\n";
    EXPECT_TRUE(std::abs(result.fopdt.dcGainK - trueK) < 0.4);

    // Verify low-pass roll-off: high-frequency magnitude should be significantly lower than DC
    const double dcMag = result.bode.magnitudeDb[1];
    const double hfMag = result.bode.magnitudeDb[result.bode.magnitudeDb.size() / 2];
    EXPECT_TRUE(hfMag < (dcMag - 6.0)); // At least 6 dB attenuation at higher frequencies

    // Verify phase lag is negative
    EXPECT_TRUE(result.bode.phaseDeg[result.bode.phaseDeg.size() / 2] < 0.0);

    std::cout << "  -> Passed!\n";
}

TEST(PlantIdentifierTest, SecondOrderResonanceDetection)
{
    std::cout << "[Test] Second-order mechanical plant with structural resonance at 4.0 Hz...\n";

    ChirpConfig config {};
    config.startFreqHz = 0.2;
    config.endFreqHz = 12.0;
    config.durationSec = 10.0;
    config.sampleRateHz = 50.0;
    config.segmentSize = 128U;
    config.type = ChirpType::Linear;

    PlantIdentifier identifier(config);

    const double dt = 1.0 / config.sampleRateHz;
    const std::size_t totalSamples = static_cast<std::size_t>(config.durationSec * config.sampleRateHz);

    // 2nd-order resonant system: y'' + 2*zeta*wn*y' + wn^2*y = wn^2*u
    // Resonant frequency fn = 4.0 Hz, zeta = 0.10 (sharp resonance)
    const double fn = 4.0;
    const double wn = TWO_PI * fn;
    const double zeta = 0.10;

    double yPos = 0.0;
    double yVel = 0.0;

    std::vector<double> u(totalSamples, 0.0);
    std::vector<double> y(totalSamples, 0.0);

    for (std::size_t k = 0; k < totalSamples; ++k) {
        const double t = static_cast<double>(k) * dt;
        u[k] = identifier.generateChirpSample(t);

        // RK2 numerical integration
        const double acc1 = wn * wn * (u[k] - yPos) - 2.0 * zeta * wn * yVel;
        const double posMid = yPos + 0.5 * dt * yVel;
        const double velMid = yVel + 0.5 * dt * acc1;
        const double acc2 = wn * wn * (u[k] - posMid) - 2.0 * zeta * wn * velMid;

        yPos += dt * velMid;
        yVel += dt * acc2;
        y[k] = yPos;
    }

    const auto result = identifier.analyze(u, y);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(!result.resonancePeaks.empty());

    const double detectedResFreq = result.resonancePeaks.front().frequencyHz;
    std::cout << "  -> Detected Resonance Mode: " << detectedResFreq
              << " Hz (Expected ~ 4.0 Hz, Q=" << result.resonancePeaks.front().qFactor << ")\n";

    EXPECT_TRUE(std::abs(detectedResFreq - fn) <= 0.6); // Within 0.6 Hz frequency resolution

    std::cout << "  -> Passed!\n";
}

TEST(PlantIdentifierTest, CoherenceEstimation)
{
    std::cout << "[Test] Spectral coherence under clean vs noisy measurements...\n";

    ChirpConfig config {};
    config.startFreqHz = 0.5;
    config.endFreqHz = 8.0;
    config.durationSec = 6.0;
    config.sampleRateHz = 50.0;
    config.segmentSize = 64U;

    PlantIdentifier identifier(config);

    const double dt = 1.0 / config.sampleRateHz;
    const std::size_t totalSamples = static_cast<std::size_t>(config.durationSec * config.sampleRateHz);

    std::vector<double> u(totalSamples);
    std::vector<double> yClean(totalSamples);
    std::vector<double> yNoisy(totalSamples);

    for (std::size_t k = 0; k < totalSamples; ++k) {
        const double t = static_cast<double>(k) * dt;
        u[k] = identifier.generateChirpSample(t);
        yClean[k] = 1.5 * u[k]; // Ideal linear gain

        // Add pseudo-random white noise
        const double noise = 2.0 * (static_cast<double>((k * 7919) % 1000) / 1000.0 - 0.5);
        yNoisy[k] = 0.5 * u[k] + noise;
    }

    const auto resClean = identifier.analyze(u, yClean);
    EXPECT_TRUE(resClean.success);

    // Coherence for clean linear system should be high (> 0.8) across in-band bins
    double meanCleanCoh = 0.0;
    std::size_t count = 0;
    for (std::size_t i = 1; i < resClean.bode.frequenciesHz.size() / 2; ++i) {
        meanCleanCoh += resClean.bode.coherence[i];
        ++count;
    }
    meanCleanCoh /= static_cast<double>(count);
    EXPECT_TRUE(meanCleanCoh > 0.85);

    const auto resNoisy = identifier.analyze(u, yNoisy);
    EXPECT_TRUE(resNoisy.success);

    // Coherence under heavy noise should be lower
    double meanNoisyCoh = 0.0;
    for (std::size_t i = 1; i < resNoisy.bode.frequenciesHz.size() / 2; ++i) {
        meanNoisyCoh += resNoisy.bode.coherence[i];
    }
    meanNoisyCoh /= static_cast<double>(count);
    EXPECT_TRUE(meanNoisyCoh < meanCleanCoh);

    std::cout << "  -> Clean Coherence: " << meanCleanCoh << ", Noisy Coherence: " << meanNoisyCoh << "\n";
    std::cout << "  -> Passed!\n";
}

TEST(PlantIdentifierTest, PidAutoTuningRules)
{
    std::cout << "[Test] PID auto-tuning algorithms (Tyreus-Luyben, ZN, AMIGO, IMC)...\n";

    PlantIdentifier identifier;

    // Simulate an identification result with Ku = 10.0, Tu = 0.5s, K=1.5, Tau=0.2s, Td=0.05s
    ChirpConfig config {};
    config.startFreqHz = 0.2;
    config.endFreqHz = 10.0;
    config.durationSec = 6.0;
    config.sampleRateHz = 50.0;
    config.segmentSize = 64U;
    identifier.setConfig(config);

    const double dt = 1.0 / config.sampleRateHz;
    const std::size_t totalSamples = static_cast<std::size_t>(config.durationSec * config.sampleRateHz);
    std::vector<double> u(totalSamples);
    std::vector<double> y(totalSamples);
    for (std::size_t k = 0; k < totalSamples; ++k) {
        u[k] = identifier.generateChirpSample(static_cast<double>(k) * dt);
        y[k] = 1.2 * u[k];
    }
    identifier.analyze(u, y);

    // 1. Tyreus-Luyben
    const auto tl = identifier.computePidGains(TuningRule::TyreusLuyben);
    EXPECT_TRUE(tl.kp > 0.0);
    EXPECT_TRUE(tl.ki > 0.0);
    EXPECT_TRUE(tl.kd > 0.0);
    std::cout << "  -> Tyreus-Luyben: Kp=" << tl.kp << ", Ki=" << tl.ki << ", Kd=" << tl.kd << "\n";

    // 2. Ziegler-Nichols
    const auto zn = identifier.computePidGains(TuningRule::ZieglerNichols);
    EXPECT_TRUE(zn.kp > 0.0);
    EXPECT_TRUE(zn.ki > 0.0);
    EXPECT_TRUE(zn.kd > 0.0);
    // Tyreus-Luyben must be more conservative than Ziegler-Nichols (lower Kp)
    EXPECT_TRUE(tl.kp < zn.kp);
    std::cout << "  -> Ziegler-Nichols: Kp=" << zn.kp << ", Ki=" << zn.ki << ", Kd=" << zn.kd << "\n";

    // 3. AMIGO
    const auto amigo = identifier.computePidGains(TuningRule::Amigo);
    EXPECT_TRUE(amigo.kp > 0.0);
    EXPECT_TRUE(amigo.ki > 0.0);
    EXPECT_TRUE(amigo.kd >= 0.0);
    std::cout << "  -> AMIGO: Kp=" << amigo.kp << ", Ki=" << amigo.ki << ", Kd=" << amigo.kd << "\n";

    // 4. IMC
    const auto imc = identifier.computePidGains(TuningRule::Imc);
    EXPECT_TRUE(imc.kp > 0.0);
    EXPECT_TRUE(imc.ki > 0.0);
    EXPECT_TRUE(imc.kd >= 0.0);
    std::cout << "  -> IMC: Kp=" << imc.kp << ", Ki=" << imc.ki << ", Kd=" << imc.kd << "\n";

    std::cout << "  -> Passed!\n";
}

TEST(PlantIdentifierTest, ChirpCalibratorWorkflowAndCancel)
{
    std::cout << "[Test] ChirpCalibrator state progression, command dispatch, and cancellation...\n";

    int commandedPanDir = 0;
    int commandedPanSpeed = 0;
    int commandedTiltDir = 0;
    int commandedTiltSpeed = 0;

    ChirpConfig config {};
    config.durationSec = 2.0;
    config.sampleRateHz = 50.0;
    config.segmentSize = 64U;

    ChirpCalibrator calibrator(
        [&](int pDir, int pSpeed, int tDir, int tSpeed) {
            commandedPanDir = pDir;
            commandedPanSpeed = pSpeed;
            commandedTiltDir = tDir;
            commandedTiltSpeed = tSpeed;
        },
        config);

    EXPECT_TRUE(calibrator.getState() == ChirpCalibratorState::Idle);

    // Start calibration sweep on Pan axis with maxSpeed = 30
    const bool started = calibrator.start(CalibrationAxis::Pan, 30, 100.0);
    EXPECT_TRUE(started);
    EXPECT_TRUE(calibrator.isRunning());
    EXPECT_TRUE(calibrator.getState() == ChirpCalibratorState::PreSettle);

    // During PreSettle, motor should be stopped
    calibrator.update(100.1);
    EXPECT_TRUE(commandedPanSpeed == 0);

    // Advance past PreSettle (0.40s) into Sweeping
    calibrator.update(100.5);
    EXPECT_TRUE(calibrator.getState() == ChirpCalibratorState::Sweeping);

    // During sweeping, motor commands should be dispatched and bounded by maxSpeed 30
    bool commandedMotion = false;
    for (double t = 100.5; t < 102.5; t += 0.02) {
        calibrator.update(t);
        calibrator.ingestVisualMotion(t, 5.0, 0.0);
        EXPECT_TRUE(commandedPanSpeed <= 30);
        EXPECT_TRUE(commandedTiltSpeed == 0); // Only pan excited
        if (commandedPanSpeed > 0) {
            commandedMotion = true;
        }
    }
    EXPECT_TRUE(commandedMotion);

    // Test cancellation halts motion and resets state
    calibrator.cancel();
    EXPECT_TRUE(!calibrator.isRunning());
    EXPECT_TRUE(calibrator.getState() == ChirpCalibratorState::Idle);
    EXPECT_TRUE(commandedPanSpeed == 0 && commandedPanDir == 0);

    std::cout << "  -> Passed!\n";
}

} // namespace
