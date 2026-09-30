#pragma once

#include "ViscaFrame.h"
#include "ViscaRxAccumulator.h"
#include "ViscaTypes.h"
#include <Transport/ITransport.h>

#include <array>
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

    // --- Pan/Tilt Drive Control (Opcode 0x06) ---

    /// @brief Drives pan and tilt axes with independent speeds and directions.
    /// @param[in] panSpeed Pan speed index (0x01..0x18).
    /// @param[in] tiltSpeed Tilt speed index (0x01..0x14).
    /// @param[in] panDir Pan motion direction.
    /// @param[in] tiltDir Tilt motion direction.
    /// @return @ref CommandResult containing execution status.
    [[nodiscard]] CommandResult panTiltDrive(
        uint8_t panSpeed, uint8_t tiltSpeed, ViscaPanDirection panDir, ViscaTiltDirection tiltDir);

    /// @brief Halts pan and tilt axis motion.
    /// @param[in] panSpeed Optional deceleration speed. Defaults to 0.
    /// @param[in] tiltSpeed Optional deceleration speed. Defaults to 0.
    /// @return @ref CommandResult containing execution status.
    [[nodiscard]] CommandResult panTiltStop(uint8_t panSpeed = 0, uint8_t tiltSpeed = 0);

    /// @brief Drives tilt axis upwards.
    /// @param[in] tiltSpeed Tilt speed index (0x01..0x14).
    /// @return @ref CommandResult.
    [[nodiscard]] CommandResult panTiltUp(uint8_t tiltSpeed);

    /// @brief Drives tilt axis downwards.
    /// @param[in] tiltSpeed Tilt speed index (0x01..0x14).
    /// @return @ref CommandResult.
    [[nodiscard]] CommandResult panTiltDown(uint8_t tiltSpeed);

    /// @brief Drives pan axis leftwards.
    /// @param[in] panSpeed Pan speed index (0x01..0x18).
    /// @return @ref CommandResult.
    [[nodiscard]] CommandResult panTiltLeft(uint8_t panSpeed);

    /// @brief Drives pan axis rightwards.
    /// @param[in] panSpeed Pan speed index (0x01..0x18).
    /// @return @ref CommandResult.
    [[nodiscard]] CommandResult panTiltRight(uint8_t panSpeed);

    /// @brief Slews pan and tilt to absolute step coordinates.
    /// @param[in] panSpeed Pan speed index (0x01..0x18).
    /// @param[in] tiltSpeed Tilt speed index (0x01..0x14).
    /// @param[in] panPos Signed 16-bit Pan target step coordinate.
    /// @param[in] tiltPos Signed 16-bit Tilt target step coordinate.
    /// @return @ref CommandResult containing execution status.
    [[nodiscard]] CommandResult panTiltAbsolute(uint8_t panSpeed, uint8_t tiltSpeed, int16_t panPos, int16_t tiltPos);

    /// @brief Applies a relative step offset displacement to pan and tilt.
    /// @param[in] panSpeed Pan speed index (0x01..0x18).
    /// @param[in] tiltSpeed Tilt speed index (0x01..0x14).
    /// @param[in] deltaPan Signed 16-bit Pan step offset.
    /// @param[in] deltaTilt Signed 16-bit Tilt step offset.
    /// @return @ref CommandResult containing execution status.
    [[nodiscard]] CommandResult panTiltRelative(
        uint8_t panSpeed, uint8_t tiltSpeed, int16_t deltaPan, int16_t deltaTilt);

    /// @brief Returns the pan/tilt mechanism to home origin (0, 0).
    /// @return @ref CommandResult containing execution status.
    [[nodiscard]] CommandResult panTiltHome();

    /// @brief Re-initializes pan/tilt mechanism motors.
    /// @return @ref CommandResult containing execution status.
    [[nodiscard]] CommandResult panTiltReset();

    /// @brief Configures mechanical boundary limit coordinates.
    /// @param[in] corner Boundary corner (DownLeft or UpRight).
    /// @param[in] panPos Signed 16-bit Pan boundary coordinate.
    /// @param[in] tiltPos Signed 16-bit Tilt boundary coordinate.
    /// @return @ref CommandResult containing execution status.
    [[nodiscard]] CommandResult panTiltLimitSet(ViscaPanTiltCorner corner, int16_t panPos, int16_t tiltPos);

    /// @brief Clears software boundary limits.
    /// @param[in] corner Boundary corner to clear.
    /// @return @ref CommandResult containing execution status.
    [[nodiscard]] CommandResult panTiltLimitClear(ViscaPanTiltCorner corner);

    // --- Pan/Tilt Inquiries ---

    /// @brief Queries current 16-bit signed Pan and Tilt coordinates.
    /// @param[in] timeout Maximum wait duration.
    /// @return Optional @ref ViscaPanTiltPosition if inquiry succeeded, std::nullopt otherwise.
    [[nodiscard]] std::optional<ViscaPanTiltPosition> queryPanTiltPosition(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));

    /// @brief Queries current Pan/Tilt status telemetry and limit flags.
    /// @param[in] timeout Maximum wait duration.
    /// @return Optional @ref ViscaPanTiltStatus if inquiry succeeded, std::nullopt otherwise.
    [[nodiscard]] std::optional<ViscaPanTiltStatus> queryPanTiltStatus(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));

    /// @brief Checks if the underlying transport is currently open.
    [[nodiscard]] bool isConnected() const noexcept;

    /// @brief Accesses the underlying transport.
    [[nodiscard]] std::shared_ptr<::Transport::ITransport> transport() const noexcept
    {
        return m_transport;
    }

    /// @brief Captures transport-layer and kernel-level communication statistics.
    /// @return Aggregated snapshot containing generic and kernel-level metrics.
    [[nodiscard]] ::Transport::TransportStatsSnapshot getTransportStats() const
    {
        if (m_transport) {
            return m_transport->getStats();
        }
        return {};
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

    struct SocketSlot {
        ViscaSocket id { ViscaSocket::None };
        SocketState state { SocketState::Idle };
        InFlightCommand command {};
    };

    struct InFlightInquiry {
        ViscaFrame inquiryFrame {};
        std::chrono::steady_clock::time_point sendTime {};
        std::function<void(const InquiryResult&)> callback { nullptr };
    };

    [[nodiscard]] SocketSlot* findSocketSlot(ViscaSocket socket) noexcept
    {
        for (auto& slot : m_sockets) {
            if (slot.id == socket) {
                return &slot;
            }
        }
        return nullptr;
    }

    [[nodiscard]] SocketSlot* findActiveSocketSlot() noexcept
    {
        for (auto& slot : m_sockets) {
            if (slot.state != SocketState::Idle) {
                return &slot;
            }
        }
        return nullptr;
    }

    void collectFramesToSendLocked(std::vector<ViscaFrame>& outFrames);
    void sendFrameUnlocked(const ViscaFrame& frame);

    std::shared_ptr<::Transport::ITransport> m_transport;
    uint8_t m_cameraAddress { 1 };

    mutable std::mutex m_mutex {};
    std::condition_variable m_cv {};

    ViscaRxAccumulator m_accumulator {};

    static constexpr size_t kSocketCount { 2 };
    std::array<SocketSlot, kSocketCount> m_sockets { { { ViscaSocket::Socket1, SocketState::Idle, {} },
        { ViscaSocket::Socket2, SocketState::Idle, {} } } };

    std::deque<InFlightCommand> m_commandQueue {};
    std::deque<InFlightInquiry> m_inquiryQueue {};

    TrafficCallback m_trafficCallback { nullptr };
};

} // namespace Visca
