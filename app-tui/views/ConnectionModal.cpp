#include "ConnectionModal.h"

#include "MockPelcoDDevice.h"
#include "SerialTransport.h"
#include "TcpTransport.h"
#include "UdpTransport.h"
#include "UtfSymbols.h"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <sstream>

namespace PelcoDTui {

ConnectionModal::ConnectionModal()
{
    m_detectedPorts = PelcoD::SerialTransport::enumeratePorts();
    if (!m_detectedPorts.empty()) {
        m_config.serialPort = m_detectedPorts.front();
    }
}

void ConnectionModal::setOpen(bool open) noexcept
{
    m_isOpen = open;
    if (open) {
        m_detectedPorts = PelcoD::SerialTransport::enumeratePorts();
        if (m_config.type == TransportType::Serial && !m_detectedPorts.empty() && m_config.serialPort.empty()) {
            m_config.serialPort = m_detectedPorts.front();
        }
        m_tcpPortStr = std::to_string(m_config.tcpPort);
        m_udpPortStr = std::to_string(m_config.udpPort);
        m_errorMessage.clear();
        resetCursor();
    }
}

void ConnectionModal::setConfig(const ConnectionConfig& config)
{
    m_config = config;
    m_tcpPortStr = std::to_string(m_config.tcpPort);
    m_udpPortStr = std::to_string(m_config.udpPort);
    resetCursor();
}

bool ConnectionModal::hasPendingConnect() noexcept
{
    const bool res = m_pendingConnect;
    m_pendingConnect = false;
    return res;
}

std::shared_ptr<PelcoD::ITransport> ConnectionModal::createTransport(const ConnectionConfig& config)
{
    switch (config.type) {
    case TransportType::Mock: {
        auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(config.address);
        mock->setKinematicsConfig(config.kinematicsConfig);
        mock->setLatencyConfig(config.latencyConfig);
        return mock;
    }
    case TransportType::Tcp:
        return std::make_shared<PelcoD::TcpTransport>(config.tcpHost, config.tcpPort);
    case TransportType::Udp:
        return std::make_shared<PelcoD::UdpTransport>(config.udpHost, config.udpPort, config.udpLocalPort);
    case TransportType::Serial:
        return std::make_shared<PelcoD::SerialTransport>(config.serialPort, config.serialBaud);
    default: {
        auto mock = std::make_shared<PelcoD::MockPelcoDDevice>(config.address);
        mock->setKinematicsConfig(config.kinematicsConfig);
        mock->setLatencyConfig(config.latencyConfig);
        return mock;
    }
    }
}

bool ConnectionModal::isTextEditingField() const noexcept
{
    if (m_config.type == TransportType::Tcp || m_config.type == TransportType::Udp) {
        return (m_selectedField == 2 || m_selectedField == 3);
    }
    if (m_config.type == TransportType::Serial) {
        return (m_selectedField == 2);
    }
    return false;
}

void ConnectionModal::resetCursor() noexcept
{
    if (m_selectedField == 2) {
        if (m_config.type == TransportType::Tcp) {
            m_cursorPos = static_cast<int>(m_config.tcpHost.size());
        } else if (m_config.type == TransportType::Udp) {
            m_cursorPos = static_cast<int>(m_config.udpHost.size());
        } else if (m_config.type == TransportType::Serial) {
            m_cursorPos = static_cast<int>(m_config.serialPort.size());
        } else {
            m_cursorPos = 0;
        }
    } else if (m_selectedField == 3) {
        if (m_config.type == TransportType::Tcp) {
            m_cursorPos = static_cast<int>(m_tcpPortStr.size());
        } else if (m_config.type == TransportType::Udp) {
            m_cursorPos = static_cast<int>(m_udpPortStr.size());
        } else {
            m_cursorPos = 0;
        }
    } else {
        m_cursorPos = 0;
    }
}

bool ConnectionModal::validateAndApply()
{
    if (m_config.type == TransportType::Tcp) {
        if (m_config.tcpHost.empty()) {
            m_errorMessage = "Error: TCP Host/IP address cannot be empty";
            return false;
        }
        if (m_tcpPortStr.empty()) {
            m_errorMessage = "Error: TCP Port cannot be empty";
            return false;
        }

        std::uint16_t portVal { 0U };
        const char* first = m_tcpPortStr.data();
        const char* last = m_tcpPortStr.data() + m_tcpPortStr.size();
        auto [ptr, ec] = std::from_chars(first, last, portVal);
        if (ec != std::errc {} || ptr != last || portVal == 0U) {
            m_errorMessage = "Error: TCP Port must be between 1 and 65535";
            return false;
        }
        m_config.tcpPort = portVal;
    } else if (m_config.type == TransportType::Udp) {
        if (m_config.udpHost.empty()) {
            m_errorMessage = "Error: UDP Host/IP address cannot be empty";
            return false;
        }
        if (m_udpPortStr.empty()) {
            m_errorMessage = "Error: UDP Port cannot be empty";
            return false;
        }

        std::uint16_t portVal { 0U };
        const char* first = m_udpPortStr.data();
        const char* last = m_udpPortStr.data() + m_udpPortStr.size();
        auto [ptr, ec] = std::from_chars(first, last, portVal);
        if (ec != std::errc {} || ptr != last || portVal == 0U) {
            m_errorMessage = "Error: UDP Port must be between 1 and 65535";
            return false;
        }
        m_config.udpPort = portVal;
    } else if (m_config.type == TransportType::Serial) {
        if (m_config.serialPort.empty()) {
            m_errorMessage = "Error: Serial device path cannot be empty";
            return false;
        }
    }

    m_errorMessage.clear();
    m_pendingConnect = true;
    m_isOpen = false;
    return true;
}

void ConnectionModal::renderTextField(Canvas& canvas, int x, int y, std::string_view text, bool isSelected,
    const Style& textStyle, const Style& cursorStyle) const
{
    if (!isSelected) {
        canvas.drawString(x, y, text.empty() ? "(empty)" : text, textStyle);
        return;
    }

    const int textLen = static_cast<int>(text.size());
    const int cursor = std::clamp(m_cursorPos, 0, textLen);

    if (cursor > 0) {
        canvas.drawString(x, y, text.substr(0, static_cast<std::size_t>(cursor)), textStyle);
    }

    if (cursor < textLen) {
        canvas.drawString(x + cursor, y, text.substr(static_cast<std::size_t>(cursor), 1), cursorStyle);
        if (cursor + 1 < textLen) {
            canvas.drawString(x + cursor + 1, y, text.substr(static_cast<std::size_t>(cursor + 1)), textStyle);
        }
    } else {
        canvas.drawString(x + cursor, y, " ", cursorStyle);
    }
}

void ConnectionModal::render(Canvas& canvas, int screenWidth, int screenHeight)
{
    if (!m_isOpen) {
        return;
    }

    const int modalW = std::min(64, screenWidth - 4);
    const int modalH = 17;
    const int startX = (screenWidth - modalW) / 2;
    const int startY = (screenHeight - modalH) / 2;

    const Style modalBg { Colors::White, Colors::PanelBg, false, false, false, false, false };
    const Style borderStyle { Colors::Cyan, Colors::PanelBg, true, false, false, false, false };
    const Style titleStyle { Colors::Yellow, Colors::PanelBg, true, false, false, false, false };
    const Style labelStyle { Colors::Gray, Colors::PanelBg, false, false, false, false, false };
    const Style textStyle { Colors::White, Colors::PanelBg, false, false, false, false, false };
    const Style selectedStyle { Colors::Black, Colors::Cyan, true, false, false, false, false };
    const Style cursorStyle { Colors::Black, Colors::Yellow, true, false, false, false, false };
    const Style btnStyle { Colors::Black, Colors::Green, true, false, false, false, false };
    const Style cancelBtnStyle { Colors::White, Colors::DarkGray, false, false, false, false, false };
    const Style errorStyle { Colors::Red, Colors::PanelBg, true, false, false, false, false };

    // Modal background fill
    for (int y = startY; y < startY + modalH; ++y) {
        for (int x = startX; x < startX + modalW; ++x) {
            canvas.setCell(x, y, " ", modalBg);
        }
    }

    canvas.drawPanel(startX, startY, modalW, modalH, "Connection & Transport Setup", borderStyle, titleStyle);

    const int contentX = startX + 4;
    int curY = startY + 2;

    // Field 0: Transport Mode
    std::string modeStr;
    switch (m_config.type) {
    case TransportType::Mock:
        modeStr = "< MOCK SIMULATOR >";
        break;
    case TransportType::Tcp:
        modeStr = "< TCP NETWORK >   ";
        break;
    case TransportType::Udp:
        modeStr = "< UDP DATAGRAM >  ";
        break;
    case TransportType::Serial:
        modeStr = "< SERIAL RS-485 > ";
        break;
    }
    canvas.drawString(contentX, curY, "1. Transport Mode : ", labelStyle);
    canvas.drawString(contentX + 20, curY, modeStr, (m_selectedField == 0) ? selectedStyle : textStyle);
    curY += 2;

    // Field 1: Device Address ID
    std::ostringstream addrOss;
    addrOss << "< ID: " << static_cast<int>(m_config.address) << " >";
    canvas.drawString(contentX, curY, "2. Camera Address : ", labelStyle);
    canvas.drawString(contentX + 20, curY, addrOss.str(), (m_selectedField == 1) ? selectedStyle : textStyle);
    curY += 2;

    // Field 2 & 3: Depending on Transport
    if (m_config.type == TransportType::Tcp) {
        canvas.drawString(contentX, curY, "3. TCP Host/IP   : ", labelStyle);
        renderTextField(canvas, contentX + 20, curY, m_config.tcpHost, (m_selectedField == 2), textStyle, cursorStyle);
        curY += 2;

        canvas.drawString(contentX, curY, "4. TCP Port      : ", labelStyle);
        renderTextField(canvas, contentX + 20, curY, m_tcpPortStr, (m_selectedField == 3), textStyle, cursorStyle);
        curY += 2;
    } else if (m_config.type == TransportType::Udp) {
        canvas.drawString(contentX, curY, "3. UDP Host/IP   : ", labelStyle);
        renderTextField(canvas, contentX + 20, curY, m_config.udpHost, (m_selectedField == 2), textStyle, cursorStyle);
        curY += 2;

        canvas.drawString(contentX, curY, "4. UDP Port      : ", labelStyle);
        renderTextField(canvas, contentX + 20, curY, m_udpPortStr, (m_selectedField == 3), textStyle, cursorStyle);
        curY += 2;
    } else if (m_config.type == TransportType::Serial) {
        canvas.drawString(contentX, curY, "3. Serial Device : ", labelStyle);
        renderTextField(
            canvas, contentX + 20, curY, m_config.serialPort, (m_selectedField == 2), textStyle, cursorStyle);
        curY += 2;

        std::ostringstream baudOss;
        baudOss << "< " << m_config.serialBaud << " bps >";
        canvas.drawString(contentX, curY, "4. Baud Rate     : ", labelStyle);
        canvas.drawString(contentX + 20, curY, baudOss.str(), (m_selectedField == 3) ? selectedStyle : textStyle);
        curY += 2;
    } else {
        canvas.drawString(contentX, curY, "3. Simulation    : In-memory Pelco-D state machine", labelStyle);
        curY += 2;
        canvas.drawString(contentX, curY, "4. Latency       : Zero delay offline simulation", labelStyle);
        curY += 2;
    }

    curY += 1;
    // Action Buttons
    const bool isConnectSel = (m_selectedField == 4);
    const bool isCancelSel = (m_selectedField == 5);
    canvas.drawString(contentX + 6, curY, " [ CONNECT & APPLY ] ", isConnectSel ? selectedStyle : btnStyle);
    canvas.drawString(contentX + 32, curY, " [ CANCEL ] ", isCancelSel ? selectedStyle : cancelBtnStyle);

    // Validation error banner if present
    if (!m_errorMessage.empty()) {
        canvas.drawString(contentX + 2, curY + 2, m_errorMessage, errorStyle, modalW - 6);
    }

    // Bottom navigation hint
    if (isTextEditingField()) {
        if (m_config.type == TransportType::Serial && !m_detectedPorts.empty()) {
            canvas.drawString(startX + 4, startY + modalH - 2,
                "[▲/▼] Navigate  [PgUp/PgDn] Cycle Ports  [Type] Edit  [Enter] Connect", labelStyle);
        } else {
            canvas.drawString(startX + 4, startY + modalH - 2,
                "[▲/▼] Navigate   [Type/Backspace] Edit   [Enter] Apply   [Esc] Cancel", labelStyle);
        }
    } else {
        canvas.drawString(
            startX + 4, startY + modalH - 2, "[▲/▼] Select   [◄/►] Change   [Enter] Apply   [Esc] Close", labelStyle);
    }
}

bool ConnectionModal::handleInput(const InputEvent& event)
{
    if (!m_isOpen) {
        return false;
    }

    const bool isEditing = isTextEditingField();

    // Escape always closes the modal
    if (event.key == Key::Escape) {
        m_isOpen = false;
        return true;
    }

    // Hotkey 'q' closes modal only when not actively typing into an editable field
    if (!isEditing && (event.ch == 'q' || event.ch == 'Q')) {
        m_isOpen = false;
        return true;
    }

    // Field switching: Up / Down / Tab / Backtab
    if (event.key == Key::Up || (!isEditing && event.ch == 'k') || event.key == Key::Backtab) {
        m_selectedField = (m_selectedField > 0) ? m_selectedField - 1 : 5;
        resetCursor();
        return true;
    }
    if (event.key == Key::Down || (!isEditing && event.ch == 'j') || event.key == Key::Tab) {
        m_selectedField = (m_selectedField < 5) ? m_selectedField + 1 : 0;
        resetCursor();
        return true;
    }

    // Non-editing fields handling (Mode, Address, Baud rate, Buttons)
    if (!isEditing) {
        const bool isLeft = (event.key == Key::Left || event.ch == 'h');
        const bool isRight = (event.key == Key::Right || event.ch == 'l');

        // Field 0: Transport mode
        if (m_selectedField == 0 && (isLeft || isRight)) {
            if (isRight) {
                if (m_config.type == TransportType::Mock) {
                    m_config.type = TransportType::Tcp;
                } else if (m_config.type == TransportType::Tcp) {
                    m_config.type = TransportType::Udp;
                } else if (m_config.type == TransportType::Udp) {
                    m_config.type = TransportType::Serial;
                } else {
                    m_config.type = TransportType::Mock;
                }
            } else {
                if (m_config.type == TransportType::Mock) {
                    m_config.type = TransportType::Serial;
                } else if (m_config.type == TransportType::Serial) {
                    m_config.type = TransportType::Udp;
                } else if (m_config.type == TransportType::Udp) {
                    m_config.type = TransportType::Tcp;
                } else {
                    m_config.type = TransportType::Mock;
                }
            }
            if (m_config.type == TransportType::Serial && !m_detectedPorts.empty() && m_config.serialPort.empty()) {
                m_config.serialPort = m_detectedPorts.front();
            }
            resetCursor();
            return true;
        }

        // Field 1: Address ID
        if (m_selectedField == 1) {
            if (isLeft && m_config.address > 1U) {
                m_config.address--;
                return true;
            }
            if (isRight && m_config.address < 254U) {
                m_config.address++;
                return true;
            }
        }

        // Field 3: Baud rate if Serial
        if (m_selectedField == 3 && m_config.type == TransportType::Serial && (isLeft || isRight)) {
            constexpr auto& bauds = PelcoD::SerialTransport::StandardBaudRates;
            constexpr int numBauds = static_cast<int>(bauds.size());
            int curIdx = 2;
            for (int i = 0; i < numBauds; ++i) {
                if (bauds[static_cast<std::size_t>(i)] == m_config.serialBaud) {
                    curIdx = i;
                    break;
                }
            }
            if (isLeft) {
                curIdx = (curIdx > 0) ? curIdx - 1 : (numBauds - 1);
            } else {
                curIdx = (curIdx < numBauds - 1) ? curIdx + 1 : 0;
            }
            m_config.serialBaud = bauds[static_cast<std::size_t>(curIdx)];
            return true;
        }

        // Enter confirmation
        if (event.key == Key::Enter) {
            if (m_selectedField == 4 || m_selectedField == 0 || m_selectedField == 1) {
                return validateAndApply();
            }
            if (m_selectedField == 5) {
                m_isOpen = false;
                return true;
            }
        }

        return true;
    }

    // Text editing fields handling (Field 2: Host/Port, Field 3: TCP Port)
    std::string* activeText = nullptr;
    const int maxLen = 36;
    bool onlyDigits = false;

    if (m_selectedField == 2) {
        if (m_config.type == TransportType::Tcp) {
            activeText = &m_config.tcpHost;
        } else if (m_config.type == TransportType::Udp) {
            activeText = &m_config.udpHost;
        } else if (m_config.type == TransportType::Serial) {
            activeText = &m_config.serialPort;
        }
    } else if (m_selectedField == 3) {
        if (m_config.type == TransportType::Tcp) {
            activeText = &m_tcpPortStr;
            onlyDigits = true;
        } else if (m_config.type == TransportType::Udp) {
            activeText = &m_udpPortStr;
            onlyDigits = true;
        }
    }

    if (!activeText) {
        return true;
    }

    // Cycle detected serial ports on PageUp / PageDown if editing serial port
    if (m_selectedField == 2 && m_config.type == TransportType::Serial && !m_detectedPorts.empty()) {
        if (event.key == Key::PageUp || event.key == Key::PageDown) {
            auto it = std::find(m_detectedPorts.begin(), m_detectedPorts.end(), m_config.serialPort);
            int idx = (it != m_detectedPorts.end()) ? static_cast<int>(std::distance(m_detectedPorts.begin(), it)) : -1;
            if (event.key == Key::PageDown) {
                idx = (idx + 1) % static_cast<int>(m_detectedPorts.size());
            } else {
                idx = (idx > 0) ? (idx - 1) : static_cast<int>(m_detectedPorts.size() - 1);
            }
            m_config.serialPort = m_detectedPorts[static_cast<std::size_t>(idx)];
            m_cursorPos = static_cast<int>(m_config.serialPort.size());
            return true;
        }
    }

    m_cursorPos = std::clamp(m_cursorPos, 0, static_cast<int>(activeText->size()));

    // Enter confirms and applies
    if (event.key == Key::Enter) {
        return validateAndApply();
    }

    // Left/Right cursor navigation
    if (event.key == Key::Left) {
        if (m_cursorPos > 0) {
            m_cursorPos--;
        }
        return true;
    }
    if (event.key == Key::Right) {
        if (m_cursorPos < static_cast<int>(activeText->size())) {
            m_cursorPos++;
        }
        return true;
    }

    // Home / End navigation
    if (event.key == Key::Home) {
        m_cursorPos = 0;
        return true;
    }
    if (event.key == Key::End) {
        m_cursorPos = static_cast<int>(activeText->size());
        return true;
    }

    // Backspace: delete character preceding cursor
    if (event.key == Key::Backspace) {
        if (m_cursorPos > 0 && !activeText->empty()) {
            activeText->erase(static_cast<std::size_t>(m_cursorPos - 1), 1);
            m_cursorPos--;
            m_errorMessage.clear();
        }
        return true;
    }

    // Delete: delete character at cursor
    if (event.key == Key::Delete) {
        if (m_cursorPos < static_cast<int>(activeText->size())) {
            activeText->erase(static_cast<std::size_t>(m_cursorPos), 1);
            m_errorMessage.clear();
        }
        return true;
    }

    // Printable character insertion
    const char c = event.ch;
    if (c >= 32 && c <= 126) {
        if (onlyDigits && (c < '0' || c > '9')) {
            return true;
        }
        if (static_cast<int>(activeText->size()) < maxLen) {
            activeText->insert(static_cast<std::size_t>(m_cursorPos), 1, c);
            m_cursorPos++;
            m_errorMessage.clear();
        }
        return true;
    }

    return true;
}

} // namespace PelcoDTui
