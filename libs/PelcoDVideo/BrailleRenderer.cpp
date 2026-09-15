#include "BrailleRenderer.h"

#include <algorithm>
#include <cmath>

namespace PelcoD::Video {

namespace {

// Standard Bayer 4x4 Dithering Matrix
constexpr int kBayer4x4[4][4] = {
    { 0, 8, 2, 10 },
    { 12, 4, 14, 6 },
    { 3, 11, 1, 9 },
    { 15, 7, 13, 5 }
};

// Standard Bayer 8x8 Dithering Matrix
constexpr int kBayer8x8[8][8] = {
    { 0, 32, 8, 40, 2, 34, 10, 42 },
    { 48, 16, 56, 24, 50, 18, 58, 26 },
    { 12, 44, 4, 36, 14, 46, 6, 38 },
    { 60, 28, 52, 20, 62, 30, 54, 22 },
    { 3, 35, 11, 43, 1, 33, 9, 41 },
    { 51, 19, 59, 27, 49, 17, 57, 25 },
    { 15, 47, 7, 39, 13, 45, 5, 37 },
    { 63, 31, 55, 23, 61, 29, 53, 21 }
};

// Unicode Braille dot position bitmask layout:
// (dx=0, dy=0) -> 0x01 | (dx=1, dy=0) -> 0x08
// (dx=0, dy=1) -> 0x02 | (dx=1, dy=1) -> 0x10
// (dx=0, dy=2) -> 0x04 | (dx=1, dy=2) -> 0x20
// (dx=0, dy=3) -> 0x40 | (dx=1, dy=3) -> 0x80
constexpr int kDotMap[4][2] = {
    { 0x01, 0x08 },
    { 0x02, 0x10 },
    { 0x04, 0x20 },
    { 0x40, 0x80 }
};

inline std::uint8_t clampToUint8(int val) noexcept
{
    return static_cast<std::uint8_t>(std::clamp(val, 0, 255));
}

} // namespace

std::string BrailleRenderer::utf8BrailleChar(std::uint8_t dotMask) noexcept
{
    // Braille patterns reside in Unicode block U+2800 .. U+28FF
    // UTF-8 encoding:
    // Byte 0: 0xE2
    // Byte 1: 0xA0 | (dotMask >> 6)
    // Byte 2: 0x80 | (dotMask & 0x3F)
    std::string s(3, '\0');
    s[0] = static_cast<char>(0xE2);
    s[1] = static_cast<char>(0xA0 | ((dotMask >> 6) & 0x03));
    s[2] = static_cast<char>(0x80 | (dotMask & 0x3F));
    return s;
}

void BrailleRenderer::applyPalette(TuiColorPalette palette, std::uint8_t inR, std::uint8_t inG, std::uint8_t inB,
    std::uint8_t& outR, std::uint8_t& outG, std::uint8_t& outB) noexcept
{
    const std::uint8_t luma = calculateLuma(inR, inG, inB);

    switch (palette) {
    case TuiColorPalette::TrueColor:
        outR = inR;
        outG = inG;
        outB = inB;
        break;

    case TuiColorPalette::Amber:
        outR = static_cast<std::uint8_t>((255U * luma) / 255U);
        outG = static_cast<std::uint8_t>((176U * luma) / 255U);
        outB = 0U;
        break;

    case TuiColorPalette::NightVisionGreen:
        outR = 0U;
        outG = static_cast<std::uint8_t>((255U * luma) / 255U);
        outB = static_cast<std::uint8_t>((100U * luma) / 255U);
        break;

    case TuiColorPalette::CyanHud:
        outR = 0U;
        outG = static_cast<std::uint8_t>((230U * luma) / 255U);
        outB = static_cast<std::uint8_t>((255U * luma) / 255U);
        break;

    case TuiColorPalette::Monochrome:
    default: {
        const std::uint8_t v = static_cast<std::uint8_t>((240U * luma) / 255U);
        outR = v;
        outG = v;
        outB = v;
        break;
    }
    }
}

const char* BrailleRenderer::ditherName(DitherAlgorithm alg) noexcept
{
    switch (alg) {
    case DitherAlgorithm::None:
        return "None (Direct)";
    case DitherAlgorithm::Bayer4x4:
        return "Bayer 4x4";
    case DitherAlgorithm::Bayer8x8:
        return "Bayer 8x8";
    case DitherAlgorithm::FloydSteinberg:
        return "Floyd-Steinberg";
    case DitherAlgorithm::Atkinson:
        return "Atkinson";
    default:
        return "Unknown";
    }
}

const char* BrailleRenderer::paletteName(TuiColorPalette pal) noexcept
{
    switch (pal) {
    case TuiColorPalette::TrueColor:
        return "TrueColor";
    case TuiColorPalette::Amber:
        return "Tactical Amber";
    case TuiColorPalette::NightVisionGreen:
        return "Night Vision";
    case TuiColorPalette::CyanHud:
        return "Cyan HUD";
    case TuiColorPalette::Monochrome:
        return "Monochrome";
    default:
        return "Unknown";
    }
}

const char* BrailleRenderer::renderModeName(TuiRenderMode mode) noexcept
{
    switch (mode) {
    case TuiRenderMode::Braille:
        return "Braille (2x4)";
    case TuiRenderMode::HalfBlock:
        return "Half-Block (1x2)";
    default:
        return "Unknown";
    }
}

void BrailleRenderer::renderFrame(const std::uint8_t* rgbData, int srcWidth, int srcHeight, int targetCols,
    int targetRows, const BrailleRenderOptions& options, std::vector<TerminalPixelCell>& outCells)
{
    if (!rgbData || srcWidth <= 0 || srcHeight <= 0 || targetCols <= 0 || targetRows <= 0) {
        outCells.clear();
        return;
    }

    if (options.mode == TuiRenderMode::HalfBlock) {
        renderHalfBlockGrid(rgbData, srcWidth, srcHeight, targetCols, targetRows, options, outCells);
    } else {
        renderBrailleGrid(rgbData, srcWidth, srcHeight, targetCols, targetRows, options, outCells);
    }
}

void BrailleRenderer::renderBrailleGrid(const std::uint8_t* rgbData, int srcWidth, int srcHeight, int targetCols,
    int targetRows, const BrailleRenderOptions& options, std::vector<TerminalPixelCell>& outCells)
{
    // Each Braille character cell spans 2 subpixels horizontally and 4 vertically
    const int subW = targetCols * 2;
    const int subH = targetRows * 4;

    const std::size_t subpixelCount = static_cast<std::size_t>(subW * subH);
    std::vector<float> lumaGrid(subpixelCount, 0.0f);
    std::vector<std::uint8_t> rGrid(subpixelCount, 0U);
    std::vector<std::uint8_t> gGrid(subpixelCount, 0U);
    std::vector<std::uint8_t> bGrid(subpixelCount, 0U);

    // 1. Sample RGB frame down to subpixel resolution with contrast/brightness adjustments
    for (int sy = 0; sy < subH; ++sy) {
        const int srcY = (sy * srcHeight) / subH;
        const int srcRowOffset = srcY * srcWidth * 3;

        for (int sx = 0; sx < subW; ++sx) {
            const int srcX = (sx * srcWidth) / subW;
            const int srcIdx = srcRowOffset + srcX * 3;

            const std::uint8_t r = rgbData[srcIdx];
            const std::uint8_t g = rgbData[srcIdx + 1];
            const std::uint8_t b = rgbData[srcIdx + 2];

            const std::size_t idx = static_cast<std::size_t>(sy * subW + sx);
            rGrid[idx] = r;
            gGrid[idx] = g;
            bGrid[idx] = b;

            float luma = static_cast<float>(calculateLuma(r, g, b));
            // Apply contrast and brightness adjustments
            luma = (luma - 128.0f) * static_cast<float>(options.contrast) + 128.0f + static_cast<float>(options.brightness);
            if (options.invert) {
                luma = 255.0f - luma;
            }
            lumaGrid[idx] = std::clamp(luma, 0.0f, 255.0f);
        }
    }

    // 2. Apply quantization / dithering to determine binary dot activation
    std::vector<bool> dotActive(subpixelCount, false);

    switch (options.dither) {
    case DitherAlgorithm::None: {
        for (std::size_t i = 0; i < subpixelCount; ++i) {
            dotActive[i] = (lumaGrid[i] >= 128.0f);
        }
        break;
    }

    case DitherAlgorithm::Bayer4x4: {
        for (int sy = 0; sy < subH; ++sy) {
            const int by = sy % 4;
            for (int sx = 0; sx < subW; ++sx) {
                const int bx = sx % 4;
                const float threshold = (kBayer4x4[by][bx] + 0.5f) * (255.0f / 16.0f);
                const std::size_t idx = static_cast<std::size_t>(sy * subW + sx);
                dotActive[idx] = (lumaGrid[idx] >= threshold);
            }
        }
        break;
    }

    case DitherAlgorithm::Bayer8x8: {
        for (int sy = 0; sy < subH; ++sy) {
            const int by = sy % 8;
            for (int sx = 0; sx < subW; ++sx) {
                const int bx = sx % 8;
                const float threshold = (kBayer8x8[by][bx] + 0.5f) * (255.0f / 64.0f);
                const std::size_t idx = static_cast<std::size_t>(sy * subW + sx);
                dotActive[idx] = (lumaGrid[idx] >= threshold);
            }
        }
        break;
    }

    case DitherAlgorithm::FloydSteinberg: {
        for (int sy = 0; sy < subH; ++sy) {
            for (int sx = 0; sx < subW; ++sx) {
                const std::size_t idx = static_cast<std::size_t>(sy * subW + sx);
                const float oldVal = lumaGrid[idx];
                const float newVal = (oldVal >= 128.0f) ? 255.0f : 0.0f;
                dotActive[idx] = (newVal > 0.0f);
                const float err = oldVal - newVal;

                if (sx + 1 < subW) {
                    lumaGrid[static_cast<std::size_t>(sy * subW + (sx + 1))] += err * (7.0f / 16.0f);
                }
                if (sy + 1 < subH) {
                    if (sx - 1 >= 0) {
                        lumaGrid[static_cast<std::size_t>((sy + 1) * subW + (sx - 1))] += err * (3.0f / 16.0f);
                    }
                    lumaGrid[static_cast<std::size_t>((sy + 1) * subW + sx)] += err * (5.0f / 16.0f);
                    if (sx + 1 < subW) {
                        lumaGrid[static_cast<std::size_t>((sy + 1) * subW + (sx + 1))] += err * (1.0f / 16.0f);
                    }
                }
            }
        }
        break;
    }

    case DitherAlgorithm::Atkinson: {
        for (int sy = 0; sy < subH; ++sy) {
            for (int sx = 0; sx < subW; ++sx) {
                const std::size_t idx = static_cast<std::size_t>(sy * subW + sx);
                const float oldVal = lumaGrid[idx];
                const float newVal = (oldVal >= 128.0f) ? 255.0f : 0.0f;
                dotActive[idx] = (newVal > 0.0f);
                const float err8 = (oldVal - newVal) * 0.125f; // 1/8th diffusion

                if (sx + 1 < subW) {
                    lumaGrid[static_cast<std::size_t>(sy * subW + (sx + 1))] += err8;
                }
                if (sx + 2 < subW) {
                    lumaGrid[static_cast<std::size_t>(sy * subW + (sx + 2))] += err8;
                }
                if (sy + 1 < subH) {
                    if (sx - 1 >= 0) {
                        lumaGrid[static_cast<std::size_t>((sy + 1) * subW + (sx - 1))] += err8;
                    }
                    lumaGrid[static_cast<std::size_t>((sy + 1) * subW + sx)] += err8;
                    if (sx + 1 < subW) {
                        lumaGrid[static_cast<std::size_t>((sy + 1) * subW + (sx + 1))] += err8;
                    }
                }
                if (sy + 2 < subH) {
                    lumaGrid[static_cast<std::size_t>((sy + 2) * subW + sx)] += err8;
                }
            }
        }
        break;
    }
    }

    // 3. Assemble 2x4 subpixels into Braille characters and average active dot colors
    outCells.resize(static_cast<std::size_t>(targetCols * targetRows));

    for (int cy = 0; cy < targetRows; ++cy) {
        for (int cx = 0; cx < targetCols; ++cx) {
            std::uint8_t dotMask = 0U;
            int actR = 0, actG = 0, actB = 0;
            int allR = 0, allG = 0, allB = 0;
            int activeCount = 0;

            for (int dy = 0; dy < 4; ++dy) {
                const int sy = cy * 4 + dy;
                for (int dx = 0; dx < 2; ++dx) {
                    const int sx = cx * 2 + dx;
                    const std::size_t idx = static_cast<std::size_t>(sy * subW + sx);

                    const std::uint8_t r = rGrid[idx];
                    const std::uint8_t g = gGrid[idx];
                    const std::uint8_t b = bGrid[idx];

                    allR += r;
                    allG += g;
                    allB += b;

                    if (dotActive[idx]) {
                        dotMask |= static_cast<std::uint8_t>(kDotMap[dy][dx]);
                        actR += r;
                        actG += g;
                        actB += b;
                        activeCount++;
                    }
                }
            }

            std::uint8_t fgR = 0U, fgG = 0U, fgB = 0U;
            if (activeCount > 0) {
                fgR = static_cast<std::uint8_t>(actR / activeCount);
                fgG = static_cast<std::uint8_t>(actG / activeCount);
                fgB = static_cast<std::uint8_t>(actB / activeCount);
            } else {
                fgR = static_cast<std::uint8_t>(allR / 8);
                fgG = static_cast<std::uint8_t>(allG / 8);
                fgB = static_cast<std::uint8_t>(allB / 8);
            }

            std::uint8_t finalR = fgR, finalG = fgG, finalB = fgB;
            applyPalette(options.palette, fgR, fgG, fgB, finalR, finalG, finalB);

            const std::size_t cellIdx = static_cast<std::size_t>(cy * targetCols + cx);
            auto& cell = outCells[cellIdx];
            cell.utf8Text = utf8BrailleChar(dotMask);
            cell.fgR = finalR;
            cell.fgG = finalG;
            cell.fgB = finalB;
            cell.hasBg = false;
        }
    }
}

void BrailleRenderer::renderHalfBlockGrid(const std::uint8_t* rgbData, int srcWidth, int srcHeight, int targetCols,
    int targetRows, const BrailleRenderOptions& options, std::vector<TerminalPixelCell>& outCells)
{
    // Half-block mode: 1 subpixel horizontally, 2 vertically per cell
    const int subW = targetCols;
    const int subH = targetRows * 2;

    outCells.resize(static_cast<std::size_t>(targetCols * targetRows));

    for (int cy = 0; cy < targetRows; ++cy) {
        const int topY = (cy * 2 * srcHeight) / subH;
        const int botY = ((cy * 2 + 1) * srcHeight) / subH;

        const int topRowOffset = topY * srcWidth * 3;
        const int botRowOffset = botY * srcWidth * 3;

        for (int cx = 0; cx < targetCols; ++cx) {
            const int srcX = (cx * srcWidth) / subW;

            const int topIdx = topRowOffset + srcX * 3;
            const int botIdx = botRowOffset + srcX * 3;

            std::uint8_t topR = rgbData[topIdx];
            std::uint8_t topG = rgbData[topIdx + 1];
            std::uint8_t topB = rgbData[topIdx + 2];

            std::uint8_t botR = rgbData[botIdx];
            std::uint8_t botG = rgbData[botIdx + 1];
            std::uint8_t botB = rgbData[botIdx + 2];

            std::uint8_t finalTopR = topR, finalTopG = topG, finalTopB = topB;
            std::uint8_t finalBotR = botR, finalBotG = botG, finalBotB = botB;

            applyPalette(options.palette, topR, topG, topB, finalTopR, finalTopG, finalTopB);
            applyPalette(options.palette, botR, botG, botB, finalBotR, finalBotG, finalBotB);

            const std::size_t cellIdx = static_cast<std::size_t>(cy * targetCols + cx);
            auto& cell = outCells[cellIdx];
            // UTF-8 upper half block character: U+2580 ("▀")
            cell.utf8Text = "\xE2\x96\x80";
            cell.fgR = finalTopR;
            cell.fgG = finalTopG;
            cell.fgB = finalTopB;
            cell.bgR = finalBotR;
            cell.bgG = finalBotG;
            cell.bgB = finalBotB;
            cell.hasBg = true;
        }
    }
}

} // namespace PelcoD::Video
