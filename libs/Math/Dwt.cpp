/// @file Dwt.cpp
/// @brief Implementation of Discrete Wavelet Transform (DWT), inverse DWT, and wavelet shrinkage denoising.

#include "Dwt.h"

#include <algorithm>
#include <cmath>

namespace Math {

namespace {

    // Precomputed filter coefficients for Haar and Daubechies-4 wavelets
    struct WaveletFilters {
        std::vector<double> h; ///< Low-pass decomposition filter
        std::vector<double> g; ///< High-pass decomposition filter
    };

    WaveletFilters getFilters(WaveletType wavelet)
    {
        WaveletFilters f {};
        if (wavelet == WaveletType::Haar) {
            const double invSqrt2 = 1.0 / std::sqrt(2.0);
            f.h = { invSqrt2, invSqrt2 };
            f.g = { invSqrt2, -invSqrt2 };
        } else { // Db4
            constexpr double sqrt3 = 1.73205080756887729353;
            constexpr double denom = 5.65685424949238019521; // 4 * sqrt(2)
            const double h0 = (1.0 + sqrt3) / denom;
            const double h1 = (3.0 + sqrt3) / denom;
            const double h2 = (3.0 - sqrt3) / denom;
            const double h3 = (1.0 - sqrt3) / denom;

            f.h = { h0, h1, h2, h3 };
            f.g = { h3, -h2, h1, -h0 };
        }
        return f;
    }

} // namespace

void dwt1D(const std::vector<double>& signal, std::vector<double>& cA, std::vector<double>& cD, WaveletType wavelet)
{
    if (signal.empty()) {
        cA.clear();
        cD.clear();
        return;
    }

    std::vector<double> padded = signal;
    if ((padded.size() % 2U) != 0U) {
        padded.push_back(padded.back());
    }

    const std::size_t n = padded.size();
    const std::size_t half = n / 2U;
    cA.assign(half, 0.0);
    cD.assign(half, 0.0);

    const WaveletFilters f = getFilters(wavelet);

    if (wavelet == WaveletType::Haar || n < 4U) {
        const double h0 = f.h[0];
        const double h1 = f.h[1];
        const double g0 = f.g[0];
        const double g1 = f.g[1];

        for (std::size_t i = 0U; i < half; ++i) {
            const double x0 = padded[2U * i];
            const double x1 = padded[2U * i + 1U];
            cA[i] = (h0 * x0) + (h1 * x1);
            cD[i] = (g0 * x0) + (g1 * x1);
        }
    } else { // Db4
        for (std::size_t i = 0U; i < half; ++i) {
            double sumA = 0.0;
            double sumD = 0.0;
            for (std::size_t k = 0U; k < 4U; ++k) {
                const std::size_t idx = (2U * i + k) % n;
                sumA += f.h[k] * padded[idx];
                sumD += f.g[k] * padded[idx];
            }
            cA[i] = sumA;
            cD[i] = sumD;
        }
    }
}

std::vector<double> idwt1D(
    const std::vector<double>& cA, const std::vector<double>& cD, WaveletType wavelet, std::size_t targetLength)
{
    if (cA.empty() || cD.empty() || cA.size() != cD.size()) {
        return {};
    }

    const std::size_t half = cA.size();
    const std::size_t n = half * 2U;
    std::vector<double> out(n, 0.0);

    const WaveletFilters f = getFilters(wavelet);

    if (wavelet == WaveletType::Haar || n < 4U) {
        const double h0 = f.h[0];
        const double h1 = f.h[1];
        const double g0 = f.g[0];
        const double g1 = f.g[1];

        for (std::size_t i = 0U; i < half; ++i) {
            out[2U * i] = (h0 * cA[i]) + (g0 * cD[i]);
            out[2U * i + 1U] = (h1 * cA[i]) + (g1 * cD[i]);
        }
    } else { // Db4
        for (std::size_t m = 0U; m < half; ++m) {
            const std::size_t nPrev = (m + half - 1U) % half;
            out[2U * m] = (f.h[0] * cA[m]) + (f.g[0] * cD[m]) + (f.h[2] * cA[nPrev]) + (f.g[2] * cD[nPrev]);
            out[2U * m + 1U] = (f.h[1] * cA[m]) + (f.g[1] * cD[m]) + (f.h[3] * cA[nPrev]) + (f.g[3] * cD[nPrev]);
        }
    }

    if (targetLength > 0U && targetLength < n) {
        out.resize(targetLength);
    }
    return out;
}

WaveletDecomposition1D wavedec(const std::vector<double>& signal, std::size_t levels, WaveletType wavelet)
{
    WaveletDecomposition1D decomp {};
    decomp.originalLength = signal.size();
    decomp.wavelet = wavelet;

    if (signal.empty() || levels == 0U) {
        return decomp;
    }

    std::vector<double> curApprox = signal;
    decomp.cD.reserve(levels);

    for (std::size_t l = 0U; l < levels; ++l) {
        std::vector<double> nextA {};
        std::vector<double> nextD {};
        dwt1D(curApprox, nextA, nextD, wavelet);

        decomp.cD.push_back(std::move(nextD));
        curApprox = std::move(nextA);

        if (curApprox.size() <= 2U) {
            break;
        }
    }
    decomp.cA = std::move(curApprox);
    return decomp;
}

std::vector<double> waverec(const WaveletDecomposition1D& decomp)
{
    if (decomp.cA.empty() || decomp.cD.empty()) {
        return decomp.cA;
    }

    std::vector<double> curApprox = decomp.cA;
    const auto numLevels = static_cast<int>(decomp.cD.size());

    for (int l = numLevels - 1; l >= 0; --l) {
        const auto levelIdx = static_cast<std::size_t>(l);
        const auto& d = decomp.cD[levelIdx];
        const std::size_t targetLen
            = (levelIdx == 0U && decomp.originalLength > 0U) ? decomp.originalLength : (d.size() * 2U);
        curApprox = idwt1D(curApprox, d, decomp.wavelet, targetLen);
    }
    return curApprox;
}

std::vector<double> waveletDenoise(const std::vector<double>& signal, std::size_t levels, WaveletType wavelet)
{
    if (signal.size() <= 4U || levels == 0U) {
        return signal;
    }

    WaveletDecomposition1D decomp = wavedec(signal, levels, wavelet);
    if (decomp.cD.empty()) {
        return signal;
    }

    // Estimate noise standard deviation sigma from median absolute deviation (MAD) of level-1 details
    std::vector<double> absDetails = decomp.cD[0];
    for (auto& val : absDetails) {
        val = std::abs(val);
    }
    std::sort(absDetails.begin(), absDetails.end());

    double median = 0.0;
    const std::size_t sz = absDetails.size();
    if (sz > 0U) {
        median = (sz % 2U == 0U) ? 0.5 * (absDetails[sz / 2U - 1U] + absDetails[sz / 2U]) : absDetails[sz / 2U];
    }
    const double sigma = median / 0.6745;

    // Universal VisuShrink threshold: lambda = sigma * sqrt(2 * ln(N))
    const double nDbl = static_cast<double>(signal.size());
    const double lambda = sigma * std::sqrt(2.0 * std::log(nDbl));

    // Soft threshold detail bands
    for (auto& band : decomp.cD) {
        for (auto& coeff : band) {
            const double mag = std::abs(coeff);
            if (mag <= lambda) {
                coeff = 0.0;
            } else {
                coeff = std::copysign(mag - lambda, coeff);
            }
        }
    }

    return waverec(decomp);
}

WaveletCoefficients2D dwt2D(const std::vector<double>& image, std::size_t rows, std::size_t cols, WaveletType wavelet)
{
    WaveletCoefficients2D result {};
    if (rows == 0U || cols == 0U || (rows % 2U) != 0U || (cols % 2U) != 0U || image.size() != (rows * cols)) {
        return result;
    }

    const std::size_t halfRows = rows / 2U;
    const std::size_t halfCols = cols / 2U;

    result.rows = halfRows;
    result.cols = halfCols;
    result.LL.assign(halfRows * halfCols, 0.0);
    result.LH.assign(halfRows * halfCols, 0.0);
    result.HL.assign(halfRows * halfCols, 0.0);
    result.HH.assign(halfRows * halfCols, 0.0);

    // Intermediate horizontal row transforms: Low-pass (L) and High-pass (H)
    std::vector<double> rowL(rows * halfCols, 0.0);
    std::vector<double> rowH(rows * halfCols, 0.0);

    std::vector<double> rowBuffer(cols, 0.0);
    std::vector<double> cA(halfCols, 0.0);
    std::vector<double> cD(halfCols, 0.0);

    for (std::size_t r = 0U; r < rows; ++r) {
        const std::size_t rowOffset = r * cols;
        for (std::size_t c = 0U; c < cols; ++c) {
            rowBuffer[c] = image[rowOffset + c];
        }
        dwt1D(rowBuffer, cA, cD, wavelet);

        const std::size_t outRowOffset = r * halfCols;
        for (std::size_t c = 0U; c < halfCols; ++c) {
            rowL[outRowOffset + c] = cA[c];
            rowH[outRowOffset + c] = cD[c];
        }
    }

    // Vertical column transforms
    std::vector<double> colBuffer(rows, 0.0);
    std::vector<double> colA(halfRows, 0.0);
    std::vector<double> colD(halfRows, 0.0);

    // Process L columns -> LL and LH
    for (std::size_t c = 0U; c < halfCols; ++c) {
        for (std::size_t r = 0U; r < rows; ++r) {
            colBuffer[r] = rowL[r * halfCols + c];
        }
        dwt1D(colBuffer, colA, colD, wavelet);
        for (std::size_t r = 0U; r < halfRows; ++r) {
            result.LL[r * halfCols + c] = colA[r];
            result.LH[r * halfCols + c] = colD[r];
        }
    }

    // Process H columns -> HL and HH
    for (std::size_t c = 0U; c < halfCols; ++c) {
        for (std::size_t r = 0U; r < rows; ++r) {
            colBuffer[r] = rowH[r * halfCols + c];
        }
        dwt1D(colBuffer, colA, colD, wavelet);
        for (std::size_t r = 0U; r < halfRows; ++r) {
            result.HL[r * halfCols + c] = colA[r];
            result.HH[r * halfCols + c] = colD[r];
        }
    }

    return result;
}

std::vector<double> idwt2D(const WaveletCoefficients2D& coeffs, WaveletType wavelet)
{
    const std::size_t halfRows = coeffs.rows;
    const std::size_t halfCols = coeffs.cols;
    if (halfRows == 0U || halfCols == 0U) {
        return {};
    }

    const std::size_t rows = halfRows * 2U;
    const std::size_t cols = halfCols * 2U;

    std::vector<double> rowL(rows * halfCols, 0.0);
    std::vector<double> rowH(rows * halfCols, 0.0);

    std::vector<double> colA(halfRows, 0.0);
    std::vector<double> colD(halfRows, 0.0);

    // Inverse vertical column transforms: (LL, LH) -> rowL, (HL, HH) -> rowH
    for (std::size_t c = 0U; c < halfCols; ++c) {
        for (std::size_t r = 0U; r < halfRows; ++r) {
            colA[r] = coeffs.LL[r * halfCols + c];
            colD[r] = coeffs.LH[r * halfCols + c];
        }
        const auto reconstructedColL = idwt1D(colA, colD, wavelet, rows);
        for (std::size_t r = 0U; r < rows; ++r) {
            rowL[r * halfCols + c] = reconstructedColL[r];
        }

        for (std::size_t r = 0U; r < halfRows; ++r) {
            colA[r] = coeffs.HL[r * halfCols + c];
            colD[r] = coeffs.HH[r * halfCols + c];
        }
        const auto reconstructedColH = idwt1D(colA, colD, wavelet, rows);
        for (std::size_t r = 0U; r < rows; ++r) {
            rowH[r * halfCols + c] = reconstructedColH[r];
        }
    }

    // Inverse horizontal row transforms: (rowL, rowH) -> output image
    std::vector<double> result(rows * cols, 0.0);
    std::vector<double> cA(halfCols, 0.0);
    std::vector<double> cD(halfCols, 0.0);

    for (std::size_t r = 0U; r < rows; ++r) {
        const std::size_t inRowOffset = r * halfCols;
        for (std::size_t c = 0U; c < halfCols; ++c) {
            cA[c] = rowL[inRowOffset + c];
            cD[c] = rowH[inRowOffset + c];
        }
        const auto reconstructedRow = idwt1D(cA, cD, wavelet, cols);
        const std::size_t outRowOffset = r * cols;
        for (std::size_t c = 0U; c < cols; ++c) {
            result[outRowOffset + c] = reconstructedRow[c];
        }
    }

    return result;
}

} // namespace Math
