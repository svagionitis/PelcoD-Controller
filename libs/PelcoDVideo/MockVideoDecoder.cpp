#include "MockVideoDecoder.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

namespace PelcoD::Video {

MockVideoDecoder::MockVideoDecoder()
{
}

bool MockVideoDecoder::initialize(
    std::string_view source, PixelFormat format, int /*threadCount*/, DeviceType /*device*/)
{
    (void)source;
    m_format = format;
    m_initialized = true;
    m_frameIndex = 0U;
    m_currentTimeSec = 0.0;
    m_initTime = std::chrono::steady_clock::now();

    const std::size_t frameBytes = static_cast<std::size_t>(m_width * m_height * 3);
    m_currentFrameBuffer.resize(frameBytes);

    if (m_tripleBufferingEnabled) {
        for (auto& slot : m_tripleBuffer.getSlots()) {
            slot.buffer.resize(frameBytes);
            slot.width = m_width;
            slot.height = m_height;
            slot.size = frameBytes;
            slot.format = m_format;
        }
    }

    return true;
}

bool MockVideoDecoder::decodeNextFrame()
{
    if (!m_initialized) {
        return false;
    }

    const auto t0 = std::chrono::steady_clock::now();

    const std::size_t frameBytes = static_cast<std::size_t>(m_width * m_height * 3);
    if (m_currentFrameBuffer.size() != frameBytes) {
        m_currentFrameBuffer.resize(frameBytes);
    }

    renderTestPattern(m_currentFrameBuffer.data());

    std::vector<std::shared_ptr<IFrameProcessor>> processors;
    {
        std::lock_guard<std::mutex> lock(m_processorMutex);
        processors = m_processors;
    }
    for (auto& processor : processors) {
        if (processor) {
            processor->process(m_currentFrameBuffer.data(), m_width, m_height, m_format);
        }
    }

    m_currentTimeSec = static_cast<double>(m_frameIndex) / m_frameRate;
    ++m_frameIndex;

    const auto t1 = std::chrono::steady_clock::now();
    m_lastDecodeTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    m_totalDecodeTimeMs += m_lastDecodeTimeMs;

    if (m_tripleBufferingEnabled) {
        auto& slot = m_tripleBuffer.getWriteBuffer();
        if (slot.buffer.size() != frameBytes) {
            slot.buffer.resize(frameBytes);
        }
        std::memcpy(slot.buffer.data(), m_currentFrameBuffer.data(), frameBytes);
        slot.width = m_width;
        slot.height = m_height;
        slot.size = frameBytes;
        slot.timestamp = m_currentTimeSec;
        slot.decodeTimeMs = m_lastDecodeTimeMs;
        slot.format = m_format;
        m_tripleBuffer.publishWriteBuffer();
    }

    return true;
}

FrameInfo MockVideoDecoder::getRawFrameData() const
{
    FrameInfo info;
    info.data = m_currentFrameBuffer.data();
    info.width = m_width;
    info.height = m_height;
    info.size = m_currentFrameBuffer.size();
    info.timestamp = m_currentTimeSec;
    info.decodeTimeMs = m_lastDecodeTimeMs;
    info.format = m_format;
    return info;
}

VideoMetadata MockVideoDecoder::getVideoMetadata() const
{
    VideoMetadata meta;
    meta.width = m_width;
    meta.height = m_height;
    meta.frameRate = m_frameRate;
    meta.duration = 0.0; // Simulated live continuous stream
    meta.codecName = "RAW_MOCK";
    meta.format = m_format;
    meta.deviceType = DeviceType::CPU;
    return meta;
}

DecoderPerformanceStats MockVideoDecoder::getPerformanceStats() const
{
    DecoderPerformanceStats stats;
    stats.initializationTimeMs = 0.5;
    stats.totalDecodedFrames = m_frameIndex;
    stats.averageDecodeTimeMs = (m_frameIndex > 0U) ? (m_totalDecodeTimeMs / static_cast<double>(m_frameIndex)) : 0.0;
    return stats;
}

bool MockVideoDecoder::seek(double timeInSeconds)
{
    if (!m_initialized) {
        return false;
    }
    m_currentTimeSec = std::max(0.0, timeInSeconds);
    m_frameIndex = static_cast<std::uint64_t>(m_currentTimeSec * m_frameRate);
    return true;
}

void MockVideoDecoder::enableTripleBuffering(bool enable)
{
    m_tripleBufferingEnabled = enable;
    if (enable && m_initialized) {
        const std::size_t frameBytes = static_cast<std::size_t>(m_width * m_height * 3);
        for (auto& slot : m_tripleBuffer.getSlots()) {
            slot.buffer.resize(frameBytes);
            slot.width = m_width;
            slot.height = m_height;
            slot.size = frameBytes;
            slot.format = m_format;
        }
    }
}

bool MockVideoDecoder::isTripleBufferingEnabled() const
{
    return m_tripleBufferingEnabled;
}

void MockVideoDecoder::addFrameProcessor(std::shared_ptr<IFrameProcessor> processor)
{
    if (processor) {
        std::lock_guard<std::mutex> lock(m_processorMutex);
        m_processors.push_back(processor);
    }
}

void MockVideoDecoder::clearFrameProcessors()
{
    std::lock_guard<std::mutex> lock(m_processorMutex);
    m_processors.clear();
}

void MockVideoDecoder::close()
{
    m_initialized = false;
    m_frameIndex = 0U;
    m_currentTimeSec = 0.0;
    m_currentFrameBuffer.clear();
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
            if (m_format == PixelFormat::RGB24) {
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
