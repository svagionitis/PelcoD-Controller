#pragma once

/// @file SyntheticDefocusGenerator.h
/// @brief Procedural optical test pattern and calibrated defocus blur synthesizer.

#include <cstddef>
#include <cstdint>

#include <random>
#include <vector>

namespace LensApp {

/// @enum DefocusPatternType
/// @brief Procedural test pattern rendered as the optical scene.
enum class DefocusPatternType : std::uint8_t { SiemensStar = 0U, IsoChart = 1U, NaturalTexture = 2U };

/// @struct SyntheticDefocusConfig
/// @brief Calibration and configuration parameters for synthetic optical simulation.
struct SyntheticDefocusConfig {
    std::uint32_t width { 1280U };
    std::uint32_t height { 720U };
    double frameRate { 30.0 };

    DefocusPatternType pattern { DefocusPatternType::SiemensStar };
    std::uint16_t targetPlaneFocus { 32768U }; ///< In-focus motor encoder position
    float blurSensitivity { 24.0F }; ///< Defocus blur scaling factor
    float zoomCoupling { 3.0F }; ///< DoF narrowing factor with zoom
    float maxBlurSigma { 16.0F }; ///< Maximum Gaussian blur radius (pixels)
    float noiseSigma { 1.5F }; ///< Additive Gaussian noise stddev
    float vignetteFactor { 0.25F }; ///< Radial lens vignetting falloff
    std::uint32_t prngSeed { 0x4C454E53U }; ///< Deterministic PRNG seed ("LENS")

    /// @brief Validates config bounds.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return (width >= 64U) && (height >= 64U) && (frameRate > 0.0) && (blurSensitivity > 0.0F)
            && (maxBlurSigma > 0.0F);
    }
};

/// @class SyntheticDefocusGenerator
/// @brief Synthesizes procedural video frames with optical defocus blur and sensor noise.
class SyntheticDefocusGenerator {
public:
    explicit SyntheticDefocusGenerator(const SyntheticDefocusConfig& config = { });
    ~SyntheticDefocusGenerator() = default;

    SyntheticDefocusGenerator(const SyntheticDefocusGenerator&) = delete;
    SyntheticDefocusGenerator& operator=(const SyntheticDefocusGenerator&) = delete;
    SyntheticDefocusGenerator(SyntheticDefocusGenerator&&) noexcept = default;
    SyntheticDefocusGenerator& operator=(SyntheticDefocusGenerator&&) noexcept = default;

    void setConfig(const SyntheticDefocusConfig& config);
    [[nodiscard]] const SyntheticDefocusConfig& config() const noexcept;

    void setTargetPlane(std::uint16_t focusPlane) noexcept;
    [[nodiscard]] std::uint16_t targetPlane() const noexcept;

    void updateMotorState(std::uint16_t focus, std::uint16_t zoom) noexcept;

    [[nodiscard]] double generateNextFrame(std::uint8_t* dstRgb, double dt);
    void reset() noexcept;

private:
    void renderBasePattern();
    void renderSiemensStar();
    void renderIsoChart();
    void renderNaturalTexture();
    void applySeparableBlur(std::uint8_t* dstRgb, float sigma);
    void applyNoiseAndVignette(std::uint8_t* dstRgb);

    SyntheticDefocusConfig config_ { };
    std::uint16_t currentFocus_ { 32768U };
    std::uint16_t currentZoom_ { 0U };
    double currentTimeSec_ { 0.0 };
    std::uint64_t frameIndex_ { 0U };

    std::mt19937 prng_ { };
    std::normal_distribution<float> normalDist_ { 0.0F, 1.0F };

    std::vector<std::uint8_t> basePattern_ { }; ///< Unblurred reference image (RGB24)
    std::vector<std::uint8_t> scratchBuffer_ { }; ///< Intermediate 1D blur buffer (RGB24)
};

} // namespace LensApp
