#include "FujinonView.h"

#include "UtfSymbols.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace PelcoDTui {

static const char* const kOisNames[] = { "Auto", "OIS Only", "EIS Only", "Off" };
static const char* const kDefogNames[] = { "Off", "Level 1", "Level 2", "Level 3" };
static const char* const kHeatHazeNames[] = { "Off", "Level 1", "Level 2" };
static const char* const kWdrNames[] = { "Off", "Level 1", "Level 2", "Level 3" };
static const char* const kDayNightNames[] = { "Auto", "Auto+Sched", "Scheduled", "Day", "Night" };
static const char* const kZoomSpeedNames[]
    = { "1 (4s)", "2 (6s)", "3 (8s)", "4 (10s)", "5 (15s)", "6 (20s)", "7 (30s)", "8 (60s)" };
static const char* const kFocusSpeedNames[] = { "1 (Fast)", "2", "3 (Def)", "4", "5 (Slow)" };
static const char* const kDigitalZoomModes[] = { "Off", "Digital Zoom", "Crop Mode" };

static constexpr int kTotalItems = 21;

void FujinonView::render(Canvas& canvas, int startY, int width, int height, const PelcoD::FujinonStatus& status)
{
    const int panelHeight = height - 1;
    const int halfWidth = width / 2;

    const Style& borderStyle = Styles::Border;
    const Style& titleStyle = Styles::Title;
    const Style& labelStyle = Styles::Label;
    const Style& textStyle = Styles::Text;
    const Style& valStyle = Styles::Highlight;
    const Style& actionStyle = Styles::Highlight;
    const Style& activeBar = Styles::ActiveBar;
    const Style& emptyBar = Styles::EmptyBar;

    // 1. Left Panel: Optics & Fine Quality (Items 0..11)
    canvas.drawPanel(
        1, startY, halfWidth - 2, panelHeight, "Fujinon SX800 Optics & Fine Settings", borderStyle, titleStyle);

    struct ItemDef {
        const char* name;
        std::string value;
        bool isSlider;
        double fraction;
    };

    const ItemDef leftItems[12] = {
        { "Stabilization (OIS/EIS)", kOisNames[m_oisIndex], false, 0.0 },
        { "VLC Filter (Visible Cut)", m_vlcFilter ? "[ON]" : "[OFF]", false, 0.0 },
        { "Defogging Intensity", kDefogNames[m_defogIndex], false, 0.0 },
        { "Heat Haze Reduction", kHeatHazeNames[m_heatHazeIndex], false, 0.0 },
        { "Wide Dynamic Range (WDR)", kWdrNames[m_wdrIndex], false, 0.0 },
        { "Anti-Aliasing Filter", m_antialiasing ? "[ON]" : "[OFF]", false, 0.0 },
        { "Fine Brightness (1..100)", std::to_string(m_brightnessFine), true,
            static_cast<double>(m_brightnessFine) / 100.0 },
        { "Fine Contrast (1..100)", std::to_string(m_contrastFine), true, static_cast<double>(m_contrastFine) / 100.0 },
        { "Fine Saturation (1..100)", std::to_string(m_saturationFine), true,
            static_cast<double>(m_saturationFine) / 100.0 },
        { "Fine Sharpness (1..100)", std::to_string(m_sharpnessFine), true,
            static_cast<double>(m_sharpnessFine) / 100.0 },
        { "Fine WB Shift Red", std::to_string(m_wbShiftRedFine), true, static_cast<double>(m_wbShiftRedFine) / 100.0 },
        { "Fine WB Shift Blue", std::to_string(m_wbShiftBlueFine), true,
            static_cast<double>(m_wbShiftBlueFine) / 100.0 },
    };

    const int leftX = 3;
    int curY = startY + 2;
    for (int i = 0; i < 12; ++i) {
        if (curY >= startY + panelHeight - 2) {
            break;
        }
        const bool isSelected = (i == m_selectedIndex);
        const Style rowStyle
            = isSelected ? Style { Colors::Black, Colors::Cyan, true, false, false, false, false } : textStyle;

        std::ostringstream oss;
        oss << (isSelected ? " ► " : "   ") << std::left << std::setw(24) << leftItems[i].name << " : " << std::setw(12)
            << leftItems[i].value;
        canvas.drawString(leftX, curY, oss.str(), rowStyle);

        if (leftItems[i].isSlider) {
            const int sliderX = leftX + 42;
            const int sliderW = std::max(6, halfWidth - 2 - sliderX - 2);
            canvas.drawProgressBar(sliderX, curY, sliderW, leftItems[i].fraction, activeBar, emptyBar);
        }
        curY += (panelHeight > 20) ? 2 : 1;
    }

    // 2. Right Panel: Telemetry Card & Advanced Controls (Items 12..20)
    const int rightPanelX = halfWidth;
    const int rightPanelW = width - halfWidth - 1;
    canvas.drawPanel(
        rightPanelX, startY, rightPanelW, panelHeight, "Telemetry & Advanced Control", borderStyle, titleStyle);

    // Telemetry summary header
    int rY = startY + 2;
    const int rX = rightPanelX + 3;

    // Focal length badge
    std::ostringstream focalOss;
    focalOss << "Focal Length: " << std::fixed << std::setprecision(1) << status.focalLengthMm << " mm  (20-800mm 40x)";
    canvas.drawString(rX, rY, focalOss.str(), valStyle);
    rY += 1;

    const double focalFraction = std::clamp((status.focalLengthMm - 20.0) / 780.0, 0.0, 1.0);
    const int barWidth = std::max(10, rightPanelW - 6);
    canvas.drawProgressBar(rX, rY, barWidth, focalFraction, activeBar, emptyBar);
    rY += 2;

    // Zoom and Focus raw counts
    std::ostringstream countsOss;
    countsOss << "Zoom: " << status.absoluteZoomPosition << " | Focus: " << status.absoluteFocusPosition;
    if (!status.serialNumber.empty()) {
        countsOss << " | SN: " << status.serialNumber;
    }
    canvas.drawString(rX, rY, countsOss.str(), labelStyle);
    rY += 1;

    std::ostringstream fwOss;
    fwOss << "FW: " << (status.firmwareVersion.empty() ? "--" : status.firmwareVersion)
          << " | Lens: " << (status.lensStatus == 0 ? "Normal" : "Fault");
    canvas.drawString(rX, rY, fwOss.str(), labelStyle);
    rY += 2;

    canvas.drawHLine(rightPanelX + 1, rY, rightPanelW - 2, Symbols::BoxHoriz, borderStyle);
    rY += 1;

    // Advanced controls items (12..20)
    const ItemDef rightItems[9] = {
        { "Day/Night Mode Ex", kDayNightNames[m_dayNightExIndex], false, 0.0 },
        { "D -> N Threshold", std::to_string(m_dayToNightThreshold), true,
            static_cast<double>(m_dayToNightThreshold) / 255.0 },
        { "N -> D Threshold", std::to_string(m_nightToDayThreshold), true,
            static_cast<double>(m_nightToDayThreshold) / 255.0 },
        { "Zoom Speed Ex", kZoomSpeedNames[m_zoomSpeedEx - 1], false, 0.0 },
        { "Focus Speed Ex", kFocusSpeedNames[m_focusSpeedEx - 1], false, 0.0 },
        { "Digital Zoom Mode", kDigitalZoomModes[m_digitalZoomMode], false, 0.0 },
        { "One-Push Auto Focus", "[Trigger AF]", false, 0.0 },
        { "Sync RTC Clock", "[Sync PC Time]", false, 0.0 },
        { "Query Telemetry", "[Refresh All]", false, 0.0 },
    };

    for (int i = 0; i < 9; ++i) {
        if (rY >= startY + panelHeight - 2) {
            break;
        }
        const int itemIdx = 12 + i;
        const bool isSelected = (itemIdx == m_selectedIndex);
        const Style rowStyle
            = isSelected ? Style { Colors::Black, Colors::Cyan, true, false, false, false, false } : textStyle;

        std::ostringstream oss;
        oss << (isSelected ? " ► " : "   ") << std::left << std::setw(22) << rightItems[i].name << " : "
            << std::setw(12) << rightItems[i].value;
        canvas.drawString(rX, rY, oss.str(), rowStyle);

        if (rightItems[i].isSlider) {
            const int sliderX = rX + 40;
            const int sliderW = std::max(6, rightPanelW - 44);
            canvas.drawProgressBar(sliderX, rY, sliderW, rightItems[i].fraction, activeBar, emptyBar);
        }
        rY += (panelHeight > 20) ? 2 : 1;
    }

    // Bottom hints
    const int bottomY = startY + panelHeight - 3;
    canvas.drawHLine(2, bottomY, width - 4, Symbols::BoxHoriz, borderStyle);
    canvas.drawString(4, bottomY + 1, "Controls: [▲/▼] Navigate   [◄/► or Enter] Change Value", labelStyle);
    canvas.drawString(width - static_cast<int>(m_lastAction.size()) - 6, bottomY + 1, m_lastAction, actionStyle);
}

bool FujinonView::handleInput(const InputEvent& event, PelcoD::FujinonSX800Device& device)
{
    if (event.key == Key::Up || event.ch == 'k') {
        m_selectedIndex = (m_selectedIndex > 0) ? m_selectedIndex - 1 : (kTotalItems - 1);
        return true;
    }
    if (event.key == Key::Down || event.ch == 'j') {
        m_selectedIndex = (m_selectedIndex < kTotalItems - 1) ? m_selectedIndex + 1 : 0;
        return true;
    }

    const bool toggleOrRight
        = (event.key == Key::Enter || event.key == Key::Right || event.ch == ' ' || event.ch == 'l');
    const bool isLeft = (event.key == Key::Left || event.ch == 'h');

    if (!toggleOrRight && !isLeft) {
        return false;
    }

    switch (m_selectedIndex) {
    case 0: { // OIS/EIS
        m_oisIndex = isLeft ? ((m_oisIndex + 3) % 4) : ((m_oisIndex + 1) % 4);
        PelcoD::FujinonOISMode mode = PelcoD::FujinonOISMode::Auto;
        if (m_oisIndex == 1)
            mode = PelcoD::FujinonOISMode::OisOn;
        else if (m_oisIndex == 2)
            mode = PelcoD::FujinonOISMode::EisOn;
        else if (m_oisIndex == 3)
            mode = PelcoD::FujinonOISMode::Off;
        device.setOISMode(mode);
        m_lastAction = std::string("Set OIS: ") + kOisNames[m_oisIndex];
        break;
    }
    case 1: { // VLC
        m_vlcFilter = !m_vlcFilter;
        device.setVLCFilter(m_vlcFilter);
        m_lastAction = m_vlcFilter ? "VLC Filter Engaged" : "VLC Filter Retracted";
        break;
    }
    case 2: { // Defog
        m_defogIndex = isLeft ? ((m_defogIndex + 3) % 4) : ((m_defogIndex + 1) % 4);
        device.setDefog(static_cast<PelcoD::FujinonDefogLevel>(m_defogIndex));
        m_lastAction = std::string("Set Defog: ") + kDefogNames[m_defogIndex];
        break;
    }
    case 3: { // Heat Haze
        m_heatHazeIndex = isLeft ? ((m_heatHazeIndex + 2) % 3) : ((m_heatHazeIndex + 1) % 3);
        device.setHeatHaze(static_cast<PelcoD::FujinonHeatHazeLevel>(m_heatHazeIndex));
        m_lastAction = std::string("Set Heat Haze: ") + kHeatHazeNames[m_heatHazeIndex];
        break;
    }
    case 4: { // WDR
        m_wdrIndex = isLeft ? ((m_wdrIndex + 3) % 4) : ((m_wdrIndex + 1) % 4);
        device.setWDR(static_cast<PelcoD::FujinonWDRLevel>(m_wdrIndex));
        m_lastAction = std::string("Set WDR: ") + kWdrNames[m_wdrIndex];
        break;
    }
    case 5: { // Anti-Aliasing
        m_antialiasing = !m_antialiasing;
        device.setAntialiasing(m_antialiasing);
        m_lastAction = m_antialiasing ? "Anti-Aliasing: ON" : "Anti-Aliasing: OFF";
        break;
    }
    case 6: { // Fine Brightness
        m_brightnessFine = std::clamp(m_brightnessFine + (isLeft ? -5 : 5), 1, 100);
        device.setBrightnessFine(static_cast<std::uint8_t>(m_brightnessFine));
        m_lastAction = "Brightness: " + std::to_string(m_brightnessFine);
        break;
    }
    case 7: { // Fine Contrast
        m_contrastFine = std::clamp(m_contrastFine + (isLeft ? -5 : 5), 1, 100);
        device.setContrastFine(static_cast<std::uint8_t>(m_contrastFine));
        m_lastAction = "Contrast: " + std::to_string(m_contrastFine);
        break;
    }
    case 8: { // Fine Saturation
        m_saturationFine = std::clamp(m_saturationFine + (isLeft ? -5 : 5), 1, 100);
        device.setSaturationFine(static_cast<std::uint8_t>(m_saturationFine));
        m_lastAction = "Saturation: " + std::to_string(m_saturationFine);
        break;
    }
    case 9: { // Fine Sharpness
        m_sharpnessFine = std::clamp(m_sharpnessFine + (isLeft ? -5 : 5), 1, 100);
        device.setSharpnessFine(static_cast<std::uint8_t>(m_sharpnessFine));
        m_lastAction = "Sharpness: " + std::to_string(m_sharpnessFine);
        break;
    }
    case 10: { // Fine WB Red
        m_wbShiftRedFine = std::clamp(m_wbShiftRedFine + (isLeft ? -5 : 5), 1, 100);
        device.setWBShiftRedFine(static_cast<std::uint8_t>(m_wbShiftRedFine));
        m_lastAction = "WB Red: " + std::to_string(m_wbShiftRedFine);
        break;
    }
    case 11: { // Fine WB Blue
        m_wbShiftBlueFine = std::clamp(m_wbShiftBlueFine + (isLeft ? -5 : 5), 1, 100);
        device.setWBShiftBlueFine(static_cast<std::uint8_t>(m_wbShiftBlueFine));
        m_lastAction = "WB Blue: " + std::to_string(m_wbShiftBlueFine);
        break;
    }
    case 12: { // Day/Night Mode Ex
        m_dayNightExIndex = isLeft ? ((m_dayNightExIndex + 4) % 5) : ((m_dayNightExIndex + 1) % 5);
        device.setDayNightModeEx(static_cast<PelcoD::FujinonDayNightModeEx>(m_dayNightExIndex));
        m_lastAction = std::string("Day/Night Ex: ") + kDayNightNames[m_dayNightExIndex];
        break;
    }
    case 13: { // D->N Threshold
        m_dayToNightThreshold = std::clamp(m_dayToNightThreshold + (isLeft ? -10 : 10), 0, 255);
        device.setDayToNightThreshold(static_cast<std::uint8_t>(m_dayToNightThreshold));
        m_lastAction = "D->N Thresh: " + std::to_string(m_dayToNightThreshold);
        break;
    }
    case 14: { // N->D Threshold
        m_nightToDayThreshold = std::clamp(m_nightToDayThreshold + (isLeft ? -10 : 10), 0, 255);
        device.setNightToDayThreshold(static_cast<std::uint8_t>(m_nightToDayThreshold));
        m_lastAction = "N->D Thresh: " + std::to_string(m_nightToDayThreshold);
        break;
    }
    case 15: { // Zoom Speed Ex
        m_zoomSpeedEx = std::clamp(m_zoomSpeedEx + (isLeft ? -1 : 1), 1, 8);
        device.setZoomSpeedEx(static_cast<std::uint8_t>(m_zoomSpeedEx));
        m_lastAction = "Zoom Speed: " + std::string(kZoomSpeedNames[m_zoomSpeedEx - 1]);
        break;
    }
    case 16: { // Focus Speed Ex
        m_focusSpeedEx = std::clamp(m_focusSpeedEx + (isLeft ? -1 : 1), 1, 5);
        device.setFocusSpeedEx(static_cast<std::uint8_t>(m_focusSpeedEx));
        m_lastAction = "Focus Speed: " + std::string(kFocusSpeedNames[m_focusSpeedEx - 1]);
        break;
    }
    case 17: { // Digital Zoom Mode
        m_digitalZoomMode = isLeft ? ((m_digitalZoomMode + 2) % 3) : ((m_digitalZoomMode + 1) % 3);
        device.setDigitalZoomMode(static_cast<PelcoD::FujinonDigitalZoomMode>(m_digitalZoomMode));
        m_lastAction = std::string("Digital Zoom: ") + kDigitalZoomModes[m_digitalZoomMode];
        break;
    }
    case 18: { // One-Push AF
        device.onePushAF();
        m_lastAction = "Triggered One-Push Auto Focus";
        break;
    }
    case 19: { // Sync RTC
        const auto now = std::chrono::system_clock::now();
        const std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm localTm {};
#if defined(_WIN32)
        localtime_s(&localTm, &tt);
#else
        localtime_r(&tt, &localTm);
#endif
        device.setRTCYear(static_cast<std::uint16_t>(localTm.tm_year + 1900));
        device.setRTCMonthDay(
            static_cast<std::uint8_t>(localTm.tm_mon + 1), static_cast<std::uint8_t>(localTm.tm_mday));
        device.setRTCHourMinute(static_cast<std::uint8_t>(localTm.tm_hour), static_cast<std::uint8_t>(localTm.tm_min));
        device.setRTCSecond(static_cast<std::uint8_t>(localTm.tm_sec));
        m_lastAction = "Synchronized RTC with PC";
        break;
    }
    case 20: { // Refresh
        device.queryPhotoSettings();
        device.queryImageQuality();
        device.queryManualSettings();
        device.queryFirmwareVersion();
        device.querySerialNumber();
        device.queryLensStatus();
        device.queryZoom();
        m_lastAction = "Sent Telemetry Queries";
        break;
    }
    default:
        break;
    }

    return true;
}

} // namespace PelcoDTui
