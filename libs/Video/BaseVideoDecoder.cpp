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

bool BaseVideoDecoder::isInitialized() const
{
    return m_isInitialized;
}

void BaseVideoDecoder::setAutoReconnect(bool enable, int retryIntervalMs)
{
    m_autoReconnect = enable;
    m_reconnectIntervalMs = std::max(100, retryIntervalMs);
}

bool BaseVideoDecoder::isAutoReconnectEnabled() const
{
    return m_autoReconnect;
}

bool BaseVideoDecoder::reconnect()
{
    m_lastReconnectAttempt = std::chrono::steady_clock::now();
    const std::string cachedPath = m_filePath;
    const PixelFormat cachedFormat = m_outputFormat;
    const int cachedThreads = m_threadCount;
    const DeviceType cachedDevice = m_deviceType;

    close();
    const bool ok = initialize(cachedPath, cachedFormat, cachedThreads, cachedDevice);
    if (!ok) {
        // Retain cached parameters so subsequent reconnect attempts can succeed once network restores
        m_filePath = cachedPath;
        m_outputFormat = cachedFormat;
        m_threadCount = cachedThreads;
        m_deviceType = cachedDevice;
    }
    return ok;
}

void BaseVideoDecoder::setRtspTransport(RtspTransportMode mode)
{
    m_rtspTransport = mode;
}

RtspTransportMode BaseVideoDecoder::rtspTransport() const noexcept
{
    return m_rtspTransport;
}

void BaseVideoDecoder::setCredentials(std::string_view user, std::string_view pass)
{
    m_rtspUsername = std::string(user);
    m_rtspPassword = std::string(pass);
}

bool BaseVideoDecoder::hasCredentials() const noexcept
{
    return !m_rtspUsername.empty();
}

std::string BaseVideoDecoder::rtspUsername() const
{
    return m_rtspUsername;
}

std::string BaseVideoDecoder::rtspPassword() const
{
    return m_rtspPassword;
}

void BaseVideoDecoder::clearCredentials()
{
    m_rtspUsername.clear();
    m_rtspPassword.clear();
}

std::string BaseVideoDecoder::maskCredentials(std::string_view uri)
{
    const std::size_t schemePos { uri.find("://") };
    if (schemePos == std::string_view::npos) {
        return std::string(uri);
    }
    const std::size_t atPos { uri.find('@', schemePos + 3U) };
    if (atPos == std::string_view::npos) {
        return std::string(uri);
    }
    const std::size_t colonPos { uri.find(':', schemePos + 3U) };
    if (colonPos != std::string_view::npos && colonPos < atPos) {
        std::string masked {};
        masked.reserve(uri.size());
        masked.append(uri.substr(0U, colonPos + 1U));
        masked.append("***");
        masked.append(uri.substr(atPos));
        return masked;
    }
    return std::string(uri);
}

std::string BaseVideoDecoder::buildAuthenticatedUri(std::string_view uri) const
{
    if (!hasCredentials()) {
        return std::string(uri);
    }
    const std::size_t schemePos { uri.find("://") };
    if (schemePos == std::string_view::npos) {
        return std::string(uri);
    }
    const std::size_t nextSlash { uri.find('/', schemePos + 3U) };
    const std::size_t hostEnd { (nextSlash != std::string_view::npos) ? nextSlash : uri.size() };
    const std::string_view authority { uri.substr(schemePos + 3U, hostEnd - (schemePos + 3U)) };

    // If already contains inline credentials, don't overwrite
    if (authority.find('@') != std::string_view::npos) {
        return std::string(uri);
    }

    std::string authUri {};
    authUri.reserve(uri.size() + m_rtspUsername.size() + m_rtspPassword.size() + 2U);
    authUri.append(uri.substr(0U, schemePos + 3U));
    authUri.append(m_rtspUsername);
    if (!m_rtspPassword.empty()) {
        authUri.push_back(':');
        authUri.append(m_rtspPassword);
    }
    authUri.push_back('@');
    authUri.append(uri.substr(schemePos + 3U));
    return authUri;
}

void BaseVideoDecoder::interrupt() noexcept
{
    // Default no-op for decoders without asynchronous interruption facilities
}

} // namespace Video
