/// @file PresetsView.cpp
/// @brief Implementation of PresetsView with automated preset tour support.

#include "PresetsView.h"
#include "UtfSymbols.h"

#include <iomanip>
#include <sstream>

namespace PelcoDTui {

PresetsView::PresetsView()
    : m_patrol(std::make_unique<PelcoD::PatrolController>())
{
    m_patrol->addStep(PelcoD::PatrolStep { 1U, 5U, "Gate 1", 0U });
    m_patrol->addStep(PelcoD::PatrolStep { 2U, 5U, "Fence West", 0U });
    m_patrol->addStep(PelcoD::PatrolStep { 3U, 5U, "Yard", 0U });
}

void PresetsView::render(Canvas& canvas, int startY, int width, int height)
{
    const int panelHeight = height - 1;
    const Style borderStyle { Colors::DarkGray, Colors::PanelBg, false, false, false, false, false };
    const Style titleStyle { Colors::Cyan, Colors::PanelBg, true, false, false, false, false };
    const Style labelStyle { Colors::Gray, Colors::PanelBg, false, false, false, false, false };
    const Style actionStyle { Colors::Yellow, Colors::PanelBg, true, false, false, false, false };
    const Style patrolActiveStyle { Colors::Green, Colors::PanelBg, true, false, false, false, false };

    canvas.drawPanel(1, startY, width - 2, panelHeight, "Presets & Touring Management (1–32)", borderStyle, titleStyle);

    // Render presets in 2 columns: 1-16 (left) and 17-32 (right)
    const int col1X = 4;
    const int col2X = width / 2 + 2;
    const int startRow = startY + 2;

    for (int i = 0; i < 32; ++i) {
        const int col = (i < 16) ? 0 : 1;
        const int row = (i < 16) ? i : (i - 16);
        const int x = (col == 0) ? col1X : col2X;
        const int y = startRow + row;

        const bool isSelected = (i == m_selectedIndex);
        Style itemStyle {};
        if (isSelected) {
            itemStyle = Style { Colors::Black, Colors::Cyan, true, false, false, false, false };
        } else {
            itemStyle = Style { Colors::White, Colors::PanelBg, false, false, false, false, false };
        }

        std::ostringstream oss;
        oss << (isSelected ? " ► " : "   ") << "Preset " << std::setw(2) << std::setfill('0') << (i + 1)
            << " [Saved: " << ((i % 3 == 0) ? "YES" : "NO ") << "] ";

        canvas.drawString(x, y, oss.str(), itemStyle);
    }

    // Bottom action summary
    const int bottomY = startY + panelHeight - 3;
    canvas.drawHLine(2, bottomY, width - 4, Symbols::BoxHoriz, borderStyle);

    canvas.drawString(
        4, bottomY + 1, "Actions: [G] GoTo  [S] Set  [C] Clear  [P] Flip  [Z] Zero  [T] Tour", labelStyle);

    if (m_patrol && m_patrol->isRunning()) {
        std::ostringstream patrolOss;
        patrolOss << " [Tour: Step " << (m_patrol->getCurrentStepIndex() + 1) << " | "
                  << m_patrol->getRemainingDwellSeconds() << "s] ";
        canvas.drawString(width - 36, bottomY + 1, patrolOss.str(), patrolActiveStyle);
    }

    canvas.drawString(width - static_cast<int>(m_lastAction.size()) - 4, bottomY + 1, m_lastAction, actionStyle);
}

bool PresetsView::handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device)
{
    if (event.key == Key::Up || event.ch == 'k') {
        m_selectedIndex = (m_selectedIndex > 0) ? m_selectedIndex - 1 : 31;
        return true;
    }
    if (event.key == Key::Down || event.ch == 'j') {
        m_selectedIndex = (m_selectedIndex < 31) ? m_selectedIndex + 1 : 0;
        return true;
    }
    if (event.key == Key::Left || event.key == Key::Right) {
        m_selectedIndex = (m_selectedIndex + 16) % 32;
        return true;
    }

    const auto presetId = static_cast<std::uint8_t>(m_selectedIndex + 1);

    if (event.key == Key::Enter || event.ch == 'g' || event.ch == 'G') {
        device.goToPreset(presetId);
        m_lastAction = "GoTo Preset " + std::to_string(presetId);
        return true;
    }
    if (event.ch == 's' || event.ch == 'S') {
        device.setPreset(presetId);
        m_lastAction = "Set Preset " + std::to_string(presetId);
        return true;
    }
    if (event.ch == 'c' || event.ch == 'C') {
        device.clearPreset(presetId);
        m_lastAction = "Clear Preset " + std::to_string(presetId);
        return true;
    }
    if (event.ch == 'p' || event.ch == 'P') {
        device.flip180();
        m_lastAction = "Flip 180°";
        return true;
    }
    if (event.ch == 'z' || event.ch == 'Z') {
        device.zeroPan();
        m_lastAction = "Zero Pan";
        return true;
    }
    if (event.ch == 't' || event.ch == 'T') {
        if (!m_patrol) {
            m_patrol = std::make_unique<PelcoD::PatrolController>(&device);
        } else {
            m_patrol->setDevice(&device);
        }
        if (m_patrol->isRunning()) {
            m_patrol->stop();
            m_lastAction = "Tour Stopped";
        } else {
            m_patrol->start();
            m_lastAction = "Tour Started";
        }
        return true;
    }

    return false;
}

} // namespace PelcoDTui
