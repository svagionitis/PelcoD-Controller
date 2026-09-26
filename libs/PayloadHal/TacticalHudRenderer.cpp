/// @file TacticalHudRenderer.cpp
/// @brief Implementation of Tactical Heads-Up Display (HUD) & Symbology Renderer.

#include "TacticalHudRenderer.h"
#include "GimbalSectorBlanking.h"
#include "ICameraPayload.h"
#include "ILaserRangeFinder.h"
#include "IPanTiltUnit.h"
#include "IPayload.h"
#include "PayloadHealthMonitor.h"
#include "PayloadStowController.h"
#include "SensorFusionManager.h"
#include "TargetKinematicsFilter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace PayloadHal {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

double normalizeHeading360(double deg) noexcept
{
    while (deg < 0.0) {
        deg += 360.0;
    }
    while (deg >= 360.0) {
        deg -= 360.0;
    }
    return deg;
}

// Minimal 5x7 ASCII bitmap font covering printable ASCII [32..126]
// Each glyph is 5 columns wide, 7 rows high, stored as 5 bytes (each byte = 1 column, LSB at top)
const std::uint8_t kFont5x7[][5] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00 }, // ' ' (32)
    { 0x00, 0x00, 0x5F, 0x00, 0x00 }, // '!'
    { 0x00, 0x07, 0x00, 0x07, 0x00 }, // '"'
    { 0x14, 0x7F, 0x14, 0x7F, 0x14 }, // '#'
    { 0x24, 0x2A, 0x7F, 0x2A, 0x12 }, // '$'
    { 0x23, 0x13, 0x08, 0x64, 0x62 }, // '%'
    { 0x36, 0x49, 0x55, 0x22, 0x50 }, // '&'
    { 0x00, 0x05, 0x03, 0x00, 0x00 }, // '''
    { 0x00, 0x1C, 0x22, 0x41, 0x00 }, // '('
    { 0x00, 0x41, 0x22, 0x1C, 0x00 }, // ')'
    { 0x14, 0x08, 0x3E, 0x08, 0x14 }, // '*'
    { 0x08, 0x08, 0x3E, 0x08, 0x08 }, // '+'
    { 0x00, 0x50, 0x30, 0x00, 0x00 }, // ','
    { 0x08, 0x08, 0x08, 0x08, 0x08 }, // '-'
    { 0x00, 0x60, 0x60, 0x00, 0x00 }, // '.'
    { 0x20, 0x10, 0x08, 0x04, 0x02 }, // '/'
    { 0x3E, 0x51, 0x49, 0x45, 0x3E }, // '0' (48)
    { 0x00, 0x42, 0x7F, 0x40, 0x00 }, // '1'
    { 0x42, 0x61, 0x51, 0x49, 0x46 }, // '2'
    { 0x21, 0x41, 0x45, 0x4B, 0x31 }, // '3'
    { 0x18, 0x14, 0x12, 0x7F, 0x10 }, // '4'
    { 0x27, 0x45, 0x45, 0x45, 0x39 }, // '5'
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, // '6'
    { 0x01, 0x71, 0x09, 0x05, 0x03 }, // '7'
    { 0x36, 0x49, 0x49, 0x49, 0x36 }, // '8'
    { 0x06, 0x49, 0x49, 0x29, 0x1E }, // '9'
    { 0x00, 0x36, 0x36, 0x00, 0x00 }, // ':' (58)
    { 0x00, 0x56, 0x36, 0x00, 0x00 }, // ';'
    { 0x08, 0x14, 0x22, 0x41, 0x00 }, // '<'
    { 0x14, 0x14, 0x14, 0x14, 0x14 }, // '='
    { 0x00, 0x41, 0x22, 0x14, 0x08 }, // '>'
    { 0x02, 0x01, 0x51, 0x09, 0x06 }, // '?'
    { 0x32, 0x49, 0x79, 0x41, 0x3E }, // '@'
    { 0x7E, 0x11, 0x11, 0x11, 0x7E }, // 'A' (65)
    { 0x7F, 0x49, 0x49, 0x49, 0x36 }, // 'B'
    { 0x3E, 0x41, 0x41, 0x41, 0x22 }, // 'C'
    { 0x7F, 0x41, 0x41, 0x22, 0x1C }, // 'D'
    { 0x7F, 0x49, 0x49, 0x49, 0x41 }, // 'E'
    { 0x7F, 0x09, 0x09, 0x09, 0x01 }, // 'F'
    { 0x3E, 0x41, 0x49, 0x49, 0x7A }, // 'G'
    { 0x7F, 0x08, 0x08, 0x08, 0x7F }, // 'H'
    { 0x00, 0x41, 0x7F, 0x41, 0x00 }, // 'I'
    { 0x20, 0x40, 0x41, 0x3F, 0x01 }, // 'J'
    { 0x7F, 0x08, 0x14, 0x22, 0x41 }, // 'K'
    { 0x7F, 0x40, 0x40, 0x40, 0x40 }, // 'L'
    { 0x7F, 0x02, 0x0C, 0x02, 0x7F }, // 'M'
    { 0x7F, 0x04, 0x08, 0x10, 0x7F }, // 'N'
    { 0x3E, 0x41, 0x41, 0x41, 0x3E }, // 'O'
    { 0x7F, 0x09, 0x09, 0x09, 0x06 }, // 'P'
    { 0x3E, 0x41, 0x51, 0x21, 0x5E }, // 'Q'
    { 0x7F, 0x09, 0x19, 0x29, 0x46 }, // 'R'
    { 0x46, 0x49, 0x49, 0x49, 0x31 }, // 'S'
    { 0x01, 0x01, 0x7F, 0x01, 0x01 }, // 'T'
    { 0x3F, 0x40, 0x40, 0x40, 0x3F }, // 'U'
    { 0x1F, 0x20, 0x40, 0x20, 0x1F }, // 'V'
    { 0x7F, 0x20, 0x18, 0x20, 0x7F }, // 'W'
    { 0x63, 0x14, 0x08, 0x14, 0x63 }, // 'X'
    { 0x07, 0x08, 0x70, 0x08, 0x07 }, // 'Y'
    { 0x61, 0x51, 0x49, 0x45, 0x43 }, // 'Z' (90)
    { 0x00, 0x7F, 0x41, 0x41, 0x00 }, // '['
    { 0x02, 0x04, 0x08, 0x10, 0x20 }, // '\'
    { 0x00, 0x41, 0x41, 0x7F, 0x00 }, // ']'
    { 0x04, 0x02, 0x01, 0x02, 0x04 }, // '^'
    { 0x40, 0x40, 0x40, 0x40, 0x40 }, // '_'
    { 0x00, 0x01, 0x02, 0x04, 0x00 }, // '`'
    { 0x20, 0x54, 0x54, 0x54, 0x78 }, // 'a' (97)
    { 0x7F, 0x48, 0x44, 0x44, 0x38 }, // 'b'
    { 0x38, 0x44, 0x44, 0x44, 0x20 }, // 'c'
    { 0x38, 0x44, 0x44, 0x48, 0x7F }, // 'd'
    { 0x38, 0x54, 0x54, 0x54, 0x18 }, // 'e'
    { 0x08, 0x7E, 0x09, 0x01, 0x02 }, // 'f'
    { 0x0C, 0x52, 0x52, 0x52, 0x3E }, // 'g'
    { 0x7F, 0x08, 0x04, 0x04, 0x78 }, // 'h'
    { 0x00, 0x44, 0x7D, 0x40, 0x00 }, // 'i'
    { 0x20, 0x40, 0x44, 0x3D, 0x00 }, // 'j'
    { 0x7F, 0x10, 0x28, 0x44, 0x00 }, // 'k'
    { 0x00, 0x41, 0x7F, 0x40, 0x00 }, // 'l'
    { 0x7C, 0x04, 0x18, 0x04, 0x78 }, // 'm'
    { 0x7C, 0x08, 0x04, 0x04, 0x78 }, // 'n'
    { 0x38, 0x44, 0x44, 0x44, 0x38 }, // 'o'
    { 0x7C, 0x14, 0x14, 0x14, 0x08 }, // 'p'
    { 0x08, 0x14, 0x14, 0x18, 0x7C }, // 'q'
    { 0x7C, 0x08, 0x04, 0x04, 0x08 }, // 'r'
    { 0x48, 0x54, 0x54, 0x54, 0x20 }, // 's'
    { 0x04, 0x3F, 0x44, 0x40, 0x20 }, // 't'
    { 0x3C, 0x40, 0x40, 0x20, 0x7C }, // 'u'
    { 0x1C, 0x20, 0x40, 0x20, 0x1C }, // 'v'
    { 0x3C, 0x40, 0x30, 0x40, 0x3C }, // 'w'
    { 0x44, 0x28, 0x10, 0x28, 0x44 }, // 'x'
    { 0x0C, 0x50, 0x50, 0x50, 0x3C }, // 'y'
    { 0x44, 0x64, 0x54, 0x4C, 0x44 }, // 'z'
    { 0x00, 0x08, 0x36, 0x41, 0x00 }, // '{'
    { 0x00, 0x00, 0x7F, 0x00, 0x00 }, // '|'
    { 0x00, 0x41, 0x36, 0x08, 0x00 }, // '}'
    { 0x08, 0x08, 0x2A, 0x1C, 0x08 }  // '~'
};

void drawPixelRgba(std::uint8_t* buf, int w, int h, int stride, int x, int y, const HudColor& c)
{
    if (x < 0 || x >= w || y < 0 || y >= h) {
        return;
    }
    const int idx = y * stride + x * 4;
    const float alpha = std::clamp(c.a, 0.0f, 1.0f);
    const float invAlpha = 1.0f - alpha;

    buf[idx + 0] = static_cast<std::uint8_t>(c.r * 255.0f * alpha + buf[idx + 0] * invAlpha);
    buf[idx + 1] = static_cast<std::uint8_t>(c.g * 255.0f * alpha + buf[idx + 1] * invAlpha);
    buf[idx + 2] = static_cast<std::uint8_t>(c.b * 255.0f * alpha + buf[idx + 2] * invAlpha);
    buf[idx + 3] = static_cast<std::uint8_t>(std::min(255.0f, alpha * 255.0f + buf[idx + 3] * invAlpha));
}

void drawLineBresenham(std::uint8_t* buf, int w, int h, int stride, int x0, int y0, int x1, int y1, const HudColor& c)
{
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        drawPixelRgba(buf, w, h, stride, x0, y0, c);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void drawCircleMidpoint(std::uint8_t* buf, int w, int h, int stride, int xc, int yc, int r, const HudColor& c)
{
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;

    auto plot8 = [&](int cx, int cy, int px, int py) {
        drawPixelRgba(buf, w, h, stride, cx + px, cy + py, c);
        drawPixelRgba(buf, w, h, stride, cx - px, cy + py, c);
        drawPixelRgba(buf, w, h, stride, cx + px, cy - py, c);
        drawPixelRgba(buf, w, h, stride, cx - px, cy - py, c);
        drawPixelRgba(buf, w, h, stride, cx + py, cy + px, c);
        drawPixelRgba(buf, w, h, stride, cx - py, cy + px, c);
        drawPixelRgba(buf, w, h, stride, cx + py, cy - px, c);
        drawPixelRgba(buf, w, h, stride, cx - py, cy - px, c);
    };

    plot8(xc, yc, x, y);
    while (y >= x) {
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
        plot8(xc, yc, x, y);
    }
}

void drawChar5x7(std::uint8_t* buf, int w, int h, int stride, int x, int y, char ch, const HudColor& c, int scale = 1)
{
    if (ch < 32 || ch > 126) {
        ch = '?';
    }
    const auto& glyph = kFont5x7[ch - 32];

    for (int col = 0; col < 5; ++col) {
        std::uint8_t line = glyph[col];
        for (int row = 0; row < 7; ++row) {
            if ((line >> row) & 1) {
                for (int sx = 0; sx < scale; ++sx) {
                    for (int sy = 0; sy < scale; ++sy) {
                        drawPixelRgba(buf, w, h, stride, x + col * scale + sx, y + row * scale + sy, c);
                    }
                }
            }
        }
    }
}

void drawStringRgba(std::uint8_t* buf, int w, int h, int stride, int x, int y, const std::string& str, const HudColor& c, int scale = 1)
{
    int curX = x;
    for (char ch : str) {
        drawChar5x7(buf, w, h, stride, curX, y, ch, c, scale);
        curX += (5 + 1) * scale;
    }
}

} // namespace

TacticalHudRenderer::TacticalHudRenderer()
{
    updatePaletteColorLocked();
}

TacticalHudRenderer::~TacticalHudRenderer() = default;

void TacticalHudRenderer::setConfig(const TacticalHudConfig& config) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    updatePaletteColorLocked();
}

TacticalHudConfig TacticalHudRenderer::config() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void TacticalHudRenderer::setReticleType(ReticleType type) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.reticle = type;
}

ReticleType TacticalHudRenderer::reticleType() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config.reticle;
}

void TacticalHudRenderer::setDeclutterLevel(HudDeclutterLevel level) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.declutterLevel = level;
}

HudDeclutterLevel TacticalHudRenderer::declutterLevel() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config.declutterLevel;
}

void TacticalHudRenderer::setColorPalette(HudColorPalette palette) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.colorPalette = palette;
    updatePaletteColorLocked();
}

HudColorPalette TacticalHudRenderer::colorPalette() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config.colorPalette;
}

void TacticalHudRenderer::setCustomColor(const HudColor& color) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentColor = color;
}

HudColor TacticalHudRenderer::currentColor() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentColor;
}

void TacticalHudRenderer::updatePaletteColorLocked() noexcept
{
    switch (m_config.colorPalette) {
    case HudColorPalette::TacticalGreen:
        m_currentColor = HudColor { 0.0f, 1.0f, 0.25f, 1.0f };
        break;
    case HudColorPalette::AviationWhite:
        m_currentColor = HudColor { 1.0f, 1.0f, 1.0f, 1.0f };
        break;
    case HudColorPalette::HighContrastAmber:
        m_currentColor = HudColor { 1.0f, 0.69f, 0.0f, 1.0f };
        break;
    case HudColorPalette::ThermalRed:
        m_currentColor = HudColor { 1.0f, 0.2f, 0.2f, 1.0f };
        break;
    case HudColorPalette::Cyan:
        m_currentColor = HudColor { 0.0f, 0.94f, 1.0f, 1.0f };
        break;
    }
}

void TacticalHudRenderer::updateTelemetry(const HudTelemetrySnapshot& telemetry)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_telemetry = telemetry;
}

HudTelemetrySnapshot TacticalHudRenderer::telemetry() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_telemetry;
}

void TacticalHudRenderer::updateFromPayload(const IPayload& payload)
{
    HudTelemetrySnapshot snap {};

    if (auto ptu = payload.panTilt()) {
        const auto telem = ptu->currentTelemetry();
        snap.panAngleDeg = telem.panAngleDeg;
        snap.tiltAngleDeg = telem.tiltAngleDeg;
        snap.rollAngleDeg = telem.rollAngleDeg;
    }

    if (auto cam = payload.primaryCamera()) {
        const auto telem = cam->currentTelemetry();
        snap.horizontalFovDeg = telem.horizontalFovDeg;
        snap.opticalZoomFactor = 1.0 + telem.normalizedZoom * 29.0;
        snap.opticalChannelName = (cam->spectrum() == CameraSpectrum::DaylightVisible) ? "VISIBLE EO" : "THERMAL LWIR";
    }

    if (auto lrf = payload.lrf()) {
        const auto meas = lrf->lastMeasurement();
        if (meas.has_value() && meas->valid) {
            snap.lrfValid = true;
            snap.slantRangeMeters = meas->slantRangeMeters;
        }
    }

    if (auto targetKinematics = payload.targetKinematics()) {
        const auto state = targetKinematics->trackState();
        if (state == TargetTrackState::Tracking) {
            snap.targetTrackActive = true;
            snap.trackerStatus = "TRACKING";
        } else if (state == TargetTrackState::Coasting) {
            snap.targetTrackActive = true;
            snap.trackerStatus = "COASTING";
        } else if (state == TargetTrackState::Acquiring) {
            snap.targetTrackActive = true;
            snap.trackerStatus = "ACQUIRING";
        }
    }

    if (auto sector = payload.sectorBlanking()) {
        snap.laserInterlockActive = !sector->isLaserAllowed(snap.panAngleDeg, snap.tiltAngleDeg);
    }

    if (auto fusion = payload.sensorFusion()) {
        snap.digitalCropFactor = fusion->currentDigitalCropFactor();
        switch (fusion->activeChannel()) {
        case OpticalChannel::Primary:
            snap.opticalChannelName = "VISIBLE EO";
            break;
        case OpticalChannel::Secondary:
            snap.opticalChannelName = "THERMAL IR";
            break;
        case OpticalChannel::Auxiliary:
            snap.opticalChannelName = "AUX CHANNEL";
            break;
        }
    }

    if (auto health = payload.healthMonitor()) {
        const auto rep = health->healthReport();
        switch (rep.overallState) {
        case DeviceState::Ready:
            snap.systemHealth = "SYS: READY";
            break;
        case DeviceState::Degraded:
            snap.systemHealth = "SYS: DEGRADED";
            break;
        case DeviceState::Fault:
            snap.systemHealth = "SYS: FAULT";
            break;
        default:
            snap.systemHealth = "SYS: INIT";
            break;
        }
    }

    if (auto stow = payload.stowController()) {
        const auto st = stow->status();
        if (st.isZeroized) {
            snap.warningBanner = "MISSION ZEROIZED - SYSTEM LOCKED";
        } else if (st.stowState == StowState::Stowed) {
            snap.systemHealth += " [STOWED]";
        }
    }

    updateTelemetry(snap);
}

HudDrawList TacticalHudRenderer::generateDrawList() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    HudDrawList dl {};

    if (m_config.reticle != ReticleType::None) {
        buildReticle(dl);
    }

    if (m_config.declutterLevel == HudDeclutterLevel::DeCluttered) {
        return dl;
    }

    if (m_config.showTrackingGate && m_telemetry.targetTrackActive) {
        buildTrackingGate(dl);
    }

    if (m_config.declutterLevel == HudDeclutterLevel::Minimal) {
        if (m_config.showLrfTelemetry) {
            buildLrfTelemetry(dl);
        }
        return dl;
    }

    if (m_config.showHeadingTape) {
        buildHeadingTape(dl);
    }

    if (m_config.showPitchLadder) {
        buildPitchLadder(dl);
    }

    if (m_config.showHorizonLine) {
        buildHorizonLine(dl);
    }

    if (m_config.showTargetGeoCoords && m_telemetry.hasTargetGeo) {
        buildTargetTelemetry(dl);
    }

    if (m_config.showSensorOpticsInfo) {
        buildSensorTelemetry(dl);
    }

    if (m_config.showLrfTelemetry) {
        buildLrfTelemetry(dl);
    }

    if (m_config.showSafetyWarnings) {
        buildWarningBanners(dl);
    }

    return dl;
}

void TacticalHudRenderer::buildReticle(HudDrawList& drawList) const
{
    const float cx = 0.5f;
    const float cy = 0.5f;
    const float sw = m_config.defaultStrokeWidth;
    const float scale = m_config.customReticleScale;

    switch (m_config.reticle) {
    case ReticleType::Crosshair: {
        const float armLen = 0.08f * scale;
        const float gap = 0.015f * scale;
        // Left arm
        drawList.lines.push_back({ { cx - armLen, cy }, { cx - gap, cy }, m_currentColor, sw });
        // Right arm
        drawList.lines.push_back({ { cx + gap, cy }, { cx + armLen, cy }, m_currentColor, sw });
        // Top arm
        drawList.lines.push_back({ { cx, cy - armLen }, { cx, cy - gap }, m_currentColor, sw });
        // Bottom arm
        drawList.lines.push_back({ { cx, cy + gap }, { cx, cy + armLen }, m_currentColor, sw });
        break;
    }
    case ReticleType::MilDotLadder: {
        const float armLen = 0.10f * scale;
        const float gap = 0.012f * scale;
        // Base crosshair lines
        drawList.lines.push_back({ { cx - armLen, cy }, { cx - gap, cy }, m_currentColor, sw });
        drawList.lines.push_back({ { cx + gap, cy }, { cx + armLen, cy }, m_currentColor, sw });
        drawList.lines.push_back({ { cx, cy - armLen }, { cx, cy - gap }, m_currentColor, sw });
        drawList.lines.push_back({ { cx, cy + gap }, { cx, cy + armLen }, m_currentColor, sw });

        // Stadiametric ladder ticks (every 1-mil increment)
        const float tickStep = 0.02f * scale;
        const float tickH = 0.006f * scale;
        for (int i = 1; i <= 4; ++i) {
            float dist = gap + static_cast<float>(i) * tickStep;
            if (dist < armLen) {
                // Horizontal arm ticks
                drawList.lines.push_back({ { cx - dist, cy - tickH }, { cx - dist, cy + tickH }, m_currentColor, sw });
                drawList.lines.push_back({ { cx + dist, cy - tickH }, { cx + dist, cy + tickH }, m_currentColor, sw });
                // Vertical arm ticks
                drawList.lines.push_back({ { cx - tickH, cy - dist }, { cx + tickH, cy - dist }, m_currentColor, sw });
                drawList.lines.push_back({ { cx - tickH, cy + dist }, { cx + tickH, cy + dist }, m_currentColor, sw });
            }
        }
        break;
    }
    case ReticleType::CircleDot: {
        drawList.circles.push_back({ { cx, cy }, 0.04f * scale, m_currentColor, sw });
        drawList.circles.push_back({ { cx, cy }, 0.005f * scale, m_currentColor, sw });
        break;
    }
    case ReticleType::BoxReticle: {
        const float halfBox = 0.035f * scale;
        drawList.rectangles.push_back({ { cx - halfBox, cy - halfBox }, halfBox * 2.0f, halfBox * 2.0f, m_currentColor, sw, false, {} });
        drawList.lines.push_back({ { cx - 0.01f * scale, cy }, { cx + 0.01f * scale, cy }, m_currentColor, sw });
        drawList.lines.push_back({ { cx, cy - 0.01f * scale }, { cx, cy + 0.01f * scale }, m_currentColor, sw });
        break;
    }
    case ReticleType::BoresightPlus: {
        const float len = 0.015f * scale;
        drawList.lines.push_back({ { cx - len, cy }, { cx + len, cy }, m_currentColor, sw });
        drawList.lines.push_back({ { cx, cy - len }, { cx, cy + len }, m_currentColor, sw });
        break;
    }
    case ReticleType::None:
        break;
    }
}

void TacticalHudRenderer::buildHeadingTape(HudDrawList& drawList) const
{
    const float yRibbon = 0.07f;
    const float xLeft = 0.25f;
    const float xRight = 0.75f;
    const float ribbonWidth = xRight - xLeft;
    const float sw = m_config.defaultStrokeWidth;

    // Baseline tape
    drawList.lines.push_back({ { xLeft, yRibbon }, { xRight, yRibbon }, m_currentColor, sw });

    // Center caret
    drawList.lines.push_back({ { 0.5f, yRibbon }, { 0.495f, yRibbon + 0.015f }, m_currentColor, sw });
    drawList.lines.push_back({ { 0.5f, yRibbon }, { 0.505f, yRibbon + 0.015f }, m_currentColor, sw });

    // Heading digital tag
    const double curHeading = normalizeHeading360(m_telemetry.panAngleDeg);
    char hdgStr[32];
    std::snprintf(hdgStr, sizeof(hdgStr), "[ %03.0f° ]", curHeading);
    drawList.textLabels.push_back({ hdgStr, { 0.5f, yRibbon - 0.025f }, m_currentColor, 14.0f, true, true });

    // Ticks across 60° FOV window
    constexpr double kHdgWindowDeg = 60.0;
    const double minHdg = curHeading - (kHdgWindowDeg / 2.0);
    const double maxHdg = curHeading + (kHdgWindowDeg / 2.0);

    const int startStep = static_cast<int>(std::floor(minHdg / 5.0)) * 5;
    const int endStep = static_cast<int>(std::ceil(maxHdg / 5.0)) * 5;

    for (int degStep = startStep; degStep <= endStep; degStep += 5) {
        const double relDeg = degStep - curHeading;
        if (relDeg < -kHdgWindowDeg / 2.0 || relDeg > kHdgWindowDeg / 2.0) {
            continue;
        }

        const float normX = 0.5f + static_cast<float>(relDeg / kHdgWindowDeg) * ribbonWidth;
        const int normAngle = static_cast<int>(normalizeHeading360(static_cast<double>(degStep)));
        const bool isMajor = (normAngle % 10 == 0) || (normAngle % 45 == 0);
        const float tickLen = isMajor ? 0.012f : 0.006f;

        drawList.lines.push_back({ { normX, yRibbon - tickLen }, { normX, yRibbon }, m_currentColor, sw });

        if (isMajor) {
            std::string lbl;
            if (normAngle == 0) lbl = "N";
            else if (normAngle == 45) lbl = "NE";
            else if (normAngle == 90) lbl = "E";
            else if (normAngle == 135) lbl = "SE";
            else if (normAngle == 180) lbl = "S";
            else if (normAngle == 225) lbl = "SW";
            else if (normAngle == 270) lbl = "W";
            else if (normAngle == 315) lbl = "NW";
            else {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%02d", normAngle / 10);
                lbl = buf;
            }
            drawList.textLabels.push_back({ lbl, { normX, yRibbon - tickLen - 0.012f }, m_currentColor, 11.0f, false, true });
        }
    }
}

void TacticalHudRenderer::buildPitchLadder(HudDrawList& drawList) const
{
    const float cx = 0.5f;
    const float cy = 0.5f;
    const float sw = m_config.defaultStrokeWidth;
    constexpr double kPitchWindowDeg = 40.0;
    const double curPitch = m_telemetry.tiltAngleDeg;
    const float ladderHalfWidth = 0.06f;

    for (int pDeg = -90; pDeg <= 90; pDeg += 5) {
        if (pDeg == 0) {
            continue; // Drawn by horizon line
        }

        const double relPitch = static_cast<double>(pDeg) - curPitch;
        if (std::abs(relPitch) > kPitchWindowDeg / 2.0) {
            continue;
        }

        // Screen Y: positive pitch moves ladder downward relative to reticle
        const float normY = cy - static_cast<float>(relPitch / kPitchWindowDeg) * 0.5f;
        if (normY < 0.15f || normY > 0.85f) {
            continue;
        }

        const float gap = 0.02f;
        const float endTickH = 0.006f * (pDeg > 0 ? 1.0f : -1.0f);

        // Left ladder bar
        drawList.lines.push_back({ { cx - ladderHalfWidth, normY }, { cx - gap, normY }, m_currentColor, sw });
        drawList.lines.push_back({ { cx - ladderHalfWidth, normY }, { cx - ladderHalfWidth, normY + endTickH }, m_currentColor, sw });

        // Right ladder bar
        drawList.lines.push_back({ { cx + gap, normY }, { cx + ladderHalfWidth, normY }, m_currentColor, sw });
        drawList.lines.push_back({ { cx + ladderHalfWidth, normY }, { cx + ladderHalfWidth, normY + endTickH }, m_currentColor, sw });

        // Pitch text label
        char pStr[16];
        std::snprintf(pStr, sizeof(pStr), "%+d", pDeg);
        drawList.textLabels.push_back({ pStr, { cx - ladderHalfWidth - 0.025f, normY }, m_currentColor, 11.0f, false, true });
    }
}

void TacticalHudRenderer::buildHorizonLine(HudDrawList& drawList) const
{
    const float cx = 0.5f;
    const float cy = 0.5f;
    const float sw = m_config.defaultStrokeWidth;
    constexpr double kPitchWindowDeg = 40.0;
    const double curPitch = m_telemetry.tiltAngleDeg;

    // Check if horizon is within vertical window
    if (std::abs(curPitch) > kPitchWindowDeg / 2.0) {
        return;
    }

    const float horizY = cy + static_cast<float>(curPitch / kPitchWindowDeg) * 0.5f;
    const float rollRad = static_cast<float>(m_telemetry.rollAngleDeg * kDegToRad);
    const float cosR = std::cos(rollRad);
    const float sinR = std::sin(rollRad);

    const float barLen = 0.18f;
    const float gap = 0.04f;

    // Left horizon line segment
    const float lx0 = cx - barLen * cosR;
    const float ly0 = horizY - barLen * sinR;
    const float lx1 = cx - gap * cosR;
    const float ly1 = horizY - gap * sinR;
    drawList.lines.push_back({ { lx0, ly0 }, { lx1, ly1 }, m_currentColor, sw * 1.5f });

    // Right horizon line segment
    const float rx0 = cx + gap * cosR;
    const float ry0 = horizY + gap * sinR;
    const float rx1 = cx + barLen * cosR;
    const float ry1 = horizY + barLen * sinR;
    drawList.lines.push_back({ { rx0, ry0 }, { rx1, ry1 }, m_currentColor, sw * 1.5f });
}

void TacticalHudRenderer::buildTrackingGate(HudDrawList& drawList) const
{
    const float cx = m_telemetry.targetCenterNormalized.x;
    const float cy = m_telemetry.targetCenterNormalized.y;
    const float hw = m_telemetry.targetWidthNormalized * 0.5f;
    const float hh = m_telemetry.targetHeightNormalized * 0.5f;
    const float sw = m_config.defaultStrokeWidth * 1.5f;

    // Corner brackets
    const float cornerLen = std::min(hw, hh) * 0.4f;

    // Top-Left
    drawList.lines.push_back({ { cx - hw, cy - hh }, { cx - hw + cornerLen, cy - hh }, m_currentColor, sw });
    drawList.lines.push_back({ { cx - hw, cy - hh }, { cx - hw, cy - hh + cornerLen }, m_currentColor, sw });
    // Top-Right
    drawList.lines.push_back({ { cx + hw, cy - hh }, { cx + hw - cornerLen, cy - hh }, m_currentColor, sw });
    drawList.lines.push_back({ { cx + hw, cy - hh }, { cx + hw, cy - hh + cornerLen }, m_currentColor, sw });
    // Bottom-Left
    drawList.lines.push_back({ { cx - hw, cy + hh }, { cx - hw + cornerLen, cy + hh }, m_currentColor, sw });
    drawList.lines.push_back({ { cx - hw, cy + hh }, { cx - hw, cy + hh - cornerLen }, m_currentColor, sw });
    // Bottom-Right
    drawList.lines.push_back({ { cx + hw, cy + hh }, { cx + hw - cornerLen, cy + hh }, m_currentColor, sw });
    drawList.lines.push_back({ { cx + hw, cy + hh }, { cx + hw, cy + hh - cornerLen }, m_currentColor, sw });

    // Status banner
    drawList.textLabels.push_back({ m_telemetry.trackerStatus, { cx, cy - hh - 0.015f }, m_currentColor, 12.0f, true, true });

    // Lead vector pip
    if (m_config.showVelocityLeadPip && m_telemetry.hasLeadVector) {
        drawList.lines.push_back({ { cx, cy }, m_telemetry.leadPipNormalized, m_currentColor, 1.0f });
        drawList.circles.push_back({ m_telemetry.leadPipNormalized, 0.008f, m_currentColor, sw });
    }
}

void TacticalHudRenderer::buildTargetTelemetry(HudDrawList& drawList) const
{
    const float x = 0.76f;
    float y = 0.82f;
    const float lineSpacing = 0.028f;

    char latStr[64];
    char lonStr[64];
    char elevStr[64];

    std::snprintf(latStr, sizeof(latStr), "TGT LAT: %+.5f°", m_telemetry.targetLatDeg);
    std::snprintf(lonStr, sizeof(lonStr), "TGT LON: %+.5f°", m_telemetry.targetLonDeg);
    std::snprintf(elevStr, sizeof(elevStr), "TGT ELEV: %.0f m MSL", m_telemetry.targetAltMslM);

    drawList.textLabels.push_back({ latStr, { x, y }, m_currentColor, 12.0f, false, false });
    y += lineSpacing;
    drawList.textLabels.push_back({ lonStr, { x, y }, m_currentColor, 12.0f, false, false });
    y += lineSpacing;
    drawList.textLabels.push_back({ elevStr, { x, y }, m_currentColor, 12.0f, false, false });

    if (!m_telemetry.targetMgrs.empty()) {
        y += lineSpacing;
        std::string mgrsStr = "MGRS: " + m_telemetry.targetMgrs;
        drawList.textLabels.push_back({ mgrsStr, { x, y }, m_currentColor, 12.0f, true, false });
    }
}

void TacticalHudRenderer::buildSensorTelemetry(HudDrawList& drawList) const
{
    const float x = 0.04f;
    float y = 0.08f;
    const float lineSpacing = 0.026f;

    drawList.textLabels.push_back({ "SENSOR: " + m_telemetry.opticalChannelName, { x, y }, m_currentColor, 12.0f, true, false });
    y += lineSpacing;

    char zoomStr[64];
    std::snprintf(zoomStr, sizeof(zoomStr), "ZOOM: %.1fx (DIG: %.1fx)", m_telemetry.opticalZoomFactor, m_telemetry.digitalCropFactor);
    drawList.textLabels.push_back({ zoomStr, { x, y }, m_currentColor, 12.0f, false, false });
    y += lineSpacing;

    char fovStr[64];
    std::snprintf(fovStr, sizeof(fovStr), "HFOV: %.1f°", m_telemetry.horizontalFovDeg);
    drawList.textLabels.push_back({ fovStr, { x, y }, m_currentColor, 12.0f, false, false });
}

void TacticalHudRenderer::buildLrfTelemetry(HudDrawList& drawList) const
{
    const float x = 0.04f;
    float y = 0.85f;
    const float lineSpacing = 0.028f;

    if (m_telemetry.lrfValid) {
        char rngStr[64];
        std::snprintf(rngStr, sizeof(rngStr), "LRF RANGE: %.0f m", m_telemetry.slantRangeMeters);
        drawList.textLabels.push_back({ rngStr, { x, y }, m_currentColor, 13.0f, true, false });
    } else {
        drawList.textLabels.push_back({ "LRF RANGE: ---- m", { x, y }, m_currentColor, 13.0f, false, false });
    }
    y += lineSpacing;

    if (m_telemetry.laserInterlockActive) {
        HudColor alertColor { 1.0f, 0.2f, 0.2f, 1.0f }; // Red
        drawList.textLabels.push_back({ "[LASER INHIBITED - KEEP-OUT]", { x, y }, alertColor, 12.0f, true, false });
    } else if (m_telemetry.laserFiring) {
        HudColor fireColor { 1.0f, 0.8f, 0.0f, 1.0f }; // Yellow
        drawList.textLabels.push_back({ ">>> LASER FIRING <<<", { x, y }, fireColor, 12.0f, true, false });
    } else if (m_telemetry.laserArmed) {
        drawList.textLabels.push_back({ "LASER: ARMED", { x, y }, m_currentColor, 12.0f, false, false });
    } else {
        drawList.textLabels.push_back({ "LASER: STANDBY", { x, y }, m_currentColor, 12.0f, false, false });
    }
}

void TacticalHudRenderer::buildWarningBanners(HudDrawList& drawList) const
{
    if (!m_telemetry.warningBanner.empty()) {
        HudColor warnColor { 1.0f, 0.3f, 0.3f, 1.0f };
        drawList.textLabels.push_back({ m_telemetry.warningBanner, { 0.5f, 0.16f }, warnColor, 14.0f, true, true });
    }

    if (!m_telemetry.systemHealth.empty()) {
        drawList.textLabels.push_back({ m_telemetry.systemHealth, { 0.5f, 0.94f }, m_currentColor, 12.0f, false, true });
    }
}

bool TacticalHudRenderer::renderRgba(std::uint8_t* rgbaBuffer, int width, int height, int strideBytes) const
{
    if (!rgbaBuffer || width <= 0 || height <= 0) {
        return false;
    }

    const int stride = (strideBytes > 0) ? strideBytes : (width * 4);
    const auto drawList = generateDrawList();

    // 1. Render rectangles
    for (const auto& r : drawList.rectangles) {
        const int rx0 = static_cast<int>(r.topLeft.x * width);
        const int ry0 = static_cast<int>(r.topLeft.y * height);
        const int rw = static_cast<int>(r.width * width);
        const int rh = static_cast<int>(r.height * height);

        if (r.filled) {
            for (int y = ry0; y < ry0 + rh; ++y) {
                for (int x = rx0; x < rx0 + rw; ++x) {
                    drawPixelRgba(rgbaBuffer, width, height, stride, x, y, r.fillColor);
                }
            }
        }

        // Outline
        drawLineBresenham(rgbaBuffer, width, height, stride, rx0, ry0, rx0 + rw, ry0, r.strokeColor);
        drawLineBresenham(rgbaBuffer, width, height, stride, rx0 + rw, ry0, rx0 + rw, ry0 + rh, r.strokeColor);
        drawLineBresenham(rgbaBuffer, width, height, stride, rx0 + rw, ry0 + rh, rx0, ry0 + rh, r.strokeColor);
        drawLineBresenham(rgbaBuffer, width, height, stride, rx0, ry0 + rh, rx0, ry0, r.strokeColor);
    }

    // 2. Render circles
    for (const auto& c : drawList.circles) {
        const int xc = static_cast<int>(c.center.x * width);
        const int yc = static_cast<int>(c.center.y * height);
        const int radius = static_cast<int>(c.radius * height);
        drawCircleMidpoint(rgbaBuffer, width, height, stride, xc, yc, std::max(1, radius), c.color);
    }

    // 3. Render lines
    for (const auto& line : drawList.lines) {
        const int x0 = static_cast<int>(line.start.x * width);
        const int y0 = static_cast<int>(line.start.y * height);
        const int x1 = static_cast<int>(line.end.x * width);
        const int y1 = static_cast<int>(line.end.y * height);
        drawLineBresenham(rgbaBuffer, width, height, stride, x0, y0, x1, y1, line.color);
    }

    // 4. Render text labels
    for (const auto& txt : drawList.textLabels) {
        int x = static_cast<int>(txt.position.x * width);
        const int y = static_cast<int>(txt.position.y * height);
        const int scale = txt.bold ? 2 : 1;

        if (txt.centerAligned) {
            const int textWidth = static_cast<int>(txt.text.length() * 6 * scale);
            x -= textWidth / 2;
        }

        drawStringRgba(rgbaBuffer, width, height, stride, x, y, txt.text, txt.color, scale);
    }

    return true;
}

} // namespace PayloadHal
