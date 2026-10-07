#pragma once

/// @file RttProfiler.h
/// @brief Round-Trip-Time (RTT), jitter, and packet loss diagnostics profiler.

#include "Connection.h"
#include "PelcoDDevice.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace PelcoD {

/// @struct RttSample
/// @brief Telemetry record representing an individual probe round-trip measurement.
struct RttSample {
    std::uint64_t sequenceNumber { 0U };
    std::chrono::steady_clock::time_point timestamp { std::chrono::steady_clock::now() };
    double rttMs { 0.0 };
    std::string queryTag {};
    bool success { false };
};

/// @struct RttStatistics
/// @brief Aggregate statistical metrics characterizing latency, dispersion, and loss.
struct RttStatistics {
    std::uint64_t totalProbes { 0U };
    std::uint64_t successfulProbes { 0U };
    std::uint64_t timedOutProbes { 0U };
    double lossPercent { 0.0 };
    double currentRttMs { 0.0 };
    double minRttMs { 0.0 };
    double maxRttMs { 0.0 };
    double avgRttMs { 0.0 };
    double jitterRfc3550Ms { 0.0 };
    double stdDevMs { 0.0 };
    double p50RttMs { 0.0 };
    double p95RttMs { 0.0 };
    double p99RttMs { 0.0 };
};

/// @enum ProfilerMode
/// @brief Operational strategy governing probe dispatch and telemetry collection.
enum class ProfilerMode : std::uint8_t {
    ActiveBurst, ///< Dispatches a finite sequence of probes at periodic intervals.
    ActiveContinuous, ///< Periodically dispatches probes indefinitely until stopped.
    Passive ///< Observes normal controller telemetry queries without extra bus traffic.
};

/// @struct RttProfilerConfig
/// @brief Settings controlling profiler mode, probe pacing, burst size, and query type.
struct RttProfilerConfig {
    ProfilerMode mode { ProfilerMode::ActiveBurst };
    std::uint32_t intervalMs { 200U };
    std::uint32_t timeoutMs { 1000U };
    std::uint32_t burstCount { 20U };
    std::uint32_t historyCapacity { 100U };
    std::string probeQueryTag { "QueryPan" };
};

/// @class RttProfiler
/// @brief Real-time round-trip latency, jitter, and link health diagnostic engine.
/// @details Thread-safe engine computing online running mean and variance (Welford's algorithm),
///          RFC 3550 packet delay variation (jitter), rolling percentiles, and CSV/JSON export.
///
///          Threading contract:
///          - start(), stop(), and setDevice() may be called from any thread, including concurrently.
///          - Active modes run on one internal worker thread. FinishedCallback and the final
///            StateChangedCallback(false) of a self-completing burst run on that worker thread.
///          - From a worker-thread callback, start() returns false; stop() and setDevice() are safe.
///          - The profiler must not be destroyed from inside one of its own callbacks.
class RttProfiler {
public:
    using SampleCallback = std::function<void(const RttSample& sample, const RttStatistics& stats)>;
    using StatisticsCallback = std::function<void(const RttStatistics& stats)>;
    using StateChangedCallback = std::function<void(bool isRunning)>;
    using FinishedCallback = std::function<void(const RttStatistics& stats)>;

    /// @brief Constructs an RttProfiler associated with an optional PelcoDDevice controller.
    /// @param[in] device Shared pointer to target device controller.
    explicit RttProfiler(std::shared_ptr<PelcoDDevice> device = nullptr);

    /// @brief Destructor terminating any background worker loop.
    /// @details Calls stop(), which joins the worker. Precondition: must not run on the worker
    ///          thread (i.e. the profiler must not be destroyed from one of its own callbacks).
    ~RttProfiler();

    // Non-copyable, non-movable
    RttProfiler(const RttProfiler&) = delete;
    RttProfiler& operator=(const RttProfiler&) = delete;
    RttProfiler(RttProfiler&&) = delete;
    RttProfiler& operator=(RttProfiler&&) = delete;

    /// @brief Assign or rebind the target device controller.
    /// @details Stops any active session first. Safe to call from a profiler callback.
    /// @param[in] device Pointer to device controller.
    void setDevice(std::shared_ptr<PelcoDDevice> device);

    /// @brief Retrieve currently assigned device controller.
    /// @return Shared pointer to PelcoDDevice or nullptr.
    [[nodiscard]] std::shared_ptr<PelcoDDevice> getDevice() const;

    /// @brief Starts diagnostic profiling using the specified configuration.
    /// @details Resets statistics and fires StateChangedCallback(true) before any probe is sent.
    ///          A worker left over from a burst that completed on its own is joined first, so a
    ///          new session can start straight after the previous one finished.
    /// @param[in] config Profiler configuration options.
    /// @return True if profiling commenced; false if parameters are invalid, a session is already
    ///         running, the device is missing or disconnected (active modes), or the call is made
    ///         from a profiler callback running on the worker thread.
    [[nodiscard]] bool start(const RttProfilerConfig& config = RttProfilerConfig {});

    /// @brief Stops active probing and unhooks passive listeners.
    /// @details Joins the worker and fires StateChangedCallback(false) if a session was running.
    ///          When called from a callback on the worker thread, it only requests the stop; the
    ///          exiting worker is joined later by start() or the destructor.
    void stop();

    /// @brief Clears accumulated metrics, counters, and sample history.
    void reset();

    /// @brief Ingest an external latency measurement sample.
    /// @param[in] duration Latency duration with microsecond precision.
    /// @param[in] queryTag Identifier tag for the query command.
    /// @param[in] success Whether the query was answered successfully.
    void recordSample(std::chrono::microseconds duration, const std::string& queryTag, bool success);

    /// @brief Ingest an external latency measurement sample in milliseconds.
    /// @param[in] rttMs Latency duration in milliseconds.
    /// @param[in] queryTag Identifier tag for the query command.
    /// @param[in] success Whether the query was answered successfully.
    void recordSampleMs(double rttMs, const std::string& queryTag, bool success);

    /// @brief Query if profiler is actively gathering telemetry.
    /// @return True if running; false otherwise.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Retrieve current active profiler configuration.
    /// @return Active RttProfilerConfig.
    [[nodiscard]] RttProfilerConfig getConfig() const;

    /// @brief Retrieve current aggregate statistical metrics.
    /// @return Snapshot of RttStatistics.
    [[nodiscard]] RttStatistics getStatistics() const;

    /// @brief Retrieve snapshot of rolling history buffer.
    /// @return Vector of recent RttSample records.
    [[nodiscard]] std::vector<RttSample> getHistory() const;

    /// @brief Register callback invoked upon recording each latency sample.
    /// @param[in] cb Callback receiving sample and updated statistics.
    void setSampleCallback(SampleCallback cb);

    /// @brief Register callback invoked upon metric summary updates.
    /// @param[in] cb Callback receiving updated statistics.
    void setStatisticsCallback(StatisticsCallback cb);

    /// @brief Register callback invoked when profiler transitions between running and idle.
    /// @param[in] cb Callback receiving running state boolean.
    void setStateChangedCallback(StateChangedCallback cb);

    /// @brief Register callback invoked when an active burst scan completes.
    /// @param[in] cb Callback receiving final statistics snapshot.
    void setFinishedCallback(FinishedCallback cb);

    /// @brief Export collected history and statistics in CSV format.
    /// @param[out] os Output stream receiving CSV data.
    /// @return True on success; false on stream error.
    bool exportCsv(std::ostream& os) const;

    /// @brief Export collected history and statistics in JSON format.
    /// @param[out] os Output stream receiving JSON text.
    /// @return True on success; false on stream error.
    bool exportJson(std::ostream& os) const;

private:
    void activeWorkerLoop(RttProfilerConfig config);
    void dispatchProbeCommand(const std::string& tag);
    void updatePercentilesLocked();
    void resetStatisticsUnderLock() noexcept;

    /// @brief Check whether the calling thread is the profiler worker thread.
    /// @return True if called on the worker thread (e.g. from a worker-side callback).
    [[nodiscard]] bool isWorkerThread() const noexcept;

    /// @brief Join the worker thread if it is joinable and not the calling thread.
    /// @details Caller must hold m_lifecycleMutex and must not hold m_mutex, because the worker
    ///          needs m_mutex (and user callbacks may call getters) before it can exit.
    /// @return False if called on the worker thread itself (join skipped); true otherwise.
    [[nodiscard]] bool joinWorker();

    mutable std::mutex m_mutex;

    /// Serialises ownership of m_worker and m_deviceLatencyConn between start() and stop().
    /// Lock order: m_lifecycleMutex before m_mutex. Never held while invoking user callbacks.
    std::mutex m_lifecycleMutex {};
    std::shared_ptr<PelcoDDevice> m_device;

    RttProfilerConfig m_config {};
    RttStatistics m_stats {};
    std::deque<RttSample> m_history {};
    std::uint64_t m_nextSeq { 1U };

    /// Incremented (under m_mutex) each time start() claims a session; lets start() detect
    /// that its session was stopped or superseded while StateChangedCallback(true) ran.
    std::uint64_t m_session { 0U };

    // Welford algorithm state
    double m_welfordMean { 0.0 };
    double m_welfordM2 { 0.0 };

    // RFC 3550 jitter state
    double m_lastRttMs { 0.0 };
    double m_jitterRfc3550Ms { 0.0 };

    std::atomic<bool> m_running { false };
    std::atomic<bool> m_stopRequested { false };
    std::thread m_worker;
    /// Id of the current or most recent worker thread; written by the worker at entry.
    std::atomic<std::thread::id> m_workerId {};
    std::condition_variable m_cv;

    ScopedConnection m_deviceLatencyConn;

    SampleCallback m_sampleCb;
    StatisticsCallback m_statsCb;
    StateChangedCallback m_stateCb;
    FinishedCallback m_finishedCb;
};

} // namespace PelcoD
