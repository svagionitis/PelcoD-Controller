#pragma once

#include "ViscaFrame.h"
#include "ViscaRxAccumulator.h"
#include <Transport/ITransport.h>
#include <Transport/TransportTypes.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace Visca::Testing {

/// @class MockViscaTransport
/// @brief Standard simulated VISCA transport for core unit testing.
class MockViscaTransport : public ::Transport::ITransport {
public:
    explicit MockViscaTransport(uint8_t cameraAddress = 1)
        : m_address(cameraAddress)
    {
        m_accumulator.setFrameCallback([this](const ViscaFrame& frame) { handleFrame(frame); });
    }

    bool open() override
    {
        m_open = true;
        if (m_stateCallback) {
            m_stateCallback(::Transport::TransportState::Connected, "");
        }
        return true;
    }

    void close() override
    {
        m_open = false;
        if (m_stateCallback) {
            m_stateCallback(::Transport::TransportState::Disconnected, "");
        }
    }

    [[nodiscard]] bool isOpen() const noexcept override
    {
        return m_open.load();
    }

    bool sendData(const std::vector<uint8_t>& data) override
    {
        if (!m_open.load()) {
            return false;
        }
        m_accumulator.addData(data);
        return true;
    }

    void setDataCallback(DataReceivedCallback callback) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dataCallback = std::move(callback);
    }

    void setStateCallback(StateChangedCallback callback) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stateCallback = std::move(callback);
    }

    bool setBaudRate(uint32_t baudRate) override
    {
        m_baudRate = baudRate;
        return true;
    }

    [[nodiscard]] uint32_t getBaudRate() const noexcept override
    {
        return m_baudRate;
    }

private:
    void sendResponse(const ViscaFrame& frame)
    {
        if (m_dataCallback && m_open.load()) {
            m_dataCallback(frame.bytes());
        }
    }

    void handleFrame(const ViscaFrame& frame)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // AddressSet broadcast (88 30 01 FF)
        if (frame.size() == 4 && frame[0] == 0x88 && frame[1] == 0x30 && frame[2] == 0x01) {
            sendResponse(ViscaFrame { 0x88, 0x30, static_cast<uint8_t>(m_address + 1), kViscaTerminator });
            return;
        }

        const uint8_t respHdr = static_cast<uint8_t>(0x80 | ((m_address & 0x07) << 4));

        // IF_Clear (8x 01 00 01 FF)
        if ((frame.isBroadcast() || frame.destinationAddress() == m_address) && frame.size() == 5 && frame[1] == 0x01
            && frame[2] == 0x00 && frame[3] == 0x01) {
            m_socket1Busy = false;
            m_socket2Busy = false;
            sendResponse(ViscaFrame { respHdr, 0x50, kViscaTerminator });
            return;
        }

        if (!frame.isBroadcast() && frame.destinationAddress() != m_address) {
            return;
        }

        // Cancel command (8x 2s FF)
        if (frame.size() == 3 && (frame[1] & 0xF0) == 0x20) {
            const uint8_t s = static_cast<uint8_t>(frame[1] & 0x0F);
            if (s == 1)
                m_socket1Busy = false;
            if (s == 2)
                m_socket2Busy = false;
            sendResponse(ViscaFrame { respHdr, static_cast<uint8_t>(0x60 | s), 0x04, kViscaTerminator });
            return;
        }

        // Version inquiry (8x 09 00 02 FF)
        if (frame.size() == 5 && frame[1] == 0x09 && frame[2] == 0x00 && frame[3] == 0x02) {
            sendResponse(ViscaFrame { respHdr, 0x50, 0x00, 0x20, // Vendor: Sony
                0x07, 0x11, // Model: EV9520L
                0x01, 0x00, // ROM
                0x02, // 2 Sockets
                kViscaTerminator });
            return;
        }

        // Standard commands
        ViscaSocket sock = ViscaSocket::None;
        if (!m_socket1Busy) {
            sock = ViscaSocket::Socket1;
            m_socket1Busy = true;
        } else if (!m_socket2Busy) {
            sock = ViscaSocket::Socket2;
            m_socket2Busy = true;
        } else {
            sendResponse(ViscaFrame { respHdr, 0x60, 0x03, kViscaTerminator });
            return;
        }

        // Send ACK
        const uint8_t ackByte = (sock == ViscaSocket::Socket1) ? 0x41 : 0x42;
        sendResponse(ViscaFrame { respHdr, ackByte, kViscaTerminator });

        // Release socket and send completion
        if (sock == ViscaSocket::Socket1)
            m_socket1Busy = false;
        else
            m_socket2Busy = false;

        const uint8_t compByte = (sock == ViscaSocket::Socket1) ? 0x51 : 0x52;
        sendResponse(ViscaFrame { respHdr, compByte, kViscaTerminator });
    }

    uint8_t m_address { 1 };
    mutable std::mutex m_mutex {};
    std::atomic<bool> m_open { true };
    uint32_t m_baudRate { 9600 };
    DataReceivedCallback m_dataCallback { nullptr };
    StateChangedCallback m_stateCallback { nullptr };
    ViscaRxAccumulator m_accumulator {};

    bool m_socket1Busy { false };
    bool m_socket2Busy { false };
};

} // namespace Visca::Testing
