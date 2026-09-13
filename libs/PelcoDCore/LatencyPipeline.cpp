/// @file LatencyPipeline.cpp
/// @brief Asynchronous transmission latency, jitter, and packet drop simulation pipeline.

#include "LatencyPipeline.h"

#include <algorithm>

namespace PelcoD {

LatencyPipeline::LatencyPipeline()
    : m_rng(std::random_device {}())
{
    m_worker = std::thread(&LatencyPipeline::workerLoop, this);
}

LatencyPipeline::~LatencyPipeline()
{
    stop();
}

void LatencyPipeline::setConfig(const LatencyConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

LatencyConfig LatencyPipeline::getConfig() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void LatencyPipeline::enqueue(std::vector<std::uint8_t> data, Callback callback)
{
    if (data.empty() || !callback) {
        return;
    }

    std::unique_lock<std::mutex> lock(m_mutex);

    // If latency simulation is disabled or 0 delay, invoke immediately
    if (!m_config.enabled || (m_config.baseLatencyMs == 0U && m_config.jitterMs == 0U && m_config.packetDropPercent <= 0.0)) {
        lock.unlock();
        callback(data);
        return;
    }

    // Simulated packet drop check
    if (m_config.packetDropPercent > 0.0) {
        std::uniform_real_distribution<double> dist(0.0, 100.0);
        if (dist(m_rng) < m_config.packetDropPercent) {
            // Frame is lost/dropped in transit
            return;
        }
    }

    // Calculate total delay with jitter
    long long delayMs = static_cast<long long>(m_config.baseLatencyMs);
    if (m_config.jitterMs > 0U) {
        const auto jitterSpan = static_cast<long long>(m_config.jitterMs);
        std::uniform_int_distribution<long long> jitterDist(-jitterSpan, jitterSpan);
        delayMs += jitterDist(m_rng);
        if (delayMs < 0) {
            delayMs = 0;
        }
    }

    const auto now = std::chrono::steady_clock::now();
    const auto readyTime = now + std::chrono::milliseconds(delayMs);

    QueuedItem item;
    item.dispatchTime = readyTime;
    item.data = std::move(data);
    item.callback = std::move(callback);

    // Insert in sorted order by dispatchTime
    const auto insertPos = std::upper_bound(m_queue.begin(), m_queue.end(), item,
        [](const QueuedItem& a, const QueuedItem& b) {
            return a.dispatchTime < b.dispatchTime;
        });
    m_queue.insert(insertPos, std::move(item));

    m_cv.notify_one();
}

void LatencyPipeline::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.clear();
    m_cv.notify_all();
}

void LatencyPipeline::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running.store(false);
        m_queue.clear();
        m_cv.notify_all();
    }

    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void LatencyPipeline::workerLoop()
{
    while (m_running.load()) {
        std::vector<QueuedItem> readyItems;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (!m_running.load()) {
                break;
            }

            if (m_queue.empty()) {
                m_cv.wait(lock, [this] {
                    return !m_running.load() || !m_queue.empty();
                });
            } else {
                const auto nextReadyTime = m_queue.front().dispatchTime;
                m_cv.wait_until(lock, nextReadyTime, [this, nextReadyTime] {
                    return !m_running.load() || (!m_queue.empty() && m_queue.front().dispatchTime < nextReadyTime);
                });
            }

            if (!m_running.load()) {
                break;
            }

            const auto now = std::chrono::steady_clock::now();
            while (!m_queue.empty() && m_queue.front().dispatchTime <= now) {
                readyItems.push_back(std::move(m_queue.front()));
                m_queue.erase(m_queue.begin());
            }
        }

        // Dispatch outside the lock
        for (const auto& item : readyItems) {
            if (item.callback) {
                item.callback(item.data);
            }
        }
    }
}

} // namespace PelcoD
