#pragma once

/// @file CbrRatePacer.h
/// @brief ISO/IEC 13818-1 Constant Bitrate (CBR) rate pacer and null packet stuffing engine.

#include <cstddef>
#include <cstdint>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace Transport {

/// @struct CbrPacerConfig
/// @brief Configuration settings for Constant Bitrate (CBR) rate pacing.
struct CbrPacerConfig {
    /// @brief Target constant transmission bitrate in bits per second (bps). Default: 4.0 Mbps.
    std::uint32_t targetBitrateBps { 4000000U };

    /// @brief Maximum number of TS packets permitted in the burst absorption queue. Default: 2048.
    std::size_t maxQueuePackets { 2048U };

    /// @brief Maximum TS packets allowed in a single unpaced burst. Default: 14.
    std::size_t maxBurstPackets { 14U };

    /// @brief ISO/IEC 13818-1 PCR jitter tolerance limit in nanoseconds (nominal: 500 ns).
    std::uint32_t pcrJitterLimitNs { 500U };

    /// @brief Whether to restamp PCR timestamps upon departure to eliminate queue jitter.
    bool enablePcrRestamping { true };

    /// @brief Whether to start the real-time background pacing thread automatically.
    bool autoStartWorker { true };
};

/// @struct CbrPacerStats
/// @brief Real-time telemetry and throughput metrics for CBR pacing.
struct CbrPacerStats {
    /// @brief Total payload TS packets received from upstream.
    std::uint64_t payloadPacketsIn { 0U };

    /// @brief Total payload TS packets emitted to sink.
    std::uint64_t payloadPacketsOut { 0U };

    /// @brief Total ISO/IEC 13818-1 Null TS packets (PID 0x1FFF) injected.
    std::uint64_t nullPacketsInjected { 0U };

    /// @brief Total TS packets dropped due to burst queue overflow.
    std::uint64_t droppedPackets { 0U };

    /// @brief Measured instantaneous output bitrate in bits per second.
    double measuredBitrateBps { 0.0 };

    /// @brief Maximum observed PCR deviation/jitter in nanoseconds.
    double maxPcrJitterNs { 0.0 };

    /// @brief Current packet queue occupancy.
    std::size_t currentQueueSize { 0U };
};

/// @class CbrRatePacer
/// @brief Converts bursty VBR MPEG-TS streams into rigid Constant Bitrate broadcasts.
/// @details Enforces ISO/IEC 13818-1 Clause 2.4.2 system target decoder compliance and
///          Clause 2.4.2.2 PCR jitter boundaries (<= 500 ns) via high-precision scheduling
///          and deterministic Null TS packet stuffing (PID 0x1FFF).
class CbrRatePacer {
public:
    /// @brief TS packet size constant (188 bytes).
    static constexpr std::size_t kTsPacketSize { 188U };

    /// @brief ISO/IEC 13818-1 Null Packet PID (0x1FFF).
    static constexpr std::uint16_t kNullPid { 0x1FFFU };

    /// @brief Callback signature for paced TS packet delivery.
    using EmitPacketCallback = std::function<void(const std::uint8_t* data, std::size_t size)>;

    /// @brief Constructs CBR rate pacer with configuration and delivery sink callback.
    /// @param[in] config Rate pacing configuration parameters.
    /// @param[in] callback Sink callback receiving paced 188-byte TS packets.
    explicit CbrRatePacer(const CbrPacerConfig& config, EmitPacketCallback callback);

    /// @brief Destructor. Stops worker thread and clears buffers.
    ~CbrRatePacer();

    CbrRatePacer(const CbrRatePacer&) = delete;
    CbrRatePacer& operator=(const CbrRatePacer&) = delete;
    CbrRatePacer(CbrRatePacer&&) = delete;
    CbrRatePacer& operator=(CbrRatePacer&&) = delete;

    /// @brief Ingests one or more contiguous 188-byte TS packets into the burst queue.
    /// @param[in] packets Pointer to TS packet buffer (must be multiple of 188 bytes).
    /// @param[in] count Number of 188-byte TS packets in the buffer.
    /// @return Number of packets successfully queued.
    [[nodiscard]] std::size_t submitTsPackets(const std::uint8_t* packets, std::size_t count);

    /// @brief Deterministically steps the pacer by a virtual time interval (for offline / unit test use).
    /// @param[in] stepDuration Time duration to advance the pacer clock.
    /// @return Total number of packets (payload + null) emitted during this step.
    [[nodiscard]] std::size_t step(std::chrono::nanoseconds stepDuration);

    /// @brief Flushes all queued payload packets immediately without rate delay.
    void flush();

    /// @brief Starts the background real-time pacing thread if stopped.
    /// @return True if worker thread is running.
    [[nodiscard]] bool startWorker();

    /// @brief Stops the background real-time pacing thread.
    void stopWorker();

    /// @brief Checks whether the background pacing worker is active.
    /// @return True if worker thread is currently running.
    [[nodiscard]] bool isRunning() const noexcept;

    /// @brief Resets all internal queues, stats, and clock baselines.
    void reset() noexcept;

    /// @brief Updates configuration dynamically (e.g. adjusts target bitrate).
    /// @param[in] config New configuration.
    /// @return True if configuration is valid and applied.
    [[nodiscard]] bool setConfig(const CbrPacerConfig& config);

    /// @brief Returns current active configuration.
    /// @return Active configuration snapshot.
    [[nodiscard]] CbrPacerConfig config() const;

    /// @brief Retrieves real-time pacing diagnostics and telemetry.
    /// @return Diagnostics metrics snapshot.
    [[nodiscard]] CbrPacerStats stats() const;

    /// @brief Returns instantaneous measured output bitrate in bps.
    [[nodiscard]] double measuredBitrate() const noexcept;

    /// @brief Returns total null packets injected since start or reset.
    [[nodiscard]] std::size_t nullPacketsInjected() const noexcept;

    /// @brief Accesses pre-formatted standard ISO/IEC 13818-1 Null TS packet.
    /// @return Reference to 188-byte static null packet array.
    [[nodiscard]] static const std::array<std::uint8_t, kTsPacketSize>& nullPacket() noexcept;

    /// @brief Checks if a 188-byte TS packet contains an adaptation field with a PCR.
    /// @param[in] packet Pointer to 188-byte TS packet.
    /// @return True if valid PCR is present.
    [[nodiscard]] static bool hasPcr(const std::uint8_t* packet) noexcept;

private:
    void workerLoop();
    void emitPacket(const std::uint8_t* packet, std::size_t size);
    void restampPcr(std::uint8_t* packet, std::uint64_t currentPcrTicks);

    CbrPacerConfig m_config { };
    EmitPacketCallback m_callback { };

    mutable std::mutex m_mutex { };
    std::condition_variable m_cv { };
    std::atomic<bool> m_running { false };
    std::thread m_workerThread { };

    // Pre-allocated circular packet queue (storing 188-byte blocks)
    std::vector<std::uint8_t> m_packetBuffer { };
    std::size_t m_queueCapacity { 0U };
    std::size_t m_queueHead { 0U };
    std::size_t m_queueTail { 0U };
    std::size_t m_queueCount { 0U };

    // Monotonic clock & timing state
    std::chrono::steady_clock::time_point m_startTime { };
    std::chrono::nanoseconds m_virtualElapsed { 0 };
    std::uint64_t m_packetsScheduled { 0U };

    // Bitrate calculation window
    std::chrono::steady_clock::time_point m_lastBitrateCalc { };
    std::uint64_t m_bytesSinceLastCalc { 0U };

    // Diagnostic telemetry counters
    mutable std::mutex m_statsMutex { };
    CbrPacerStats m_stats { };
};

} // namespace Transport
