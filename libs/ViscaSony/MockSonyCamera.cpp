#include "MockSonyCamera.h"
#include <Transport/TransportTypes.h>

namespace Visca::Sony {

MockSonyCamera::MockSonyCamera(SonyCameraModelType model, uint8_t address)
    : m_modelType(model)
    , m_address(address)
{
    m_registers.fill(0);
    m_accumulator.setFrameCallback([this](const ViscaFrame& frame) { processIncomingFrame(frame); });
}

bool MockSonyCamera::open()
{
    m_open = true;
    if (m_stateCallback) {
        m_stateCallback(::Transport::TransportState::Connected, "");
    }
    return true;
}

void MockSonyCamera::close()
{
    m_open = false;
    if (m_stateCallback) {
        m_stateCallback(::Transport::TransportState::Disconnected, "");
    }
}

bool MockSonyCamera::isOpen() const noexcept
{
    return m_open.load();
}

bool MockSonyCamera::sendData(const std::vector<uint8_t>& data)
{
    if (!m_open.load()) {
        return false;
    }
    m_accumulator.addData(data);
    return true;
}

void MockSonyCamera::setDataCallback(DataReceivedCallback callback)
{
    std::scoped_lock lock(m_mutex);
    m_dataCallback = std::move(callback);
}

void MockSonyCamera::setStateCallback(StateChangedCallback callback)
{
    std::scoped_lock lock(m_mutex);
    m_stateCallback = std::move(callback);
}

bool MockSonyCamera::setBaudRate(uint32_t baudRate)
{
    m_baudRate = baudRate;
    return true;
}

uint32_t MockSonyCamera::getBaudRate() const noexcept
{
    return m_baudRate;
}

void MockSonyCamera::setModelType(SonyCameraModelType model)
{
    std::scoped_lock lock(m_mutex);
    m_modelType = model;
}

SonyCameraModelType MockSonyCamera::modelType() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_modelType;
}

void MockSonyCamera::setCameraAddress(uint8_t address) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_address = address;
}

uint8_t MockSonyCamera::cameraAddress() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_address;
}

uint16_t MockSonyCamera::zoomPosition() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_zoomPosition;
}

void MockSonyCamera::setZoomPosition(uint16_t pos) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_zoomPosition = pos;
}

uint16_t MockSonyCamera::focusPosition() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_focusPosition;
}

void MockSonyCamera::setFocusPosition(uint16_t pos) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_focusPosition = pos;
}

uint8_t MockSonyCamera::registerValue(uint8_t reg) const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_registers[reg & 0x7F];
}

void MockSonyCamera::setRegisterValue(uint8_t reg, uint8_t val) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_registers[reg & 0x7F] = val;
}

SonyExposureMode MockSonyCamera::exposureMode() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_exposureMode;
}

SonyWhiteBalanceMode MockSonyCamera::wbMode() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_wbMode;
}

SonyStabilizerMode MockSonyCamera::stabilizerMode() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_stabilizerMode;
}

SonyDefogMode MockSonyCamera::defogMode() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_defogMode;
}

void MockSonyCamera::injectNextError(ViscaErrorCode code) noexcept
{
    std::scoped_lock lock(m_mutex);
    m_injectedError = code;
}

void MockSonyCamera::sendResponse(const ViscaFrame& frame)
{
    if (m_dataCallback && m_open.load()) {
        m_dataCallback(frame.bytes());
    }
}

void MockSonyCamera::processIncomingFrame(const ViscaFrame& frame)
{
    std::scoped_lock lock(m_mutex);

    // AddressSet broadcast (88 30 01 FF)
    if (frame.size() == 4 && frame[0] == 0x88 && frame[1] == 0x30 && frame[2] == 0x01) {
        const uint8_t nextAddress = static_cast<uint8_t>(m_address + 1);
        sendResponse(ViscaFrame { 0x88, 0x30, nextAddress, kViscaTerminator });
        return;
    }

    const uint8_t respHdr = static_cast<uint8_t>(0x80 | ((m_address & 0x07) << 4));

    // IF_Clear broadcast (88 01 00 01 FF) or targeted IF_Clear
    if ((frame.isBroadcast() || frame.destinationAddress() == m_address) && frame.size() == 5 && frame[1] == 0x01
        && frame[2] == 0x00 && frame[3] == 0x01) {
        m_socket1Busy = false;
        m_socket2Busy = false;
        sendResponse(ViscaFrame { respHdr, 0x50, kViscaTerminator });
        return;
    }

    // Check if addressed to this camera
    if (!frame.isBroadcast() && frame.destinationAddress() != m_address) {
        return;
    }

    // Cancel command (8x 2s FF)
    if (frame.size() == 3 && (frame[1] & 0xF0) == 0x20) {
        const uint8_t s = static_cast<uint8_t>(frame[1] & 0x0F);
        if (s == 1) {
            m_socket1Busy = false;
        } else if (s == 2) {
            m_socket2Busy = false;
        }
        sendResponse(ViscaFrame { respHdr, static_cast<uint8_t>(0x60 | s), 0x04, kViscaTerminator });
        return;
    }

    // Inquiries (8x 09 ... FF)
    if (frame.messageType() == ViscaMessageType::Inquiry || (frame.size() >= 2 && frame[1] == 0x09)) {
        handleInquiry(frame);
    } else {
        handleCommand(frame);
    }
}

void MockSonyCamera::handleInquiry(const ViscaFrame& frame)
{
    const uint8_t respHdr = static_cast<uint8_t>(0x80 | ((m_address & 0x07) << 4));

    // CAM_VersionInq (8x 09 00 02 FF)
    if (frame.size() == 5 && frame[2] == 0x00 && frame[3] == 0x02) {
        uint8_t modelH = 0x07;
        uint8_t modelL = 0x11; // Default EV9520L
        if (m_modelType == SonyCameraModelType::FCB_EW9500H) {
            modelL = 0x0F;
        } else if (m_modelType == SonyCameraModelType::GenericSony) {
            modelH = 0x00;
            modelL = 0x01;
        }

        sendResponse(ViscaFrame { respHdr, 0x50, 0x00, 0x20, // Vendor: Sony
            modelH, modelL, 0x01, 0x00, // ROM 1.00
            0x02, // 2 Sockets
            kViscaTerminator });
        return;
    }

    // CAM_PowerInq (8x 09 04 00 FF)
    if (frame.size() == 5 && frame[2] == 0x04 && frame[3] == 0x00) {
        sendResponse(ViscaFrame { respHdr, 0x50, 0x02, kViscaTerminator });
        return;
    }

    // Register Inquiry (8x 09 04 24 mm FF)
    if (frame.size() == 6 && frame[2] == 0x04 && frame[3] == 0x24) {
        const uint8_t reg = static_cast<uint8_t>(frame[4] & 0x7F);
        const uint8_t val = m_registers[reg];
        sendResponse(ViscaFrame { respHdr, 0x50, static_cast<uint8_t>((val >> 4) & 0x0F),
            static_cast<uint8_t>(val & 0x0F), kViscaTerminator });
        return;
    }

    // Block Inquiries (8x 09 7E 7E 0p FF)
    if (frame.size() == 6 && frame[2] == 0x7E && frame[3] == 0x7E) {
        const uint8_t blockIdx = static_cast<uint8_t>(frame[4] & 0x0F);
        switch (blockIdx) {
        case 0:
            sendResponse(buildBlock00Response());
            break;
        case 1:
            sendResponse(buildBlock01Response());
            break;
        case 2:
            sendResponse(buildBlock02Response());
            break;
        case 3:
            sendResponse(buildBlock03Response());
            break;
        case 4:
            sendResponse(buildBlock04Response());
            break;
        default:
            sendResponse(ViscaFrame { respHdr, 0x60, 0x02, kViscaTerminator });
            break;
        }
        return;
    }

    // Unhandled inquiry
    sendResponse(ViscaFrame { respHdr, 0x60, 0x02, kViscaTerminator });
}

void MockSonyCamera::handleCommand(const ViscaFrame& frame)
{
    const uint8_t respHdr = static_cast<uint8_t>(0x80 | ((m_address & 0x07) << 4));

    if (m_injectedError != ViscaErrorCode::None) {
        const auto err = m_injectedError;
        m_injectedError = ViscaErrorCode::None;
        sendResponse(ViscaFrame { respHdr, 0x60, static_cast<uint8_t>(err), kViscaTerminator });
        return;
    }

    // Allocate execution socket
    ViscaSocket sock = ViscaSocket::None;
    if (!m_socket1Busy) {
        sock = ViscaSocket::Socket1;
        m_socket1Busy = true;
    } else if (!m_socket2Busy) {
        sock = ViscaSocket::Socket2;
        m_socket2Busy = true;
    } else {
        // Both sockets busy -> Buffer Full
        sendResponse(
            ViscaFrame { respHdr, 0x60, static_cast<uint8_t>(ViscaErrorCode::CommandBufferFull), kViscaTerminator });
        return;
    }

    // Dispatch ACK (y0 4s FF)
    const uint8_t ackByte = (sock == ViscaSocket::Socket1) ? 0x41 : 0x42;
    sendResponse(ViscaFrame { respHdr, ackByte, kViscaTerminator });

    // Execute command mutations
    if (frame.size() >= 5 && frame[1] == 0x01 && frame[2] == 0x04) {
        const uint8_t cmdCategory = frame[3];

        if (cmdCategory == 0x47 && frame.size() >= 9) {
            // Zoom Direct
            m_zoomPosition = ViscaFrame::unpackWordNibbles(frame.data() + 4);
        } else if (cmdCategory == 0x48 && frame.size() >= 9) {
            // Focus Direct
            m_focusPosition = ViscaFrame::unpackWordNibbles(frame.data() + 4);
        } else if (cmdCategory == 0x38 && frame.size() >= 6) {
            // Focus Auto/Manual
            m_focusAuto = (frame[4] == 0x02);
        } else if (cmdCategory == 0x39 && frame.size() >= 6) {
            // Exposure Mode
            m_exposureMode = static_cast<SonyExposureMode>(frame[4]);
        } else if (cmdCategory == 0x4A && frame.size() >= 9) {
            // Shutter Direct
            m_shutterPosition = static_cast<uint8_t>(((frame[6] & 0x0F) << 4) | (frame[7] & 0x0F));
        } else if (cmdCategory == 0x4B && frame.size() >= 9) {
            // Iris Direct
            m_irisPosition = static_cast<uint8_t>(((frame[6] & 0x0F) << 4) | (frame[7] & 0x0F));
        } else if (cmdCategory == 0x4C && frame.size() >= 9) {
            // Gain Direct
            m_gainPosition = static_cast<uint8_t>(((frame[6] & 0x0F) << 4) | (frame[7] & 0x0F));
        } else if (cmdCategory == 0x34 && frame.size() >= 6) {
            // Stabilizer
            if (frame[4] == 0x00) {
                m_stabilizerOn = false;
                m_stabilizerMode = SonyStabilizerMode::Off;
            } else if (frame[4] == 0x02) {
                m_stabilizerOn = true;
                m_stabilizerMode = SonyStabilizerMode::Normal;
            } else if (frame[4] == 0x04) {
                m_stabilizerOn = true;
                m_stabilizerMode = SonyStabilizerMode::Super;
            } else if (frame[4] == 0x05) {
                m_stabilizerOn = true;
                m_stabilizerMode = SonyStabilizerMode::SuperPlus;
            }
        } else if (cmdCategory == 0x37 && frame.size() >= 6) {
            // Defog
            m_defogMode = static_cast<SonyDefogMode>(frame[4]);
        } else if (cmdCategory == 0x01 && frame.size() >= 6) {
            // ICR
            m_icrOn = (frame[4] == 0x02);
        } else if (cmdCategory == 0x24 && frame.size() >= 8) {
            // Write Register (8x 01 04 24 mm 0p 0q FF)
            const uint8_t reg = static_cast<uint8_t>(frame[4] & 0x7F);
            const uint8_t val = static_cast<uint8_t>(((frame[5] & 0x0F) << 4) | (frame[6] & 0x0F));
            m_registers[reg] = val;
        }
    }

    // Release socket
    if (sock == ViscaSocket::Socket1) {
        m_socket1Busy = false;
    } else {
        m_socket2Busy = false;
    }

    // Dispatch Completion (y0 5s FF)
    const uint8_t compByte = (sock == ViscaSocket::Socket1) ? 0x51 : 0x52;
    sendResponse(ViscaFrame { respHdr, compByte, kViscaTerminator });
}

ViscaFrame MockSonyCamera::buildBlock00Response() const
{
    const uint8_t respHdr = static_cast<uint8_t>(0x80 | ((m_address & 0x07) << 4));
    const auto zNibbles = ViscaFrame::packWordNibbles(m_zoomPosition);
    const auto fNibbles = ViscaFrame::packWordNibbles(m_focusPosition);
    const auto lNibbles = ViscaFrame::packWordNibbles(m_focusNearLimit);

    uint8_t b2 = 0x00;
    if (m_focusAuto)
        b2 |= 0x01;
    if (m_dzoomOn)
        b2 |= 0x02;
    if (!m_dzoomCombine)
        b2 |= 0x20;

    return ViscaFrame { respHdr, 0x50, b2, zNibbles[0], zNibbles[1], zNibbles[2], zNibbles[3], fNibbles[0], fNibbles[1],
        fNibbles[2], fNibbles[3], lNibbles[0], lNibbles[1], 0x00, 0x00, kViscaTerminator };
}

ViscaFrame MockSonyCamera::buildBlock01Response() const
{
    const uint8_t respHdr = static_cast<uint8_t>(0x80 | ((m_address & 0x07) << 4));
    uint8_t b2 = 0x00;
    if (m_exposureCompOn)
        b2 |= 0x02;

    const auto rNibbles = ViscaFrame::packByteNibbles(static_cast<uint8_t>(m_rGain & 0xFF));
    const auto bNibbles = ViscaFrame::packByteNibbles(static_cast<uint8_t>(m_bGain & 0xFF));

    return ViscaFrame { respHdr, 0x50, b2, static_cast<uint8_t>(m_wbMode),
        0x05, // Aperture
        static_cast<uint8_t>(m_exposureMode), m_shutterPosition, m_irisPosition, m_gainPosition, m_exposureCompPosition,
        0x00, rNibbles[0], rNibbles[1], bNibbles[0], bNibbles[1], kViscaTerminator };
}

ViscaFrame MockSonyCamera::buildBlock02Response() const
{
    const uint8_t respHdr = static_cast<uint8_t>(0x80 | ((m_address & 0x07) << 4));
    uint8_t b2 = 0x01; // Power On
    if (m_modelType == SonyCameraModelType::FCB_EV9520L) {
        b2 |= 0x10; // ICR Color
    }

    uint8_t b3 = 0x00;
    if (m_stabilizerMode == SonyStabilizerMode::SuperPlus) {
        b3 |= 0x03;
    } else if (m_stabilizerMode == SonyStabilizerMode::Super) {
        b3 |= 0x02;
    }
    if (m_icrOn)
        b3 |= 0x10;
    if (m_stabilizerOn)
        b3 |= 0x40;

    return ViscaFrame { respHdr, 0x50, b2, b3,
        0x00, // Display/Title/Privacy
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Camera ID
        0x0E, // System features
        0x00, 0x00, kViscaTerminator };
}

ViscaFrame MockSonyCamera::buildBlock03Response() const
{
    const uint8_t respHdr = static_cast<uint8_t>(0x80 | ((m_address & 0x07) << 4));
    return ViscaFrame { respHdr, 0x50,
        0x05, // Features provided
        0x00, 0x00, // DZoom pos
        0x00, 0x00, // AF Active
        0x00, 0x00, // AF Interval
        0x01, // Gamma / AE resp
        0x02, // NR level
        0x07, // Color gain
        0x00, // Chroma suppress
        0x00, 0x00, kViscaTerminator };
}

ViscaFrame MockSonyCamera::buildBlock04Response() const
{
    const uint8_t respHdr = static_cast<uint8_t>(0x80 | ((m_address & 0x07) << 4));
    const uint8_t defogByte = (m_defogMode != SonyDefogMode::Off) ? 0x01 : 0x00;
    const uint8_t defogLvl = static_cast<uint8_t>(m_defogMode);

    return ViscaFrame { respHdr, 0x50, 0x00, 0x00, 0x00,
        0x02, // Brightness comp
        0x00, 0x00, defogByte,
        0x00, // Wide-D
        0x00,
        0x01, // VE comp level
        0x00, defogLvl, 0x00, kViscaTerminator };
}

} // namespace Visca::Sony
