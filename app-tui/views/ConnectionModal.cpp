#include "ConnectionModal.h"

#include "MockPelcoDDevice.h"
#include "SerialTransport.h"
#include "TcpTransport.h"
#include "UtfSymbols.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace PelcoDTui {

static const std::uint32_t kBaudRates[] = { 2400, 4800, 9600, 19200, 38400, 57600, 115200 };
static const int kNumBauds = 7;

ConnectionModal::ConnectionModal() = default;

bool ConnectionModal::hasPendingConnect() noexcept
{
    const bool res = m_pendingConnect;
    m_pendingConnect = false;
    return res;
}

std::shared_ptr<PelcoD::ITransport> ConnectionModal::createTransport() const
{
    switch (m_config.type) {
    case TransportType::Mock:
        return std::make_shared<PelcoD::MockPelcoDDevice>(m_config.address);
    case TransportType::Tcp:
        return std::make_shared<PelcoD::TcpTransport>(m_config.tcpHost, m_config.tcpPort);
    case TransportType::Serial:
        return std::make_shared<PelcoD::SerialTransport>(m_config.serialPort, m_config.serialBaud);
    default:
        return std::make_shared<PelcoD::MockPelcoDDevice>(m_config.address);
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
    const Style btnStyle { Colors::Black, Colors::Green, true, false, false, false, false };
    const Style cancelBtnStyle { Colors::White, Colors::DarkGray, false, false, false, false, false };

    // Dim background
    for (int y = 0; y < screenHeight; ++y) {
        for (int x = 0; x < screenWidth; ++x) {
            if (x < startX || x >= startX + modalW || y < startY || y >= startY + modalH) {
                // Dimming outer edges
            }
        }
    }

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
        canvas.drawString(contentX + 20, curY, m_config.tcpHost, (m_selectedField == 2) ? selectedStyle : textStyle);
        curY += 2;

        canvas.drawString(contentX, curY, "4. TCP Port      : ", labelStyle);
        canvas.drawString(
            contentX + 20, curY, std::to_string(m_config.tcpPort), (m_selectedField == 3) ? selectedStyle : textStyle);
        curY += 2;
    } else if (m_config.type == TransportType::Serial) {
        canvas.drawString(contentX, curY, "3. Serial Device : ", labelStyle);
        canvas.drawString(contentX + 20, curY, m_config.serialPort, (m_selectedField == 2) ? selectedStyle : textStyle);
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

    // Bottom note
    canvas.drawString(
        startX + 4, startY + modalH - 2, "[▲/▼] Select   [◄/►] Change   [Enter] Submit   [Esc] Close", labelStyle);
}

bool ConnectionModal::handleInput(const InputEvent& event)
{
    if (!m_isOpen) {
        return false;
    }

    if (event.key == Key::Escape || event.ch == 'q' || event.ch == 'Q') {
        m_isOpen = false;
        return true;
    }

    if (event.key == Key::Up || event.ch == 'k') {
        m_selectedField = (m_selectedField > 0) ? m_selectedField - 1 : 5;
        return true;
    }
    if (event.key == Key::Down || event.ch == 'j' || event.key == Key::Tab) {
        m_selectedField = (m_selectedField < 5) ? m_selectedField + 1 : 0;
        return true;
    }

    const bool isLeft = (event.key == Key::Left || event.ch == 'h');
    const bool isRight = (event.key == Key::Right || event.ch == 'l');

    // Field 0: Transport mode
    if (m_selectedField == 0 && (isLeft || isRight)) {
        if (m_config.type == TransportType::Mock) {
            m_config.type = isLeft ? TransportType::Serial : TransportType::Tcp;
        } else if (m_config.type == TransportType::Tcp) {
            m_config.type = isLeft ? TransportType::Mock : TransportType::Serial;
        } else {
            m_config.type = isLeft ? TransportType::Tcp : TransportType::Mock;
        }
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
        int curIdx = 2;
        for (int i = 0; i < kNumBauds; ++i) {
            if (kBaudRates[i] == m_config.serialBaud) {
                curIdx = i;
                break;
            }
        }
        if (isLeft) {
            curIdx = (curIdx > 0) ? curIdx - 1 : (kNumBauds - 1);
        } else {
            curIdx = (curIdx < kNumBauds - 1) ? curIdx + 1 : 0;
        }
        m_config.serialBaud = kBaudRates[curIdx];
        return true;
    }

    // Enter confirmation
    if (event.key == Key::Enter) {
        if (m_selectedField == 4 || m_selectedField == 0 || m_selectedField == 1) {
            m_pendingConnect = true;
            m_isOpen = false;
            return true;
        }
        if (m_selectedField == 5) {
            m_isOpen = false;
            return true;
        }
    }

    return true;
}

} // namespace PelcoDTui
