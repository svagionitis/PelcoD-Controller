#pragma once

/// @file LatencyPipeline.h
/// @brief Asynchronous transmission latency, jitter, and packet drop simulation pipeline.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace PelcoD {

/// @struct LatencyConfig
/// @brief Configuration controlling link delay, jitter distribution, and loss rate.
struct LatencyConfig {
    bool enabled { false };
    std::uint32_t baseLatencyMs { 0U };
    std::uint32_t jitterMs { 0U };
    double packetDropPercent { 0.0 };
};

/// @class LatencyPipeline
/// @brief Thread-safe asynchronous delay queue simulating transport transit times.
class LatencyPipeline {
public:
    using Callback = std::function<void(const std::vector<std::uint8_t>&)>;

    LatencyPipeline();
    ~LatencyPipeline();

    // Non-copyable, non-movable
    LatencyPipeline(const LatencyPipeline&) = delete;
    LatencyPipeline& operator=(const LatencyPipeline&) = delete;
    LatencyPipeline(LatencyPipeline&&) = delete;
    LatencyPipeline& operator=(LatencyPipeline&&) = delete;

    /// @brief Update latency and packet drop parameters.
    /// @param[in] config Active latency configuration.
    void setConfig(const LatencyConfig& config);

    /// @brief Retrieve current latency configuration.
    /// @return Copy of LatencyConfig.
    [[nodiscard]] LatencyConfig getConfig() const;

    /// @brief Enqueue a response buffer for delayed dispatch.
    /// @param[in] data Byte buffer to transmit.
    /// @param[in] callback Callback invoked upon simulated arrival.
    void enqueue(std::vector<std::uint8_t> data, Callback callback);

    /// @brief Clear all pending packets without dispatching them.
    void flush();

    /// @brief Shut down background dispatcher worker thread.
    void stop();

private:
    struct QueuedItem {
        std::chrono::steady_clock::time_point dispatchTime;
        std::vector<std::uint8_t> data;
        Callback callback;
    };

    void workerLoop();

    mutable std::mutex m_mutex;
    LatencyConfig m_config {};

    std::vector<QueuedItem> m_queue;
    std::condition_variable m_cv;
    std::thread m_worker;
    std::atomic<bool> m_running { true };

    std::mt19937 m_rng;
};

} // namespace PelcoD
