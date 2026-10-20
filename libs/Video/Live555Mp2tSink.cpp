/// @file Live555Mp2tSink.cpp
/// @brief Live555 MediaSink implementation receiving MPEG-2 TS packets and driving MpegTsH264Demuxer.

#include "Live555Mp2tSink.h"

#if defined(PELCOD_HAS_LIVE555)

#include <cstddef>
#include <cstdint>

#include <utility>
#include <vector>

#include <glog/logging.h>
#include <liveMedia.hh>

namespace Video {

Live555Mp2tSink* Live555Mp2tSink::createNew(UsageEnvironment& env, NaluCallback callback, std::size_t bufferSize)
{
    return new Live555Mp2tSink(env, std::move(callback), bufferSize);
}

Live555Mp2tSink::Live555Mp2tSink(UsageEnvironment& env, NaluCallback callback, std::size_t bufferSize)
    : MediaSink(env)
    , m_demuxer(std::move(callback))
    , m_buffer(bufferSize, 0U)
{
}

Boolean Live555Mp2tSink::continuePlaying()
{
    if (fSource == nullptr) {
        return False;
    }

    fSource->getNextFrame(
        m_buffer.data(), static_cast<unsigned>(m_buffer.size()), afterGettingFrame, this, onSourceClosure, this);
    return True;
}

void Live555Mp2tSink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
    struct timeval presentationTime, unsigned /*durationInMicroseconds*/)
{
    auto* sink { static_cast<Live555Mp2tSink*>(clientData) };
    if (sink != nullptr) {
        sink->handleFrame(frameSize, numTruncatedBytes, presentationTime);
    }
}

void Live555Mp2tSink::handleFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval /*presentationTime*/)
{
    if (numTruncatedBytes > 0U) {
        LOG(WARNING) << "Live555Mp2tSink: TS frame truncated by " << numTruncatedBytes << " bytes; expanding buffer.";
        const std::size_t newSize { m_buffer.size() + static_cast<std::size_t>(numTruncatedBytes) + 65536U };
        m_buffer.resize(newSize, 0U);
    }

    if (frameSize > 0U) {
        static_cast<void>(m_demuxer.processData(m_buffer.data(), static_cast<std::size_t>(frameSize)));
    }

    continuePlaying();
}

} // namespace Video

#endif // PELCOD_HAS_LIVE555
