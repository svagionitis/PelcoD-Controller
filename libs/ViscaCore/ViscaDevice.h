#pragma once

#include "ViscaFrame.h"
#include "ViscaRxAccumulator.h"
#include "ViscaTypes.h"
#include <Transport/ITransport.h>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace Visca {

/// @struct CommandResult
/// @brief Result of a VISCA command execution.
struct CommandResult {
    bool success { false }; ///< True if command completed successfully
    ViscaSocket socket { ViscaSocket::None }; ///< Socket that executed the command
    ViscaErrorCode errorCode { ViscaErrorCode::None }; ///< Error code if command failed
    std::string errorMessage { "" }; ///< Diagnostic error description
};

/// @struct InquiryResult
/// @brief Result of a VISCA inquiry execution.
struct InquiryResult {
    bool success { false }; ///< True if inquiry response was received
    ViscaFrame responseFrame {}; ///< Raw response frame (y0 50 ... FF)
    std::string errorMessage { "" }; ///< Diagnostic error description
};

/// @class ViscaDevice
/// @brief Controller device managing the VISCA 2-socket state machine over an ITransport layer.
/// @details Handles packet pacing, concurrent socket 1 & 2 execution, ACK/Completion tracking,
/// fast-path inquiries, timeout detection, and daisy-chain address dispatch.
class ViscaDevice {
public:
    /// @brief Callback invoked when raw frames are sent or received (for logging/traffic inspector).
    using TrafficCallback = std::function<void(const ViscaFrame& frame, bool outgoing)>;

    /// @brief Constructs a ViscaDevice wrapping an underlying transport.
    /// @param[in] transport Shared pointer to a protocol-agnostic @ref Transport::ITransport.
    /// @param[in] cameraAddress Target camera address (1..7). Defaults to 1.
    explicit ViscaDevice(std::shared_ptr<::Transport::ITransport> transport, uint8_t cameraAddress = 1);

    /// @brief Destructor. Closes in-flight transactions.
    virtual ~ViscaDevice();

    // Non-copyable
    ViscaDevice(const ViscaDevice&) = delete;
    ViscaDevice& operator=(const ViscaDevice&) = delete;

    /// @brief Sets or updates the target camera address for this device.
    /// @param[in] address Camera address (1..7).
    void setCameraAddress(uint8_t address) noexcept;

    /// @brief Retrieves current target camera address.
    [[nodiscard]] uint8_t cameraAddress() const noexcept;

    /// @brief Sets a callback to observe incoming and outgoing VISCA traffic.
    /// @param[in] callback Callable accepting frame and direction flag.
    void setTrafficCallback(TrafficCallback callback);

    /// @brief Sends a command synchronously, blocking until completed or timed out.
    /// @param[in] command Frame to send.
    /// @param[in] timeout Maximum wait duration.
    /// @return @ref CommandResult containing execution status.
    [[nodiscard]] CommandResult sendCommandSync(
        const ViscaFrame& command, std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

    /// @brief Sends a command asynchronously with a completion callback.
    /// @param[in] command Frame to send.
    /// @param[in] onComplete Callback invoked upon completion or failure.
    void sendCommandAsync(const ViscaFrame& command, std::function<void(const CommandResult&)> onComplete);

    /// @brief Sends an inquiry synchronously, blocking until response received.
    /// @param[in] inquiry Inquiry frame (8x 09 ... FF).
    /// @param[in] timeout Maximum wait duration.
    /// @return @ref InquiryResult containing response payload.
    [[nodiscard]] InquiryResult sendInquirySync(
        const ViscaFrame& inquiry, std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));

    /// @brief Sends an inquiry asynchronously with a completion callback.
    /// @param[in] inquiry Inquiry frame.
    /// @param[in] onComplete Callback invoked upon response or timeout.
    void sendInquiryAsync(const ViscaFrame& inquiry, std::function<void(const InquiryResult&)> onComplete);

    /// @brief Cancels command execution on the specified socket (8x 2s FF).
    /// @param[in] socket Target socket to cancel (Socket1 or Socket2).
    /// @return True if cancel command was dispatched.
    bool cancelSocket(ViscaSocket socket);

    /// @brief Clears camera interface buffer (IF_Clear 8x 01 00 01 FF).
    /// @return CommandResult status.
    [[nodiscard]] CommandResult ifClear();

    /// @brief Checks if the underlying transport is currently open.
    [[nodiscard]] bool isConnected() const noexcept;

    /// @brief Accesses the underlying transport.
    [[nodiscard]] std::shared_ptr<::Transport::ITransport> transport() const noexcept
    {
        return m_transport;
    }

protected:
    /// @brief Internal handler called when a valid frame arrives from accumulator.
    void onFrameReceived(const ViscaFrame& frame);

private:
    enum class SocketState { Idle, AwaitingAck, Executing };

    struct InFlightCommand {
        ViscaFrame frame {};
        ViscaSocket assignedSocket { ViscaSocket::None };
        std::chrono::steady_clock::time_point sendTime {};
        std::function<void(const CommandResult&)> callback { nullptr };
    };

    struct InFlightInquiry {
        ViscaFrame inquiryFrame {};
        std::chrono::steady_clock::time_point sendTime {};
        std::function<void(const InquiryResult&)> callback { nullptr };
    };

    void collectFramesToSendLocked(std::vector<ViscaFrame>& outFrames);
    void sendFrameUnlocked(const ViscaFrame& frame);

    std::shared_ptr<::Transport::ITransport> m_transport;
    uint8_t m_cameraAddress { 1 };

    mutable std::mutex m_mutex {};
    std::condition_variable m_cv {};

    ViscaRxAccumulator m_accumulator {};

    SocketState m_socket1State { SocketState::Idle };
    SocketState m_socket2State { SocketState::Idle };
    InFlightCommand m_socket1Command {};
    InFlightCommand m_socket2Command {};

    std::deque<InFlightCommand> m_commandQueue {};
    std::deque<InFlightInquiry> m_inquiryQueue {};

    TrafficCallback m_trafficCallback { nullptr };
};

} // namespace Visca
