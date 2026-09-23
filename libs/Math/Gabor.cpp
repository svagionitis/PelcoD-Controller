/// @file Gabor.cpp
/// @brief Implementation of 1D and 2D Gabor Transform, filter kernels, and filterbanks.

#include "Gabor.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace Math {

namespace {

    constexpr double kPi = 3.14159265358979323846;
    constexpr double kEpsilon = 1e-12;

    inline int clampCoord(int val, int maxVal) noexcept
    {
        if (val < 0) {
            return 0;
        }
        if (val >= maxVal) {
            return maxVal - 1;
        }
        return val;
    }

} // namespace

GaborKernel1D createGaborKernel1D(const Gabor1DConfig& config)
{
    GaborKernel1D result {};
    result.centerFrequencyHz = config.centerFrequencyHz;
    result.sampleRateHz = config.sampleRateHz;

    std::size_t taps = config.tapCount;
    if (taps < 3U) {
        taps = 3U;
    }
    if ((taps % 2U) == 0U) {
        taps += 1U;
    }

    result.realTaps.resize(taps, 0.0);
    result.imagTaps.resize(taps, 0.0);

    const int halfTaps = static_cast<int>(taps / 2U);
    const double dt = (config.sampleRateHz > 0.0) ? (1.0 / config.sampleRateHz) : 0.01;
    const double sigma = (config.sigmaSeconds > 0.0) ? config.sigmaSeconds : 0.01;
    const double twoSigmaSq = 2.0 * sigma * sigma;
    const double twoPiF0 = 2.0 * kPi * config.centerFrequencyHz;

    double energySum = 0.0;
    for (int n = -halfTaps; n <= halfTaps; ++n) {
        const double t = static_cast<double>(n) * dt;
        const double env = std::exp(-(t * t) / twoSigmaSq);
        const double cosVal = std::cos(twoPiF0 * t);
        const double sinVal = std::sin(twoPiF0 * t);

        const std::size_t idx = static_cast<std::size_t>(n + halfTaps);
        const double realVal = env * cosVal;
        const double imagVal = env * sinVal;

        result.realTaps[idx] = realVal;
        result.imagTaps[idx] = imagVal;
        energySum += (realVal * realVal) + (imagVal * imagVal);
    }

    if (energySum > kEpsilon) {
        const double norm = std::sqrt(energySum);
        for (std::size_t i = 0U; i < taps; ++i) {
            result.realTaps[i] /= norm;
            result.imagTaps[i] /= norm;
        }
    }

    return result;
}

GaborKernel2D createGaborKernel2D(const Gabor2DConfig& config)
{
    GaborKernel2D result {};
    result.wavelength = config.wavelength;
    result.orientationRad = config.orientationRad;

    int kSize = config.kernelSize;
    if (kSize < 3) {
        kSize = 3;
    }
    if ((kSize % 2) == 0) {
        kSize += 1;
    }
    result.kernelSize = kSize;

    const std::size_t totalPixels = static_cast<std::size_t>(kSize * kSize);
    result.realPart.resize(totalPixels, 0.0);
    result.imagPart.resize(totalPixels, 0.0);

    const int halfSize = kSize / 2;
    const double lambda = (config.wavelength > 0.0) ? config.wavelength : 4.0;
    const double sigma = (config.sigma > 0.0) ? config.sigma : 2.0;
    const double gamma = (config.spatialAspectRatio > 0.0) ? config.spatialAspectRatio : 0.5;
    const double theta = config.orientationRad;
    const double psi = config.phaseOffsetRad;

    const double cosTheta = std::cos(theta);
    const double sinTheta = std::sin(theta);
    const double twoSigmaSq = 2.0 * sigma * sigma;
    const double gammaSq = gamma * gamma;
    const double twoPiOverLambda = (2.0 * kPi) / lambda;

    double realSum = 0.0;

    for (int y = -halfSize; y <= halfSize; ++y) {
        for (int x = -halfSize; x <= halfSize; ++x) {
            const double xPrime = static_cast<double>(x) * cosTheta + static_cast<double>(y) * sinTheta;
            const double yPrime = -static_cast<double>(x) * sinTheta + static_cast<double>(y) * cosTheta;

            const double env = std::exp(-(xPrime * xPrime + gammaSq * yPrime * yPrime) / twoSigmaSq);
            const double carrierAngle = twoPiOverLambda * xPrime + psi;

            const double realVal = env * std::cos(carrierAngle);
            const double imagVal = env * std::sin(carrierAngle);

            const std::size_t idx = static_cast<std::size_t>((y + halfSize) * kSize + (x + halfSize));
            result.realPart[idx] = realVal;
            result.imagPart[idx] = imagVal;
            realSum += realVal;
        }
    }

    if (config.removeDc && totalPixels > 0U) {
        const double meanReal = realSum / static_cast<double>(totalPixels);
        for (std::size_t i = 0U; i < totalPixels; ++i) {
            result.realPart[i] -= meanReal;
        }
    }

    double energySum = 0.0;
    for (std::size_t i = 0U; i < totalPixels; ++i) {
        energySum += (result.realPart[i] * result.realPart[i]) + (result.imagPart[i] * result.imagPart[i]);
    }

    if (energySum > kEpsilon) {
        const double norm = std::sqrt(energySum);
        for (std::size_t i = 0U; i < totalPixels; ++i) {
            result.realPart[i] /= norm;
            result.imagPart[i] /= norm;
        }
    }

    return result;
}

std::vector<double> convolve2D(
    const std::uint8_t* pixels, int width, int height, int stride, const std::vector<double>& kernel, int kernelSize)
{
    if (pixels == nullptr || width <= 0 || height <= 0 || kernelSize <= 0) {
        return {};
    }

    const int pitch = (stride > 0) ? stride : width;
    const int halfK = kernelSize / 2;
    const std::size_t totalPixels = static_cast<std::size_t>(width * height);
    std::vector<double> output(totalPixels, 0.0);

    for (int y = 0; y < height; ++y) {
        const int outRowOffset = y * width;
        for (int x = 0; x < width; ++x) {
            double sum = 0.0;
            std::size_t kIdx = 0U;

            for (int ky = -halfK; ky <= halfK; ++ky) {
                const int py = clampCoord(y + ky, height);
                const int inRowOffset = py * pitch;

                for (int kx = -halfK; kx <= halfK; ++kx) {
                    const int px = clampCoord(x + kx, width);
                    const double pixelVal = static_cast<double>(pixels[inRowOffset + px]);
                    sum += pixelVal * kernel[kIdx++];
                }
            }

            output[static_cast<std::size_t>(outRowOffset + x)] = sum;
        }
    }

    return output;
}

std::vector<double> convolve2D(
    const std::vector<double>& image, int width, int height, const std::vector<double>& kernel, int kernelSize)
{
    if (image.empty() || width <= 0 || height <= 0 || kernelSize <= 0) {
        return {};
    }

    const int halfK = kernelSize / 2;
    const std::size_t totalPixels = static_cast<std::size_t>(width * height);
    std::vector<double> output(totalPixels, 0.0);

    for (int y = 0; y < height; ++y) {
        const int outRowOffset = y * width;
        for (int x = 0; x < width; ++x) {
            double sum = 0.0;
            std::size_t kIdx = 0U;

            for (int ky = -halfK; ky <= halfK; ++ky) {
                const int py = clampCoord(y + ky, height);
                const int inRowOffset = py * width;

                for (int kx = -halfK; kx <= halfK; ++kx) {
                    const int px = clampCoord(x + kx, width);
                    const double pixelVal = image[static_cast<std::size_t>(inRowOffset + px)];
                    sum += pixelVal * kernel[kIdx++];
                }
            }

            output[static_cast<std::size_t>(outRowOffset + x)] = sum;
        }
    }

    return output;
}

// -----------------------------------------------------------------------------
// GaborFilterBank Implementation
// -----------------------------------------------------------------------------

GaborFilterBank::GaborFilterBank(GaborFilterBankConfig config)
    : m_config(std::move(config))
{
    buildKernels();
}

void GaborFilterBank::setConfig(const GaborFilterBankConfig& config)
{
    m_config = config;
    buildKernels();
}

GaborFilterBankConfig GaborFilterBank::getConfig() const noexcept
{
    return m_config;
}

std::size_t GaborFilterBank::getChannelCount() const noexcept
{
    return m_kernels.size();
}

const GaborKernel2D& GaborFilterBank::getKernel(std::size_t scaleIndex, std::size_t orientationIndex) const
{
    const std::size_t idx = scaleIndex * m_config.numOrientations + orientationIndex;
    if (idx >= m_kernels.size()) {
        throw std::out_of_range("GaborFilterBank::getKernel: index out of bounds");
    }
    return m_kernels[idx];
}

void GaborFilterBank::buildKernels()
{
    m_kernels.clear();
    m_orientationAnglesRad.clear();

    const std::size_t S = (m_config.numScales > 0U) ? m_config.numScales : 1U;
    const std::size_t O = (m_config.numOrientations > 0U) ? m_config.numOrientations : 1U;

    m_kernels.reserve(S * O);
    m_orientationAnglesRad.reserve(O);

    for (std::size_t o = 0U; o < O; ++o) {
        const double theta = (static_cast<double>(o) * kPi) / static_cast<double>(O);
        m_orientationAnglesRad.push_back(theta);
    }

    for (std::size_t s = 0U; s < S; ++s) {
        const double scaleMultiplier = std::pow(m_config.scaleFactor, static_cast<double>(s));
        const double lambda = m_config.baseWavelength * scaleMultiplier;
        const double sigma = lambda * m_config.sigmaScaleRatio;

        for (std::size_t o = 0U; o < O; ++o) {
            Gabor2DConfig cfg {};
            cfg.wavelength = lambda;
            cfg.orientationRad = m_orientationAnglesRad[o];
            cfg.sigma = sigma;
            cfg.spatialAspectRatio = m_config.spatialAspectRatio;
            cfg.phaseOffsetRad = 0.0;
            cfg.kernelSize = m_config.kernelSize;
            cfg.removeDc = m_config.removeDc;

            m_kernels.push_back(createGaborKernel2D(cfg));
        }
    }
}

GaborFilterBankResult GaborFilterBank::evaluate(
    const std::uint8_t* grayPixels, int width, int height, int stride, bool computeSpatialEnergy) const
{
    GaborFilterBankResult result {};
    result.imageWidth = width;
    result.imageHeight = height;

    if (grayPixels == nullptr || width <= 0 || height <= 0 || m_kernels.empty()) {
        return result;
    }

    const std::size_t totalPixels = static_cast<std::size_t>(width * height);
    const std::size_t S = (m_config.numScales > 0U) ? m_config.numScales : 1U;
    const std::size_t O = (m_config.numOrientations > 0U) ? m_config.numOrientations : 1U;

    result.channels.resize(m_kernels.size());
    result.orientationAnglesRad = m_orientationAnglesRad;
    result.directionalSharpness.assign(O, 0.0);
    result.maxEnergyProjection.assign(totalPixels, 0.0);
    result.featureVector.reserve(m_kernels.size() * 2U);

    for (std::size_t s = 0U; s < S; ++s) {
        for (std::size_t o = 0U; o < O; ++o) {
            const std::size_t chIdx = s * O + o;
            const auto& kernel = m_kernels[chIdx];

            const std::vector<double> realResp
                = convolve2D(grayPixels, width, height, stride, kernel.realPart, kernel.kernelSize);
            const std::vector<double> imagResp
                = convolve2D(grayPixels, width, height, stride, kernel.imagPart, kernel.kernelSize);

            GaborChannelResult ch {};
            ch.scaleIndex = s;
            ch.orientationIndex = o;
            ch.orientationRad = m_orientationAnglesRad[o];
            ch.wavelength = kernel.wavelength;

            if (computeSpatialEnergy) {
                ch.energy.resize(totalPixels, 0.0);
            }

            double sumEnergy = 0.0;
            double sumSqEnergy = 0.0;
            double maxEn = 0.0;

            for (std::size_t p = 0U; p < totalPixels; ++p) {
                const double r = realResp[p];
                const double im = imagResp[p];
                const double energy = std::sqrt(r * r + im * im);

                if (computeSpatialEnergy) {
                    ch.energy[p] = energy;
                }

                sumEnergy += energy;
                sumSqEnergy += energy * energy;
                if (energy > maxEn) {
                    maxEn = energy;
                }

                if (energy > result.maxEnergyProjection[p]) {
                    result.maxEnergyProjection[p] = energy;
                }
            }

            const double n = static_cast<double>(totalPixels);
            ch.meanEnergy = (n > 0.0) ? (sumEnergy / n) : 0.0;
            ch.energyVariance = (n > 1.0) ? ((sumSqEnergy - (sumEnergy * sumEnergy / n)) / (n - 1.0)) : 0.0;
            if (ch.energyVariance < 0.0) {
                ch.energyVariance = 0.0;
            }
            ch.maxEnergy = maxEn;

            result.directionalSharpness[o] += ch.meanEnergy;

            result.featureVector.push_back(ch.meanEnergy);
            result.featureVector.push_back(ch.energyVariance);

            result.channels[chIdx] = std::move(ch);
        }
    }

    if (S > 0U) {
        for (std::size_t o = 0U; o < O; ++o) {
            result.directionalSharpness[o] /= static_cast<double>(S);
        }
    }

    double maxSharpness = -1.0;
    double minSharpness = 1e18;
    std::size_t bestOrientation = 0U;

    for (std::size_t o = 0U; o < O; ++o) {
        const double sVal = result.directionalSharpness[o];
        if (sVal > maxSharpness) {
            maxSharpness = sVal;
            bestOrientation = o;
        }
        if (sVal < minSharpness) {
            minSharpness = sVal;
        }
    }

    result.dominantOrientationRad = m_orientationAnglesRad[bestOrientation];

    if ((maxSharpness + minSharpness) > kEpsilon) {
        result.anisotropyIndex = (maxSharpness - minSharpness) / (maxSharpness + minSharpness);
    } else {
        result.anisotropyIndex = 0.0;
    }

    return result;
}

GaborFilterBankResult GaborFilterBank::evaluate(
    const std::vector<double>& image, int width, int height, bool computeSpatialEnergy) const
{
    GaborFilterBankResult result {};
    result.imageWidth = width;
    result.imageHeight = height;

    if (image.empty() || width <= 0 || height <= 0 || m_kernels.empty()) {
        return result;
    }

    const std::size_t totalPixels = static_cast<std::size_t>(width * height);
    const std::size_t S = (m_config.numScales > 0U) ? m_config.numScales : 1U;
    const std::size_t O = (m_config.numOrientations > 0U) ? m_config.numOrientations : 1U;

    result.channels.resize(m_kernels.size());
    result.orientationAnglesRad = m_orientationAnglesRad;
    result.directionalSharpness.assign(O, 0.0);
    result.maxEnergyProjection.assign(totalPixels, 0.0);
    result.featureVector.reserve(m_kernels.size() * 2U);

    for (std::size_t s = 0U; s < S; ++s) {
        for (std::size_t o = 0U; o < O; ++o) {
            const std::size_t chIdx = s * O + o;
            const auto& kernel = m_kernels[chIdx];

            const std::vector<double> realResp = convolve2D(image, width, height, kernel.realPart, kernel.kernelSize);
            const std::vector<double> imagResp = convolve2D(image, width, height, kernel.imagPart, kernel.kernelSize);

            GaborChannelResult ch {};
            ch.scaleIndex = s;
            ch.orientationIndex = o;
            ch.orientationRad = m_orientationAnglesRad[o];
            ch.wavelength = kernel.wavelength;

            if (computeSpatialEnergy) {
                ch.energy.resize(totalPixels, 0.0);
            }

            double sumEnergy = 0.0;
            double sumSqEnergy = 0.0;
            double maxEn = 0.0;

            for (std::size_t p = 0U; p < totalPixels; ++p) {
                const double r = realResp[p];
                const double im = imagResp[p];
                const double energy = std::sqrt(r * r + im * im);

                if (computeSpatialEnergy) {
                    ch.energy[p] = energy;
                }

                sumEnergy += energy;
                sumSqEnergy += energy * energy;
                if (energy > maxEn) {
                    maxEn = energy;
                }

                if (energy > result.maxEnergyProjection[p]) {
                    result.maxEnergyProjection[p] = energy;
                }
            }

            const double n = static_cast<double>(totalPixels);
            ch.meanEnergy = (n > 0.0) ? (sumEnergy / n) : 0.0;
            ch.energyVariance = (n > 1.0) ? ((sumSqEnergy - (sumEnergy * sumEnergy / n)) / (n - 1.0)) : 0.0;
            if (ch.energyVariance < 0.0) {
                ch.energyVariance = 0.0;
            }
            ch.maxEnergy = maxEn;

            result.directionalSharpness[o] += ch.meanEnergy;

            result.featureVector.push_back(ch.meanEnergy);
            result.featureVector.push_back(ch.energyVariance);

            result.channels[chIdx] = std::move(ch);
        }
    }

    if (S > 0U) {
        for (std::size_t o = 0U; o < O; ++o) {
            result.directionalSharpness[o] /= static_cast<double>(S);
        }
    }

    double maxSharpness = -1.0;
    double minSharpness = 1e18;
    std::size_t bestOrientation = 0U;

    for (std::size_t o = 0U; o < O; ++o) {
        const double sVal = result.directionalSharpness[o];
        if (sVal > maxSharpness) {
            maxSharpness = sVal;
            bestOrientation = o;
        }
        if (sVal < minSharpness) {
            minSharpness = sVal;
        }
    }

    result.dominantOrientationRad = m_orientationAnglesRad[bestOrientation];

    if ((maxSharpness + minSharpness) > kEpsilon) {
        result.anisotropyIndex = (maxSharpness - minSharpness) / (maxSharpness + minSharpness);
    } else {
        result.anisotropyIndex = 0.0;
    }

    return result;
}

std::vector<double> GaborFilterBank::evaluateRoiDirectionalSharpness(const std::uint8_t* grayPixels, int width,
    int height, int roiX, int roiY, int roiWidth, int roiHeight, int stride) const
{
    const std::size_t O = (m_config.numOrientations > 0U) ? m_config.numOrientations : 1U;
    if (grayPixels == nullptr || width <= 0 || height <= 0 || roiWidth <= 0 || roiHeight <= 0) {
        return std::vector<double>(O, 0.0);
    }

    const int x0 = std::max(0, roiX);
    const int y0 = std::max(0, roiY);
    const int x1 = std::min(width, roiX + roiWidth);
    const int y1 = std::min(height, roiY + roiHeight);
    const int effW = x1 - x0;
    const int effH = y1 - y0;

    if (effW <= 0 || effH <= 0) {
        return std::vector<double>(O, 0.0);
    }

    const int pitch = (stride > 0) ? stride : width;
    std::vector<std::uint8_t> roiPixels(static_cast<std::size_t>(effW * effH));

    for (int y = 0; y < effH; ++y) {
        const std::uint8_t* srcRow = grayPixels + (y0 + y) * pitch + x0;
        std::uint8_t* dstRow = roiPixels.data() + y * effW;
        std::copy_n(srcRow, effW, dstRow);
    }

    const GaborFilterBankResult res = evaluate(roiPixels.data(), effW, effH, effW, false);
    return res.directionalSharpness;
}

} // namespace Math
