#include "DiagnosticsView.h"

#include "UtfSymbols.h"

#include <iomanip>
#include <sstream>

namespace PelcoDTui {

static std::string generateSparkline(const std::vector<PelcoD::RttSample>& history)
{
    if (history.empty()) {
        return "No Data";
    }
    const char* blocks[] = { " ", " ", "▂", "▃", "▄", "▅", "▆", "▇" };
    double maxVal = 10.0;
    for (const auto& s : history) {
        if (s.success && s.rttMs > maxVal) {
            maxVal = s.rttMs;
        }
    }
    std::string out;
    for (const auto& s : history) {
        if (!s.success) {
            out += "x";
        } else {
            int idx = static_cast<int>((s.rttMs / maxVal) * 7.0);
            idx = std::max(0, std::min(7, idx));
            out += blocks[idx];
        }
    }
    return out;
}

DiagnosticsView::DiagnosticsView()
{
    PelcoD::RttProfilerConfig cfg;
    cfg.historyCapacity = 20U;
    cfg.mode = PelcoD::ProfilerMode::Passive;
    m_profiler.start(cfg);
}

void DiagnosticsView::ensureProfilerConnected(PelcoD::PelcoDDevice& device)
{
    if (!m_profilerConnected) {
        m_latencyConn
            = device.addQueryLatencyCallback([this](const std::string& tag, std::chrono::microseconds duration,
                                                 bool success) { m_profiler.recordSample(duration, tag, success); });
        m_profilerConnected = true;
    }
}

void DiagnosticsView::render(Canvas& canvas, int startY, int width, int height, const PelcoD::DeviceStatus& status,
    const PelcoD::DeviceInfo& info)
{
    const int panelHeight = height - 1;
    const int halfW = width / 2;

    const Style& borderStyle = Styles::Border;
    const Style& titleStyle = Styles::Title;
    const Style& textStyle = Styles::Text;
    const Style& labelStyle = Styles::Label;
    const Style& actionStyle = Styles::Highlight;
    const Style& okStyle = Styles::Ok;
    const Style& warnStyle = Styles::Warn;

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
    curY += 2;

    // RTT & Jitter Telemetry
    const auto rttStats = m_profiler.getStatistics();
    std::ostringstream rttOss;
    rttOss << "RTT Latency      : " << std::fixed << std::setprecision(1) << rttStats.currentRttMs
           << " ms (Avg: " << rttStats.avgRttMs << " ms)";
    canvas.drawString(leftX, curY, rttOss.str(), textStyle);
    curY += 1;

    std::ostringstream jitOss;
    jitOss << "Jitter (RFC/Std) : " << std::fixed << std::setprecision(2) << rttStats.jitterRfc3550Ms << " / "
           << rttStats.stdDevMs << " ms";
    canvas.drawString(leftX, curY, jitOss.str(), textStyle);
    curY += 1;

    std::ostringstream lossOss;
    lossOss << "Packet Loss Rate : " << std::fixed << std::setprecision(1) << rttStats.lossPercent << "% ("
            << rttStats.successfulProbes << " OK / " << rttStats.timedOutProbes << " Drop)";
    canvas.drawString(leftX, curY, lossOss.str(), (rttStats.lossPercent > 0.0) ? warnStyle : okStyle);
    curY += 1;

    const std::string spark = generateSparkline(m_profiler.getHistory());
    canvas.drawString(leftX, curY, "RTT Trend Line   : [" + spark + "]", actionStyle);

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
    canvas.drawString(rightX + 5, rY + 7, "[P]    Ping Burst (RTT & Jitter Profile)", actionStyle);
    canvas.drawString(rightX + 5, rY + 8, "[R]    Reset Profiler Statistics", textStyle);
    canvas.drawString(rightX + 5, rY + 9, "[X]    Remote Camera Reset", warnStyle);

    // Bottom action summary
    const int bottomY = startY + panelHeight - 3;
    canvas.drawHLine(2, bottomY, width - 4, Symbols::BoxHoriz, borderStyle);
    canvas.drawString(4, bottomY + 1, "Status: ", labelStyle);
    canvas.drawString(12, bottomY + 1, m_lastAction, actionStyle);
}

bool DiagnosticsView::handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device)
{
    ensureProfilerConnected(device);

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
    if (event.ch == 'p' || event.ch == 'P') {
        for (int i = 0; i < 5; ++i) {
            device.queryPan();
        }
        m_lastAction = "Triggered RTT Ping Burst (5 queries)";
        return true;
    }
    if (event.ch == 'r' || event.ch == 'R') {
        m_profiler.reset();
        m_lastAction = "Reset RTT Profiler Statistics";
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
