#pragma once

/// @file BaseTransport.h
/// @brief Abstract base class managing callbacks, thread synchronization, and lifecycle for streaming transports.

#include "ITransport.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Transport {

/// @class BaseTransport
/// @brief Abstract base class implementing callback storage, thread synchronization, and worker lifecycle.
/// @details Thread-safe base class providing write mutex protection, state notification dispatching,
///          and read thread lifecycle management common to streaming transports (e.g., TCP, Serial).
class BaseTransport : public ITransport {
public:
    BaseTransport() = default;
    ~BaseTransport() override = default;

    // Non-copyable, non-movable
    BaseTransport(const BaseTransport&) = delete;
    BaseTransport& operator=(const BaseTransport&) = delete;
    BaseTransport(BaseTransport&&) = delete;
    BaseTransport& operator=(BaseTransport&&) = delete;

    /// @brief Registers callback for incoming raw bytes.
    /// @param[in] callback Function invoked when new data arrives.
    void setDataCallback(DataReceivedCallback callback) override;

    /// @brief Registers callback for transport state changes.
    /// @param[in] callback Function invoked on connect, disconnect, or error.
    void setStateCallback(StateChangedCallback callback) override;

    /// @brief Captures real-time transport telemetry and diagnostic counters.
    /// @return Aggregated snapshot containing generic metrics. Subclasses can add kernel metrics.
    [[nodiscard]] TransportStatsSnapshot getStats() const override;

    /// @brief Resets transport error and performance counters.
    void resetStats() noexcept override;

protected:
    /// @brief Records transmitted byte and packet counters.
    /// @param[in] bytes Number of bytes successfully sent.
    void recordBytesSent(std::size_t bytes) noexcept;

    /// @brief Records received byte and packet counters.
    /// @param[in] bytes Number of bytes successfully received.
    void recordBytesReceived(std::size_t bytes) noexcept;

    /// @brief Records a transmission failure error.
    void recordTxError() noexcept;

    /// @brief Records a reception failure error.
    void recordRxError() noexcept;

    /// @brief Records a reconnection event.
    void recordReconnect() noexcept;

    /// @brief Populates generic transport statistics into a snapshot.
    /// @param[out] snapshot Target snapshot structure.
    void populateGenericStats(TransportStatsSnapshot& snapshot) const;

    /// @brief Dispatches a transport state notification to the registered state callback.
    /// @param[in] state The new transport connection state.
    /// @param[in] errorMsg Diagnostic message or error description.
    void notifyState(TransportState state, const std::string& errorMsg);

    /// @brief Dispatches incoming received bytes to the registered data callback.
    /// @param[in] data Received byte buffer.
    void invokeDataCallback(const std::vector<std::uint8_t>& data);

    /// @brief Signals the worker thread to terminate and joins it if active.
    void stopReadThread();

    std::atomic<bool> m_running { false };
    std::thread m_readThread;

    mutable std::mutex m_writeMutex;

    mutable std::mutex m_callbackMutex;
    DataReceivedCallback m_dataCallback;
    StateChangedCallback m_stateCallback;

    std::atomic<std::uint64_t> m_bytesSent { 0U };
    std::atomic<std::uint64_t> m_bytesReceived { 0U };
    std::atomic<std::uint64_t> m_packetsSent { 0U };
    std::atomic<std::uint64_t> m_packetsReceived { 0U };
    std::atomic<std::uint64_t> m_txErrorCount { 0U };
    std::atomic<std::uint64_t> m_rxErrorCount { 0U };
    std::atomic<std::uint32_t> m_reconnectCount { 0U };

    mutable std::mutex m_timeMutex;
    std::chrono::steady_clock::time_point m_lastTxTime {};
    std::chrono::steady_clock::time_point m_lastRxTime {};
};

} // namespace Transport
