#include "PtzView.h"

#include "UtfSymbols.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace PelcoDTui {

void PtzView::render(Canvas& canvas, int startY, int width, int height, const PelcoD::DeviceStatus& status)
{
    const int panelHeight = height - 1;
    const int halfWidth = width / 2;

    const Style borderStyle { Colors::DarkGray, Colors::PanelBg, false, false, false, false, false };
    const Style titleStyle { Colors::Cyan, Colors::PanelBg, true, false, false, false, false };
    const Style textStyle { Colors::White, Colors::PanelBg, false, false, false, false, false };
    const Style labelStyle { Colors::Gray, Colors::PanelBg, false, false, false, false, false };
    const Style valStyle { Colors::Yellow, Colors::PanelBg, true, false, false, false, false };
    const Style activeBar { Colors::Cyan, Colors::PanelBg, false, false, false, false, false };
    const Style emptyBar { Colors::DarkGray, Colors::PanelBg, false, false, false, false, false };

    // 1. Left Panel: PTZ Compass Rose & Coordinates
    canvas.drawPanel(1, startY, halfWidth - 2, panelHeight, "PTZ Compass & Coordinates", borderStyle, titleStyle);

    // Compass box inside left panel
    const int compassX = 4;
    const int compassY = startY + 2;

    canvas.drawString(compassX + 11, compassY, std::string(Symbols::ArrowUp) + " [0° N]", titleStyle);
    canvas.drawString(compassX + 4, compassY + 2, std::string(Symbols::ArrowUpLeft), labelStyle);
    canvas.drawString(compassX + 14, compassY + 2, "│", borderStyle);
    canvas.drawString(compassX + 24, compassY + 2, std::string(Symbols::ArrowUpRight), labelStyle);

    canvas.drawString(compassX, compassY + 4,
        std::string(Symbols::ArrowLeft) + " [270° W] ────┼──── [90° E] " + std::string(Symbols::ArrowRight),
        borderStyle);

    canvas.drawString(compassX + 4, compassY + 6, std::string(Symbols::ArrowDownLeft), labelStyle);
    canvas.drawString(compassX + 14, compassY + 6, "│", borderStyle);
    canvas.drawString(compassX + 24, compassY + 6, std::string(Symbols::ArrowDownRight), labelStyle);
    canvas.drawString(compassX + 10, compassY + 8, std::string(Symbols::ArrowDown) + " [180° S]", titleStyle);

    // Pan & Tilt values
    const double panDeg = static_cast<double>(status.panCentidegrees) / 100.0;
    const double tiltDeg = static_cast<double>(status.tiltCentidegrees) / 100.0;

    std::ostringstream panOss;
    panOss << "Pan Azimuth : " << std::fixed << std::setprecision(2) << panDeg << "°  (" << status.panCentidegrees
           << ")";
    canvas.drawString(compassX, compassY + 11, panOss.str(), textStyle);

    std::ostringstream tiltOss;
    tiltOss << "Tilt Angle  : " << std::fixed << std::setprecision(2) << tiltDeg << "°  (" << status.tiltCentidegrees
            << ")";
    canvas.drawString(compassX, compassY + 12, tiltOss.str(), textStyle);

    canvas.drawString(compassX, compassY + 14, "Action      : ", labelStyle);
    canvas.drawString(compassX + 14, compassY + 14, m_lastAction, valStyle);

    // 2. Right Panel: Optics & Speed Gauges
    const int rightX = halfWidth + 1;
    const int rightW = width - rightX - 1;
    canvas.drawPanel(rightX, startY, rightW, panelHeight, "Speeds & Optics Control", borderStyle, titleStyle);

    const int optX = rightX + 3;
    int curY = startY + 2;

    // Pan Speed
    std::ostringstream pSpdOss;
    pSpdOss << "Pan Speed  [" << std::setw(2) << static_cast<int>(m_panSpeed) << " / 63]: ";
    canvas.drawString(optX, curY, pSpdOss.str(), labelStyle);
    canvas.drawProgressBar(optX + 22, curY, rightW - 28, static_cast<double>(m_panSpeed) / 63.0, activeBar, emptyBar);
    curY += 2;

    // Tilt Speed
    std::ostringstream tSpdOss;
    tSpdOss << "Tilt Speed [" << std::setw(2) << static_cast<int>(m_tiltSpeed) << " / 63]: ";
    canvas.drawString(optX, curY, tSpdOss.str(), labelStyle);
    canvas.drawProgressBar(optX + 22, curY, rightW - 28, static_cast<double>(m_tiltSpeed) / 63.0, activeBar, emptyBar);
    curY += 2;

    // Zoom Position
    std::ostringstream zoomOss;
    zoomOss << "Zoom Pos   [" << std::setw(4) << status.zoomPosition << "]: ";
    canvas.drawString(optX, curY, zoomOss.str(), labelStyle);
    canvas.drawProgressBar(
        optX + 22, curY, rightW - 28, static_cast<double>(status.zoomPosition) / 65535.0, activeBar, emptyBar);
    curY += 2;

    // Magnification
    std::ostringstream magOss;
    magOss << "Magnif.    [" << std::setw(4) << status.magnification << "]: ";
    canvas.drawString(optX, curY, magOss.str(), labelStyle);
    canvas.drawProgressBar(
        optX + 22, curY, rightW - 28, static_cast<double>(status.magnification) / 10000.0, activeBar, emptyBar);
    curY += 3;

    // Hotkeys Guide Panel inside right panel
    canvas.drawPanel(
        optX, curY, rightW - 6, panelHeight - (curY - startY) - 2, "Keyboard Controls", borderStyle, labelStyle);
    canvas.drawString(optX + 2, curY + 2, "[W/A/S/D] or [Arrows]  Pan / Tilt", textStyle);
    canvas.drawString(optX + 2, curY + 3, "[Space]                Emergency Stop", textStyle);
    canvas.drawString(optX + 2, curY + 4, "[[] / []]              Speed +/-", textStyle);
    canvas.drawString(optX + 2, curY + 5, "[Z] / [X]              Zoom Tele / Wide", textStyle);
    canvas.drawString(optX + 2, curY + 6, "[F] / [R]              Focus Near / Far", textStyle);
    canvas.drawString(optX + 2, curY + 7, "[I] / [O]              Iris Open / Close", textStyle);
}

bool PtzView::handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device)
{
    if (event.key == Key::Up || event.ch == 'w' || event.ch == 'W') {
        device.tiltUp(m_tiltSpeed);
        m_lastAction = "Tilt Up (" + std::to_string(m_tiltSpeed) + ")";
        return true;
    }
    if (event.key == Key::Down || event.ch == 's' || event.ch == 'S') {
        device.tiltDown(m_tiltSpeed);
        m_lastAction = "Tilt Down (" + std::to_string(m_tiltSpeed) + ")";
        return true;
    }
    if (event.key == Key::Left || event.ch == 'a' || event.ch == 'A') {
        device.panLeft(m_panSpeed);
        m_lastAction = "Pan Left (" + std::to_string(m_panSpeed) + ")";
        return true;
    }
    if (event.key == Key::Right || event.ch == 'd' || event.ch == 'D') {
        device.panRight(m_panSpeed);
        m_lastAction = "Pan Right (" + std::to_string(m_panSpeed) + ")";
        return true;
    }
    if (event.key == Key::Space) {
        device.stopMotion();
        m_lastAction = "Stop Motion";
        return true;
    }
    if (event.ch == '[') {
        if (m_panSpeed > 4U)
            m_panSpeed = static_cast<std::uint8_t>(m_panSpeed - 4U);
        if (m_tiltSpeed > 4U)
            m_tiltSpeed = static_cast<std::uint8_t>(m_tiltSpeed - 4U);
        m_lastAction = "Speed Decreased (" + std::to_string(m_panSpeed) + ")";
        return true;
    }
    if (event.ch == ']') {
        if (m_panSpeed <= 59U)
            m_panSpeed = static_cast<std::uint8_t>(m_panSpeed + 4U);
        if (m_tiltSpeed <= 59U)
            m_tiltSpeed = static_cast<std::uint8_t>(m_tiltSpeed + 4U);
        m_lastAction = "Speed Increased (" + std::to_string(m_panSpeed) + ")";
        return true;
    }
    if (event.ch == 'z' || event.ch == 'Z') {
        device.zoomTele();
        m_lastAction = "Zoom Tele";
        return true;
    }
    if (event.ch == 'x' || event.ch == 'X') {
        device.zoomWide();
        m_lastAction = "Zoom Wide";
        return true;
    }
    if (event.ch == 'f' || event.ch == 'F') {
        device.focusNear();
        m_lastAction = "Focus Near";
        return true;
    }
    if (event.ch == 'r' || event.ch == 'R') {
        device.focusFar();
        m_lastAction = "Focus Far";
        return true;
    }
    if (event.ch == 'i' || event.ch == 'I') {
        device.irisOpen();
        m_lastAction = "Iris Open";
        return true;
    }
    if (event.ch == 'o' || event.ch == 'O') {
        device.irisClose();
        m_lastAction = "Iris Close";
        return true;
    }

    return false;
}

} // namespace PelcoDTui
