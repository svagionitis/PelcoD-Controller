#include "MockVideoDecoder.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace PelcoD::Video {

MockVideoDecoder::MockVideoDecoder()
{
}

bool MockVideoDecoder::initialize(
    std::string_view source, PixelFormat format, int /*threadCount*/, DeviceType /*device*/)
{
    (void)source;
    m_outputFormat = format;
    m_reportedDeviceType = DeviceType::CPU;
    m_isInitialized = true;
    m_frameIndex = 0U;
    m_currentTimeSec = 0.0;
    m_initTime = std::chrono::steady_clock::now();

    m_width = 640;
    m_height = 360;
    m_frameRate = 30.0;

    const std::size_t frameBytes = static_cast<std::size_t>(m_width * m_height * 3);
    m_frameBuffer.resize(frameBytes);

    if (m_tripleBufferingEnabled) {
        initTripleBufferSlots(m_width, m_height, m_outputFormat);
    }

    return true;
}

bool MockVideoDecoder::decodeNextFrame()
{
    if (!m_isInitialized) {
        return false;
    }

    const auto t0 = std::chrono::steady_clock::now();

    const std::size_t frameBytes = static_cast<std::size_t>(m_width * m_height * 3);
    if (m_frameBuffer.size() != frameBytes) {
        m_frameBuffer.resize(frameBytes);
    }

    renderTestPattern(m_frameBuffer.data());

    // Apply registered frame processors in-place
    dispatchFrameProcessors(m_frameBuffer.data(), m_width, m_height, m_outputFormat);

    m_currentTimeSec = static_cast<double>(m_frameIndex) / m_frameRate;
    m_timestamp = m_currentTimeSec;
    ++m_frameIndex;

    const auto t1 = std::chrono::steady_clock::now();
    m_lastDecodeTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    m_totalDecodeTimeMs += m_lastDecodeTimeMs;
    ++m_decodedFramesCount;

    publishToTripleBuffer(
        m_frameBuffer.data(), m_width, m_height, frameBytes, m_timestamp, m_lastDecodeTimeMs, m_outputFormat);

    return true;
}

bool MockVideoDecoder::seek(double timeInSeconds)
{
    if (!m_isInitialized) {
        return false;
    }
    m_currentTimeSec = std::max(0.0, timeInSeconds);
    m_frameIndex = static_cast<std::uint64_t>(m_currentTimeSec * m_frameRate);
    return true;
}

void MockVideoDecoder::close()
{
    m_isInitialized = false;
    m_frameIndex = 0U;
    m_currentTimeSec = 0.0;
    m_frameBuffer.clear();
}

void MockVideoDecoder::renderTestPattern(std::uint8_t* buffer)
{
    // 8 Standard SMPTE Color Bars (Top 75% of screen)
    // 0: White   (235, 235, 235)
    // 1: Yellow  (219, 219,  16)
    // 2: Cyan    ( 16, 219, 219)
    // 3: Green   ( 16, 219,  16)
    // 4: Magenta (219,  16, 219)
    // 5: Red     (219,  16,  16)
    // 6: Blue    ( 16,  16, 219)
    // 7: Black   ( 16,  16,  16)
    struct ColorRgb {
        std::uint8_t r;
        std::uint8_t g;
        std::uint8_t b;
    };

    static constexpr ColorRgb kBars[8] = {
        { 235U, 235U, 235U }, // White
        { 219U, 219U, 16U }, // Yellow
        { 16U, 219U, 219U }, // Cyan
        { 16U, 219U, 16U }, // Green
        { 219U, 16U, 219U }, // Magenta
        { 219U, 16U, 16U }, // Red
        { 16U, 16U, 219U }, // Blue
        { 16U, 16U, 16U } // Black
    };

    const int barWidth = m_width / 8;
    const int topHeight = (m_height * 3) / 4;

    // Moving scanline/tick position for dynamic animation
    const int movingX = static_cast<int>((m_frameIndex * 4U) % static_cast<std::size_t>(m_width));

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            ColorRgb c;
            if (y < topHeight) {
                // Top Color Bars
                int barIdx = std::min(7, x / std::max(1, barWidth));
                c = kBars[barIdx];

                // Moving vertical indicator line across the bars
                if (std::abs(x - movingX) <= 2) {
                    c = { 255U, 255U, 255U };
                }
            } else {
                // Bottom Section: Grayscale ramp and cast shadow
                const int grayVal = (x * 255) / std::max(1, m_width);
                c = { static_cast<std::uint8_t>(grayVal), static_cast<std::uint8_t>(grayVal),
                    static_cast<std::uint8_t>(grayVal) };

                // Moving tick marker on bottom ramp
                if (std::abs(x - movingX) <= 3) {
                    c = { 255U, 0U, 0U }; // Bright Red moving cursor
                }
            }

            const int pixelOffset = (y * m_width + x) * 3;
            if (m_outputFormat == PixelFormat::RGB24) {
                buffer[pixelOffset + 0] = c.r;
                buffer[pixelOffset + 1] = c.g;
                buffer[pixelOffset + 2] = c.b;
            } else { // BGR24
                buffer[pixelOffset + 0] = c.b;
                buffer[pixelOffset + 1] = c.g;
                buffer[pixelOffset + 2] = c.r;
            }
        }
    }
}

} // namespace PelcoD::Video
