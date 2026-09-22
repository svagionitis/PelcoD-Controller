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
        if (slot.state != SocketState::Idle) {
            if (slot.command.callback) {
                slot.command.callback(
                    CommandResult { false, slot.id, ViscaErrorCode::CommandCanceled, "Device destroying" });
            }
            slot.state = SocketState::Idle;
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
    std::vector<ViscaFrame> framesToSend;
    {
        std::scoped_lock lock(m_mutex);
        for (auto& slot : m_sockets) {
            if (slot.state != SocketState::Idle && slot.command.frame == command) {
                slot.state = SocketState::Idle;
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
            if (slot.state == SocketState::Idle) {
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
        idleSlot->state = SocketState::AwaitingAck;
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
                if (slot->state == SocketState::AwaitingAck) {
                    slot->state = SocketState::Executing;
                }
            }
        }
        // Check if frame is Command Completion (y0 5s FF, size == 3)
        else if (frame.isCompletion() && frame.size() == 3) {
            const ViscaSocket sock = frame.socket();
            SocketSlot* slot = (sock != ViscaSocket::None) ? findSocketSlot(sock) : findActiveSocketSlot();
            if (slot && slot->state != SocketState::Idle) {
                slot->state = SocketState::Idle;
                cmdCallback = std::move(slot->command.callback);
                cmdRes = CommandResult { true, slot->id, ViscaErrorCode::None, "" };
                slot->command = {};
                collectFramesToSendLocked(framesToSend);
                m_cv.notify_all();
            }
        }
        // Check if frame is Inquiry Response (y0 50 ... FF, size >= 4)
        else if (frame.isInquiryResponse()) {
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

            if (auto* slot = findSocketSlot(sock); slot && slot->state != SocketState::Idle) {
                slot->state = SocketState::Idle;
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

} // namespace Visca
