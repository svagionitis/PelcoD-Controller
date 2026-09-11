#include "DiagnosticsView.h"

#include "UtfSymbols.h"

#include <iomanip>
#include <sstream>

namespace PelcoDTui {

void DiagnosticsView::render(Canvas& canvas, int startY, int width, int height, const PelcoD::DeviceStatus& status,
    const PelcoD::DeviceInfo& info)
{
    const int panelHeight = height - 1;
    const int halfW = width / 2;

    const Style borderStyle { Colors::DarkGray, Colors::PanelBg, false, false, false, false, false };
    const Style titleStyle { Colors::Cyan, Colors::PanelBg, true, false, false, false, false };
    const Style textStyle { Colors::White, Colors::PanelBg, false, false, false, false, false };
    const Style labelStyle { Colors::Gray, Colors::PanelBg, false, false, false, false, false };
    const Style actionStyle { Colors::Yellow, Colors::PanelBg, true, false, false, false, false };
    const Style okStyle { Colors::Green, Colors::PanelBg, true, false, false, false, false };
    const Style warnStyle { Colors::Red, Colors::PanelBg, true, false, false, false, false };

    // 1. Left Panel: Telemetry & Sensor Readings
    canvas.drawPanel(1, startY, halfW - 2, panelHeight, "Device Telemetry & Sensors", borderStyle, titleStyle);

    const int leftX = 4;
    int curY = startY + 2;

    // Pan / Tilt Telemetry
    std::ostringstream ptOss;
    ptOss << "Pan / Tilt Angle : " << std::fixed << std::setprecision(2) << status.panDegrees() << "° / "
          << status.tiltDegrees() << "°";
    canvas.drawString(leftX, curY, ptOss.str(), textStyle);
    curY += 2;

    // Zoom Position
    std::ostringstream zOss;
    zOss << "Zoom Position    : " << status.zoomPosition << " / 65535";
    canvas.drawString(leftX, curY, zOss.str(), textStyle);
    curY += 2;

    // Magnification
    std::ostringstream mOss;
    mOss << "Magnification    : " << status.magnification;
    canvas.drawString(leftX, curY, mOss.str(), textStyle);
    curY += 2;

    // Temperature
    std::ostringstream tempOss;
    tempOss << "Internal Temp    : " << static_cast<int>(status.diagnosticTemp) << " °C";
    canvas.drawString(leftX, curY, tempOss.str(), textStyle);
    canvas.drawString(leftX + 26, curY, (status.diagnosticTemp > 65U) ? "[OVERHEAT]" : "[NORMAL]",
        (status.diagnosticTemp > 65U) ? warnStyle : okStyle);
    curY += 2;

    // Optical Sensor ID
    std::ostringstream sensorOss;
    sensorOss << "Optic Sensor ID  : 0x" << std::hex << std::uppercase << static_cast<int>(status.diagnosticSensorId)
              << std::dec;
    canvas.drawString(leftX, curY, sensorOss.str(), textStyle);
    curY += 2;

    // ACK / NAK status
    std::ostringstream ackOss;
    ackOss << "Last ACK Status  : " << (status.lastAckOk ? "ACK (OK)" : "PENDING/NAK") << " [Op: 0x" << std::hex
           << std::uppercase << static_cast<int>(status.lastAckOpcode) << std::dec << "]";
    canvas.drawString(leftX, curY, ackOss.str(), status.lastAckOk ? okStyle : warnStyle);

    // 2. Right Panel: Query Commands & Hardware Info
    const int rightX = halfW + 1;
    const int rightW = width - rightX - 1;
    canvas.drawPanel(rightX, startY, rightW, panelHeight, "Query Triggers & Hardware Info", borderStyle, titleStyle);

    int rY = startY + 2;
    std::string modelStr = info.modelName.empty() ? "PELCO-D CAMERA" : info.modelName;
    canvas.drawString(rightX + 3, rY, "Model Name       : " + modelStr, textStyle);
    rY += 1;

    std::ostringstream hwOss;
    hwOss << "HW / SW Type     : 0x" << std::hex << std::uppercase << static_cast<int>(info.hardwareType) << " / 0x"
          << static_cast<int>(info.softwareType) << std::dec;
    canvas.drawString(rightX + 3, rY, hwOss.str(), textStyle);
    rY += 3;

    canvas.drawPanel(rightX + 3, rY, rightW - 6, panelHeight - (rY - startY) - 3, "Interactive Query Hotkeys",
        borderStyle, labelStyle);
    canvas.drawString(rightX + 5, rY + 2, "[Q]    Query Pan & Tilt Position", textStyle);
    canvas.drawString(rightX + 5, rY + 3, "[Z]    Query Zoom Position", textStyle);
    canvas.drawString(rightX + 5, rY + 4, "[M]    Query Magnification", textStyle);
    canvas.drawString(rightX + 5, rY + 5, "[D]    Query Diagnostics (Temp/Sensor)", textStyle);
    canvas.drawString(rightX + 5, rY + 6, "[A]    Query All Telemetry", textStyle);
    canvas.drawString(rightX + 5, rY + 7, "[X]    Remote Camera Reset", warnStyle);

    // Bottom action summary
    const int bottomY = startY + panelHeight - 3;
    canvas.drawHLine(2, bottomY, width - 4, Symbols::BoxHoriz, borderStyle);
    canvas.drawString(4, bottomY + 1, "Status: ", labelStyle);
    canvas.drawString(12, bottomY + 1, m_lastAction, actionStyle);
}

bool DiagnosticsView::handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device)
{
    if (event.ch == 'q' || event.ch == 'Q') {
        device.queryPan();
        device.queryTilt();
        m_lastAction = "Dispatched Pan / Tilt Queries";
        return true;
    }
    if (event.ch == 'z' || event.ch == 'Z') {
        device.queryZoom();
        m_lastAction = "Dispatched Zoom Query";
        return true;
    }
    if (event.ch == 'm' || event.ch == 'M') {
        device.queryMagnification();
        m_lastAction = "Dispatched Magnification Query";
        return true;
    }
    if (event.ch == 'd' || event.ch == 'D') {
        device.queryDiagnostics();
        m_lastAction = "Dispatched Diagnostics Query (Temp/Sensor)";
        return true;
    }
    if (event.ch == 'a' || event.ch == 'A') {
        device.queryAll();
        m_lastAction = "Dispatched Query All";
        return true;
    }
    if (event.ch == 'x' || event.ch == 'X') {
        device.remoteReset();
        m_lastAction = "Dispatched Remote Reset";
        return true;
    }

    return false;
}

} // namespace PelcoDTui
