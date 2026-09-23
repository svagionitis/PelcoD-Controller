/// @file TestGabor.cpp
/// @brief GoogleTest suite verifying 1D and 2D Gabor transforms, kernels, filterbanks, and anisotropy estimation.

#include "Gabor.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numeric>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

// Helper to generate a 2D synthetic sinusoidal grating
// intensity(x, y) = 128 + 120 * cos(2*pi * (x*cos(theta) + y*sin(theta)) / wavelength)
std::vector<std::uint8_t> createSyntheticGrating(
    int width, int height, double wavelength, double orientationRad, double phase = 0.0)
{
    std::vector<std::uint8_t> img(static_cast<std::size_t>(width * height), 0);
    const double cosTheta = std::cos(orientationRad);
    const double sinTheta = std::sin(orientationRad);
    const double twoPiOverLambda = (2.0 * kPi) / wavelength;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double proj = static_cast<double>(x) * cosTheta + static_cast<double>(y) * sinTheta;
            const double val = 128.0 + 120.0 * std::cos(twoPiOverLambda * proj + phase);
            const int clamped = std::clamp(static_cast<int>(std::round(val)), 0, 255);
            img[static_cast<std::size_t>(y * width + x)] = static_cast<std::uint8_t>(clamped);
        }
    }
    return img;
}

} // namespace

TEST(TestGabor, Kernel1DProperties)
{
    Math::Gabor1DConfig cfg {};
    cfg.centerFrequencyHz = 20.0;
    cfg.sampleRateHz = 200.0;
    cfg.sigmaSeconds = 0.025;
    cfg.tapCount = 41U;

    const Math::GaborKernel1D kernel = Math::createGaborKernel1D(cfg);

    ASSERT_EQ(kernel.realTaps.size(), 41U);
    ASSERT_EQ(kernel.imagTaps.size(), 41U);

    // Center index
    const std::size_t center = 20U;

    // Imaginary center tap must be 0 (sin(0) = 0)
    EXPECT_NEAR(kernel.imagTaps[center], 0.0, 1e-9);

    // Real part should be symmetric around center: real[center - k] == real[center + k]
    // Imag part should be antisymmetric: imag[center - k] == -imag[center + k]
    for (std::size_t k = 1U; k <= center; ++k) {
        EXPECT_NEAR(kernel.realTaps[center - k], kernel.realTaps[center + k], 1e-9);
        EXPECT_NEAR(kernel.imagTaps[center - k], -kernel.imagTaps[center + k], 1e-9);
    }

    // Energy normalization: sum(real^2 + imag^2) == 1.0
    double energy = 0.0;
    for (std::size_t i = 0U; i < kernel.realTaps.size(); ++i) {
        energy += (kernel.realTaps[i] * kernel.realTaps[i]) + (kernel.imagTaps[i] * kernel.imagTaps[i]);
    }
    EXPECT_NEAR(energy, 1.0, 1e-6);
}

TEST(TestGabor, Kernel2DPropertiesAndDcSuppression)
{
    Math::Gabor2DConfig cfg {};
    cfg.wavelength = 8.0;
    cfg.orientationRad = kPi / 4.0; // 45 deg
    cfg.sigma = 4.0;
    cfg.spatialAspectRatio = 0.5;
    cfg.kernelSize = 25;
    cfg.removeDc = true;

    const Math::GaborKernel2D kernel = Math::createGaborKernel2D(cfg);

    EXPECT_EQ(kernel.kernelSize, 25);
    EXPECT_EQ(kernel.realPart.size(), 625U);
    EXPECT_EQ(kernel.imagPart.size(), 625U);

    // DC suppression verification: sum of real taps must be zero
    const double realSum = std::accumulate(kernel.realPart.begin(), kernel.realPart.end(), 0.0);
    EXPECT_NEAR(realSum, 0.0, 1e-9);

    // Energy normalization
    double totalEnergy = 0.0;
    for (std::size_t i = 0U; i < kernel.realPart.size(); ++i) {
        totalEnergy += (kernel.realPart[i] * kernel.realPart[i]) + (kernel.imagPart[i] * kernel.imagPart[i]);
    }
    EXPECT_NEAR(totalEnergy, 1.0, 1e-6);
}

TEST(TestGabor, OrientationSelectivityOnSyntheticGratings)
{
    const int W = 64;
    const int H = 64;
    const double lambda = 8.0;

    // Filterbank with 4 orientations: 0 deg (0), 45 deg (pi/4), 90 deg (pi/2), 135 deg (3*pi/4)
    Math::GaborFilterBankConfig fbConfig {};
    fbConfig.numScales = 1U;
    fbConfig.numOrientations = 4U;
    fbConfig.baseWavelength = lambda;
    fbConfig.kernelSize = 21;
    fbConfig.sigmaScaleRatio = 0.5;
    fbConfig.spatialAspectRatio = 0.5;

    Math::GaborFilterBank fb(fbConfig);

    // 1. Grating at orientation 0 (vertical bars)
    const std::vector<std::uint8_t> verticalGrating = createSyntheticGrating(W, H, lambda, 0.0);
    const Math::GaborFilterBankResult resVert = fb.evaluate(verticalGrating.data(), W, H);

    ASSERT_EQ(resVert.directionalSharpness.size(), 4U);

    // Orientation 0 must be dominant
    EXPECT_NEAR(resVert.dominantOrientationRad, 0.0, 1e-5);
    // Response at orientation 0 must exceed orthogonal orientation pi/2 by a large factor
    EXPECT_GT(resVert.directionalSharpness[0], resVert.directionalSharpness[2] * 4.0);
    EXPECT_GT(resVert.anisotropyIndex, 0.6);

    // 2. Grating at orientation pi/2 (horizontal bars)
    const std::vector<std::uint8_t> horizGrating = createSyntheticGrating(W, H, lambda, kPi / 2.0);
    const Math::GaborFilterBankResult resHoriz = fb.evaluate(horizGrating.data(), W, H);

    EXPECT_NEAR(resHoriz.dominantOrientationRad, kPi / 2.0, 1e-5);
    EXPECT_GT(resHoriz.directionalSharpness[2], resHoriz.directionalSharpness[0] * 4.0);
    EXPECT_GT(resHoriz.anisotropyIndex, 0.6);
}

TEST(TestGabor, ScaleSelectivity)
{
    const int W = 64;
    const int H = 64;

    // 2 scales: lambda = 4.0 (fine) and lambda = 16.0 (coarse)
    Math::GaborFilterBankConfig fbConfig {};
    fbConfig.numScales = 2U;
    fbConfig.numOrientations = 2U;
    fbConfig.baseWavelength = 4.0;
    fbConfig.scaleFactor = 4.0; // scale 0: 4.0, scale 1: 16.0
    fbConfig.kernelSize = 25;

    Math::GaborFilterBank fb(fbConfig);

    // Fine grating (wavelength = 4.0, orientation = 0)
    const std::vector<std::uint8_t> fineGrating = createSyntheticGrating(W, H, 4.0, 0.0);
    const Math::GaborFilterBankResult resFine = fb.evaluate(fineGrating.data(), W, H);

    // Scale 0 orientation 0 vs Scale 1 orientation 0
    // Channel 0 = scale 0, orient 0. Channel 2 = scale 1, orient 0
    EXPECT_GT(resFine.channels[0].meanEnergy, resFine.channels[2].meanEnergy * 2.0);

    // Coarse grating (wavelength = 16.0, orientation = 0)
    const std::vector<std::uint8_t> coarseGrating = createSyntheticGrating(W, H, 16.0, 0.0);
    const Math::GaborFilterBankResult resCoarse = fb.evaluate(coarseGrating.data(), W, H);

    EXPECT_GT(resCoarse.channels[2].meanEnergy, resCoarse.channels[0].meanEnergy * 2.0);
}

TEST(TestGabor, DirectionalMotionBlurDetectionAndAnisotropy)
{
    const int W = 64;
    const int H = 64;

    // Create an orthogonal grid with equal horizontal and vertical frequency components
    std::vector<std::uint8_t> gridImg(static_cast<std::size_t>(W * H), 128);
    const double twoPiOverLambda = (2.0 * kPi) / 8.0;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const double val = 128.0 + 55.0 * std::cos(twoPiOverLambda * static_cast<double>(x))
                + 55.0 * std::cos(twoPiOverLambda * static_cast<double>(y));
            gridImg[static_cast<std::size_t>(y * W + x)]
                = static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::round(val)), 0, 255));
        }
    }

    Math::GaborFilterBankConfig fbConfig {};
    fbConfig.numScales = 2U;
    fbConfig.numOrientations = 4U; // 0, 45, 90, 135 deg
    fbConfig.baseWavelength = 4.0;
    fbConfig.scaleFactor = 2.0; // scale 0: 4.0, scale 1: 8.0
    fbConfig.kernelSize = 17;

    Math::GaborFilterBank fb(fbConfig);

    // Baseline unblurred grid: orientation 0 (0 deg) and orientation 2 (90 deg) must have equal response
    const Math::GaborFilterBankResult resBaseline = fb.evaluate(gridImg.data(), W, H);
    EXPECT_NEAR(resBaseline.directionalSharpness[0], resBaseline.directionalSharpness[2], 0.5);

    // Simulate severe horizontal pan motion blur (1D horizontal box blur of 9 pixels)
    std::vector<std::uint8_t> horizBlurred(static_cast<std::size_t>(W * H), 128);
    const int blurRadius = 4;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            int sum = 0;
            int count = 0;
            for (int dx = -blurRadius; dx <= blurRadius; ++dx) {
                const int px = std::clamp(x + dx, 0, W - 1);
                sum += gridImg[static_cast<std::size_t>(y * W + px)];
                count++;
            }
            horizBlurred[static_cast<std::size_t>(y * W + x)] = static_cast<std::uint8_t>(sum / count);
        }
    }

    const Math::GaborFilterBankResult resBlurred = fb.evaluate(horizBlurred.data(), W, H);

    // Horizontal blur destroys vertical lines (which vary horizontally, orientation theta = 0)
    // Horizontal lines (which vary vertically, orientation theta = pi/2) remain preserved!
    // Therefore, sharpness at pi/2 (orientation index 2) should be significantly higher than at 0 (index 0).
    EXPECT_GT(resBlurred.directionalSharpness[2], resBlurred.directionalSharpness[0] * 3.0);
    EXPECT_GT(resBlurred.anisotropyIndex, 0.4);
}

TEST(TestGabor, FeatureVectorConsistencyAndRoiEvaluation)
{
    const int W = 64;
    const int H = 64;

    Math::GaborFilterBankConfig fbConfig {};
    fbConfig.numScales = 3U;
    fbConfig.numOrientations = 4U;
    fbConfig.baseWavelength = 4.0;
    fbConfig.kernelSize = 15;

    Math::GaborFilterBank fb(fbConfig);

    EXPECT_EQ(fb.getChannelCount(), 12U);

    const std::vector<std::uint8_t> img = createSyntheticGrating(W, H, 8.0, kPi / 4.0);
    const Math::GaborFilterBankResult res = fb.evaluate(img.data(), W, H);

    // Feature vector length must be 2 * numScales * numOrientations = 24
    ASSERT_EQ(res.featureVector.size(), 24U);

    for (double val : res.featureVector) {
        EXPECT_FALSE(std::isnan(val));
        EXPECT_GE(val, 0.0);
    }

    // ROI sharpness
    const std::vector<double> roiSharpness = fb.evaluateRoiDirectionalSharpness(img.data(), W, H, 10, 10, 40, 40);

    ASSERT_EQ(roiSharpness.size(), 4U);
    for (double s : roiSharpness) {
        EXPECT_FALSE(std::isnan(s));
        EXPECT_GE(s, 0.0);
    }
}
