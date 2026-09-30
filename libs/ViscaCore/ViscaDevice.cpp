#include "ViscaDevice.h"
#include "ViscaBuilder.h"
#include "ViscaParser.h"

namespace Visca {

ViscaDevice::ViscaDevice(std::shared_ptr<::Transport::ITransport> transport, uint8_t cameraAddress)
    : m_transport(std::move(transport))
    , m_cameraAddress(cameraAddress)
{
    if (m_transport) {
        m_transport->setDataCallback([this](const std::vector<uint8_t>& data) { m_accumulator.addData(data); });
    }
    m_accumulator.setFrameCallback([this](const ViscaFrame& frame) { onFrameReceived(frame); });
}

ViscaDevice::~ViscaDevice()
{
    std::scoped_lock lock(m_mutex);
    // Drain pending queues
    for (auto& cmd : m_commandQueue) {
        if (cmd.callback) {
            cmd.callback(
                CommandResult { false, ViscaSocket::None, ViscaErrorCode::CommandCanceled, "Device destroying" });
        }
    }
    m_commandQueue.clear();

    // Drain active socket commands
    for (auto& slot : m_sockets) {
        if (slot.state != ViscaSocketState::Idle) {
            if (slot.command.callback) {
                slot.command.callback(
                    CommandResult { false, slot.id, ViscaErrorCode::CommandCanceled, "Device destroying" });
            }
            slot.state = ViscaSocketState::Idle;
            slot.command = {};
        }
    }

    for (auto& inq : m_inquiryQueue) {
        if (inq.callback) {
            inq.callback(InquiryResult { false, ViscaFrame {}, "Device destroying" });
        }
    }
    m_inquiryQueue.clear();
}

void ViscaDevice::setCameraAddress(uint8_t address) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_cameraAddress = address;
}

uint8_t ViscaDevice::cameraAddress() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_cameraAddress;
}

void ViscaDevice::setTrafficCallback(TrafficCallback callback)
{
    std::scoped_lock lock(m_mutex);
    m_trafficCallback = std::move(callback);
}

bool ViscaDevice::isConnected() const noexcept
{
    return m_transport && m_transport->isOpen();
}

CommandResult ViscaDevice::sendCommandSync(const ViscaFrame& command, std::chrono::milliseconds timeout)
{
    auto promise = std::make_shared<std::promise<CommandResult>>();
    auto future = promise->get_future();

    sendCommandAsync(command, [promise](const CommandResult& res) { promise->set_value(res); });

    if (future.wait_for(timeout) == std::future_status::ready) {
        return future.get();
    }

    // Timed out: release assigned socket so subsequent commands are not blocked
    m_timeouts.fetch_add(1U, std::memory_order_relaxed);
    std::vector<ViscaFrame> framesToSend;
    {
        std::scoped_lock lock(m_mutex);
        for (auto& slot : m_sockets) {
            if (slot.state != ViscaSocketState::Idle && slot.command.frame == command) {
                slot.state = ViscaSocketState::Idle;
                slot.command = InFlightCommand {};
                collectFramesToSendLocked(framesToSend);
                break;
            }
        }
    }
    for (const auto& f : framesToSend) {
        sendFrameUnlocked(f);
    }

    return CommandResult { false, ViscaSocket::None, ViscaErrorCode::None,
        "Command timed out waiting for camera completion" };
}

void ViscaDevice::sendCommandAsync(const ViscaFrame& command, std::function<void(const CommandResult&)> onComplete)
{
    InFlightCommand inflight;
    inflight.frame = command;
    inflight.sendTime = std::chrono::steady_clock::now();
    inflight.callback = std::move(onComplete);

    std::vector<ViscaFrame> framesToSend;
    {
        std::scoped_lock lock(m_mutex);
        m_commandQueue.push_back(std::move(inflight));
        collectFramesToSendLocked(framesToSend);
    }

    for (const auto& f : framesToSend) {
        sendFrameUnlocked(f);
    }
}

InquiryResult ViscaDevice::sendInquirySync(const ViscaFrame& inquiry, std::chrono::milliseconds timeout)
{
    auto promise = std::make_shared<std::promise<InquiryResult>>();
    auto future = promise->get_future();

    sendInquiryAsync(inquiry, [promise](const InquiryResult& res) { promise->set_value(res); });

    if (future.wait_for(timeout) == std::future_status::ready) {
        return future.get();
    }

    m_timeouts.fetch_add(1U, std::memory_order_relaxed);
    return InquiryResult { false, ViscaFrame {}, "Inquiry timed out waiting for camera response" };
}

void ViscaDevice::sendInquiryAsync(const ViscaFrame& inquiry, std::function<void(const InquiryResult&)> onComplete)
{
    InFlightInquiry inflight;
    inflight.inquiryFrame = inquiry;
    inflight.sendTime = std::chrono::steady_clock::now();
    inflight.callback = std::move(onComplete);

    {
        std::scoped_lock lock(m_mutex);
        m_inquiryQueue.push_back(std::move(inflight));
    }
    sendFrameUnlocked(inquiry);
}

bool ViscaDevice::cancelSocket(ViscaSocket socket)
{
    if (socket == ViscaSocket::None) {
        return false;
    }
    const ViscaFrame cancelCmd = ViscaBuilder::commandCancel(m_cameraAddress, socket);
    sendFrameUnlocked(cancelCmd);
    return true;
}

CommandResult ViscaDevice::ifClear()
{
    const ViscaFrame cmd = ViscaBuilder::ifClear(m_cameraAddress);
    return sendCommandSync(cmd, std::chrono::milliseconds(1000));
}

void ViscaDevice::sendFrameUnlocked(const ViscaFrame& frame)
{
    std::shared_ptr<::Transport::ITransport> trans;
    TrafficCallback cb { nullptr };
    {
        std::scoped_lock lock(m_mutex);
        trans = m_transport;
        cb = m_trafficCallback;
    }

    bool sent = false;
    if (trans && trans->isOpen()) {
        sent = trans->sendData(frame.bytes());
    }
    if (cb && sent) {
        cb(frame, true);
    }
}

void ViscaDevice::collectFramesToSendLocked(std::vector<ViscaFrame>& outFrames)
{
    while (!m_commandQueue.empty()) {
        SocketSlot* idleSlot = nullptr;
        for (auto& slot : m_sockets) {
            if (slot.state == ViscaSocketState::Idle) {
                idleSlot = &slot;
                break;
            }
        }
        if (!idleSlot) {
            // All sockets are occupied
            break;
        }

        idleSlot->command = std::move(m_commandQueue.front());
        m_commandQueue.pop_front();
        idleSlot->command.assignedSocket = idleSlot->id;
        idleSlot->state = ViscaSocketState::AwaitingAck;
        outFrames.push_back(idleSlot->command.frame);
    }
}

void ViscaDevice::onFrameReceived(const ViscaFrame& frame)
{
    TrafficCallback trafficCb { nullptr };
    std::function<void(const CommandResult&)> cmdCallback { nullptr };
    CommandResult cmdRes {};
    std::function<void(const InquiryResult&)> inqCallback { nullptr };
    InquiryResult inqRes {};
    std::vector<ViscaFrame> framesToSend;

    {
        std::scoped_lock lock(m_mutex);
        trafficCb = m_trafficCallback;

        // Check if frame is ACK (y0 4s FF)
        if (frame.isAck()) {
            if (auto* slot = findSocketSlot(frame.socket())) {
                if (slot->state == ViscaSocketState::AwaitingAck) {
                    slot->state = ViscaSocketState::Executing;
                }
            }
        }
        // Check if frame is Command Completion (y0 5s FF, size == 3)
        else if (frame.isCompletion() && frame.size() == 3) {
            const ViscaSocket sock = frame.socket();
            SocketSlot* slot = (sock != ViscaSocket::None) ? findSocketSlot(sock) : findActiveSocketSlot();
            if (slot && slot->state != ViscaSocketState::Idle) {
                if (slot->id == ViscaSocket::Socket1) {
                    m_socket1Processed.fetch_add(1U, std::memory_order_relaxed);
                } else if (slot->id == ViscaSocket::Socket2) {
                    m_socket2Processed.fetch_add(1U, std::memory_order_relaxed);
                }
                const auto turnaroundUs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - slot->command.sendTime)
                                                                    .count());
                m_totalTurnaroundUs.fetch_add(turnaroundUs, std::memory_order_relaxed);
                m_lastTurnaroundUs.store(turnaroundUs, std::memory_order_relaxed);

                slot->state = ViscaSocketState::Idle;
                cmdCallback = std::move(slot->command.callback);
                cmdRes = CommandResult { true, slot->id, ViscaErrorCode::None, "" };
                slot->command = {};
                collectFramesToSendLocked(framesToSend);
                m_cv.notify_all();
            }
        }
        // Check if frame is Inquiry Response (y0 50 ... FF, size >= 4)
        else if (frame.isInquiryResponse()) {
            m_inquiriesProcessed.fetch_add(1U, std::memory_order_relaxed);
            if (!m_inquiryQueue.empty()) {
                InFlightInquiry inq = std::move(m_inquiryQueue.front());
                m_inquiryQueue.pop_front();
                inqCallback = std::move(inq.callback);
                inqRes = InquiryResult { true, frame, "" };
                m_cv.notify_all();
            }
        }
        // Check if frame is Error (y0 6s ee FF)
        else if (frame.isError()) {
            const ViscaSocket sock = frame.socket();
            const ViscaErrorCode code = frame.errorCode();
            const std::string_view errStr = errorCodeToString(code);

            switch (code) {
            case ViscaErrorCode::SyntaxError:
                m_syntaxErrors.fetch_add(1U, std::memory_order_relaxed);
                break;
            case ViscaErrorCode::CommandBufferFull:
                m_bufferFullErrors.fetch_add(1U, std::memory_order_relaxed);
                break;
            case ViscaErrorCode::CommandCanceled:
                m_cancelledCommands.fetch_add(1U, std::memory_order_relaxed);
                break;
            case ViscaErrorCode::NoSocket:
                m_noSocketErrors.fetch_add(1U, std::memory_order_relaxed);
                break;
            case ViscaErrorCode::CommandNotExecutable:
                m_executionErrors.fetch_add(1U, std::memory_order_relaxed);
                break;
            default:
                break;
            }

            if (auto* slot = findSocketSlot(sock); slot && slot->state != ViscaSocketState::Idle) {
                slot->state = ViscaSocketState::Idle;
                cmdCallback = std::move(slot->command.callback);
                cmdRes = CommandResult { false, slot->id, code, std::string(errStr) };
                slot->command = {};
                collectFramesToSendLocked(framesToSend);
                m_cv.notify_all();
            } else if (!m_inquiryQueue.empty()) {
                // Inquiry failed with error
                InFlightInquiry inq = std::move(m_inquiryQueue.front());
                m_inquiryQueue.pop_front();
                inqCallback = std::move(inq.callback);
                inqRes = InquiryResult { false, frame, std::string(errStr) };
                m_cv.notify_all();
            }
        }
    }

    if (trafficCb) {
        trafficCb(frame, false);
    }
    if (cmdCallback) {
        cmdCallback(cmdRes);
    }
    if (inqCallback) {
        inqCallback(inqRes);
    }

    for (const auto& f : framesToSend) {
        sendFrameUnlocked(f);
    }
}

CommandResult ViscaDevice::panTiltDrive(
    uint8_t panSpeed, uint8_t tiltSpeed, ViscaPanDirection panDir, ViscaTiltDirection tiltDir)
{
    const ViscaFrame cmd = ViscaBuilder::panTiltDrive(cameraAddress(), panSpeed, tiltSpeed, panDir, tiltDir);
    return sendCommandSync(cmd);
}

CommandResult ViscaDevice::panTiltStop(uint8_t panSpeed, uint8_t tiltSpeed)
{
    const ViscaFrame cmd = ViscaBuilder::panTiltStop(cameraAddress(), panSpeed, tiltSpeed);
    return sendCommandSync(cmd);
}

CommandResult ViscaDevice::panTiltUp(uint8_t tiltSpeed)
{
    const ViscaFrame cmd = ViscaBuilder::panTiltUp(cameraAddress(), tiltSpeed);
    return sendCommandSync(cmd);
}

CommandResult ViscaDevice::panTiltDown(uint8_t tiltSpeed)
{
    const ViscaFrame cmd = ViscaBuilder::panTiltDown(cameraAddress(), tiltSpeed);
    return sendCommandSync(cmd);
}

CommandResult ViscaDevice::panTiltLeft(uint8_t panSpeed)
{
    const ViscaFrame cmd = ViscaBuilder::panTiltLeft(cameraAddress(), panSpeed);
    return sendCommandSync(cmd);
}

CommandResult ViscaDevice::panTiltRight(uint8_t panSpeed)
{
    const ViscaFrame cmd = ViscaBuilder::panTiltRight(cameraAddress(), panSpeed);
    return sendCommandSync(cmd);
}

CommandResult ViscaDevice::panTiltAbsolute(uint8_t panSpeed, uint8_t tiltSpeed, int16_t panPos, int16_t tiltPos)
{
    const ViscaFrame cmd = ViscaBuilder::panTiltAbsolute(cameraAddress(), panSpeed, tiltSpeed, panPos, tiltPos);
    return sendCommandSync(cmd);
}

CommandResult ViscaDevice::panTiltRelative(uint8_t panSpeed, uint8_t tiltSpeed, int16_t deltaPan, int16_t deltaTilt)
{
    const ViscaFrame cmd = ViscaBuilder::panTiltRelative(cameraAddress(), panSpeed, tiltSpeed, deltaPan, deltaTilt);
    return sendCommandSync(cmd);
}

CommandResult ViscaDevice::panTiltHome()
{
    const ViscaFrame cmd = ViscaBuilder::panTiltHome(cameraAddress());
    return sendCommandSync(cmd);
}

CommandResult ViscaDevice::panTiltReset()
{
    const ViscaFrame cmd = ViscaBuilder::panTiltReset(cameraAddress());
    return sendCommandSync(cmd);
}

CommandResult ViscaDevice::panTiltLimitSet(ViscaPanTiltCorner corner, int16_t panPos, int16_t tiltPos)
{
    const ViscaFrame cmd = ViscaBuilder::panTiltLimitSet(cameraAddress(), corner, panPos, tiltPos);
    return sendCommandSync(cmd);
}

CommandResult ViscaDevice::panTiltLimitClear(ViscaPanTiltCorner corner)
{
    const ViscaFrame cmd = ViscaBuilder::panTiltLimitClear(cameraAddress(), corner);
    return sendCommandSync(cmd);
}

std::optional<ViscaPanTiltPosition> ViscaDevice::queryPanTiltPosition(std::chrono::milliseconds timeout)
{
    const ViscaFrame inq = ViscaBuilder::panTiltPositionInquiry(cameraAddress());
    const InquiryResult res = sendInquirySync(inq, timeout);
    if (!res.success) {
        return std::nullopt;
    }
    return ViscaParser::parsePanTiltPosition(res.responseFrame);
}

std::optional<ViscaPanTiltStatus> ViscaDevice::queryPanTiltStatus(std::chrono::milliseconds timeout)
{
    const ViscaFrame inq = ViscaBuilder::panTiltStatusInquiry(cameraAddress());
    const InquiryResult res = sendInquirySync(inq, timeout);
    if (!res.success) {
        return std::nullopt;
    }
    return ViscaParser::parsePanTiltStatus(res.responseFrame);
}

ViscaProtocolStats ViscaDevice::getProtocolStats() const
{
    ViscaProtocolStats stats {};
    {
        std::scoped_lock lock(m_mutex);
        for (const auto& slot : m_sockets) {
            if (slot.id == ViscaSocket::Socket1) {
                stats.socket1State = slot.state;
            } else if (slot.id == ViscaSocket::Socket2) {
                stats.socket2State = slot.state;
            }
        }
        stats.pendingCommands = m_commandQueue.size();
        stats.pendingInquiries = m_inquiryQueue.size();
    }
    stats.socket1Processed = m_socket1Processed.load(std::memory_order_relaxed);
    stats.socket2Processed = m_socket2Processed.load(std::memory_order_relaxed);
    stats.inquiriesProcessed = m_inquiriesProcessed.load(std::memory_order_relaxed);
    stats.syntaxErrors = m_syntaxErrors.load(std::memory_order_relaxed);
    stats.bufferFullErrors = m_bufferFullErrors.load(std::memory_order_relaxed);
    stats.cancelledCommands = m_cancelledCommands.load(std::memory_order_relaxed);
    stats.noSocketErrors = m_noSocketErrors.load(std::memory_order_relaxed);
    stats.executionErrors = m_executionErrors.load(std::memory_order_relaxed);
    stats.timeouts = m_timeouts.load(std::memory_order_relaxed);

    const uint64_t totalCompleted = stats.socket1Processed + stats.socket2Processed;
    const uint64_t totalUs = m_totalTurnaroundUs.load(std::memory_order_relaxed);
    if (totalCompleted > 0U) {
        stats.avgTurnaroundMs = static_cast<double>(totalUs) / (1000.0 * static_cast<double>(totalCompleted));
    }
    stats.lastTurnaroundMs = static_cast<double>(m_lastTurnaroundUs.load(std::memory_order_relaxed)) / 1000.0;
    return stats;
}

void ViscaDevice::resetProtocolStats() noexcept
{
    m_socket1Processed.store(0U, std::memory_order_relaxed);
    m_socket2Processed.store(0U, std::memory_order_relaxed);
    m_inquiriesProcessed.store(0U, std::memory_order_relaxed);
    m_syntaxErrors.store(0U, std::memory_order_relaxed);
    m_bufferFullErrors.store(0U, std::memory_order_relaxed);
    m_cancelledCommands.store(0U, std::memory_order_relaxed);
    m_noSocketErrors.store(0U, std::memory_order_relaxed);
    m_executionErrors.store(0U, std::memory_order_relaxed);
    m_timeouts.store(0U, std::memory_order_relaxed);
    m_totalTurnaroundUs.store(0U, std::memory_order_relaxed);
    m_lastTurnaroundUs.store(0U, std::memory_order_relaxed);
}

} // namespace Visca
