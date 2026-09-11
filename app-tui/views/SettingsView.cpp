#include "SettingsView.h"

#include "UtfSymbols.h"

#include <iomanip>
#include <sstream>

namespace PelcoDTui {

static const std::uint32_t kBaudRates[] = { 2400, 4800, 9600, 19200, 38400, 57600, 115200 };
static const int kNumBauds = 7;
static const int kNumSettings = 9;

void SettingsView::render(Canvas& canvas, int startY, int width, int height)
{
    const int panelHeight = height - 1;
    const Style borderStyle { Colors::DarkGray, Colors::PanelBg, false, false, false, false, false };
    const Style titleStyle { Colors::Cyan, Colors::PanelBg, true, false, false, false, false };
    const Style labelStyle { Colors::Gray, Colors::PanelBg, false, false, false, false, false };
    const Style textStyle { Colors::White, Colors::PanelBg, false, false, false, false, false };
    const Style actionStyle { Colors::Yellow, Colors::PanelBg, true, false, false, false, false };
    const Style activeBar { Colors::Cyan, Colors::PanelBg, false, false, false, false, false };
    const Style emptyBar { Colors::DarkGray, Colors::PanelBg, false, false, false, false, false };

    canvas.drawPanel(
        1, startY, width - 2, panelHeight, "Device Optics & Transmission Settings", borderStyle, titleStyle);

    const int leftX = 4;
    int curY = startY + 2;

    struct SettingItem {
        const char* name;
        std::string value;
        bool isSlider;
        double fraction;
    };

    std::ostringstream baudOss;
    baudOss << kBaudRates[m_baudIndex] << " bps";

    const SettingItem items[kNumSettings] = { { "Auto Focus Mode", m_autoFocus ? "[AUTO]" : "[MANUAL]", false, 0.0 },
        { "Auto Iris Mode", m_autoIris ? "[AUTO]" : "[MANUAL]", false, 0.0 },
        { "Auto Gain Control (AGC)", m_agc ? "[ON]  " : "[OFF] ", false, 0.0 },
        { "Backlight Comp (BLC)", m_blc ? "[ON]  " : "[OFF] ", false, 0.0 },
        { "Auto White Balance", m_awb ? "[ON]  " : "[OFF] ", false, 0.0 },
        { "Line Lock Delay", std::to_string(m_lineLockDelay) + " cdeg", true,
            static_cast<double>(m_lineLockDelay) / 36000.0 },
        { "White Balance R-B", std::to_string(m_wbRB), true, static_cast<double>(m_wbRB) / 255.0 },
        { "White Balance M-G", std::to_string(m_wbMG), true, static_cast<double>(m_wbMG) / 255.0 },
        { "Baud Rate (Remote)", baudOss.str(), false, 0.0 } };

    for (int i = 0; i < kNumSettings; ++i) {
        const bool isSelected = (i == m_selectedIndex);
        Style rowStyle {};
        if (isSelected) {
            rowStyle = Style { Colors::Black, Colors::Cyan, true, false, false, false, false };
        } else {
            rowStyle = textStyle;
        }

        std::ostringstream oss;
        oss << (isSelected ? " ► " : "   ") << std::left << std::setw(26) << items[i].name << " : " << std::setw(12)
            << items[i].value;

        canvas.drawString(leftX, curY, oss.str(), rowStyle);

        if (items[i].isSlider) {
            canvas.drawProgressBar(leftX + 44, curY, width - leftX - 48, items[i].fraction, activeBar, emptyBar);
        }

        curY += 2;
    }

    // Bottom hints
    const int bottomY = startY + panelHeight - 3;
    canvas.drawHLine(2, bottomY, width - 4, Symbols::BoxHoriz, borderStyle);
    canvas.drawString(4, bottomY + 1, "Controls: [▲/▼] Navigate   [◄/► or Enter] Change Value", labelStyle);
    canvas.drawString(width - static_cast<int>(m_lastAction.size()) - 6, bottomY + 1, m_lastAction, actionStyle);
}

bool SettingsView::handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device)
{
    if (event.key == Key::Up || event.ch == 'k') {
        m_selectedIndex = (m_selectedIndex > 0) ? m_selectedIndex - 1 : (kNumSettings - 1);
        return true;
    }
    if (event.key == Key::Down || event.ch == 'j') {
        m_selectedIndex = (m_selectedIndex < kNumSettings - 1) ? m_selectedIndex + 1 : 0;
        return true;
    }

    const bool toggleOrRight
        = (event.key == Key::Enter || event.key == Key::Right || event.ch == ' ' || event.ch == 'l');
    const bool isLeft = (event.key == Key::Left || event.ch == 'h');

    if (toggleOrRight || isLeft) {
        switch (m_selectedIndex) {
        case 0: { // Auto Focus
            m_autoFocus = !m_autoFocus;
            device.setAutoFocus(m_autoFocus ? PelcoD::AutoMode::Auto : PelcoD::AutoMode::Off);
            m_lastAction = m_autoFocus ? "Auto Focus: ON" : "Auto Focus: OFF";
            return true;
        }
        case 1: { // Auto Iris
            m_autoIris = !m_autoIris;
            device.setAutoIris(m_autoIris ? PelcoD::AutoMode::Auto : PelcoD::AutoMode::Off);
            m_lastAction = m_autoIris ? "Auto Iris: ON" : "Auto Iris: OFF";
            return true;
        }
        case 2: { // AGC
            m_agc = !m_agc;
            device.setAgc(m_agc ? PelcoD::AutoMode::Auto : PelcoD::AutoMode::Off);
            m_lastAction = m_agc ? "AGC: ON" : "AGC: OFF";
            return true;
        }
        case 3: { // BLC
            m_blc = !m_blc;
            device.setBacklightComp(m_blc ? PelcoD::SwitchState::On : PelcoD::SwitchState::Off);
            m_lastAction = m_blc ? "BLC: ON" : "BLC: OFF";
            return true;
        }
        case 4: { // AWB
            m_awb = !m_awb;
            device.setAutoWhiteBalance(m_awb ? PelcoD::SwitchState::On : PelcoD::SwitchState::Off);
            m_lastAction = m_awb ? "AWB: ON" : "AWB: OFF";
            return true;
        }
        case 5: { // Line lock delay
            if (isLeft && m_lineLockDelay >= 500U)
                m_lineLockDelay = static_cast<std::uint16_t>(m_lineLockDelay - 500U);
            else if (!isLeft && m_lineLockDelay <= 35500U)
                m_lineLockDelay = static_cast<std::uint16_t>(m_lineLockDelay + 500U);
            device.adjustLineLockDelay(m_lineLockDelay);
            m_lastAction = "Line Lock: " + std::to_string(m_lineLockDelay);
            return true;
        }
        case 6: { // WB R-B
            if (isLeft && m_wbRB >= 8U)
                m_wbRB = static_cast<std::uint16_t>(m_wbRB - 8U);
            else if (!isLeft && m_wbRB <= 247U)
                m_wbRB = static_cast<std::uint16_t>(m_wbRB + 8U);
            device.adjustWhiteBalanceRB(m_wbRB);
            m_lastAction = "WB R-B: " + std::to_string(m_wbRB);
            return true;
        }
        case 7: { // WB M-G
            if (isLeft && m_wbMG >= 8U)
                m_wbMG = static_cast<std::uint16_t>(m_wbMG - 8U);
            else if (!isLeft && m_wbMG <= 247U)
                m_wbMG = static_cast<std::uint16_t>(m_wbMG + 8U);
            device.adjustWhiteBalanceMG(m_wbMG);
            m_lastAction = "WB M-G: " + std::to_string(m_wbMG);
            return true;
        }
        case 8: { // Baud Rate
            if (isLeft) {
                m_baudIndex = (m_baudIndex > 0) ? m_baudIndex - 1 : (kNumBauds - 1);
            } else {
                m_baudIndex = (m_baudIndex < kNumBauds - 1) ? m_baudIndex + 1 : 0;
            }
            device.setBaudRate(kBaudRates[m_baudIndex]);
            m_lastAction = "Baud Set: " + std::to_string(kBaudRates[m_baudIndex]);
            return true;
        }
        default:
            break;
        }
    }

    return false;
}

} // namespace PelcoDTui
