#include "VideoView.h"

#include "UtfSymbols.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace PelcoDTui {

using namespace PelcoD::Video;

namespace {

inline std::string compassCardinal(double headingDeg) noexcept
{
    while (headingDeg < 0.0)
        headingDeg += 360.0;
    while (headingDeg >= 360.0)
        headingDeg -= 360.0;

    static const char* const kDirections[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    const int idx = static_cast<int>(std::floor((headingDeg + 22.5) / 45.0)) % 8;
    return kDirections[idx];
}

} // namespace

VideoView::VideoView()
{
    m_options.mode = videodecoder::TuiRenderMode::Braille;
    m_options.dither = videodecoder::DitherAlgorithm::Bayer4x4;
    m_options.palette = videodecoder::TuiColorPalette::TrueColor;
    m_options.contrast = 1.0;
    m_options.brightness = 0.0;
    m_options.invert = false;
}

void VideoView::updateFrame(
    const std::uint8_t* data, int width, int height, double timestamp, double decodeMs)
{
    if (!data || width <= 0 || height <= 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_frameMutex);
    const std::size_t size = static_cast<std::size_t>(width * height * 3);
    m_frameBuffer.assign(data, data + size);
    m_frameWidth = width;
    m_frameHeight = height;
    m_framePts = timestamp;
    m_decodeMs = decodeMs;
}

void VideoView::setStreamInfo(
    videodecoder::StreamState state, const std::string& source, const std::string& backend, double fps)
{
    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_streamState = state;
    m_sourceName = source;
    m_backendName = backend;
    if (fps > 0.0) {
        m_fps = fps;
    }
}

void VideoView::cycleRenderMode() noexcept
{
    if (m_options.mode == videodecoder::TuiRenderMode::Braille) {
        m_options.mode = videodecoder::TuiRenderMode::HalfBlock;
    } else {
        m_options.mode = videodecoder::TuiRenderMode::Braille;
    }
}

void VideoView::cycleDither() noexcept
{
    switch (m_options.dither) {
    case videodecoder::DitherAlgorithm::None:
        m_options.dither = videodecoder::DitherAlgorithm::Bayer4x4;
        break;
    case videodecoder::DitherAlgorithm::Bayer4x4:
        m_options.dither = videodecoder::DitherAlgorithm::Bayer8x8;
        break;
    case videodecoder::DitherAlgorithm::Bayer8x8:
        m_options.dither = videodecoder::DitherAlgorithm::FloydSteinberg;
        break;
    case videodecoder::DitherAlgorithm::FloydSteinberg:
        m_options.dither = videodecoder::DitherAlgorithm::Atkinson;
        break;
    case videodecoder::DitherAlgorithm::Atkinson:
    default:
        m_options.dither = videodecoder::DitherAlgorithm::None;
        break;
    }
}

void VideoView::cyclePalette() noexcept
{
    switch (m_options.palette) {
    case videodecoder::TuiColorPalette::TrueColor:
        m_options.palette = videodecoder::TuiColorPalette::Amber;
        break;
    case videodecoder::TuiColorPalette::Amber:
        m_options.palette = videodecoder::TuiColorPalette::NightVisionGreen;
        break;
    case videodecoder::TuiColorPalette::NightVisionGreen:
        m_options.palette = videodecoder::TuiColorPalette::CyanHud;
        break;
    case videodecoder::TuiColorPalette::CyanHud:
        m_options.palette = videodecoder::TuiColorPalette::Monochrome;
        break;
    case videodecoder::TuiColorPalette::Monochrome:
    default:
        m_options.palette = videodecoder::TuiColorPalette::TrueColor;
        break;
    }
}

bool VideoView::handleInput(const InputEvent& event, PelcoD::PelcoDDevice& device)
{
    // Mode toggles
    if (event.ch == 'm' || event.ch == 'M') {
        cycleRenderMode();
        return true;
    }
    if (event.ch == 'd' || event.ch == 'D') {
        cycleDither();
        return true;
    }
    if (event.ch == 't' || event.ch == 'T') {
        cyclePalette();
        return true;
    }
    if (event.ch == 'i' || event.ch == 'I') {
        m_options.invert = !m_options.invert;
        return true;
    }
    if (event.ch == 'p' || event.ch == 'P') {
        m_paused = !m_paused;
        return true;
    }
    if (event.ch == 'l' || event.ch == 'L') {
        m_loop = !m_loop;
        return true;
    }

    // Speed controls
    if (event.ch == '[') {
        if (m_panSpeed > 4U)
            m_panSpeed -= 4U;
        if (m_tiltSpeed > 4U)
            m_tiltSpeed -= 4U;
        return true;
    }
    if (event.ch == ']') {
        if (m_panSpeed < 60U)
            m_panSpeed += 4U;
        if (m_tiltSpeed < 60U)
            m_tiltSpeed += 4U;
        return true;
    }

    // PTZ Motion controls
    if (event.key == Key::Up || event.ch == 'w' || event.ch == 'W') {
        device.tiltUp(m_tiltSpeed);
        return true;
    }
    if (event.key == Key::Down || event.ch == 's' || event.ch == 'S') {
        device.tiltDown(m_tiltSpeed);
        return true;
    }
    if (event.key == Key::Left || event.ch == 'a' || event.ch == 'A') {
        device.panLeft(m_panSpeed);
        return true;
    }
    if (event.key == Key::Right || event.ch == 'd' || event.ch == 'D') {
        device.panRight(m_panSpeed);
        return true;
    }
    if (event.key == Key::Space) {
        device.stop();
        return true;
    }

    // Zoom controls
    if (event.ch == 'z' || event.ch == 'Z') {
        device.zoomTele();
        return true;
    }
    if (event.ch == 'x' || event.ch == 'X') {
        device.zoomWide();
        return true;
    }

    return false;
}

void VideoView::renderHudBar(Canvas& canvas, int y, int width, const PelcoD::DeviceStatus& status)
{
    const Style hudBg { Colors::White, Colors::HeaderBg, false, false, false, false, false };
    const Style titleStyle { Colors::Cyan, Colors::HeaderBg, true, false, false, false, false };
    const Style valStyle { Colors::Yellow, Colors::HeaderBg, true, false, false, false, false };
    const Style badgeStyle { Colors::Green, Colors::HeaderBg, true, false, false, false, false };
    const Style dimStyle { Colors::DarkGray, Colors::HeaderBg, false, false, false, false, false };

    // Clear bar
    for (int x = 0; x < width; ++x) {
        canvas.setCell(x, y, " ", hudBg);
    }

    // 1. PTZ Telemetry: Heading & Pitch
    const double panDeg = status.panDegrees();
    const double rawTilt = status.tiltDegrees();
    const double tiltDeg = (rawTilt > 180.0) ? rawTilt - 360.0 : rawTilt;
    const std::string cardinal = compassCardinal(panDeg);

    std::ostringstream ptzOss;
    ptzOss << std::fixed << std::setprecision(1) << " HDG: " << panDeg << "\xC2\xB0 [" << cardinal
           << "]  PITCH: " << (tiltDeg >= 0.0 ? "+" : "") << tiltDeg << "\xC2\xB0";
    canvas.drawString(2, y, ptzOss.str(), valStyle);

    // 2. Optics Telemetry: Zoom
    const double zoomFrac = static_cast<double>(status.zoomPosition) / 65535.0;
    const double zoomMult = 1.0 + zoomFrac * 29.0;
    const double focalLen = 4.3 + zoomFrac * (129.0 - 4.3);

    std::ostringstream opticsOss;
    opticsOss << std::fixed << std::setprecision(1) << "ZOOM: " << zoomMult << "x (f=" << focalLen << "mm)";
    canvas.drawString(38, y, opticsOss.str(), titleStyle);

    // 3. Diagnostics & Stream Badges (right aligned)
    std::string diagBadge;
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        std::ostringstream oss;
        if (m_streamState == videodecoder::StreamState::Streaming) {
            oss << Symbols::CircleFilled << " LIVE [" << m_backendName << "] ";
            if (m_frameWidth > 0) {
                oss << m_frameWidth << "x" << m_frameHeight << " ";
            }
            if (m_fps > 0.0) {
                oss << std::fixed << std::setprecision(1) << m_fps << " FPS ";
            }
            if (m_decodeMs > 0.0) {
                oss << std::fixed << std::setprecision(1) << m_decodeMs << "ms";
            }
        } else if (m_streamState == videodecoder::StreamState::Connecting) {
            oss << Symbols::CircleOutline << " CONNECTING...";
        } else {
            oss << Symbols::CircleOutline << " STANDBY";
        }
        diagBadge = oss.str();
    }

    const int diagX = width - static_cast<int>(diagBadge.size()) - 2;
    if (diagX > 60) {
        canvas.drawString(diagX, y, diagBadge, badgeStyle);
    }
}

void VideoView::renderControlsBar(Canvas& canvas, int y, int width)
{
    const Style bgStyle { Colors::White, Colors::HeaderBg, false, false, false, false, false };
    const Style keyStyle { Colors::Black, Colors::Cyan, true, false, false, false, false };
    const Style labelStyle { Colors::Gray, Colors::HeaderBg, false, false, false, false, false };
    const Style valStyle { Colors::Yellow, Colors::HeaderBg, true, false, false, false, false };

    for (int x = 0; x < width; ++x) {
        canvas.setCell(x, y, " ", bgStyle);
    }

    int curX = 2;
    auto drawBadge = [&](const std::string& key, const std::string& label, const std::string& val) {
        canvas.drawString(curX, y, " " + key + " ", keyStyle);
        curX += static_cast<int>(key.size()) + 2;
        canvas.drawString(curX, y, " " + label + ":", labelStyle);
        curX += static_cast<int>(label.size()) + 2;
        canvas.drawString(curX, y, val + " ", valStyle);
        curX += static_cast<int>(val.size()) + 2;
    };

    drawBadge("M", "Mode", videodecoder::BrailleRenderer::renderModeName(m_options.mode));
    drawBadge("D", "Dither", videodecoder::BrailleRenderer::ditherName(m_options.dither));
    drawBadge("T", "Palette", videodecoder::BrailleRenderer::paletteName(m_options.palette));
    drawBadge("L", "Loop", m_loop ? "ON" : "OFF");
    drawBadge("Spd", "PTZ", std::to_string(static_cast<int>(m_panSpeed)));

    const std::string ptzHint = " [WASD] Move | [ZX] Zoom | [Space] Stop ";
    const int hintX = width - static_cast<int>(ptzHint.size()) - 2;
    if (hintX > curX + 2) {
        canvas.drawString(hintX, y, ptzHint, labelStyle);
    }
}

void VideoView::render(
    Canvas& canvas, int startY, int width, int height, const PelcoD::DeviceStatus& status)
{
    if (width < 20 || height < 6) {
        return;
    }

    // 1. Render Top HUD Bar
    renderHudBar(canvas, startY, width, status);
    canvas.drawHLine(0, startY + 1, width, Symbols::BoxHoriz, Style { Colors::DarkGray, Colors::HeaderBg });

    // 2. Render Bottom Controls Bar
    const int ctrlY = startY + height - 1;
    canvas.drawHLine(0, ctrlY - 1, width, Symbols::BoxHoriz, Style { Colors::DarkGray, Colors::HeaderBg });
    renderControlsBar(canvas, ctrlY, width);

    // 3. Render Video Viewport
    const int viewStartY = startY + 2;
    const int viewHeight = height - 4; // Between top HUD and bottom controls
    if (viewHeight <= 0) {
        return;
    }

    // Fetch frame copy safely
    std::vector<std::uint8_t> frameCopy;
    int frameW = 0, frameH = 0;
    videodecoder::StreamState curState;
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        frameCopy = m_frameBuffer;
        frameW = m_frameWidth;
        frameH = m_frameHeight;
        curState = m_streamState;
    }

    if (frameCopy.empty() || frameW <= 0 || frameH <= 0 || curState == videodecoder::StreamState::Disconnected) {
        // No frame available: display standby pattern box
        const int boxW = std::min(width - 4, 60);
        const int boxH = std::min(viewHeight, 10);
        const int boxX = (width - boxW) / 2;
        const int boxY = viewStartY + (viewHeight - boxH) / 2;

        canvas.drawBox(boxX, boxY, boxW, boxH, Style { Colors::DarkGray, Colors::PanelBg }, true);
        canvas.drawString(boxX + 4, boxY + 2, "TACTICAL VIDEO VIEWPORT (STANDBY)",
            Style { Colors::Cyan, Colors::PanelBg, true });
        canvas.drawString(boxX + 4, boxY + 4, "Source: " + m_sourceName, Style { Colors::White, Colors::PanelBg });
        canvas.drawString(boxX + 4, boxY + 6, "Press 'C' to connect or pass --video <source>",
            Style { Colors::Yellow, Colors::PanelBg, false, true });
        return;
    }

    // Calculate aspect ratio scaling inside available terminal cell viewport
    const double videoAspect = static_cast<double>(frameW) / frameH;
    int targetCols = 0;
    int targetRows = 0;

    if (m_options.mode == videodecoder::TuiRenderMode::Braille) {
        // Braille cells: 2 subpixels horizontal, 4 vertical
        int targetSubW = width * 2;
        int targetSubH = static_cast<int>(targetSubW / videoAspect);
        if (targetSubH > viewHeight * 4) {
            targetSubH = viewHeight * 4;
            targetSubW = static_cast<int>(targetSubH * videoAspect);
        }
        targetCols = std::clamp(targetSubW / 2, 2, width);
        targetRows = std::clamp(targetSubH / 4, 2, viewHeight);
    } else {
        // Half-block cells: 1 subpixel horizontal, 2 vertical
        int targetSubW = width;
        int targetSubH = static_cast<int>((targetSubW * 2.0) / videoAspect);
        if (targetSubH > viewHeight * 2) {
            targetSubH = viewHeight * 2;
            targetSubW = static_cast<int>(targetSubH * videoAspect / 2.0);
        }
        targetCols = std::clamp(targetSubW, 2, width);
        targetRows = std::clamp(targetSubH / 2, 2, viewHeight);
    }

    const int offsetX = (width - targetCols) / 2;
    const int offsetY = viewStartY + (viewHeight - targetRows) / 2;

    // Rasterize RGB frame using BrailleRenderer
    videodecoder::BrailleRenderer::renderFrame(
        frameCopy.data(), frameW, frameH, targetCols, targetRows, m_options, m_renderedCells);

    // Blit cells into canvas
    for (int cy = 0; cy < targetRows; ++cy) {
        const int screenY = offsetY + cy;
        for (int cx = 0; cx < targetCols; ++cx) {
            const int screenX = offsetX + cx;
            const auto& cell = m_renderedCells[static_cast<std::size_t>(cy * targetCols + cx)];

            Style cellStyle {};
            cellStyle.fg = Color::fromRgb(cell.fgR, cell.fgG, cell.fgB);
            if (cell.hasBg) {
                cellStyle.bg = Color::fromRgb(cell.bgR, cell.bgG, cell.bgB);
            }

            canvas.setCell(screenX, screenY, cell.utf8Text, cellStyle);
        }
    }
}

} // namespace PelcoDTui
