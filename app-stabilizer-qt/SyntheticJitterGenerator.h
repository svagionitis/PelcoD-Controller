#pragma once

/// @file SyntheticJitterGenerator.h
/// @brief Procedural test pattern and calibrated vibration synthesizer.

#include <cstddef>
#include <cstdint>

#include <random>
#include <vector>

namespace StabilizerApp {

/// @struct SyntheticJitterConfig
/// @brief Configuration parameters for procedural vibration synthesis.
struct SyntheticJitterConfig {
    std::uint32_t width { 1280U }; ///< Frame width in pixels
    std::uint32_t height { 720U }; ///< Frame height in pixels
    double frameRate { 30.0 }; ///< Synthetic stream frame rate (FPS)

    // Harmonic Mast Vibration
    float mastFreqX { 5.0F }; ///< Horizontal vibration frequency (Hz)
    float mastFreqY { 7.0F }; ///< Vertical vibration frequency (Hz)
    float mastAmpX { 15.0F }; ///< Horizontal vibration amplitude (pixels)
    float mastAmpY { 10.0F }; ///< Vertical vibration amplitude (pixels)

    // Handheld Random-Walk Drift
    bool enableDrift { true }; ///< Enable Brownian motion drift
    float driftDamping { 0.95F }; ///< Velocity damping factor [0.80 .. 0.99]
    float driftNoiseSigma { 0.50F }; ///< Acceleration noise variance

    // Rolling Shutter Wobble
    bool enableRollingShutter { false }; ///< Enable scanline shear wobble
    float rsFreq { 12.0F }; ///< Rolling shutter oscillation frequency (Hz)
    float rsAmp { 8.0F }; ///< Rolling shutter shear amplitude (pixels)

    std::uint32_t seed { 0x54616253U }; ///< Deterministic PRNG seed

    /// @brief Validates config bounds.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return (width >= 64U) && (height >= 64U) && (frameRate > 0.0) && (mastFreqX >= 0.0F) && (mastFreqY >= 0.0F)
            && (mastAmpX >= 0.0F) && (mastAmpY >= 0.0F);
    }
};

/// @class SyntheticJitterGenerator
/// @brief Generates procedural high-contrast geometric frames perturbed by calibrated motion.
class SyntheticJitterGenerator {
public:
    explicit SyntheticJitterGenerator(const SyntheticJitterConfig& config = { });
    ~SyntheticJitterGenerator() = default;

    SyntheticJitterGenerator(const SyntheticJitterGenerator&) = delete;
    SyntheticJitterGenerator& operator=(const SyntheticJitterGenerator&) = delete;
    SyntheticJitterGenerator(SyntheticJitterGenerator&&) noexcept = default;
    SyntheticJitterGenerator& operator=(SyntheticJitterGenerator&&) noexcept = default;

    /// @brief Reconfigures generator parameters and resets timeline.
    /// @param config New configuration parameters.
    void setConfig(const SyntheticJitterConfig& config);

    /// @brief Current configuration parameters.
    [[nodiscard]] const SyntheticJitterConfig& config() const noexcept;

    /// @brief Synthesizes next perturbed video frame into destination buffer.
    /// @param dstRgb Destination buffer (must hold at least width * height * 3 bytes).
    /// @param dt Time delta since previous frame in seconds.
    /// @return Presentation timestamp (PTS) in seconds.
    [[nodiscard]] double generateNextFrame(std::uint8_t* dstRgb, double dt);

    /// @brief Resets time index and drift accumulation to zero.
    void reset() noexcept;

private:
    void renderBasePattern();
    void sampleSubpixel(float srcX, float srcY, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) const noexcept;

    SyntheticJitterConfig config_ { };
    double currentTimeSec_ { 0.0 };
    std::uint64_t frameIndex_ { 0U };

    float driftVx_ { 0.0F };
    float driftVy_ { 0.0F };
    float driftPosX_ { 0.0F };
    float driftPosY_ { 0.0F };

    std::mt19937 prng_ { };
    std::normal_distribution<float> normalDist_ { 0.0F, 1.0F };

    std::vector<std::uint8_t> basePattern_ { }; ///< High-contrast reference frame
};

} // namespace StabilizerApp
