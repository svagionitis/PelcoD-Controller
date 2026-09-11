#include "TrafficView.h"

#include "UtfSymbols.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace PelcoDTui {

namespace {
inline void safeLocalTime(const std::time_t* timep, std::tm* result)
{
#ifndef _WIN32
    ::localtime_r(timep, result);
#else
    ::localtime_s(result, timep);
#endif
}
} // namespace

std::string_view TrafficView::filterToString(TrafficFilter filter) noexcept
{
    switch (filter) {
    case TrafficFilter::All:
        return "ALL";
    case TrafficFilter::TxOnly:
        return "TX ONLY";
    case TrafficFilter::RxOnly:
        return "RX ONLY";
    }
    return "ALL";
}

std::string TrafficView::decodeFrame(bool isTx, const std::vector<std::uint8_t>& frame)
{
    if (frame.size() < 7U) {
        return "Fragment (" + std::to_string(frame.size()) + " bytes)";
    }

    const std::uint8_t cmd1 = frame[2];
    const std::uint8_t cmd2 = frame[3];
    const std::uint8_t d1 = frame[4];
    const std::uint8_t d2 = frame[5];

    if (!isTx) {
        // Responses
        if (cmd2 == 0x01U) {
            return (d2 == 0x01U) ? "ACK (OK) for Opcode 0x" + std::to_string(d1)
                                 : "NAK (Error) for Opcode 0x" + std::to_string(d1);
        }
        if (cmd2 == 0x59U) {
            const std::uint16_t cdeg = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
            std::ostringstream oss;
            oss << "Pan Response: " << std::fixed << std::setprecision(2) << (static_cast<double>(cdeg) / 100.0) << "°";
            return oss.str();
        }
        if (cmd2 == 0x5BU) {
            const std::uint16_t cdeg = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
            std::ostringstream oss;
            oss << "Tilt Response: " << std::fixed << std::setprecision(2) << (static_cast<double>(cdeg) / 100.0)
                << "°";
            return oss.str();
        }
        if (cmd2 == 0x5DU) {
            const std::uint16_t z = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
            return "Zoom Response: " + std::to_string(z);
        }
        if (cmd2 == 0x63U) {
            const std::uint16_t mag = static_cast<std::uint16_t>((static_cast<std::uint16_t>(d1) << 8U) | d2);
            return "Magnification: " + std::to_string(mag);
        }
        if (cmd2 == 0x71U) {
            return "Diagnostics: Temp=" + std::to_string(d1) + "°C, SensorID=0x" + std::to_string(d2);
        }
        return "Generic Response (Cmd2=0x" + std::to_string(cmd2) + ")";
    }

    // Outbound Commands (TX)
    if (cmd1 == 0x00U && cmd2 == 0x00U) {
        return "Stop Motion";
    }

    std::string actions;
    if (cmd2 & 0x02U)
        actions += "PanRight ";
    if (cmd2 & 0x04U)
        actions += "PanLeft ";
    if (cmd2 & 0x08U)
        actions += "TiltUp ";
    if (cmd2 & 0x10U)
        actions += "TiltDown ";
    if (cmd2 & 0x20U)
        actions += "ZoomTele ";
    if (cmd2 & 0x40U)
        actions += "ZoomWide ";
    if (cmd1 & 0x01U)
        actions += "FocusNear ";
    if (cmd1 & 0x02U)
        actions += "IrisOpen ";
    if (cmd1 & 0x04U)
        actions += "IrisClose ";
    if (cmd1 & 0x08U)
        actions += "FocusFar ";

    if (!actions.empty()) {
        std::ostringstream oss;
        oss << actions << "(panSpd=" << static_cast<int>(d1) << ", tiltSpd=" << static_cast<int>(d2) << ")";
        return oss.str();
    }

    // Extended Commands
    switch (cmd2) {
    case 0x03U:
        return "Set Preset " + std::to_string(d2);
    case 0x05U:
        return "Clear Preset " + std::to_string(d2);
    case 0x07U:
        return "GoTo Preset " + std::to_string(d2);
    case 0x09U:
        return "Set Aux " + std::to_string(d2) + " ON";
    case 0x0BU:
        return "Clear Aux " + std::to_string(d2) + " OFF";
    case 0x49U:
        return "Set Zero Position";
    case 0x51U:
        return "Query Pan Position";
    case 0x53U:
        return "Query Tilt Position";
    case 0x55U:
        return "Query Zoom Position";
    case 0x61U:
        return "Query Magnification";
    case 0x67U:
        return "Set Baud Rate";
    case 0x6FU:
        return "Query Diagnostics";
    case 0x0FU:
        return "Remote Reset";
    default:
        break;
    }

    std::ostringstream oss;
    oss << "Command (Cmd1=0x" << std::hex << static_cast<int>(cmd1) << " Cmd2=0x" << static_cast<int>(cmd2) << std::dec
        << ")";
    return oss.str();
}

void TrafficView::addPacket(bool isTx, const std::vector<std::uint8_t>& frame)
{
    std::ostringstream hexOss;
    for (std::size_t i = 0; i < frame.size(); ++i) {
        if (i > 0)
            hexOss << " ";
        hexOss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(frame[i]);
    }

    PacketRecord rec {};
    rec.timestamp = std::chrono::system_clock::now();
    rec.isTx = isTx;
    rec.frame = frame;
    rec.hexStr = hexOss.str();
    rec.decoded = decodeFrame(isTx, frame);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_paused) {
        m_packets.push_back(rec);
        if (m_packets.size() > kMaxPackets) {
            m_packets.pop_front();
        }
    }
}

std::size_t TrafficView::getPacketCount() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_packets.size();
}

std::size_t TrafficView::getFilteredPacketCount() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_filter == TrafficFilter::All) {
        return m_packets.size();
    }
    std::size_t count = 0;
    for (const auto& pkt : m_packets) {
        if (m_filter == TrafficFilter::TxOnly && pkt.isTx) {
            count++;
        } else if (m_filter == TrafficFilter::RxOnly && !pkt.isTx) {
            count++;
        }
    }
    return count;
}

bool TrafficView::exportToFile(const std::string& filename) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_packets.empty()) {
        return false;
    }

    std::ofstream ofs(filename);
    if (!ofs.is_open()) {
        return false;
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t nowT = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf {};
    safeLocalTime(&nowT, &tmBuf);

    char dateBuf[32];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d %H:%M:%S", &tmBuf);

    ofs << "================================================================================\n"
        << "Pelco-D Controller - Traffic Capture Log\n"
        << "Export Date : " << dateBuf << "\n"
        << "Filter Mode : " << filterToString(m_filter) << "\n"
        << "================================================================================\n"
        << std::left << std::setw(14) << "TIME" << std::setw(6) << "DIR" << std::setw(26) << "RAW HEX STREAM"
        << "PROTOCOL DECODE\n"
        << "--------------------------------------------------------------------------------\n";

    std::size_t count = 0;
    for (const auto& pkt : m_packets) {
        if (m_filter == TrafficFilter::TxOnly && !pkt.isTx) {
            continue;
        }
        if (m_filter == TrafficFilter::RxOnly && pkt.isTx) {
            continue;
        }

        const std::time_t pktT = std::chrono::system_clock::to_time_t(pkt.timestamp);
        std::tm pktTm {};
        safeLocalTime(&pktT, &pktTm);

        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(pkt.timestamp.time_since_epoch()) % 1000;

        char tBuf[16];
        std::strftime(tBuf, sizeof(tBuf), "%H:%M:%S", &pktTm);

        ofs << tBuf << "." << std::setfill('0') << std::setw(3) << ms.count() << "  " << std::setfill(' ')
            << (pkt.isTx ? "TX   " : "RX   ") << std::left << std::setw(26) << pkt.hexStr << pkt.decoded << "\n";
        count++;
    }

    ofs << "================================================================================\n"
        << "Total records exported: " << count << "\n";

    return true;
}

void TrafficView::render(Canvas& canvas, int startY, int width, int height)
{
    const int panelHeight = height - 1;
    const Style borderStyle { Colors::DarkGray, Colors::PanelBg, false, false, false, false, false };
    const Style titleStyle { Colors::Cyan, Colors::PanelBg, true, false, false, false, false };
    const Style labelStyle { Colors::Gray, Colors::PanelBg, false, false, false, false, false };
    const Style textStyle { Colors::White, Colors::PanelBg, false, false, false, false, false };
    const Style txStyle { Colors::Cyan, Colors::PanelBg, true, false, false, false, false };
    const Style rxStyle { Colors::Magenta, Colors::PanelBg, true, false, false, false, false };
    const Style pauseStyle { Colors::Yellow, Colors::PanelBg, true, false, false, false, false };

    std::ostringstream titleOss;
    titleOss << "Live Hex Traffic Monitor & Protocol Inspector " << (m_paused ? "[PAUSED]" : "[LIVE]")
             << " [Filter: " << filterToString(m_filter) << "]";
    canvas.drawPanel(
        1, startY, width - 2, panelHeight, titleOss.str(), borderStyle, m_paused ? pauseStyle : titleStyle);

    // Table Header
    const int headerY = startY + 1;
    canvas.drawString(3, headerY, "TIME", labelStyle);
    canvas.drawString(16, headerY, "DIR", labelStyle);
    canvas.drawString(24, headerY, "RAW HEX STREAM", labelStyle);
    canvas.drawString(50, headerY, "PROTOCOL DECODING", labelStyle);
    canvas.drawHLine(2, headerY + 1, width - 4, Symbols::BoxHoriz, borderStyle);

    std::lock_guard<std::mutex> lock(m_mutex);
    const int visibleRows = panelHeight - 4;

    std::vector<const PacketRecord*> visiblePackets;
    visiblePackets.reserve(m_packets.size());
    for (const auto& pkt : m_packets) {
        if (m_filter == TrafficFilter::TxOnly && !pkt.isTx) {
            continue;
        }
        if (m_filter == TrafficFilter::RxOnly && pkt.isTx) {
            continue;
        }
        visiblePackets.push_back(&pkt);
    }

    const int total = static_cast<int>(visiblePackets.size());
    const int startIdx = std::max(0, total - visibleRows - m_scrollOffset);

    int curY = headerY + 2;
    for (int i = startIdx; i < total && curY < (startY + panelHeight - 1); ++i) {
        const auto* pkt = visiblePackets[static_cast<std::size_t>(i)];

        // Timestamp
        const std::time_t t = std::chrono::system_clock::to_time_t(pkt->timestamp);
        std::tm tmBuf {};
        safeLocalTime(&t, &tmBuf);

        char timeBuf[16];
        std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tmBuf);
        canvas.drawString(3, curY, timeBuf, labelStyle);

        // Direction
        if (pkt->isTx) {
            canvas.drawString(16, curY, std::string(Symbols::ArrowUp) + " TX", txStyle);
        } else {
            canvas.drawString(16, curY, std::string(Symbols::ArrowDown) + " RX", rxStyle);
        }

        // Raw Hex
        canvas.drawString(24, curY, pkt->hexStr, textStyle, 24);

        // Decoded
        canvas.drawString(50, curY, pkt->decoded, textStyle, width - 52);

        curY++;
    }

    // Bottom action summary
    const int bottomY = startY + panelHeight - 2;
    canvas.drawHLine(2, bottomY, width - 4, Symbols::BoxHoriz, borderStyle);

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_statusTime).count();
    if (!m_statusMessage.empty() && elapsed < 4) {
        const Style bannerStyle { Colors::Green, Colors::PanelBg, true, false, false, false, false };
        canvas.drawString(4, bottomY + 1, m_statusMessage, bannerStyle, width - 8);
    } else {
        canvas.drawString(4, bottomY + 1,
            "Controls: [P/Space] Pause  [F] Filter (" + std::string(filterToString(m_filter))
                + ")  [E] Export Log  [C] Clear  [▲/▼] Scroll",
            labelStyle);
    }
}

bool TrafficView::handleInput(const InputEvent& event, [[maybe_unused]] PelcoD::PelcoDDevice& device)
{
    if (event.ch == 'p' || event.ch == 'P' || event.key == Key::Space) {
        m_paused = !m_paused;
        return true;
    }
    if (event.ch == 'f' || event.ch == 'F') {
        switch (m_filter) {
        case TrafficFilter::All:
            m_filter = TrafficFilter::TxOnly;
            break;
        case TrafficFilter::TxOnly:
            m_filter = TrafficFilter::RxOnly;
            break;
        case TrafficFilter::RxOnly:
            m_filter = TrafficFilter::All;
            break;
        }
        m_scrollOffset = 0;
        return true;
    }
    if (event.ch == 'e' || event.ch == 'E') {
        const auto now = std::chrono::system_clock::now();
        const std::time_t nowT = std::chrono::system_clock::to_time_t(now);
        std::tm tmBuf {};
        safeLocalTime(&nowT, &tmBuf);

        char fnBuf[64];
        std::strftime(fnBuf, sizeof(fnBuf), "pelcod_traffic_%Y%m%d_%H%M%S.log", &tmBuf);
        const std::string fn(fnBuf);

        if (exportToFile(fn)) {
            m_statusMessage = "Exported traffic log to " + fn;
        } else {
            m_statusMessage = "Export failed: buffer empty or cannot open file";
        }
        m_statusTime = std::chrono::steady_clock::now();
        return true;
    }
    if (event.ch == 'c' || event.ch == 'C') {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_packets.clear();
        m_scrollOffset = 0;
        return true;
    }
    if (event.key == Key::Up || event.ch == 'k') {
        m_scrollOffset = std::min(m_scrollOffset + 1, static_cast<int>(m_packets.size()));
        return true;
    }
    if (event.key == Key::Down || event.ch == 'j') {
        m_scrollOffset = std::max(0, m_scrollOffset - 1);
        return true;
    }

    return false;
}

} // namespace PelcoDTui
