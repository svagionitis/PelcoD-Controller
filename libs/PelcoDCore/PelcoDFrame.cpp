/// @file PelcoDFrame.cpp
/// @brief Implementation of Pelco-D frame calculation, validation, and parsing.

#include "PelcoDFrame.h"

#include <numeric>

namespace PelcoD {

std::uint8_t PelcoDFrame::calculateChecksum(const std::vector<std::uint8_t>& bytes) noexcept
{
    return calculateChecksum(bytes.data(), bytes.size());
}

std::uint8_t PelcoDFrame::calculateChecksum(const std::uint8_t* data, std::size_t length) noexcept
{
    if (data == nullptr || length == 0U) {
        return 0x00U;
    }

    std::uint32_t sum { 0U };
    for (std::size_t i { 0U }; i < length; ++i) {
        sum += static_cast<std::uint32_t>(data[i]);
    }
    return static_cast<std::uint8_t>(sum & 0xFFU);
}

std::vector<std::uint8_t> PelcoDFrame::createFrame(
    std::uint8_t address, std::uint8_t cmd1, std::uint8_t cmd2, std::uint8_t data1, std::uint8_t data2)
{
    std::vector<std::uint8_t> frame(StandardFrameSize, 0x00U);
    frame[0] = SyncByte;
    frame[1] = address;
    frame[2] = cmd1;
    frame[3] = cmd2;
    frame[4] = data1;
    frame[5] = data2;

    // Checksum is sum of bytes 1 through 5 modulo 256
    frame[6] = calculateChecksum(&frame[1], 5U);
    return frame;
}

bool PelcoDFrame::isValidFrame(const std::vector<std::uint8_t>& frame) noexcept
{
    if (frame.empty() || frame[0] != SyncByte) {
        return false;
    }

    const std::size_t len = frame.size();

    // 4-byte General Response: [0xFF, addr, alarms, cksm]
    if (len == GeneralResponseSize) {
        const std::uint8_t expected = calculateChecksum(&frame[1], 2U);
        return frame[3] == expected;
    }

    // 7-byte Standard Command or Extended Response
    if (len == StandardFrameSize) {
        const std::uint8_t expected = calculateChecksum(&frame[1], 5U);
        return frame[6] == expected;
    }

    // 18-byte Query Response: [0xFF, addr, data1..data15, cksm]
    if (len == QueryResponseSize) {
        const std::uint8_t expected = calculateChecksum(&frame[1], 16U);
        if (frame[17] != expected) {
            return false;
        }

        bool sawNull = false;
        for (std::size_t i { 2U }; i < 17U; ++i) {
            const std::uint8_t b = frame[i];
            if (b == 0x00U) {
                sawNull = true;
            } else if (sawNull || b < 32U || b > 126U) {
                return false;
            }
        }
        return true;
    }

    return false;
}

std::vector<std::vector<std::uint8_t>> PelcoDFrame::splitStream(const std::vector<std::uint8_t>& stream)
{
    std::vector<std::vector<std::uint8_t>> frames;
    constexpr std::size_t MaxStreamSize { 1024U * 1024U }; // 1 MB limit
    constexpr std::size_t MaxFrames { 2048U };

    if (stream.size() < GeneralResponseSize || stream.size() > MaxStreamSize) {
        return frames;
    }

    std::size_t idx { 0U };
    const std::size_t total = stream.size();

    while (idx < total && frames.size() < MaxFrames) {
        // Find next sync byte
        if (stream[idx] != SyncByte) {
            ++idx;
            continue;
        }

        const std::size_t remaining = total - idx;

        // Try 7-byte standard frame first (most common)
        if (remaining >= StandardFrameSize) {
            const std::uint8_t cksm = calculateChecksum(&stream[idx + 1U], 5U);
            if (stream[idx + 6U] == cksm) {
                std::vector<std::uint8_t> cand7(stream.begin() + static_cast<std::ptrdiff_t>(idx),
                    stream.begin() + static_cast<std::ptrdiff_t>(idx + StandardFrameSize));
                frames.push_back(std::move(cand7));
                idx += StandardFrameSize;
                continue;
            }
        }

        // Try 4-byte general response with valid checksum
        if (remaining >= GeneralResponseSize) {
            const std::uint8_t cksm4 = calculateChecksum(&stream[idx + 1U], 2U);
            if (stream[idx + 3U] == cksm4) {
                std::vector<std::uint8_t> cand4(stream.begin() + static_cast<std::ptrdiff_t>(idx),
                    stream.begin() + static_cast<std::ptrdiff_t>(idx + GeneralResponseSize));
                frames.push_back(std::move(cand4));
                idx += GeneralResponseSize;
                continue;
            }
        }

        // Try 18-byte query response
        if (remaining >= QueryResponseSize) {
            std::vector<std::uint8_t> cand18(stream.begin() + static_cast<std::ptrdiff_t>(idx),
                stream.begin() + static_cast<std::ptrdiff_t>(idx + QueryResponseSize));

            if (isValidFrame(cand18)) {
                frames.push_back(std::move(cand18));
                idx += QueryResponseSize;
                continue;
            }
        }

        // Unrecognized or corrupted frame starting at sync byte; advance by 1
        ++idx;
    }

    return frames;
}

namespace {
constexpr char kHexDigits[] = "0123456789ABCDEF";

inline int hexDigitVal(char c) noexcept
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}
} // namespace

std::string PelcoDFrame::toHexString(const std::vector<std::uint8_t>& bytes, char delimiter)
{
    return toHexString(bytes.data(), bytes.size(), delimiter);
}

std::string PelcoDFrame::toHexString(const std::uint8_t* data, std::size_t length, char delimiter)
{
    if (data == nullptr || length == 0U) {
        return {};
    }

    std::string result;
    const std::size_t allocSize = (delimiter != '\0') ? (length * 3U - 1U) : (length * 2U);
    result.reserve(allocSize);

    for (std::size_t i { 0U }; i < length; ++i) {
        const std::uint8_t b = data[i];
        result.push_back(kHexDigits[(b >> 4U) & 0x0FU]);
        result.push_back(kHexDigits[b & 0x0FU]);
        if (delimiter != '\0' && (i + 1U < length)) {
            result.push_back(delimiter);
        }
    }

    return result;
}

std::vector<std::uint8_t> PelcoDFrame::fromHexString(std::string_view hexStr)
{
    std::vector<std::uint8_t> bytes;
    int highNibble { -1 };

    for (std::size_t i { 0U }; i < hexStr.size(); ++i) {
        const char c = hexStr[i];
        // Handle "0x" or "0X" prefix
        if (c == '0' && (i + 1U < hexStr.size()) && (hexStr[i + 1U] == 'x' || hexStr[i + 1U] == 'X')) {
            ++i; // skip 'x'
            continue;
        }

        const int val = hexDigitVal(c);
        if (val < 0) {
            // Non-hex character (whitespace or delimiter such as ' ', ':', '-', ',')
            continue;
        }

        if (highNibble < 0) {
            highNibble = val;
        } else {
            bytes.push_back(static_cast<std::uint8_t>((highNibble << 4) | val));
            highNibble = -1;
        }
    }

    return bytes;
}

} // namespace PelcoD

