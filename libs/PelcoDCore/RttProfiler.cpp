/// @file RttProfiler.cpp
/// @brief Implementation of Round-Trip-Time (RTT), jitter, and packet loss diagnostics profiler.

#include "RttProfiler.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace PelcoD {

RttProfiler::RttProfiler(std::shared_ptr<PelcoDDevice> device)
    : m_device { std::move(device) }
{
}

RttProfiler::~RttProfiler()
{
    stop();
}

void RttProfiler::setDevice(std::shared_ptr<PelcoDDevice> device)
{
    stop();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_device = std::move(device);
}

std::shared_ptr<PelcoDDevice> RttProfiler::getDevice() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_device;
}

bool RttProfiler::start(const RttProfilerConfig& config)
{
    if (config.intervalMs < 10U || config.timeoutMs < 20U || config.historyCapacity == 0U) {
        return false;
    }
    if (config.mode == ProfilerMode::ActiveBurst && config.burstCount == 0U) {
        return false;
    }

    std::shared_ptr<PelcoDDevice> dev;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_running.load()) {
            return false;
        }
        dev = m_device;
        if (!dev && config.mode != ProfilerMode::Passive) {
            return false;
        }
        if (dev && !dev->isConnected() && config.mode != ProfilerMode::Passive) {
            return false;
        }

        m_config = config;
        m_stopRequested = false;
        m_running = true;

        // Reset statistics for new session
        resetStatisticsUnderLock();
    }

    if (dev) {
        m_deviceLatencyConn
            = dev->addQueryLatencyCallback([this](const std::string& tag, std::chrono::microseconds duration,
                                               bool success) { recordSample(duration, tag, success); });
    }

    StateChangedCallback stateCb;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        stateCb = m_stateCb;
    }
    if (stateCb) {
        stateCb(true);
    }

    if (config.mode == ProfilerMode::ActiveBurst || config.mode == ProfilerMode::ActiveContinuous) {
        m_worker = std::thread(&RttProfiler::activeWorkerLoop, this, config);
    }

    return true;
}

void RttProfiler::stop()
{
    m_stopRequested = true;
    m_deviceLatencyConn.disconnect();
    m_cv.notify_all();

    if (m_worker.joinable()) {
        m_worker.join();
    }

    const bool wasRunning = m_running.exchange(false);
    if (wasRunning) {
        StateChangedCallback stateCb;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            stateCb = m_stateCb;
        }
        if (stateCb) {
            stateCb(false);
        }
    }
}

void RttProfiler::resetStatisticsUnderLock() noexcept
{
    m_stats = RttStatistics {};
    m_history.clear();
    m_nextSeq = 1U;
    m_welfordMean = 0.0;
    m_welfordM2 = 0.0;
    m_lastRttMs = 0.0;
    m_jitterRfc3550Ms = 0.0;
}

void RttProfiler::reset()
{
    StatisticsCallback statsCb;
    RttStatistics statsSnapshot;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        resetStatisticsUnderLock();
        statsSnapshot = m_stats;
        statsCb = m_statsCb;
    }
    if (statsCb) {
        statsCb(statsSnapshot);
    }
}

void RttProfiler::recordSample(std::chrono::microseconds duration, const std::string& queryTag, bool success)
{
    const double rttMs = static_cast<double>(duration.count()) / 1000.0;
    recordSampleMs(rttMs, queryTag, success);
}

void RttProfiler::recordSampleMs(double rttMs, const std::string& queryTag, bool success)
{
    RttSample sample;
    RttStatistics statsCopy;
    SampleCallback sampleCb;
    StatisticsCallback statsCb;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        sample.sequenceNumber = m_nextSeq++;
        sample.timestamp = std::chrono::steady_clock::now();
        sample.rttMs = rttMs;
        sample.queryTag = queryTag;
        sample.success = success;

        m_history.push_back(sample);
        while (m_history.size() > m_config.historyCapacity) {
            m_history.pop_front();
        }

        m_stats.totalProbes++;
        if (success) {
            m_stats.successfulProbes++;
            m_stats.currentRttMs = rttMs;

            const std::uint64_t k = m_stats.successfulProbes;
            if (k == 1U) {
                m_stats.minRttMs = rttMs;
                m_stats.maxRttMs = rttMs;
                m_welfordMean = rttMs;
                m_welfordM2 = 0.0;
                m_lastRttMs = rttMs;
                m_jitterRfc3550Ms = 0.0;
            } else {
                m_stats.minRttMs = std::min(m_stats.minRttMs, rttMs);
                m_stats.maxRttMs = std::max(m_stats.maxRttMs, rttMs);

                // Welford's algorithm for online variance/mean
                const double delta = rttMs - m_welfordMean;
                m_welfordMean += delta / static_cast<double>(k);
                const double delta2 = rttMs - m_welfordMean;
                m_welfordM2 += delta * delta2;

                // RFC 3550 inter-arrival jitter
                const double diff = std::abs(rttMs - m_lastRttMs);
                m_jitterRfc3550Ms += (diff - m_jitterRfc3550Ms) / 16.0;
                m_lastRttMs = rttMs;
            }

            m_stats.avgRttMs = m_welfordMean;
            m_stats.stdDevMs = (k > 1U) ? std::sqrt(m_welfordM2 / static_cast<double>(k - 1U)) : 0.0;
            m_stats.jitterRfc3550Ms = m_jitterRfc3550Ms;
        } else {
            m_stats.timedOutProbes++;
        }

        m_stats.lossPercent = (m_stats.totalProbes > 0U)
            ? (static_cast<double>(m_stats.timedOutProbes) / static_cast<double>(m_stats.totalProbes) * 100.0)
            : 0.0;

        updatePercentilesLocked();

        statsCopy = m_stats;
        sampleCb = m_sampleCb;
        statsCb = m_statsCb;
    }

    if (sampleCb) {
        sampleCb(sample, statsCopy);
    }
    if (statsCb) {
        statsCb(statsCopy);
    }
}

void RttProfiler::updatePercentilesLocked()
{
    std::vector<double> validRtts;
    validRtts.reserve(m_history.size());
    for (const auto& s : m_history) {
        if (s.success) {
            validRtts.push_back(s.rttMs);
        }
    }

    if (validRtts.empty()) {
        m_stats.p50RttMs = 0.0;
        m_stats.p95RttMs = 0.0;
        m_stats.p99RttMs = 0.0;
        return;
    }

    std::sort(validRtts.begin(), validRtts.end());
    const std::size_t n = validRtts.size();

    const std::size_t idx50 = std::min(n - 1U, (n * 50U) / 100U);
    const std::size_t idx95 = std::min(n - 1U, (n * 95U) / 100U);
    const std::size_t idx99 = std::min(n - 1U, (n * 99U) / 100U);

    m_stats.p50RttMs = validRtts[idx50];
    m_stats.p95RttMs = validRtts[idx95];
    m_stats.p99RttMs = validRtts[idx99];
}

bool RttProfiler::isRunning() const noexcept
{
    return m_running.load();
}

RttProfilerConfig RttProfiler::getConfig() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

RttStatistics RttProfiler::getStatistics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

std::vector<RttSample> RttProfiler::getHistory() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return { m_history.begin(), m_history.end() };
}

void RttProfiler::setSampleCallback(SampleCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sampleCb = std::move(cb);
}

void RttProfiler::setStatisticsCallback(StatisticsCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statsCb = std::move(cb);
}

void RttProfiler::setStateChangedCallback(StateChangedCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateCb = std::move(cb);
}

void RttProfiler::setFinishedCallback(FinishedCallback cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_finishedCb = std::move(cb);
}

void RttProfiler::activeWorkerLoop(RttProfilerConfig config)
{
    std::uint32_t probesSent = 0U;

    while (!m_stopRequested.load()) {
        dispatchProbeCommand(config.probeQueryTag);
        probesSent++;

        if (config.mode == ProfilerMode::ActiveBurst && probesSent >= config.burstCount) {
            break;
        }

        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait_for(lock, std::chrono::milliseconds(config.intervalMs), [this] { return m_stopRequested.load(); });
    }

    // In burst mode, allow a brief settling time for final response to be received
    if (config.mode == ProfilerMode::ActiveBurst && !m_stopRequested.load()) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait_for(lock, std::chrono::milliseconds(std::min(config.timeoutMs, 500U)),
            [this] { return m_stopRequested.load(); });
    }

    m_deviceLatencyConn.disconnect();
    m_running.store(false);

    FinishedCallback finishCb;
    StateChangedCallback stateCb;
    RttStatistics finalStats;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        finishCb = m_finishedCb;
        stateCb = m_stateCb;
        finalStats = m_stats;
    }

    if (finishCb) {
        finishCb(finalStats);
    }
    if (stateCb) {
        stateCb(false);
    }
}

void RttProfiler::dispatchProbeCommand(const std::string& tag)
{
    std::shared_ptr<PelcoDDevice> dev;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        dev = m_device;
    }

    if (!dev || !dev->isConnected()) {
        return;
    }

    if (tag == "QueryTilt") {
        dev->queryTilt();
    } else if (tag == "QueryZoom") {
        dev->queryZoom();
    } else if (tag == "QueryDeviceType") {
        dev->queryDeviceType();
    } else if (tag == "QueryDiagnostics") {
        dev->queryDiagnostics();
    } else if (tag == "QueryGeneral") {
        dev->queryGeneral();
    } else {
        dev->queryPan();
    }
}

bool RttProfiler::exportCsv(std::ostream& os) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!os.good()) {
        return false;
    }

    os << "Sequence,RTT_ms,Status,QueryTag\n";
    for (const auto& sample : m_history) {
        os << sample.sequenceNumber << "," << std::fixed << std::setprecision(3) << sample.rttMs << ","
           << (sample.success ? "OK" : "TIMEOUT") << "," << sample.queryTag << "\n";
    }

    return os.good();
}

bool RttProfiler::exportJson(std::ostream& os) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!os.good()) {
        return false;
    }

    os << "{\n";
    os << "  \"summary\": {\n";
    os << "    \"totalProbes\": " << m_stats.totalProbes << ",\n";
    os << "    \"successfulProbes\": " << m_stats.successfulProbes << ",\n";
    os << "    \"timedOutProbes\": " << m_stats.timedOutProbes << ",\n";
    os << "    \"lossPercent\": " << std::fixed << std::setprecision(2) << m_stats.lossPercent << ",\n";
    os << "    \"minRttMs\": " << std::setprecision(3) << m_stats.minRttMs << ",\n";
    os << "    \"maxRttMs\": " << m_stats.maxRttMs << ",\n";
    os << "    \"avgRttMs\": " << m_stats.avgRttMs << ",\n";
    os << "    \"jitterRfc3550Ms\": " << m_stats.jitterRfc3550Ms << ",\n";
    os << "    \"stdDevMs\": " << m_stats.stdDevMs << ",\n";
    os << "    \"p50RttMs\": " << m_stats.p50RttMs << ",\n";
    os << "    \"p95RttMs\": " << m_stats.p95RttMs << ",\n";
    os << "    \"p99RttMs\": " << m_stats.p99RttMs << "\n";
    os << "  },\n";
    os << "  \"samples\": [\n";

    for (std::size_t i = 0U; i < m_history.size(); ++i) {
        const auto& s = m_history[i];
        os << "    {\"seq\": " << s.sequenceNumber << ", \"rttMs\": " << std::fixed << std::setprecision(3) << s.rttMs
           << ", \"success\": " << (s.success ? "true" : "false") << ", \"tag\": \"" << s.queryTag << "\"}";
        if (i + 1U < m_history.size()) {
            os << ",";
        }
        os << "\n";
    }

    os << "  ]\n";
    os << "}\n";

    return os.good();
}

} // namespace PelcoD
