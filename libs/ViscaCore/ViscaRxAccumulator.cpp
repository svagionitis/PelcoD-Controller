#include "ViscaRxAccumulator.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <iterator>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace Visca {

void ViscaRxAccumulator::setFrameCallback(FrameCallback callback)
{
    std::scoped_lock lock(m_mutex);
    m_callback = std::move(callback);
}

void ViscaRxAccumulator::addData(const std::uint8_t* data, std::size_t length)
{
    if (data == nullptr || length == 0U) {
        return;
    }

    std::vector<ViscaFrame> framesToNotify {};

    {
        std::scoped_lock lock(m_mutex);
        m_buffer.insert(m_buffer.end(), data, data + length);
        processBufferLocked();

        if (m_callback) {
            // Drain queue into local batch for callback dispatch outside lock
            while (!m_frameQueue.empty()) {
                framesToNotify.push_back(std::move(m_frameQueue.front()));
                m_frameQueue.pop_front();
            }
        }
    }

    for (const auto& frame : framesToNotify) {
        if (m_callback) {
            m_callback(frame);
        }
    }
}

void ViscaRxAccumulator::addData(const std::vector<std::uint8_t>& data)
{
    addData(data.data(), data.size());
}

std::optional<ViscaFrame> ViscaRxAccumulator::popFrame()
{
    std::scoped_lock lock(m_mutex);
    if (m_frameQueue.empty()) {
        return std::nullopt;
    }
    ViscaFrame frame = std::move(m_frameQueue.front());
    m_frameQueue.pop_front();
    return frame;
}

bool ViscaRxAccumulator::hasFrames() const
{
    std::scoped_lock lock(m_mutex);
    return !m_frameQueue.empty();
}

std::size_t ViscaRxAccumulator::pendingFrameCount() const
{
    std::scoped_lock lock(m_mutex);
    return m_frameQueue.size();
}

void ViscaRxAccumulator::clear()
{
    std::scoped_lock lock(m_mutex);
    m_buffer.clear();
    m_frameQueue.clear();
}

void ViscaRxAccumulator::processBufferLocked()
{
    while (!m_buffer.empty()) {
        auto it = std::find(m_buffer.begin(), m_buffer.end(), kViscaTerminator);
        if (it == m_buffer.end()) {
            // No terminator found yet
            if (m_buffer.size() > kMaxAccumulatorBufferSize) {
                // Drop oldest data to prevent unbounded buffer growth
                const std::size_t keep = kMaxPacketLength;
                m_buffer.erase(m_buffer.begin(), m_buffer.end() - static_cast<std::ptrdiff_t>(keep));
            }
            break;
        }

        const std::size_t frameLength = static_cast<std::size_t>(std::distance(m_buffer.begin(), it) + 1);

        // Check valid packet criteria
        if (frameLength >= kMinPacketLength && frameLength <= kMaxPacketLength && (m_buffer[0] & 0x80U) != 0U) {
            ViscaFrame frame(m_buffer.data(), frameLength);
            m_frameQueue.push_back(std::move(frame));
        }

        // Erase processed bytes up through the 0xFF delimiter
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(frameLength));
    }
}

} // namespace Visca
