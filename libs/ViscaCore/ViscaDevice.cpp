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
    std::lock_guard<std::mutex> lock(m_mutex);
    // Drain pending queues
    for (auto& cmd : m_commandQueue) {
        if (cmd.callback) {
            cmd.callback(
                CommandResult { false, ViscaSocket::None, ViscaErrorCode::CommandCanceled, "Device destroying" });
        }
    }
    m_commandQueue.clear();

    for (auto& inq : m_inquiryQueue) {
        if (inq.callback) {
            inq.callback(InquiryResult { false, ViscaFrame {}, "Device destroying" });
        }
    }
    m_inquiryQueue.clear();
}

void ViscaDevice::setCameraAddress(uint8_t address) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cameraAddress = address;
}

uint8_t ViscaDevice::cameraAddress() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cameraAddress;
}

void ViscaDevice::setTrafficCallback(TrafficCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
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
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_socket1State != SocketState::Idle && m_socket1Command.frame == command) {
            m_socket1State = SocketState::Idle;
            m_socket1Command = InFlightCommand {};
            collectFramesToSendLocked(framesToSend);
        } else if (m_socket2State != SocketState::Idle && m_socket2Command.frame == command) {
            m_socket2State = SocketState::Idle;
            m_socket2Command = InFlightCommand {};
            collectFramesToSendLocked(framesToSend);
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
        std::lock_guard<std::mutex> lock(m_mutex);
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
        std::lock_guard<std::mutex> lock(m_mutex);
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
        std::lock_guard<std::mutex> lock(m_mutex);
        trans = m_transport;
        cb = m_trafficCallback;
    }

    if (trans && trans->isOpen()) {
        static_cast<void>(trans->sendData(frame.bytes()));
    }
    if (cb) {
        cb(frame, true);
    }
}

void ViscaDevice::collectFramesToSendLocked(std::vector<ViscaFrame>& outFrames)
{
    while (!m_commandQueue.empty()) {
        if (m_socket1State == SocketState::Idle) {
            m_socket1Command = std::move(m_commandQueue.front());
            m_commandQueue.pop_front();
            m_socket1Command.assignedSocket = ViscaSocket::Socket1;
            m_socket1State = SocketState::AwaitingAck;
            outFrames.push_back(m_socket1Command.frame);
        } else if (m_socket2State == SocketState::Idle) {
            m_socket2Command = std::move(m_commandQueue.front());
            m_commandQueue.pop_front();
            m_socket2Command.assignedSocket = ViscaSocket::Socket2;
            m_socket2State = SocketState::AwaitingAck;
            outFrames.push_back(m_socket2Command.frame);
        } else {
            // Both sockets are occupied
            break;
        }
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
        std::lock_guard<std::mutex> lock(m_mutex);
        trafficCb = m_trafficCallback;

        // Check if frame is ACK (y0 4s FF)
        if (frame.isAck()) {
            const ViscaSocket sock = frame.socket();
            if (sock == ViscaSocket::Socket1 && m_socket1State == SocketState::AwaitingAck) {
                m_socket1State = SocketState::Executing;
            } else if (sock == ViscaSocket::Socket2 && m_socket2State == SocketState::AwaitingAck) {
                m_socket2State = SocketState::Executing;
            }
        }
        // Check if frame is Command Completion (y0 5s FF, size == 3)
        else if (frame.isCompletion() && frame.size() == 3) {
            const ViscaSocket sock = frame.socket();
            if (sock == ViscaSocket::Socket1 && m_socket1State != SocketState::Idle) {
                m_socket1State = SocketState::Idle;
                cmdCallback = std::move(m_socket1Command.callback);
                cmdRes = CommandResult { true, ViscaSocket::Socket1, ViscaErrorCode::None, "" };
                collectFramesToSendLocked(framesToSend);
                m_cv.notify_all();
            } else if (sock == ViscaSocket::Socket2 && m_socket2State != SocketState::Idle) {
                m_socket2State = SocketState::Idle;
                cmdCallback = std::move(m_socket2Command.callback);
                cmdRes = CommandResult { true, ViscaSocket::Socket2, ViscaErrorCode::None, "" };
                collectFramesToSendLocked(framesToSend);
                m_cv.notify_all();
            } else if (sock == ViscaSocket::None) {
                // Sockets not distinguished (e.g. IF_Clear completion y0 50 FF)
                if (m_socket1State != SocketState::Idle) {
                    m_socket1State = SocketState::Idle;
                    cmdCallback = std::move(m_socket1Command.callback);
                    cmdRes = CommandResult { true, ViscaSocket::Socket1, ViscaErrorCode::None, "" };
                    collectFramesToSendLocked(framesToSend);
                    m_cv.notify_all();
                } else if (m_socket2State != SocketState::Idle) {
                    m_socket2State = SocketState::Idle;
                    cmdCallback = std::move(m_socket2Command.callback);
                    cmdRes = CommandResult { true, ViscaSocket::Socket2, ViscaErrorCode::None, "" };
                    collectFramesToSendLocked(framesToSend);
                    m_cv.notify_all();
                }
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
            const std::string errStr(errorCodeToString(code));

            if (sock == ViscaSocket::Socket1 && m_socket1State != SocketState::Idle) {
                m_socket1State = SocketState::Idle;
                cmdCallback = std::move(m_socket1Command.callback);
                cmdRes = CommandResult { false, ViscaSocket::Socket1, code, errStr };
                collectFramesToSendLocked(framesToSend);
                m_cv.notify_all();
            } else if (sock == ViscaSocket::Socket2 && m_socket2State != SocketState::Idle) {
                m_socket2State = SocketState::Idle;
                cmdCallback = std::move(m_socket2Command.callback);
                cmdRes = CommandResult { false, ViscaSocket::Socket2, code, errStr };
                collectFramesToSendLocked(framesToSend);
                m_cv.notify_all();
            } else if (!m_inquiryQueue.empty()) {
                // Inquiry failed with error
                InFlightInquiry inq = std::move(m_inquiryQueue.front());
                m_inquiryQueue.pop_front();
                inqCallback = std::move(inq.callback);
                inqRes = InquiryResult { false, frame, errStr };
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
