#include "Live555Mp2tLiveSource.h"

#if defined(PELCOD_HAS_LIVE555)

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>

#include <algorithm>
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

namespace {
    constexpr std::size_t kTsPacketSize { 188U };
    constexpr std::size_t kMaxTsPacketsPerRtp { 7U };
    constexpr std::size_t kIdealRtpPayloadSize { kTsPacketSize * kMaxTsPacketsPerRtp }; // 1316 bytes
} // namespace

Live555Mp2tLiveSource* Live555Mp2tLiveSource::createNew(
    UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe)
{
    return new Live555Mp2tLiveSource(env, queue, syncToKeyframe);
}

Live555Mp2tLiveSource::Live555Mp2tLiveSource(UsageEnvironment& env, Live555FrameQueue& queue, bool syncToKeyframe)
    : FramedSource(env)
    , m_reader(queue.createReader(syncToKeyframe))
    , m_eventTriggerId(env.taskScheduler().createEventTrigger(deliverFrame0))
{
}

Live555Mp2tLiveSource::~Live555Mp2tLiveSource()
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

void Live555Mp2tLiveSource::setClosedCallback(ClosedCallback callback)
{
    m_closedCallback = std::move(callback);
}

EventTriggerId Live555Mp2tLiveSource::eventTriggerId() const noexcept
{
    return m_eventTriggerId;
}

bool Live555Mp2tLiveSource::isAwaitingData() const noexcept
{
    return (isCurrentlyAwaitingData() != False);
}

bool Live555Mp2tLiveSource::hasBufferedData() const noexcept
{
    return (m_pendingFrame.has_value() && m_pendingFrame->data && (m_currentOffset < m_pendingFrame->data->size()));
}

unsigned Live555Mp2tLiveSource::maxFrameSize() const
{
    return static_cast<unsigned>(kIdealRtpPayloadSize);
}

void Live555Mp2tLiveSource::doGetNextFrame()
{
    m_isAwaitingData = true;

    // Schedule delivery via delay 0 on the event loop to prevent deep recursion,
    // ensuring immediate delivery if packets are already buffered or queued in m_reader.
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0, deliverFrame0, this);
}

void Live555Mp2tLiveSource::deliverFrame0(void* clientData)
{
    auto* source { static_cast<Live555Mp2tLiveSource*>(clientData) };
    if (source != nullptr) {
        source->deliverFrame();
    }
}

void Live555Mp2tLiveSource::deliverFrame()
{
    if (!isCurrentlyAwaitingData()) {
        return;
    }

    if (!m_reader) {
        handleClosure();
        return;
    }

    // If no active frame slice is pending, acquire the next frame from the queue reader
    if (!m_pendingFrame.has_value() || !m_pendingFrame->data || (m_currentOffset >= m_pendingFrame->data->size())) {
        bool dropped { false };
        m_pendingFrame = m_reader->getNextFrame(dropped);
        m_currentOffset = 0U;

        if (!m_pendingFrame.has_value() || !m_pendingFrame->data || m_pendingFrame->data->empty()) {
            m_pendingFrame = std::nullopt;
            m_isAwaitingData = true;
            return;
        }
    }

    m_isAwaitingData = false;
    const QueuedFrame& frame { *m_pendingFrame };
    const std::size_t totalBytes { frame.data->size() };
    const std::size_t remainingBytes { totalBytes - m_currentOffset };

    if (remainingBytes < kTsPacketSize) {
        LOG(WARNING) << "Live555Mp2tLiveSource: Discarding " << remainingBytes
                     << " unaligned trailing bytes in TS buffer.";
        m_pendingFrame = std::nullopt;
        m_currentOffset = 0U;
        m_isAwaitingData = true;
        return;
    }

    const std::size_t sinkPacketCapacity { static_cast<std::size_t>(fMaxSize) / kTsPacketSize };
    const std::size_t availablePackets { remainingBytes / kTsPacketSize };
    const std::size_t packetsToDeliver { std::min({ sinkPacketCapacity, availablePackets, kMaxTsPacketsPerRtp }) };

    if (packetsToDeliver == 0U) {
        // fMaxSize is smaller than a single 188-byte TS packet
        fFrameSize = fMaxSize;
        fNumTruncatedBytes = static_cast<unsigned>(kTsPacketSize - static_cast<std::size_t>(fMaxSize));
        const auto* srcPtr { frame.data->data() + m_currentOffset };
        std::memmove(fTo, srcPtr, fFrameSize);
        m_currentOffset += kTsPacketSize;
    } else {
        const std::size_t bytesToDeliver { packetsToDeliver * kTsPacketSize };
        const auto* srcPtr { frame.data->data() + m_currentOffset };
        std::memmove(fTo, srcPtr, bytesToDeliver);

        fFrameSize = static_cast<unsigned>(bytesToDeliver);
        fNumTruncatedBytes = 0U;
        m_currentOffset += bytesToDeliver;
    }

    if (m_currentOffset >= totalBytes) {
        m_pendingFrame = std::nullopt;
        m_currentOffset = 0U;
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
