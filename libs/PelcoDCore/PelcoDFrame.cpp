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
        return true;
    }

    // 7-byte Standard Command or Extended Response
    if (len == StandardFrameSize) {
        const std::uint8_t expected = calculateChecksum(&frame[1], 5U);
        return frame[6] == expected;
    }

    // 18-byte Query Response: [0xFF, addr, data1..data15, cksm]
    if (len == QueryResponseSize) {
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

        // Try 18-byte query response first if sufficient bytes exist
        if (remaining >= QueryResponseSize) {
            std::vector<std::uint8_t> cand18(stream.begin() + static_cast<std::ptrdiff_t>(idx),
                stream.begin() + static_cast<std::ptrdiff_t>(idx + QueryResponseSize));

            // If 7-byte frame is also valid at this location, prefer 7-byte unless 18-byte is query
            const std::uint8_t cksm7 = calculateChecksum(&stream[idx + 1U], 5U);
            const bool is7Valid = (stream[idx + 6U] == cksm7);

            if (!is7Valid) {
                frames.push_back(std::move(cand18));
                idx += QueryResponseSize;
                continue;
            }
        }

        // Try 7-byte standard frame
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

        // Try 4-byte general response
        if (remaining >= GeneralResponseSize) {
            std::vector<std::uint8_t> cand4(stream.begin() + static_cast<std::ptrdiff_t>(idx),
                stream.begin() + static_cast<std::ptrdiff_t>(idx + GeneralResponseSize));
            frames.push_back(std::move(cand4));
            idx += GeneralResponseSize;
            continue;
        }

        // Less than 4 bytes remaining from sync byte
        break;
    }

    return frames;
}

} // namespace PelcoD
