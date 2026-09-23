#include "BaseVideoDecoder.h"

#include <algorithm>
#include <cstring>

namespace Video {

// ---------------------------------------------------------------------------
// Concrete IVideoDecoder implementations
// ---------------------------------------------------------------------------

FrameInfo BaseVideoDecoder::getRawFrameData() const
{
    FrameInfo info;
    info.data = m_frameBuffer.data();
    info.width = m_width;
    info.height = m_height;
    info.size = m_frameBuffer.size();
    info.timestamp = m_timestamp;
    info.decodeTimeMs = m_lastDecodeTimeMs;
    info.format = m_outputFormat;
    return info;
}

VideoMetadata BaseVideoDecoder::getVideoMetadata() const
{
    VideoMetadata meta;
    meta.width = m_width;
    meta.height = m_height;
    meta.frameRate = m_frameRate;
    meta.duration = m_duration;
    meta.codecName = m_codecName;
    meta.format = m_outputFormat;
    meta.deviceType = m_reportedDeviceType;
    return meta;
}

DecoderPerformanceStats BaseVideoDecoder::getPerformanceStats() const
{
    DecoderPerformanceStats stats;
    stats.initializationTimeMs = m_initTimeMs;
    stats.totalDecodedFrames = m_decodedFramesCount;
    stats.averageDecodeTimeMs
        = (m_decodedFramesCount > 0U) ? (m_totalDecodeTimeMs / static_cast<double>(m_decodedFramesCount)) : 0.0;
    return stats;
}

void BaseVideoDecoder::enableTripleBuffering(bool enable)
{
    m_tripleBufferingEnabled = enable;
    if (enable && m_isInitialized && m_width > 0 && m_height > 0) {
        initTripleBufferSlots(m_width, m_height, m_outputFormat);
    }
}

bool BaseVideoDecoder::isTripleBufferingEnabled() const
{
    return m_tripleBufferingEnabled;
}

void BaseVideoDecoder::addFrameProcessor(std::shared_ptr<IFrameProcessor> processor)
{
    if (processor) {
        std::scoped_lock lock(m_processorMutex);
        m_processors.push_back(std::move(processor));
    }
}

void BaseVideoDecoder::clearFrameProcessors()
{
    std::scoped_lock lock(m_processorMutex);
    m_processors.clear();
}

// ---------------------------------------------------------------------------
// Protected helpers
// ---------------------------------------------------------------------------

void BaseVideoDecoder::initTripleBufferSlots(int w, int h, PixelFormat fmt)
{
    const std::size_t frameBytes = static_cast<std::size_t>(w * h * 3);
    for (auto& slot : m_tripleBuffer.getSlots()) {
        slot.buffer.resize(frameBytes);
        slot.width = w;
        slot.height = h;
        slot.size = frameBytes;
        slot.format = fmt;
    }
}

void BaseVideoDecoder::dispatchFrameProcessors(std::uint8_t* data, int w, int h, PixelFormat fmt)
{
    std::vector<std::shared_ptr<IFrameProcessor>> processors;
    {
        std::scoped_lock lock(m_processorMutex);
        processors = m_processors;
    }
    for (auto& processor : processors) {
        if (processor) {
            processor->process(data, w, h, fmt);
        }
    }
}

void BaseVideoDecoder::publishToTripleBuffer(
    const std::uint8_t* src, int w, int h, std::size_t bytes, double ts, double decodeMs, PixelFormat fmt)
{
    if (!m_tripleBufferingEnabled) {
        return;
    }
    auto& slot = m_tripleBuffer.getWriteBuffer();
    if (slot.buffer.size() != bytes) {
        slot.buffer.resize(bytes);
    }
    std::memcpy(slot.buffer.data(), src, bytes);
    slot.width = w;
    slot.height = h;
    slot.size = bytes;
    slot.timestamp = ts;
    slot.decodeTimeMs = decodeMs;
    slot.format = fmt;
    m_tripleBuffer.publishWriteBuffer();
}

bool BaseVideoDecoder::reconnect()
{
    const std::string cachedPath = m_filePath;
    const PixelFormat cachedFormat = m_outputFormat;
    const int cachedThreads = m_threadCount;
    const DeviceType cachedDevice = m_deviceType;

    close();
    return initialize(cachedPath, cachedFormat, cachedThreads, cachedDevice);
}

} // namespace Video
