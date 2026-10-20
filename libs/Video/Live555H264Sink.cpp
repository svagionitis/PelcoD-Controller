#include "Live555H264Sink.h"

#if defined(PELCOD_HAS_LIVE555)

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <glog/logging.h>
#include <liveMedia.hh>

namespace Video {

Live555H264Sink* Live555H264Sink::createNew(UsageEnvironment& env, NaluCallback callback, std::size_t bufferSize)
{
    return new Live555H264Sink(env, std::move(callback), bufferSize);
}

Live555H264Sink::Live555H264Sink(UsageEnvironment& env, NaluCallback callback, std::size_t bufferSize)
    : MediaSink(env)
    , m_callback(std::move(callback))
    , m_buffer(bufferSize + 4U, 0U)
{
    m_buffer[0] = 0x00U;
    m_buffer[1] = 0x00U;
    m_buffer[2] = 0x00U;
    m_buffer[3] = 0x01U;
}

Boolean Live555H264Sink::continuePlaying()
{
    if (fSource == nullptr) {
        return False;
    }

    fSource->getNextFrame(
        &m_buffer[4], static_cast<unsigned>(m_buffer.size() - 4U), afterGettingFrame, this, onSourceClosure, this);
    return True;
}

void Live555H264Sink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
    struct timeval presentationTime, unsigned /*durationInMicroseconds*/)
{
    auto* sink { static_cast<Live555H264Sink*>(clientData) };
    if (sink != nullptr) {
        sink->handleFrame(frameSize, numTruncatedBytes, presentationTime);
    }
}

void Live555H264Sink::handleFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime)
{
    if (numTruncatedBytes > 0U) {
        LOG(WARNING) << "Live555H264Sink: Frame truncated by " << numTruncatedBytes << " bytes; expanding buffer.";
        const std::size_t newSize { m_buffer.size() + static_cast<std::size_t>(numTruncatedBytes) + 65536U };
        m_buffer.resize(newSize, 0U);
        m_buffer[0] = 0x00U;
        m_buffer[1] = 0x00U;
        m_buffer[2] = 0x00U;
        m_buffer[3] = 0x01U;
    }

    if ((frameSize > 0U) && m_callback) {
        const double ptsSeconds { static_cast<double>(presentationTime.tv_sec)
            + (static_cast<double>(presentationTime.tv_usec) / 1000000.0) };
        const std::size_t totalSize { static_cast<std::size_t>(frameSize) + 4U };
        m_callback(m_buffer.data(), totalSize, ptsSeconds);
    }

    continuePlaying();
}

} // namespace Video

#endif // PELCOD_HAS_LIVE555
