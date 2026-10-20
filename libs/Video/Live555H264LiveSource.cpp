#include "Live555H264LiveSource.h"

#if defined(PELCOD_HAS_LIVE555)

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <GroupsockHelper.hh>
#include <UsageEnvironment.hh>
#include <glog/logging.h>
#include <liveMedia.hh>

#include "Live555FrameQueue.h"
#include "Live555FrameTypes.h"

namespace Video {

Live555H264LiveSource* Live555H264LiveSource::createNew(
    UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe)
{
    return new Live555H264LiveSource(env, queue, syncToKeyframe);
}

Live555H264LiveSource::Live555H264LiveSource(UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe)
    : FramedSource(env)
    , m_reader(queue.createReader(syncToKeyframe))
    , m_eventTriggerId(env.taskScheduler().createEventTrigger(deliverFrame0))
{
}

Live555H264LiveSource::~Live555H264LiveSource()
{
    if (m_closedCallback) {
        m_closedCallback(this);
    }
    if (m_eventTriggerId != 0U) {
        envir().taskScheduler().deleteEventTrigger(m_eventTriggerId);
        m_eventTriggerId = 0U;
    }
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    nextTask() = nullptr;
}

void Live555H264LiveSource::setClosedCallback(ClosedCallback callback)
{
    m_closedCallback = std::move(callback);
}

EventTriggerId Live555H264LiveSource::eventTriggerId() const noexcept
{
    return m_eventTriggerId;
}

bool Live555H264LiveSource::isAwaitingData() const noexcept
{
    return (isCurrentlyAwaitingData() != False);
}

unsigned Live555H264LiveSource::maxFrameSize() const
{
    // Return generous 300 KB limit to handle high-definition I-frames without truncation
    return 300000U;
}

void Live555H264LiveSource::doGetNextFrame()
{
    m_isAwaitingData = true;

    // Schedule delivery via delay 0 on the event loop to prevent deep recursion,
    // ensuring immediate delivery if a frame is already queued in m_reader.
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0, deliverFrame0, this);
}

void Live555H264LiveSource::deliverFrame0(void* clientData)
{
    auto* source { static_cast<Live555H264LiveSource*>(clientData) };
    if (source != nullptr) {
        source->deliverFrame();
    }
}

void Live555H264LiveSource::deliverFrame()
{
    if (!isCurrentlyAwaitingData()) {
        return;
    }

    if (!m_reader) {
        handleClosure();
        return;
    }

    bool dropped { false };
    auto frameOpt { m_reader->getNextFrame(dropped) };
    if (!frameOpt.has_value() || !frameOpt->data || frameOpt->data->empty()) {
        m_isAwaitingData = true;
        return;
    }

    m_isAwaitingData = false;
    const QueuedFrame& frame { *frameOpt };
    const auto* rawData { frame.data->data() };
    const std::size_t totalBytes { frame.data->size() };

    // Inspect and strip Annex-B start code prefixes if present (4-byte or 3-byte)
    std::size_t offset { 0U };
    if ((totalBytes >= 4U) && (rawData[0] == 0x00U) && (rawData[1] == 0x00U) && (rawData[2] == 0x00U)
        && (rawData[3] == 0x01U)) {
        offset = 4U;
    } else if ((totalBytes >= 3U) && (rawData[0] == 0x00U) && (rawData[1] == 0x00U) && (rawData[2] == 0x01U)) {
        offset = 3U;
    }

    const std::size_t naluSize { totalBytes - offset };
    const auto* naluPayload { rawData + offset };

    if (naluSize > static_cast<std::size_t>(fMaxSize)) {
        fFrameSize = fMaxSize;
        fNumTruncatedBytes = static_cast<unsigned>(naluSize - static_cast<std::size_t>(fMaxSize));
        LOG(WARNING) << "Live555H264LiveSource: NALU size " << naluSize << " exceeds buffer " << fMaxSize
                     << "; truncated " << fNumTruncatedBytes << " bytes.";
    } else {
        fFrameSize = static_cast<unsigned>(naluSize);
        fNumTruncatedBytes = 0U;
    }

    if (fFrameSize > 0U) {
        std::memmove(fTo, naluPayload, fFrameSize);
    }

    if (frame.ptsSeconds > 0.0) {
        const auto sec { static_cast<long>(frame.ptsSeconds) };
        const auto usec { static_cast<long>((frame.ptsSeconds - static_cast<double>(sec)) * 1000000.0) };
        fPresentationTime.tv_sec = sec;
        fPresentationTime.tv_usec = usec;
    } else {
        gettimeofday(&fPresentationTime, nullptr);
    }

    fDurationInMicroseconds = 0U;

    FramedSource::afterGetting(this);
}

} // namespace Video

#endif // PELCOD_HAS_LIVE555
