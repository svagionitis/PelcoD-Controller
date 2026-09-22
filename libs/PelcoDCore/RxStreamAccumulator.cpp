/// @file RxStreamAccumulator.cpp
/// @brief Implementation of Pelco-D byte stream accumulator and packet framer.

#include "RxStreamAccumulator.h"

#include <algorithm>
#include <glog/logging.h>

namespace PelcoD {

RxStreamAccumulator::RxStreamAccumulator(std::size_t maxBufferSize)
    : m_maxBufferSize { (maxBufferSize > 0U) ? maxBufferSize : DefaultMaxBufferSize }
{
}

std::vector<std::vector<std::uint8_t>> RxStreamAccumulator::push(
    const std::vector<std::uint8_t>& data, bool awaitingQuery)
{
    return push(data.data(), data.size(), awaitingQuery);
}

std::vector<std::vector<std::uint8_t>> RxStreamAccumulator::push(
    const std::uint8_t* data, std::size_t size, bool awaitingQuery)
{
    if (data == nullptr || size == 0U) {
        return {};
    }

    std::vector<std::vector<std::uint8_t>> framesToDispatch;

    std::scoped_lock lock(m_mutex);

    if (m_buffer.size() + size > m_maxBufferSize) {
        LOG(WARNING) << "RxStreamAccumulator overflow (" << (m_buffer.size() + size) << " > "
                     << m_maxBufferSize << " bytes): resetting accumulator buffer";
        m_buffer.clear();
    }

    m_buffer.insert(m_buffer.end(), data, data + size);

    while (m_buffer.size() >= PelcoDFrame::GeneralResponseSize) {
        const auto syncIt = std::find(m_buffer.begin(), m_buffer.end(), PelcoDFrame::SyncByte);
        if (syncIt == m_buffer.end()) {
            m_buffer.clear();
            break;
        }

        if (syncIt != m_buffer.begin()) {
            m_buffer.erase(m_buffer.begin(), syncIt);
        }

        const std::size_t available = m_buffer.size();
        if (available < PelcoDFrame::GeneralResponseSize) {
            break;
        }

        constexpr std::size_t candidateSizes[] = {
            PelcoDFrame::StandardFrameSize,
            PelcoDFrame::GeneralResponseSize,
            PelcoDFrame::QueryResponseSize
        };

        bool frameExtracted = false;

        for (const std::size_t candidateSize : candidateSizes) {
            if (available >= candidateSize) {
                std::vector<std::uint8_t> frame(
                    m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(candidateSize));
                if (PelcoDFrame::isValidFrame(frame)) {
                    m_buffer.erase(
                        m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(candidateSize));
                    framesToDispatch.push_back(std::move(frame));
                    frameExtracted = true;
                    break;
                }
            }
        }

        if (!frameExtracted) {
            const std::size_t maxExpectedSize
                = awaitingQuery ? PelcoDFrame::QueryResponseSize : PelcoDFrame::StandardFrameSize;
            if (available < maxExpectedSize) {
                break;
            }
            m_buffer.erase(m_buffer.begin());
        }
    }

    return framesToDispatch;
}

void RxStreamAccumulator::clear()
{
    std::scoped_lock lock(m_mutex);
    m_buffer.clear();
}

std::size_t RxStreamAccumulator::size() const
{
    std::scoped_lock lock(m_mutex);
    return m_buffer.size();
}

std::size_t RxStreamAccumulator::maxBufferSize() const noexcept
{
    return m_maxBufferSize;
}

} // namespace PelcoD
