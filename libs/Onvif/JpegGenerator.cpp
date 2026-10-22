/// @file JpegGenerator.cpp
/// @brief Standards-compliant lightweight JFIF baseline JPEG encoder and test card generator.

#include "JpegGenerator.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>

namespace Onvif {

namespace {

    // ============================================================================
    // JPEG Constants & Standard Quantization / Huffman Tables (ITU-T T.81 / Annex K)
    // ============================================================================

    constexpr std::uint8_t kJpegMarkerPrefix = 0xFFU;
    constexpr std::uint8_t kJpegSoi = 0xD8U;
    constexpr std::uint8_t kJpegEoi = 0xD9U;
    constexpr std::uint8_t kJpegApp0 = 0xE0U;
    constexpr std::uint8_t kJpegDqt = 0xDBU;
    constexpr std::uint8_t kJpegSof0 = 0xC0U;
    constexpr std::uint8_t kJpegDht = 0xC4U;
    constexpr std::uint8_t kJpegSos = 0xDAU;

    constexpr double kPi = 3.14159265358979323846;

    // Standard Zig-Zag reordering array
    constexpr std::array<std::uint8_t, 64> kZigZag = { 0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19,
        26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37,
        44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63 };

    // Standard Luminance Quantization Table (Table K.1)
    constexpr std::array<std::uint8_t, 64> kStdLumaQuant = { 16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60,
        55, 14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62, 18, 22, 37, 56, 68, 109, 103, 77, 24, 35,
        55, 64, 81, 104, 113, 92, 49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99 };

    // Standard Chrominance Quantization Table (Table K.2)
    constexpr std::array<std::uint8_t, 64> kStdChromaQuant = { 17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99,
        99, 99, 24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99 };

    // Standard Huffman Tables (Table K.3 - K.6)
    constexpr std::array<std::uint8_t, 16> kLumaDcBits = { 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
    constexpr std::array<std::uint8_t, 12> kLumaDcVal = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

    constexpr std::array<std::uint8_t, 16> kChromaDcBits = { 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
    constexpr std::array<std::uint8_t, 12> kChromaDcVal = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

    constexpr std::array<std::uint8_t, 16> kLumaAcBits = { 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125 };
    constexpr std::array<std::uint8_t, 162> kLumaAcVal = { 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31,
        0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1,
        0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26,
        0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73,
        0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94,
        0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
        0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
        0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2,
        0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa };

    constexpr std::array<std::uint8_t, 16> kChromaAcBits = { 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119 };
    constexpr std::array<std::uint8_t, 162> kChromaAcVal = { 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06,
        0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09,
        0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19,
        0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
        0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92,
        0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2,
        0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
        0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2,
        0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa };

    struct HuffEntry {
        std::uint16_t code { 0 };
        std::uint8_t len { 0 };
    };

    // Generates Huffman lookup table for symbols [0..255]
    std::array<HuffEntry, 256> buildHuffTable(const std::uint8_t* bits, const std::uint8_t* val, std::size_t numVal)
    {
        std::array<HuffEntry, 256> table {};
        std::uint16_t code { 0 };
        std::size_t valIdx { 0 };

        for (std::uint8_t len = 1; len <= 16; ++len) {
            for (std::uint8_t count = 0; count < bits[len - 1]; ++count) {
                if (valIdx < numVal) {
                    const std::uint8_t sym = val[valIdx++];
                    table[sym].code = code;
                    table[sym].len = len;
                    ++code;
                }
            }
            code = static_cast<std::uint16_t>(code << 1U);
        }
        return table;
    }

    // BitWriter packing MSB first and handling 0xFF byte stuffing
    class BitWriter {
    public:
        explicit BitWriter(std::vector<std::uint8_t>& out) noexcept
            : m_out(out)
        {
        }

        void write(std::uint32_t bits, std::uint8_t count)
        {
            if (count == 0U) {
                return;
            }

            m_bitBuffer = (m_bitBuffer << count) | (bits & ((1U << count) - 1U));
            m_bitCount += count;

            while (m_bitCount >= 8) {
                m_bitCount -= 8;
                const auto byte = static_cast<std::uint8_t>((m_bitBuffer >> m_bitCount) & 0xFFU);
                m_out.push_back(byte);
                if (byte == kJpegMarkerPrefix) {
                    m_out.push_back(0x00U); // Byte stuffing per JPEG standard
                }
            }
        }

        void flush()
        {
            if (m_bitCount > 0) {
                const auto padBits = static_cast<std::uint8_t>(8 - m_bitCount);
                write((1U << padBits) - 1U, padBits);
            }
        }

    private:
        std::vector<std::uint8_t>& m_out;
        std::uint32_t m_bitBuffer { 0 };
        int m_bitCount { 0 };
    };

    [[nodiscard]] std::uint8_t getCategory(std::int32_t val) noexcept
    {
        std::uint32_t absVal = static_cast<std::uint32_t>(val < 0 ? -val : val);
        std::uint8_t cat { 0 };
        while (absVal > 0U) {
            ++cat;
            absVal >>= 1U;
        }
        return cat;
    }

    // Precomputes scaled quantization table based on quality (1..100)
    std::array<std::uint8_t, 64> scaleQuantTable(const std::array<std::uint8_t, 64>& base, int quality)
    {
        const int clampedQ = std::clamp(quality, 1, 100);
        const int scale = (clampedQ < 50) ? (5000 / clampedQ) : (200 - clampedQ * 2);

        std::array<std::uint8_t, 64> result {};
        for (std::size_t i = 0; i < 64; ++i) {
            const std::int32_t val = (static_cast<std::int32_t>(base[i]) * scale + 50) / 100;
            result[i] = static_cast<std::uint8_t>(std::clamp(val, 1, 255));
        }
        return result;
    }

    // Standard 2D Forward DCT on an 8x8 block
    void computeFdct(const float in[8][8], float out[8][8]) noexcept
    {
        float temp[8][8] {};

        for (int y = 0; y < 8; ++y) {
            for (int u = 0; u < 8; ++u) {
                double sum { 0.0 };
                for (int x = 0; x < 8; ++x) {
                    sum += static_cast<double>(in[y][x])
                        * std::cos((2.0 * static_cast<double>(x) + 1.0) * static_cast<double>(u) * kPi / 16.0);
                }
                const double cu = (u == 0) ? (1.0 / std::sqrt(2.0)) : 1.0;
                temp[y][u] = static_cast<float>(sum * cu * 0.5);
            }
        }

        for (int u = 0; u < 8; ++u) {
            for (int v = 0; v < 8; ++v) {
                double sum { 0.0 };
                for (int y = 0; y < 8; ++y) {
                    sum += static_cast<double>(temp[y][u])
                        * std::cos((2.0 * static_cast<double>(y) + 1.0) * static_cast<double>(v) * kPi / 16.0);
                }
                const double cv = (v == 0) ? (1.0 / std::sqrt(2.0)) : 1.0;
                out[v][u] = static_cast<float>(sum * cv * 0.5);
            }
        }
    }

    // Encodes an 8x8 block into the entropy bitstream
    void encodeBlock(const float block[8][8], const std::array<std::uint8_t, 64>& qTable, std::int32_t& prevDc,
        const std::array<HuffEntry, 256>& dcTable, const std::array<HuffEntry, 256>& acTable, BitWriter& bitWriter)
    {
        float dct[8][8] {};
        computeFdct(block, dct);

        // Quantize and reorder to zig-zag
        std::array<std::int32_t, 64> zigZag {};
        for (int v = 0; v < 8; ++v) {
            for (int u = 0; u < 8; ++u) {
                const int natIdx = v * 8 + u;
                const float val = dct[v][u] / static_cast<float>(qTable[static_cast<std::size_t>(natIdx)]);
                zigZag[kZigZag[static_cast<std::size_t>(natIdx)]] = static_cast<std::int32_t>(std::round(val));
            }
        }

        // DC encoding
        const std::int32_t diff = zigZag[0] - prevDc;
        prevDc = zigZag[0];

        const std::uint8_t dcCat = getCategory(diff);
        bitWriter.write(dcTable[dcCat].code, dcTable[dcCat].len);
        if (dcCat > 0U) {
            const std::uint32_t valBits
                = static_cast<std::uint32_t>((diff < 0) ? (diff - 1) : diff) & ((1U << dcCat) - 1U);
            bitWriter.write(valBits, dcCat);
        }

        // Find last non-zero AC coefficient
        int lastNonZero { 0 };
        for (int i = 63; i >= 1; --i) {
            if (zigZag[static_cast<std::size_t>(i)] != 0) {
                lastNonZero = i;
                break;
            }
        }

        // AC encoding
        int zeroRun { 0 };
        for (int i = 1; i <= lastNonZero; ++i) {
            const std::int32_t val = zigZag[static_cast<std::size_t>(i)];
            if (val == 0) {
                ++zeroRun;
            } else {
                while (zeroRun >= 16) {
                    bitWriter.write(acTable[0xF0].code, acTable[0xF0].len); // ZRL
                    zeroRun -= 16;
                }
                const std::uint8_t acCat = getCategory(val);
                const auto symbol = static_cast<std::uint8_t>((zeroRun << 4) | acCat);
                bitWriter.write(acTable[symbol].code, acTable[symbol].len);

                const std::uint32_t valBits
                    = static_cast<std::uint32_t>((val < 0) ? (val - 1) : val) & ((1U << acCat) - 1U);
                bitWriter.write(valBits, acCat);
                zeroRun = 0;
            }
        }

        if (lastNonZero < 63) {
            bitWriter.write(acTable[0x00].code, acTable[0x00].len); // EOB
        }
    }

    // 5x7 ASCII bitmap font (characters 32 to 90: space through Z)
    constexpr std::uint8_t kFont5x7[59][5] = {
        { 0x00, 0x00, 0x00, 0x00, 0x00 }, // ' ' (32)
        { 0x00, 0x00, 0x5F, 0x00, 0x00 }, // '!'
        { 0x00, 0x07, 0x00, 0x07, 0x00 }, // '"'
        { 0x14, 0x7F, 0x14, 0x7F, 0x14 }, // '#'
        { 0x24, 0x2A, 0x7F, 0x2A, 0x12 }, // '$'
        { 0x23, 0x13, 0x08, 0x64, 0x62 }, // '%'
        { 0x36, 0x49, 0x55, 0x22, 0x50 }, // '&'
        { 0x00, 0x05, 0x03, 0x00, 0x00 }, // '''
        { 0x00, 0x1C, 0x22, 0x41, 0x00 }, // '('
        { 0x00, 0x41, 0x22, 0x1C, 0x00 }, // ')'
        { 0x14, 0x08, 0x3E, 0x08, 0x14 }, // '*'
        { 0x08, 0x08, 0x3E, 0x08, 0x08 }, // '+'
        { 0x00, 0x50, 0x30, 0x00, 0x00 }, // ','
        { 0x08, 0x08, 0x08, 0x08, 0x08 }, // '-'
        { 0x00, 0x60, 0x60, 0x00, 0x00 }, // '.'
        { 0x20, 0x10, 0x08, 0x04, 0x02 }, // '/'
        { 0x3E, 0x51, 0x49, 0x45, 0x3E }, // '0' (48)
        { 0x00, 0x42, 0x7F, 0x40, 0x00 }, // '1'
        { 0x42, 0x61, 0x51, 0x49, 0x46 }, // '2'
        { 0x21, 0x41, 0x45, 0x4B, 0x31 }, // '3'
        { 0x18, 0x14, 0x12, 0x7F, 0x10 }, // '4'
        { 0x27, 0x45, 0x45, 0x45, 0x39 }, // '5'
        { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, // '6'
        { 0x01, 0x71, 0x09, 0x05, 0x03 }, // '7'
        { 0x36, 0x49, 0x49, 0x49, 0x36 }, // '8'
        { 0x06, 0x49, 0x49, 0x29, 0x1E }, // '9'
        { 0x00, 0x36, 0x36, 0x00, 0x00 }, // ':'
        { 0x00, 0x56, 0x36, 0x00, 0x00 }, // ';'
        { 0x08, 0x14, 0x22, 0x41, 0x00 }, // '<'
        { 0x14, 0x14, 0x14, 0x14, 0x14 }, // '='
        { 0x00, 0x41, 0x22, 0x14, 0x08 }, // '>'
        { 0x02, 0x01, 0x51, 0x09, 0x06 }, // '?'
        { 0x32, 0x49, 0x79, 0x41, 0x3E }, // '@'
        { 0x7E, 0x11, 0x11, 0x11, 0x7E }, // 'A' (65)
        { 0x7F, 0x49, 0x49, 0x49, 0x36 }, // 'B'
        { 0x3E, 0x41, 0x41, 0x41, 0x22 }, // 'C'
        { 0x7F, 0x41, 0x41, 0x22, 0x1C }, // 'D'
        { 0x7F, 0x49, 0x49, 0x49, 0x41 }, // 'E'
        { 0x7F, 0x09, 0x09, 0x09, 0x01 }, // 'F'
        { 0x3E, 0x41, 0x49, 0x49, 0x7A }, // 'G'
        { 0x7F, 0x08, 0x08, 0x08, 0x7F }, // 'H'
        { 0x00, 0x41, 0x7F, 0x41, 0x00 }, // 'I'
        { 0x20, 0x40, 0x41, 0x3F, 0x01 }, // 'J'
        { 0x7F, 0x08, 0x14, 0x22, 0x41 }, // 'K'
        { 0x7F, 0x40, 0x40, 0x40, 0x40 }, // 'L'
        { 0x7F, 0x02, 0x0C, 0x02, 0x7F }, // 'M'
        { 0x7F, 0x04, 0x08, 0x10, 0x7F }, // 'N'
        { 0x3E, 0x41, 0x41, 0x41, 0x3E }, // 'O'
        { 0x7F, 0x09, 0x09, 0x09, 0x06 }, // 'P'
        { 0x3E, 0x41, 0x51, 0x21, 0x5E }, // 'Q'
        { 0x7F, 0x09, 0x19, 0x29, 0x46 }, // 'R'
        { 0x46, 0x49, 0x49, 0x49, 0x31 }, // 'S'
        { 0x01, 0x01, 0x7F, 0x01, 0x01 }, // 'T'
        { 0x3F, 0x40, 0x40, 0x40, 0x3F }, // 'U'
        { 0x1F, 0x20, 0x40, 0x20, 0x1F }, // 'V'
        { 0x3F, 0x40, 0x38, 0x40, 0x3F }, // 'W'
        { 0x63, 0x14, 0x08, 0x14, 0x63 }, // 'X'
        { 0x07, 0x08, 0x70, 0x08, 0x07 }, // 'Y'
        { 0x61, 0x51, 0x49, 0x45, 0x43 } // 'Z' (90)
    };

    void drawChar(std::vector<std::uint8_t>& rgb, int width, int height, int startX, int startY, char c, std::uint8_t r,
        std::uint8_t g, std::uint8_t b, int scale = 2)
    {
        char upper = c;
        if (upper >= 'a' && upper <= 'z') {
            upper = static_cast<char>(upper - ('a' - 'A'));
        }

        if (upper < 32 || upper > 90) {
            upper = ' ';
        }

        const int fontIdx = upper - 32;
        for (int col = 0; col < 5; ++col) {
            const std::uint8_t line = kFont5x7[fontIdx][col];
            for (int row = 0; row < 7; ++row) {
                if ((line & (1U << row)) != 0U) {
                    for (int sy = 0; sy < scale; ++sy) {
                        for (int sx = 0; sx < scale; ++sx) {
                            const int px = startX + col * scale + sx;
                            const int py = startY + row * scale + sy;
                            if (px >= 0 && px < width && py >= 0 && py < height) {
                                const std::size_t idx = static_cast<std::size_t>((py * width + px) * 3);
                                rgb[idx] = r;
                                rgb[idx + 1] = g;
                                rgb[idx + 2] = b;
                            }
                        }
                    }
                }
            }
        }
    }

    void drawText(std::vector<std::uint8_t>& rgb, int width, int height, int startX, int startY,
        const std::string& text, std::uint8_t r, std::uint8_t g, std::uint8_t b, int scale = 2)
    {
        int currX = startX;
        for (const char ch : text) {
            drawChar(rgb, width, height, currX, startY, ch, r, g, b, scale);
            currX += (5 * scale) + scale; // Advance with inter-character gap
        }
    }

    std::string getUtcTimestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tmBuf {};
#if defined(_WIN32)
        gmtime_s(&tmBuf, &tt);
#else
        gmtime_r(&tt, &tmBuf);
#endif
        std::array<char, 64> buf {};
        std::strftime(buf.data(), buf.size(), "%Y-%m-%d %H:%M:%S UTC", &tmBuf);
        return std::string(buf.data());
    }

} // namespace

std::vector<std::uint8_t> JpegGenerator::encodeRgb(const std::uint8_t* rgbData, int width, int height, int quality)
{
    if (rgbData == nullptr || width <= 0 || height <= 0) {
        return {};
    }

    const std::array<std::uint8_t, 64> qLuma = scaleQuantTable(kStdLumaQuant, quality);
    const std::array<std::uint8_t, 64> qChroma = scaleQuantTable(kStdChromaQuant, quality);

    const auto dcLumaTbl = buildHuffTable(kLumaDcBits.data(), kLumaDcVal.data(), kLumaDcVal.size());
    const auto dcChromaTbl = buildHuffTable(kChromaDcBits.data(), kChromaDcVal.data(), kChromaDcVal.size());
    const auto acLumaTbl = buildHuffTable(kLumaAcBits.data(), kLumaAcVal.data(), kLumaAcVal.size());
    const auto acChromaTbl = buildHuffTable(kChromaAcBits.data(), kChromaAcVal.data(), kChromaAcVal.size());

    std::vector<std::uint8_t> jpeg {};
    jpeg.reserve(static_cast<std::size_t>(width * height / 4));

    // 1. SOI
    jpeg.push_back(kJpegMarkerPrefix);
    jpeg.push_back(kJpegSoi);

    // 2. APP0 (JFIF)
    jpeg.push_back(kJpegMarkerPrefix);
    jpeg.push_back(kJpegApp0);
    jpeg.push_back(0x00U);
    jpeg.push_back(0x10U); // Length = 16
    jpeg.push_back(0x4AU); // 'J'
    jpeg.push_back(0x46U); // 'F'
    jpeg.push_back(0x49U); // 'I'
    jpeg.push_back(0x46U); // 'F'
    jpeg.push_back(0x00U);
    jpeg.push_back(0x01U); // Version 1.1
    jpeg.push_back(0x01U);
    jpeg.push_back(0x00U); // Aspect ratio units
    jpeg.push_back(0x00U);
    jpeg.push_back(0x01U); // X density = 1
    jpeg.push_back(0x00U);
    jpeg.push_back(0x01U); // Y density = 1
    jpeg.push_back(0x00U); // Thumbnail = 0x0
    jpeg.push_back(0x00U);

    // 3. DQT (Luma = Table 0)
    jpeg.push_back(kJpegMarkerPrefix);
    jpeg.push_back(kJpegDqt);
    jpeg.push_back(0x00U);
    jpeg.push_back(0x43U); // Length = 67
    jpeg.push_back(0x00U); // Precision 0, Table 0
    for (std::size_t i = 0; i < 64; ++i) {
        jpeg.push_back(qLuma[kZigZag[i]]);
    }

    // 4. DQT (Chroma = Table 1)
    jpeg.push_back(kJpegMarkerPrefix);
    jpeg.push_back(kJpegDqt);
    jpeg.push_back(0x00U);
    jpeg.push_back(0x43U); // Length = 67
    jpeg.push_back(0x01U); // Precision 0, Table 1
    for (std::size_t i = 0; i < 64; ++i) {
        jpeg.push_back(qChroma[kZigZag[i]]);
    }

    // 5. SOF0 (Baseline DCT)
    jpeg.push_back(kJpegMarkerPrefix);
    jpeg.push_back(kJpegSof0);
    jpeg.push_back(0x00U);
    jpeg.push_back(0x11U); // Length = 17
    jpeg.push_back(0x08U); // Precision = 8
    jpeg.push_back(static_cast<std::uint8_t>((height >> 8) & 0xFF));
    jpeg.push_back(static_cast<std::uint8_t>(height & 0xFF));
    jpeg.push_back(static_cast<std::uint8_t>((width >> 8) & 0xFF));
    jpeg.push_back(static_cast<std::uint8_t>(width & 0xFF));
    jpeg.push_back(0x03U); // 3 components (YCbCr 4:4:4)

    // Component Y (ID 1, H=1, V=1, QuantTable 0)
    jpeg.push_back(0x01U);
    jpeg.push_back(0x11U);
    jpeg.push_back(0x00U);

    // Component Cb (ID 2, H=1, V=1, QuantTable 1)
    jpeg.push_back(0x02U);
    jpeg.push_back(0x11U);
    jpeg.push_back(0x01U);

    // Component Cr (ID 3, H=1, V=1, QuantTable 1)
    jpeg.push_back(0x03U);
    jpeg.push_back(0x11U);
    jpeg.push_back(0x01U);

    // 6. DHT (Huffman Tables)
    // DHT 0: DC Luma
    jpeg.push_back(kJpegMarkerPrefix);
    jpeg.push_back(kJpegDht);
    const auto dht0Len = static_cast<std::uint16_t>(2U + 1U + 16U + kLumaDcVal.size());
    jpeg.push_back(static_cast<std::uint8_t>((dht0Len >> 8U) & 0xFFU));
    jpeg.push_back(static_cast<std::uint8_t>(dht0Len & 0xFFU));
    jpeg.push_back(0x00U); // Class 0 (DC), ID 0
    for (std::uint8_t b : kLumaDcBits) {
        jpeg.push_back(b);
    }
    for (std::uint8_t v : kLumaDcVal) {
        jpeg.push_back(v);
    }

    // DHT 1: AC Luma
    jpeg.push_back(kJpegMarkerPrefix);
    jpeg.push_back(kJpegDht);
    const auto dht1Len = static_cast<std::uint16_t>(2U + 1U + 16U + kLumaAcVal.size());
    jpeg.push_back(static_cast<std::uint8_t>((dht1Len >> 8U) & 0xFFU));
    jpeg.push_back(static_cast<std::uint8_t>(dht1Len & 0xFFU));
    jpeg.push_back(0x10U); // Class 1 (AC), ID 0
    for (std::uint8_t b : kLumaAcBits) {
        jpeg.push_back(b);
    }
    for (std::uint8_t v : kLumaAcVal) {
        jpeg.push_back(v);
    }

    // DHT 2: DC Chroma
    jpeg.push_back(kJpegMarkerPrefix);
    jpeg.push_back(kJpegDht);
    const auto dht2Len = static_cast<std::uint16_t>(2U + 1U + 16U + kChromaDcVal.size());
    jpeg.push_back(static_cast<std::uint8_t>((dht2Len >> 8U) & 0xFFU));
    jpeg.push_back(static_cast<std::uint8_t>(dht2Len & 0xFFU));
    jpeg.push_back(0x01U); // Class 0 (DC), ID 1
    for (std::uint8_t b : kChromaDcBits) {
        jpeg.push_back(b);
    }
    for (std::uint8_t v : kChromaDcVal) {
        jpeg.push_back(v);
    }

    // DHT 3: AC Chroma
    jpeg.push_back(kJpegMarkerPrefix);
    jpeg.push_back(kJpegDht);
    const auto dht3Len = static_cast<std::uint16_t>(2U + 1U + 16U + kChromaAcVal.size());
    jpeg.push_back(static_cast<std::uint8_t>((dht3Len >> 8U) & 0xFFU));
    jpeg.push_back(static_cast<std::uint8_t>(dht3Len & 0xFFU));
    jpeg.push_back(0x11U); // Class 1 (AC), ID 1
    for (std::uint8_t b : kChromaAcBits) {
        jpeg.push_back(b);
    }
    for (std::uint8_t v : kChromaAcVal) {
        jpeg.push_back(v);
    }

    // 7. SOS (Start of Scan)
    jpeg.push_back(kJpegMarkerPrefix);
    jpeg.push_back(kJpegSos);
    jpeg.push_back(0x00U);
    jpeg.push_back(0x0CU); // Length = 12
    jpeg.push_back(0x03U); // 3 components
    jpeg.push_back(0x01U); // Y: DC 0, AC 0
    jpeg.push_back(0x00U);
    jpeg.push_back(0x02U); // Cb: DC 1, AC 1
    jpeg.push_back(0x11U);
    jpeg.push_back(0x03U); // Cr: DC 1, AC 1
    jpeg.push_back(0x11U);
    jpeg.push_back(0x00U); // Spectral start = 0
    jpeg.push_back(0x3FU); // Spectral end = 63
    jpeg.push_back(0x00U); // Successive approximation = 0

    // 8. Entropy Coded Scan Data (MCUs in 4:4:4 order: Y, Cb, Cr)
    BitWriter bitWriter(jpeg);
    std::int32_t prevDcY { 0 };
    std::int32_t prevDcCb { 0 };
    std::int32_t prevDcCr { 0 };

    const int mcuCols = (width + 7) / 8;
    const int mcuRows = (height + 7) / 8;

    float blockY[8][8] {};
    float blockCb[8][8] {};
    float blockCr[8][8] {};

    for (int my = 0; my < mcuRows; ++my) {
        for (int mx = 0; mx < mcuCols; ++mx) {
            // Fill 8x8 blocks with edge clamping
            for (int y = 0; y < 8; ++y) {
                const int py = std::min(my * 8 + y, height - 1);
                for (int x = 0; x < 8; ++x) {
                    const int px = std::min(mx * 8 + x, width - 1);
                    const std::size_t pixelIdx = static_cast<std::size_t>((py * width + px) * 3);
                    const auto r = static_cast<float>(rgbData[pixelIdx]);
                    const auto g = static_cast<float>(rgbData[pixelIdx + 1]);
                    const auto b = static_cast<float>(rgbData[pixelIdx + 2]);

                    // RGB to YCbCr conversion shifted by -128
                    const float yVal = 0.299f * r + 0.587f * g + 0.114f * b;
                    const float cbVal = -0.168736f * r - 0.331264f * g + 0.5f * b + 128.0f;
                    const float crVal = 0.5f * r - 0.418688f * g - 0.081312f * b + 128.0f;

                    blockY[y][x] = yVal - 128.0f;
                    blockCb[y][x] = cbVal - 128.0f;
                    blockCr[y][x] = crVal - 128.0f;
                }
            }

            encodeBlock(blockY, qLuma, prevDcY, dcLumaTbl, acLumaTbl, bitWriter);
            encodeBlock(blockCb, qChroma, prevDcCb, dcChromaTbl, acChromaTbl, bitWriter);
            encodeBlock(blockCr, qChroma, prevDcCr, dcChromaTbl, acChromaTbl, bitWriter);
        }
    }

    bitWriter.flush();

    // 9. EOI
    jpeg.push_back(kJpegMarkerPrefix);
    jpeg.push_back(kJpegEoi);

    return jpeg;
}

std::vector<std::uint8_t> JpegGenerator::createTestFrame(
    int width, int height, const std::string& timestamp, const std::string& channelName)
{
    const int w = (width <= 0) ? 640 : width;
    const int h = (height <= 0) ? 480 : height;

    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(w * h * 3), 0U);

    // Standard 8 SMPTE-style Color Bars:
    // White, Yellow, Cyan, Green, Magenta, Red, Blue, Black
    constexpr std::array<std::array<std::uint8_t, 3>, 8> kColors = { {
        { 240, 240, 240 }, // White
        { 240, 240, 0 }, // Yellow
        { 0, 240, 240 }, // Cyan
        { 0, 240, 0 }, // Green
        { 240, 0, 240 }, // Magenta
        { 240, 0, 0 }, // Red
        { 0, 0, 240 }, // Blue
        { 20, 20, 20 } // Black
    } };

    const int barWidth = w / 8;
    const int bannerHeight = std::min(h / 4, 90);
    const int barsHeight = h - bannerHeight;

    for (int y = 0; y < barsHeight; ++y) {
        for (int x = 0; x < w; ++x) {
            const int barIdx = std::clamp(x / barWidth, 0, 7);
            const std::size_t idx = static_cast<std::size_t>((y * w + x) * 3);
            rgb[idx] = kColors[static_cast<std::size_t>(barIdx)][0];
            rgb[idx + 1] = kColors[static_cast<std::size_t>(barIdx)][1];
            rgb[idx + 2] = kColors[static_cast<std::size_t>(barIdx)][2];
        }
    }

    // Bottom Status Banner: Dark Navy/Slate (RGB: 26, 32, 44)
    for (int y = barsHeight; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t idx = static_cast<std::size_t>((y * w + x) * 3);
            // Thin cyan separator line at the top of banner
            if (y == barsHeight || y == barsHeight + 1) {
                rgb[idx] = 0U;
                rgb[idx + 1] = 180U;
                rgb[idx + 2] = 220U;
            } else {
                rgb[idx] = 26U;
                rgb[idx + 1] = 32U;
                rgb[idx + 2] = 44U;
            }
        }
    }

    // Text Overlays
    const std::string ts = timestamp.empty() ? getUtcTimestamp() : timestamp;
    const std::string resStr = "RES: " + std::to_string(w) + "x" + std::to_string(h) + " | ONVIF PROFILE S";

    const int scale = (w >= 1280) ? 3 : 2;
    drawText(rgb, w, h, 20, barsHeight + 10, channelName, 255U, 255U, 255U, scale);
    drawText(rgb, w, h, 20, barsHeight + 14 + (7 * scale), ts, 240U, 200U, 40U, scale);
    drawText(rgb, w, h, 20, barsHeight + 18 + (14 * scale), resStr, 180U, 190U, 205U, scale);

    return encodeRgb(rgb.data(), w, h, 80);
}

bool JpegGenerator::isValidJpeg(const std::vector<std::uint8_t>& data)
{
    if (data.size() < 4U) {
        return false;
    }

    // Check SOI (0xFF, 0xD8) and EOI (0xFF, 0xD9)
    if (data[0] != kJpegMarkerPrefix || data[1] != kJpegSoi) {
        return false;
    }
    if (data[data.size() - 2U] != kJpegMarkerPrefix || data[data.size() - 1U] != kJpegEoi) {
        return false;
    }

    // Check presence of standard marker immediately following SOI
    if (data.size() >= 4U && data[2] == kJpegMarkerPrefix) {
        const std::uint8_t marker = data[3];
        // Common first markers: APP0, APP1, DQT, or SOF0
        if (marker == kJpegApp0 || marker == 0xE1U || marker == kJpegDqt || marker == kJpegSof0) {
            return true;
        }
    }
    return true;
}

bool JpegGenerator::getDimensions(const std::vector<std::uint8_t>& data, int& width, int& height)
{
    width = 0;
    height = 0;

    if (!isValidJpeg(data)) {
        return false;
    }

    std::size_t offset { 2U }; // Immediately after SOI
    while (offset + 4U <= data.size()) {
        if (data[offset] != kJpegMarkerPrefix) {
            ++offset;
            continue;
        }

        while (offset < data.size() && data[offset] == kJpegMarkerPrefix) {
            ++offset;
        }

        if (offset >= data.size()) {
            break;
        }

        const std::uint8_t marker = data[offset++];
        if (marker == 0x00U || marker == kJpegMarkerPrefix) {
            continue; // Byte stuffing
        }
        if (marker == kJpegEoi || marker == kJpegSos) {
            break; // Stop at SOS or EOI
        }

        if (offset + 2U > data.size()) {
            break;
        }

        const auto length = static_cast<std::size_t>(
            (static_cast<std::size_t>(data[offset]) << 8U) | static_cast<std::size_t>(data[offset + 1U]));

        // SOF0 (Baseline), SOF1 (Extended), SOF2 (Progressive)
        if (marker == kJpegSof0 || marker == 0xC1U || marker == 0xC2U) {
            if (offset + 7U <= data.size()) {
                height = static_cast<int>(
                    (static_cast<std::size_t>(data[offset + 3U]) << 8U) | static_cast<std::size_t>(data[offset + 4U]));
                width = static_cast<int>(
                    (static_cast<std::size_t>(data[offset + 5U]) << 8U) | static_cast<std::size_t>(data[offset + 6U]));
                return (width > 0 && height > 0);
            }
        }

        offset += length;
    }

    return false;
}

} // namespace Onvif
