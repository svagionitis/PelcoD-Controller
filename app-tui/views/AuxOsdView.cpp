#include "AuxOsdView.h"

#include "UtfSymbols.h"

#include <iomanip>
#include <sstream>

namespace PelcoDTui {

void AuxOsdView::render(Canvas& canvas, int startY, int width, int height)
{
    const int panelHeight = height - 1;
    const int halfW = width / 2;

    const Style borderStyle { Colors::DarkGray, Colors::PanelBg, false, false, false, false, false };
    const Style titleStyle { Colors::Cyan, Colors::PanelBg, true, false, false, false, false };
    const Style textStyle { Colors::White, Colors::PanelBg, false, false, false, false, false };
    const Style labelStyle { Colors::Gray, Colors::PanelBg, false, false, false, false, false };
    const Style actionStyle { Colors::Yellow, Colors::PanelBg, true, false, false, false, false };
    const Style onStyle { Colors::Black, Colors::Green, true, false, false, false, false };
    const Style offStyle { Colors::White, Colors::DarkGray, false, false, false, false, false };

    const Style selectedStyle { Colors::Black, Colors::Cyan, true, false, false, false, false };

    // 1. Left Panel: Auxiliaries 1–8
    canvas.drawPanel(1, startY, halfW - 2, panelHeight, "Auxiliary Relays (1–8)", borderStyle, titleStyle);

    const int auxX = 4;
    int curY = startY + 2;

    for (int i = 0; i < 8; ++i) {
        const bool isSelected = (i == m_selectedAux);
        const bool isOn = m_auxStates[static_cast<std::size_t>(i)];

        Style rowStyle = isSelected ? selectedStyle : textStyle;

        std::ostringstream oss;
        oss << (isSelected ? " ► " : "   ") << "Auxiliary " << (i + 1) << " : ";
        canvas.drawString(auxX, curY, oss.str(), rowStyle);

        canvas.drawString(auxX + 22, curY, isOn ? " [ ON  ] " : " [ OFF ] ", isOn ? onStyle : offStyle);
        curY += 2;
    }

    // 2. Right Panel: Zone Scan & OSD Menu
    const int rightX = halfW + 1;
    const int rightW = width - rightX - 1;
    canvas.drawPanel(rightX, startY, rightW, panelHeight, "Zone Scan & OSD Navigation", borderStyle, titleStyle);

    int rY = startY + 2;
    canvas.drawString(rightX + 3, rY, "Zone Scan Status : ", textStyle);
    canvas.drawString(
        rightX + 22, rY, m_zoneScanActive ? " [ SCANNING ] " : " [ STOPPED  ] ", m_zoneScanActive ? onStyle : offStyle);
    rY += 2;

    canvas.drawString(rightX + 3, rY, "Zone Controls    : [Z] Toggle Scan   [[ / ]] Set Start / End", labelStyle);
    rY += 3;

    canvas.drawPanel(rightX + 3, rY, rightW - 6, panelHeight - (rY - startY) - 3, "Camera On-Screen Display (OSD)",
        borderStyle, labelStyle);
    canvas.drawString(rightX + 5, rY + 2, "[M]          Toggle OSD Menu", textStyle);
    canvas.drawString(rightX + 5, rY + 3, "[W / S]      Menu Cursor Up / Down", textStyle);
    canvas.drawString(rightX + 5, rY + 4, "[A / D]      Menu Option Left / Right", textStyle);
    canvas.drawString(rightX + 5, rY + 5, "[Enter]      Select / Enter Submenu", textStyle);

    // Bottom action summary
    const int bottomY = startY + panelHeight - 3;
    canvas.drawHLine(2, bottomY, width - 4, Symbols::BoxHoriz, borderStyle);
    canvas.drawString(
        4, bottomY + 1, "Controls: [▲/▼] Select Aux   [Enter / Space] Toggle Aux   [Z] Zone Scan", labelStyle);
    canvas.drawString(width - static_cast<int>(m_lastAction.size()) - 6, bottomY + 1, m_lastAction, actionStyle);
}

bool AuxOsdView::handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device)
{
    // Navigate Auxiliaries with Up / Down
    if (event.key == Key::Up || event.ch == 'k') {
        m_selectedAux = (m_selectedAux > 0) ? m_selectedAux - 1 : 7;
        return true;
    }
    if (event.key == Key::Down || event.ch == 'j') {
        m_selectedAux = (m_selectedAux < 7) ? m_selectedAux + 1 : 0;
        return true;
    }

    // Toggle selected Aux with Enter or Space
    if (event.key == Key::Enter || event.key == Key::Space) {
        const auto auxId = static_cast<std::uint8_t>(m_selectedAux + 1);
        const bool newState = !m_auxStates[static_cast<std::size_t>(m_selectedAux)];
        m_auxStates[static_cast<std::size_t>(m_selectedAux)] = newState;

        if (newState) {
            device.setAuxiliary(auxId);
            m_lastAction = "Aux " + std::to_string(auxId) + " Turned ON";
        } else {
            device.clearAuxiliary(auxId);
            m_lastAction = "Aux " + std::to_string(auxId) + " Turned OFF";
        }
        return true;
    }

    // Zone scan toggle
    if (event.ch == 'z' || event.ch == 'Z') {
        m_zoneScanActive = !m_zoneScanActive;
        device.setZoneScan(m_zoneScanActive);
        m_lastAction = m_zoneScanActive ? "Zone Scan Started" : "Zone Scan Stopped";
        return true;
    }
    if (event.ch == '[') {
        device.setZoneStart(1U);
        m_lastAction = "Zone 1 Start Position Recorded";
        return true;
    }
    if (event.ch == ']') {
        device.setZoneEnd(1U);
        m_lastAction = "Zone 1 End Position Recorded";
        return true;
    }

    // OSD Menu actions
    if (event.ch == 'm' || event.ch == 'M') {
        // Pelco-D Preset 95 is standard "Open OSD Menu"
        device.goToPreset(95U);
        m_lastAction = "OSD Menu Activated (Preset 95)";
        return true;
    }

    return false;
}

} // namespace PelcoDTui
