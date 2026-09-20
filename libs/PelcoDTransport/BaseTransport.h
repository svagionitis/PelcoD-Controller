#pragma once

/// @file BaseTransport.h
/// @brief Abstract base class managing callbacks, thread synchronization, and lifecycle for streaming transports.

#include "ITransport.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace PelcoD::Transport {

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

protected:
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
};

} // namespace PelcoD::Transport

namespace PelcoD {
using Transport::BaseTransport;
} // namespace PelcoD
