/**
 * @file TacticalHudFilter.cpp
 * @brief Implementation of STANAG 4609 compliant military Head-Up Display (HUD) OSD overlay filter.
 */

#include "TacticalHudFilter.h"
#include "Misb1909OverlayRenderer.h"
#include "Misb1909Types.h"

#if defined(PELCOD_HAS_FILTERS)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace Video::Filters {

namespace {

    constexpr double kPi { 3.14159265358979323846 };

    inline double deg2rad(double deg) noexcept
    {
        return deg * (kPi / 180.0);
    }

    cv::Scalar getThemeColor(TacticalHudFilter::ColorPalette palette, PixelFormat format) noexcept
    {
        cv::Scalar rgb;
        switch (palette) {
        case TacticalHudFilter::ColorPalette::TacticalGreen:
            rgb = cv::Scalar(0, 255, 64);
            break;
        case TacticalHudFilter::ColorPalette::Amber:
            rgb = cv::Scalar(255, 191, 0);
            break;
        case TacticalHudFilter::ColorPalette::ElectricCyan:
            rgb = cv::Scalar(0, 255, 255);
            break;
        case TacticalHudFilter::ColorPalette::CombatRed:
            rgb = cv::Scalar(255, 48, 48);
            break;
        case TacticalHudFilter::ColorPalette::White:
        default:
            rgb = cv::Scalar(255, 255, 255);
            break;
        }

        if (format == PixelFormat::BGR24) {
            return cv::Scalar(rgb[2], rgb[1], rgb[0]);
        }
        return rgb;
    }

    void drawOutlinedText(cv::Mat& mat, const std::string& text, const cv::Point& pt, int fontFace, double fontScale,
        const cv::Scalar& color, int thickness = 1)
    {
        // Draw black outline
        cv::putText(mat, text, pt, fontFace, fontScale, cv::Scalar(0, 0, 0), thickness + 2, cv::LINE_AA);
        // Draw main color
        cv::putText(mat, text, pt, fontFace, fontScale, color, thickness, cv::LINE_AA);
    }

    void drawOutlinedLine(
        cv::Mat& mat, const cv::Point& p1, const cv::Point& p2, const cv::Scalar& color, int thickness = 1)
    {
        cv::line(mat, p1, p2, cv::Scalar(0, 0, 0), thickness + 2, cv::LINE_AA);
        cv::line(mat, p1, p2, color, thickness, cv::LINE_AA);
    }

    std::string formatDms(double degrees, bool isLatitude)
    {
        const char hem = isLatitude ? (degrees >= 0.0 ? 'N' : 'S') : (degrees >= 0.0 ? 'E' : 'W');
        double val = std::abs(degrees);
        const int d = static_cast<int>(val);
        val = (val - d) * 60.0;
        const int m = static_cast<int>(val);
        const double s = (val - m) * 60.0;

        char buf[64];
        std::snprintf(buf, sizeof(buf), "%02d*%02d'%04.1f\"%c", d, m, s, hem);
        return std::string(buf);
    }

    std::string formatRange(double meters)
    {
        char buf[32];
        if (meters >= 1000.0) {
            std::snprintf(buf, sizeof(buf), "%.2f km", meters / 1000.0);
        } else {
            std::snprintf(buf, sizeof(buf), "%d m", static_cast<int>(std::round(meters)));
        }
        return std::string(buf);
    }

    std::string formatUtcTimestamp(std::uint64_t epochUs)
    {
        if (epochUs == 0U) {
            return "N/A";
        }
        const std::time_t sec = static_cast<std::time_t>(epochUs / 1000000ULL);
        const auto ms = static_cast<int>((epochUs % 1000000ULL) / 1000ULL);
        std::tm tmBuf {};
#if defined(_WIN32)
        gmtime_s(&tmBuf, &sec);
#else
        gmtime_r(&sec, &tmBuf);
#endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuf);
        char fullBuf[80];
        std::snprintf(fullBuf, sizeof(fullBuf), "%s.%03d UTC", buf, ms);
        return std::string(fullBuf);
    }

    void renderMisb1909(cv::Mat& mat, int width, int height, const cv::Scalar& theme, bool showSecBanners,
        const TacticalHudFilter::PlatformData& plat, const TacticalHudFilter::TargetData& tgt,
        const TacticalHudFilter::LaserData& laser, const std::optional<Klv::SecurityMetadata>& sec);

} // namespace

TacticalHudFilter::TacticalHudFilter(HudMode mode, ColorPalette palette)
    : m_mode(mode)
    , m_palette(palette)
{
}

void TacticalHudFilter::setMode(HudMode mode)
{
    std::scoped_lock lock(m_mutex);
    m_mode = mode;
}

TacticalHudFilter::HudMode TacticalHudFilter::getMode() const
{
    std::scoped_lock lock(m_mutex);
    return m_mode;
}

void TacticalHudFilter::setColorPalette(ColorPalette palette)
{
    std::scoped_lock lock(m_mutex);
    m_palette = palette;
}

TacticalHudFilter::ColorPalette TacticalHudFilter::getColorPalette() const
{
    std::scoped_lock lock(m_mutex);
    return m_palette;
}

void TacticalHudFilter::setCoordinateFormat(CoordinateFormat format)
{
    std::scoped_lock lock(m_mutex);
    m_coordFormat = format;
}

TacticalHudFilter::CoordinateFormat TacticalHudFilter::getCoordinateFormat() const
{
    std::scoped_lock lock(m_mutex);
    return m_coordFormat;
}

void TacticalHudFilter::setShowHorizon(bool show)
{
    std::scoped_lock lock(m_mutex);
    m_showHorizon = show;
}

bool TacticalHudFilter::getShowHorizon() const
{
    std::scoped_lock lock(m_mutex);
    return m_showHorizon;
}

void TacticalHudFilter::setShowCompassTape(bool show)
{
    std::scoped_lock lock(m_mutex);
    m_showCompassTape = show;
}

bool TacticalHudFilter::getShowCompassTape() const
{
    std::scoped_lock lock(m_mutex);
    return m_showCompassTape;
}

void TacticalHudFilter::setShowSecurityBanners(bool show)
{
    std::scoped_lock lock(m_mutex);
    m_showSecurityBanners = show;
}

bool TacticalHudFilter::getShowSecurityBanners() const
{
    std::scoped_lock lock(m_mutex);
    return m_showSecurityBanners;
}

void TacticalHudFilter::setPlatformAttitude(double headingDeg, double pitchDeg, double rollDeg)
{
    std::scoped_lock lock(m_mutex);
    m_platform.headingDeg = headingDeg;
    m_platform.pitchDeg = pitchDeg;
    m_platform.rollDeg = rollDeg;
}

void TacticalHudFilter::setSensorOrientation(
    double azimuthDeg, double elevationDeg, double hfovDeg, double zoomMagnification)
{
    std::scoped_lock lock(m_mutex);
    m_platform.sensorAzimuthDeg = azimuthDeg;
    m_platform.sensorElevationDeg = elevationDeg;
    m_platform.hfovDeg = hfovDeg;
    m_platform.zoomLevel = zoomMagnification;
}

void TacticalHudFilter::setLaserData(const LaserData& laser)
{
    std::scoped_lock lock(m_mutex);
    m_laser = laser;
}

TacticalHudFilter::LaserData TacticalHudFilter::getLaserData() const
{
    std::scoped_lock lock(m_mutex);
    return m_laser;
}

std::string TacticalHudFilter::formatSt1909Angle(std::optional<double> deg, int digitsBefore, int digitsAfter)
{
    return Klv::Misb1909OverlayRenderer::formatSt1909Angle(deg, digitsBefore, digitsAfter);
}

std::string TacticalHudFilter::formatSt1909Meters(std::optional<double> meters, int digits)
{
    return Klv::Misb1909OverlayRenderer::formatSt1909Meters(meters, digits);
}

std::string TacticalHudFilter::formatSt1909IsoTime(std::optional<std::uint64_t> epochUs)
{
    return Klv::Misb1909OverlayRenderer::formatSt1909IsoTime(epochUs);
}

void TacticalHudFilter::setPlatformData(const PlatformData& platform)
{
    std::scoped_lock lock(m_mutex);
    m_platform = platform;
}

TacticalHudFilter::PlatformData TacticalHudFilter::getPlatformData() const
{
    std::scoped_lock lock(m_mutex);
    return m_platform;
}

void TacticalHudFilter::setTargetData(const TargetData& target)
{
    std::scoped_lock lock(m_mutex);
    m_target = target;
}

TacticalHudFilter::TargetData TacticalHudFilter::getTargetData() const
{
    std::scoped_lock lock(m_mutex);
    return m_target;
}

void TacticalHudFilter::setSecurityMetadata(const Klv::SecurityMetadata& security)
{
    std::scoped_lock lock(m_mutex);
    m_security = security;
}

void TacticalHudFilter::updateTelemetry(const Klv::UasDatalinkMessage& msg)
{
    std::scoped_lock lock(m_mutex);

    if (msg.platformHeadingDeg)
        m_platform.headingDeg = *msg.platformHeadingDeg;
    if (msg.platformPitchDeg)
        m_platform.pitchDeg = *msg.platformPitchDeg;
    if (msg.platformRollDeg)
        m_platform.rollDeg = *msg.platformRollDeg;
    if (msg.sensorRelAzimuthDeg)
        m_platform.sensorAzimuthDeg = *msg.sensorRelAzimuthDeg;
    if (msg.sensorRelElevationDeg)
        m_platform.sensorElevationDeg = *msg.sensorRelElevationDeg;
    if (msg.sensorHfovDeg)
        m_platform.hfovDeg = *msg.sensorHfovDeg;
    if (msg.sensorVfovDeg)
        m_platform.vfovDeg = *msg.sensorVfovDeg;
    if (msg.sensorLatitudeDeg)
        m_platform.latitudeDeg = *msg.sensorLatitudeDeg;
    if (msg.sensorLongitudeDeg)
        m_platform.longitudeDeg = *msg.sensorLongitudeDeg;
    if (msg.sensorTrueAltitudeM) {
        m_platform.altitudeM = *msg.sensorTrueAltitudeM;
        m_platform.altitudeIsHae = false;
    } else if (msg.sensorAltitudeHaeM) {
        m_platform.altitudeM = *msg.sensorAltitudeHaeM;
        m_platform.altitudeIsHae = true;
    }
    if (msg.platformTailNumber)
        m_platform.tailNumber = *msg.platformTailNumber;
    if (msg.platformDesignation)
        m_platform.designation = *msg.platformDesignation;
    if (msg.missionId)
        m_platform.missionId = *msg.missionId;
    if (msg.imageSourceSensor)
        m_platform.sensorPayload = *msg.imageSourceSensor;
    if (msg.precisionTimeStampUs)
        m_platform.timestampUs = *msg.precisionTimeStampUs;

    if (msg.frameCenterLatDeg) {
        m_target.latitudeDeg = *msg.frameCenterLatDeg;
        m_target.isTrueLocation = false;
    }
    if (msg.frameCenterLonDeg) {
        m_target.longitudeDeg = *msg.frameCenterLonDeg;
        m_target.isTrueLocation = false;
    }
    if (msg.frameCenterElevM) {
        m_target.elevationM = *msg.frameCenterElevM;
        m_target.elevationIsHae = false;
    } else if (msg.frameCenterElevHaeM) {
        m_target.elevationM = *msg.frameCenterElevHaeM;
        m_target.elevationIsHae = true;
    }
    if (msg.slantRangeM)
        m_target.slantRangeM = *msg.slantRangeM;
    if (msg.targetWidthM)
        m_target.widthM = *msg.targetWidthM;

    if (msg.security)
        m_security = *msg.security;
    if (msg.cornerCoordinates)
        m_footprintCorners = *msg.cornerCoordinates;
}

void TacticalHudFilter::process(std::uint8_t* data, int width, int height, PixelFormat format)
{
    if (data == nullptr || width <= 0 || height <= 0) {
        return;
    }

    PlatformData plat;
    TargetData tgt;
    LaserData laser;
    std::optional<Klv::SecurityMetadata> sec;
    std::optional<Klv::FrustumCorners> corners;
    HudMode mode;
    ColorPalette palette;
    CoordinateFormat coordFmt;
    bool showHorizon;
    bool showCompass;
    bool showSecBanners;

    {
        std::scoped_lock lock(m_mutex);
        plat = m_platform;
        tgt = m_target;
        laser = m_laser;
        sec = m_security;
        corners = m_footprintCorners;
        mode = m_mode;
        palette = m_palette;
        coordFmt = m_coordFormat;
        showHorizon = m_showHorizon;
        showCompass = m_showCompassTape;
        showSecBanners = m_showSecurityBanners;
    }

    cv::Mat mat(height, width, CV_8UC3, data);
    const cv::Scalar theme = getThemeColor(palette, format);
    const int cx = width / 2;
    const int cy = height / 2;

    if (mode == HudMode::Misb1909) {
        renderMisb1909(mat, width, height, theme, showSecBanners, plat, tgt, laser, sec);
        return;
    }

    // 1. Security Classification Banners (Top & Bottom)
    if (showSecBanners && sec.has_value()) {
        const int bannerH = std::max(20, height / 35);
        cv::Scalar bannerBg;
        std::string classText = "UNCLASSIFIED";

        switch (sec->classification) {
        case Klv::SecurityClassification::Unclassified:
            bannerBg = (format == PixelFormat::BGR24) ? cv::Scalar(50, 130, 0) : cv::Scalar(0, 130, 50);
            classText = "UNCLASSIFIED";
            break;
        case Klv::SecurityClassification::Restricted:
            bannerBg = (format == PixelFormat::BGR24) ? cv::Scalar(180, 130, 0) : cv::Scalar(0, 130, 180);
            classText = "RESTRICTED";
            break;
        case Klv::SecurityClassification::Confidential:
            bannerBg = (format == PixelFormat::BGR24) ? cv::Scalar(180, 50, 0) : cv::Scalar(0, 50, 180);
            classText = "CONFIDENTIAL";
            break;
        case Klv::SecurityClassification::Secret:
            bannerBg = (format == PixelFormat::BGR24) ? cv::Scalar(20, 20, 190) : cv::Scalar(190, 20, 20);
            classText = "SECRET";
            break;
        case Klv::SecurityClassification::TopSecret:
            bannerBg = (format == PixelFormat::BGR24) ? cv::Scalar(0, 120, 220) : cv::Scalar(220, 120, 0);
            classText = "TOP SECRET";
            break;
        }

        std::string fullBanner = "// " + classText;
        if (!sec->classifyingCountry.empty())
            fullBanner += " // " + sec->classifyingCountry;
        if (!sec->caveats.empty())
            fullBanner += " // " + sec->caveats;
        fullBanner += " //";

        // Top banner
        cv::rectangle(mat, cv::Rect(0, 0, width, bannerH), bannerBg, cv::FILLED);
        cv::rectangle(mat, cv::Rect(0, 0, width, bannerH), cv::Scalar(0, 0, 0), 1);
        int baseLine = 0;
        const cv::Size sz = cv::getTextSize(fullBanner, cv::FONT_HERSHEY_SIMPLEX, 0.45, 1, &baseLine);
        cv::putText(mat, fullBanner, cv::Point(std::max(0, (width - sz.width) / 2), (bannerH + sz.height) / 2),
            cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);

        // Bottom banner
        cv::rectangle(mat, cv::Rect(0, height - bannerH, width, bannerH), bannerBg, cv::FILLED);
        cv::rectangle(mat, cv::Rect(0, height - bannerH, width, bannerH), cv::Scalar(0, 0, 0), 1);
        cv::putText(mat, fullBanner,
            cv::Point(std::max(0, (width - sz.width) / 2), height - bannerH + (bannerH + sz.height) / 2),
            cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    }

    // 2. Compass Heading Tape (Top Center)
    if (showCompass) {
        const int tapeY = std::max(35, height / 15);
        const int tapeWidth = std::min(400, width * 3 / 4);
        const int leftX = cx - tapeWidth / 2;
        const int rightX = cx + tapeWidth / 2;

        drawOutlinedLine(mat, cv::Point(leftX, tapeY), cv::Point(rightX, tapeY), theme, 1);

        // Heading angle: sensor azimuth relative to platform or true compass
        double heading = plat.headingDeg + plat.sensorAzimuthDeg;
        heading = std::fmod(heading, 360.0);
        if (heading < 0.0)
            heading += 360.0;

        constexpr double kFovTape = 60.0; // 60 degrees visible span
        const double pxPerDeg = static_cast<double>(tapeWidth) / kFovTape;

        const int startDeg = static_cast<int>(std::floor(heading - kFovTape / 2.0));
        const int endDeg = static_cast<int>(std::ceil(heading + kFovTape / 2.0));

        for (int deg = startDeg; deg <= endDeg; ++deg) {
            if (deg % 5 != 0)
                continue;

            const double offsetDeg = deg - heading;
            const int tickX = static_cast<int>(cx + offsetDeg * pxPerDeg);
            if (tickX < leftX || tickX > rightX)
                continue;

            int wrapped = deg % 360;
            if (wrapped < 0)
                wrapped += 360;

            const bool isMajor = (deg % 10 == 0);
            const int tickLen = isMajor ? 10 : 5;
            drawOutlinedLine(mat, cv::Point(tickX, tapeY), cv::Point(tickX, tapeY - tickLen), theme, 1);

            if (isMajor) {
                std::string label;
                if (wrapped == 0)
                    label = "N";
                else if (wrapped == 45)
                    label = "NE";
                else if (wrapped == 90)
                    label = "E";
                else if (wrapped == 135)
                    label = "SE";
                else if (wrapped == 180)
                    label = "S";
                else if (wrapped == 225)
                    label = "SW";
                else if (wrapped == 270)
                    label = "W";
                else if (wrapped == 315)
                    label = "NW";
                else {
                    char buf[16] {};
                    std::snprintf(buf, sizeof(buf), "%02d", wrapped / 10);
                    label = buf;
                }

                int bLine = 0;
                const cv::Size lblSz = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.38, 1, &bLine);
                drawOutlinedText(mat, label, cv::Point(tickX - lblSz.width / 2, tapeY - tickLen - 3),
                    cv::FONT_HERSHEY_SIMPLEX, 0.38, theme, 1);
            }
        }

        // Center pointer triangle
        const std::vector<cv::Point> caret
            = { cv::Point(cx, tapeY + 2), cv::Point(cx - 5, tapeY + 8), cv::Point(cx + 5, tapeY + 8) };
        cv::fillConvexPoly(mat, caret, theme, cv::LINE_AA);

        // Digital heading readout
        char hdgBuf[32];
        std::snprintf(hdgBuf, sizeof(hdgBuf), "%03.0f*", heading);
        drawOutlinedText(mat, hdgBuf, cv::Point(cx - 15, tapeY + 22), cv::FONT_HERSHEY_SIMPLEX, 0.42, theme, 1);
    }

    // 3. Artificial Horizon & Pitch Ladder (Standard and FullTactical modes)
    if (showHorizon && mode != HudMode::Minimal) {
        const double rollRad = deg2rad(-plat.rollDeg);
        const double pitchPx = plat.pitchDeg * 4.0; // 4 pixels per degree pitch

        const double cosR = std::cos(rollRad);
        const double sinR = std::sin(rollRad);

        // Center horizon line with gap
        constexpr int kHalfHorizon = 70;
        constexpr int kCenterGap = 20;

        auto rotateAndTranslate = [&](double lx, double ly) -> cv::Point {
            const double rx = lx * cosR - (ly + pitchPx) * sinR;
            const double ry = lx * sinR + (ly + pitchPx) * cosR;
            return cv::Point(static_cast<int>(cx + rx), static_cast<int>(cy + ry));
        };

        // Horizon bar (Left and Right wings)
        drawOutlinedLine(mat, rotateAndTranslate(-kHalfHorizon, 0), rotateAndTranslate(-kCenterGap, 0), theme, 1);
        drawOutlinedLine(mat, rotateAndTranslate(+kCenterGap, 0), rotateAndTranslate(+kHalfHorizon, 0), theme, 1);

        // Pitch rungs at +/- 10 and +/- 20 deg
        const int pitchSteps[] = { 10, 20, -10, -20 };
        for (const int pDeg : pitchSteps) {
            const double yOffset = -pDeg * 4.0;
            constexpr int rungLen = 40;
            constexpr int rungGap = 15;
            const int tickDir = (pDeg > 0) ? +4 : -4; // Inward tick marks

            // Left rung
            const cv::Point pL1 = rotateAndTranslate(-rungLen, yOffset);
            const cv::Point pL2 = rotateAndTranslate(-rungGap, yOffset);
            drawOutlinedLine(mat, pL1, pL2, theme, 1);
            drawOutlinedLine(mat, pL1, rotateAndTranslate(-rungLen, yOffset + tickDir), theme, 1);

            // Right rung
            const cv::Point pR1 = rotateAndTranslate(+rungGap, yOffset);
            const cv::Point pR2 = rotateAndTranslate(+rungLen, yOffset);
            drawOutlinedLine(mat, pR1, pR2, theme, 1);
            drawOutlinedLine(mat, pR2, rotateAndTranslate(+rungLen, yOffset + tickDir), theme, 1);

            // Angle text
            char pStr[16] {};
            std::snprintf(pStr, sizeof(pStr), "%d", std::abs(pDeg));
            drawOutlinedText(
                mat, pStr, rotateAndTranslate(-rungLen - 16, yOffset + 4), cv::FONT_HERSHEY_SIMPLEX, 0.32, theme, 1);
        }
    }

    // 4. Center Tactical Reticle (Boresight)
    {
        constexpr int r = 10;
        constexpr int arm = 20;

        // Center circle
        cv::circle(mat, cv::Point(cx, cy), r + 1, cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
        cv::circle(mat, cv::Point(cx, cy), r, theme, 1, cv::LINE_AA);

        // 4 crosshair arms
        drawOutlinedLine(mat, cv::Point(cx - r - arm, cy), cv::Point(cx - r, cy), theme, 1);
        drawOutlinedLine(mat, cv::Point(cx + r, cy), cv::Point(cx + r + arm, cy), theme, 1);
        drawOutlinedLine(mat, cv::Point(cx, cy - r - arm), cv::Point(cx, cy - r), theme, 1);
        drawOutlinedLine(mat, cv::Point(cx, cy + r), cv::Point(cx, cy + r + arm), theme, 1);

        // Center dot
        cv::circle(mat, cv::Point(cx, cy), 1, theme, cv::FILLED, cv::LINE_AA);

        // Sensor angle and FOV readout under reticle
        char azElBuf[64];
        std::snprintf(
            azElBuf, sizeof(azElBuf), "AZ: %+05.1f*  EL: %+05.1f*", plat.sensorAzimuthDeg, plat.sensorElevationDeg);
        drawOutlinedText(mat, azElBuf, cv::Point(cx - 65, cy + r + arm + 16), cv::FONT_HERSHEY_SIMPLEX, 0.38, theme, 1);

        char fovBuf[64];
        std::snprintf(fovBuf, sizeof(fovBuf), "HFOV: %04.1f*  Z: %.1fx", plat.hfovDeg, plat.zoomLevel);
        drawOutlinedText(mat, fovBuf, cv::Point(cx - 55, cy + r + arm + 30), cv::FONT_HERSHEY_SIMPLEX, 0.38, theme, 1);
    }

    // 5. Target Geodetic Telemetry Data Card (Bottom Left)
    if (mode != HudMode::Minimal && (tgt.latitudeDeg.has_value() || tgt.slantRangeM.has_value())) {
        const int cardX = 20;
        const int cardY = height - (showSecBanners && sec.has_value() ? 30 : 15);
        int curY = cardY - 70;

        drawOutlinedText(mat, "[ TARGET TELEMETRY ]", cv::Point(cardX, curY), cv::FONT_HERSHEY_SIMPLEX, 0.40, theme, 1);
        curY += 16;

        if (tgt.latitudeDeg.has_value() && tgt.longitudeDeg.has_value()) {
            std::string posStr;
            if (coordFmt == CoordinateFormat::Dms) {
                posStr = "TGT: " + formatDms(*tgt.latitudeDeg, true) + " " + formatDms(*tgt.longitudeDeg, false);
            } else {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "TGT: %.6f*N %.6f*W", *tgt.latitudeDeg, *tgt.longitudeDeg);
                posStr = buf;
            }
            drawOutlinedText(mat, posStr, cv::Point(cardX, curY), cv::FONT_HERSHEY_SIMPLEX, 0.38, theme, 1);
            curY += 15;
        }

        if (tgt.elevationM.has_value()) {
            char elBuf[48];
            std::snprintf(elBuf, sizeof(elBuf), "ELEV: %.0f m MSL", *tgt.elevationM);
            drawOutlinedText(mat, elBuf, cv::Point(cardX, curY), cv::FONT_HERSHEY_SIMPLEX, 0.38, theme, 1);
            curY += 15;
        }

        if (tgt.slantRangeM.has_value()) {
            const std::string rngStr = "SLANT RNG: " + formatRange(*tgt.slantRangeM);
            drawOutlinedText(mat, rngStr, cv::Point(cardX, curY), cv::FONT_HERSHEY_SIMPLEX, 0.38, theme, 1);
            curY += 15;
        }

        if (tgt.widthM.has_value()) {
            char wBuf[48];
            std::snprintf(wBuf, sizeof(wBuf), "FOOTPRINT: %.0f m", *tgt.widthM);
            drawOutlinedText(mat, wBuf, cv::Point(cardX, curY), cv::FONT_HERSHEY_SIMPLEX, 0.38, theme, 1);
        }
    }

    // 6. Ownship Platform & Navigation Card (Bottom Right, FullTactical Mode)
    if (mode == HudMode::FullTactical) {
        const int cardW = 240;
        const int cardX = width - cardW;
        const int cardY = height - (showSecBanners && sec.has_value() ? 30 : 15);
        int curY = cardY - 70;

        drawOutlinedText(mat, "[ PLATFORM & NAV ]", cv::Point(cardX, curY), cv::FONT_HERSHEY_SIMPLEX, 0.40, theme, 1);
        curY += 16;

        if (!plat.tailNumber.empty()) {
            const std::string tailStr = "PLATFORM: " + plat.tailNumber;
            drawOutlinedText(mat, tailStr, cv::Point(cardX, curY), cv::FONT_HERSHEY_SIMPLEX, 0.38, theme, 1);
            curY += 15;
        }

        if (!plat.missionId.empty()) {
            const std::string misStr = "MISSION: " + plat.missionId;
            drawOutlinedText(mat, misStr, cv::Point(cardX, curY), cv::FONT_HERSHEY_SIMPLEX, 0.38, theme, 1);
            curY += 15;
        }

        if (plat.latitudeDeg.has_value() && plat.longitudeDeg.has_value()) {
            char posBuf[64];
            std::snprintf(posBuf, sizeof(posBuf), "POS: %.4f*N %.4f*W", *plat.latitudeDeg, *plat.longitudeDeg);
            drawOutlinedText(mat, posBuf, cv::Point(cardX, curY), cv::FONT_HERSHEY_SIMPLEX, 0.38, theme, 1);
            curY += 15;
        }

        if (plat.altitudeM.has_value()) {
            char altBuf[48];
            std::snprintf(altBuf, sizeof(altBuf), "ALT: %.0f m MSL", *plat.altitudeM);
            drawOutlinedText(mat, altBuf, cv::Point(cardX, curY), cv::FONT_HERSHEY_SIMPLEX, 0.38, theme, 1);
            curY += 15;
        }

        if (plat.timestampUs > 0U) {
            const std::string timeStr = "UTC: " + formatUtcTimestamp(plat.timestampUs);
            drawOutlinedText(mat, timeStr, cv::Point(cardX, curY), cv::FONT_HERSHEY_SIMPLEX, 0.32, theme, 1);
        }
    }

    // 7. Tactical Frustum Radar Inset (Upper Right, FullTactical Mode)
    if (mode == HudMode::FullTactical && corners.has_value()) {
        const int radarSz = std::min(100, width / 5);
        const int radarX = width - radarSz - 15;
        const int radarY = (showSecBanners && sec.has_value() ? 30 : 15);

        // Scrim background
        cv::rectangle(mat, cv::Rect(radarX, radarY, radarSz, radarSz), cv::Scalar(0, 0, 0), cv::FILLED);
        cv::rectangle(mat, cv::Rect(radarX, radarY, radarSz, radarSz), theme, 1);

        const cv::Point rCenter(radarX + radarSz / 2, radarY + radarSz / 2);
        cv::circle(mat, rCenter, 2, theme, cv::FILLED); // Ownship center

        // Heading vector
        const double hRad = deg2rad(plat.headingDeg - 90.0);
        const cv::Point headPt(
            rCenter.x + static_cast<int>(18.0 * std::cos(hRad)), rCenter.y + static_cast<int>(18.0 * std::sin(hRad)));
        cv::line(mat, rCenter, headPt, theme, 1, cv::LINE_AA);

        // Footprint quadrilateral
        const double fAz = deg2rad(plat.headingDeg + plat.sensorAzimuthDeg - 90.0);
        const int fDist = radarSz / 3;
        const cv::Point fCenter(
            rCenter.x + static_cast<int>(fDist * std::cos(fAz)), rCenter.y + static_cast<int>(fDist * std::sin(fAz)));

        constexpr int kw = 12;
        constexpr int kh = 8;
        const std::vector<cv::Point> quad
            = { cv::Point(fCenter.x - kw, fCenter.y - kh), cv::Point(fCenter.x + kw, fCenter.y - kh),
                  cv::Point(fCenter.x + kw, fCenter.y + kh), cv::Point(fCenter.x - kw, fCenter.y + kh) };
        cv::polylines(mat, quad, true, theme, 1, cv::LINE_AA);
        cv::line(mat, rCenter, fCenter, theme, 1, cv::LINE_AA);

        drawOutlinedText(mat, "RADAR", cv::Point(radarX + 4, radarY + 12), cv::FONT_HERSHEY_SIMPLEX, 0.30, theme, 1);
    }
}

namespace {

    void renderMisb1909(cv::Mat& mat, int width, int height, const cv::Scalar& theme, bool showSecBanners,
        const TacticalHudFilter::PlatformData& plat, const TacticalHudFilter::TargetData& tgt,
        const TacticalHudFilter::LaserData& laser, const std::optional<Klv::SecurityMetadata>& sec)
    {
        const double fontScale = std::clamp(static_cast<double>(height) * 0.00045, 0.35, 0.90);
        const int fontFace = cv::FONT_HERSHEY_SIMPLEX;
        const int thickness = 1;
        int bLine = 0;
        const cv::Size sampleSz = cv::getTextSize("A", fontFace, fontScale, thickness, &bLine);
        const int lineH = static_cast<int>(std::round(sampleSz.height * 1.6));

        Klv::UasDatalinkMessage msg {};
        if (!plat.designation.empty())
            msg.platformDesignation = plat.designation;
        if (!plat.tailNumber.empty())
            msg.platformTailNumber = plat.tailNumber;
        if (plat.latitudeDeg.has_value())
            msg.sensorLatitudeDeg = *plat.latitudeDeg;
        if (plat.longitudeDeg.has_value())
            msg.sensorLongitudeDeg = *plat.longitudeDeg;
        if (plat.altitudeM.has_value()) {
            if (plat.altitudeIsHae)
                msg.sensorAltitudeHaeM = *plat.altitudeM;
            else
                msg.sensorTrueAltitudeM = *plat.altitudeM;
        }
        msg.platformHeadingDeg = plat.headingDeg;
        msg.platformPitchDeg = plat.pitchDeg;
        msg.platformRollDeg = plat.rollDeg;
        msg.sensorRelAzimuthDeg = plat.sensorAzimuthDeg;
        msg.sensorRelElevationDeg = plat.sensorElevationDeg;
        msg.sensorHfovDeg = plat.hfovDeg;
        msg.sensorVfovDeg = plat.vfovDeg;
        if (!plat.sensorPayload.empty())
            msg.imageSourceSensor = plat.sensorPayload;
        if (plat.timestampUs > 0)
            msg.precisionTimeStampUs = plat.timestampUs;

        if (tgt.latitudeDeg.has_value())
            msg.frameCenterLatDeg = *tgt.latitudeDeg;
        if (tgt.longitudeDeg.has_value())
            msg.frameCenterLonDeg = *tgt.longitudeDeg;
        if (tgt.elevationM.has_value()) {
            if (tgt.elevationIsHae)
                msg.frameCenterElevHaeM = *tgt.elevationM;
            else
                msg.frameCenterElevM = *tgt.elevationM;
        }
        if (tgt.slantRangeM.has_value())
            msg.slantRangeM = *tgt.slantRangeM;
        if (tgt.widthM.has_value())
            msg.targetWidthM = *tgt.widthM;

        if (sec.has_value())
            msg.security = *sec;

        Klv::Misb1909Config config {};
        config.density = Klv::SymbologyDensity::Misb1909Standard;
        config.showSecurityBanners = showSecBanners;
        const Klv::TacticalOverlayLayout layout = Klv::Misb1909OverlayRenderer::buildLayout(msg, config);

        const double w = static_cast<double>(width);
        const double h = static_cast<double>(height);

        auto drawLayoutGroup = [&](const Klv::Misb1909GroupData& grp) {
            if (!grp.visible || grp.lines.empty())
                return;
            const int col = static_cast<int>(std::round(w * static_cast<double>(grp.anchor.x.value())));
            const int row = static_cast<int>(std::round(h * static_cast<double>(grp.anchor.y.value())));

            const int totalH = static_cast<int>(grp.lines.size() - 1U) * lineH;
            int curY = (grp.stackDirection == Klv::StackDirection::Upwards) ? (row - totalH) : (row + sampleSz.height);

            for (const auto& line : grp.lines) {
                const cv::Size sz = cv::getTextSize(line, fontFace, fontScale, thickness, &bLine);
                int curX = col;
                if (grp.alignment == Klv::TextAlignment::Center) {
                    curX -= sz.width / 2;
                } else if (grp.alignment == Klv::TextAlignment::Right) {
                    curX -= sz.width;
                }
                drawOutlinedText(mat, line, cv::Point(curX, curY), fontFace, fontScale, theme, thickness);
                curY += lineH;
            }
        };

        // 1. Main Sensor Group
        drawLayoutGroup(layout.mainSensorGroup);

        // 2. Classification Group
        drawLayoutGroup(layout.securityGroup);

        // 3. Platform Information Group
        drawLayoutGroup(layout.platformGroup);

        // 4. True North Arrow Group (Mid-Left: 50.00% row, 2.00% col)
        {
            const int col
                = static_cast<int>(std::round(w * static_cast<double>(layout.northArrowGroup.anchor.x.value())));
            const int row
                = static_cast<int>(std::round(h * static_cast<double>(layout.northArrowGroup.anchor.y.value())));
            const int radius = std::max(14, height / 45);
            const cv::Point center(col + radius + 2, row);

            cv::circle(mat, center, radius + 1, cv::Scalar(0, 0, 0), thickness + 2, cv::LINE_AA);
            cv::circle(mat, center, radius, theme, thickness, cv::LINE_AA);

            const double totalHeading = plat.headingDeg + plat.sensorAzimuthDeg;
            const double northAngleRad = deg2rad(-totalHeading - 90.0);

            const int arrowLen = radius + 6;
            const cv::Point tip(static_cast<int>(center.x + arrowLen * std::cos(northAngleRad)),
                static_cast<int>(center.y + arrowLen * std::sin(northAngleRad)));
            const double wingRad1 = northAngleRad + deg2rad(150.0);
            const double wingRad2 = northAngleRad - deg2rad(150.0);
            const cv::Point w1(
                static_cast<int>(tip.x + 8.0 * std::cos(wingRad1)), static_cast<int>(tip.y + 8.0 * std::sin(wingRad1)));
            const cv::Point w2(
                static_cast<int>(tip.x + 8.0 * std::cos(wingRad2)), static_cast<int>(tip.y + 8.0 * std::sin(wingRad2)));
            const std::vector<cv::Point> triangle = { tip, w1, w2 };
            cv::fillConvexPoly(mat, triangle, theme, cv::LINE_AA);

            const cv::Size nSz = cv::getTextSize("N", fontFace, fontScale, thickness, &bLine);
            drawOutlinedText(mat, "N", cv::Point(center.x - nSz.width / 2, center.y + nSz.height / 2), fontFace,
                fontScale, theme, thickness);
        }

        // 5. Laser Sensor Group (With active/PRF status)
        {
            Klv::Misb1909GroupData laserGrp = layout.laserSensorGroup;
            laserGrp.lines.clear();
            laserGrp.lines.push_back(laser.name.empty() ? "Laser" : laser.name);
            laserGrp.lines.push_back(laser.active ? "Laser ON" : "Laser OFF");
            if (laser.prfCode.has_value()) {
                char prfBuf[32];
                std::snprintf(prfBuf, sizeof(prfBuf), "Laser PRF Code %06u", *laser.prfCode);
                laserGrp.lines.push_back(prfBuf);
            }
            drawLayoutGroup(laserGrp);
        }

        // 6. Date/Time Group (With optional FT frame timestamp)
        {
            Klv::Misb1909GroupData dtGrp = layout.dateTimeGroup;
            if (plat.frameTimestampUs.has_value()) {
                dtGrp.lines.insert(dtGrp.lines.begin(),
                    "FT " + Klv::Misb1909OverlayRenderer::formatSt1909IsoTime(plat.frameTimestampUs));
            }
            drawLayoutGroup(dtGrp);
        }

        // 7. Target Group (With TL vs FC distinction)
        {
            Klv::Misb1909GroupData tgtGrp = layout.targetGroup;
            if (tgt.isTrueLocation) {
                for (auto& line : tgtGrp.lines) {
                    const auto pos = line.find(" FC ");
                    if (pos != std::string::npos) {
                        line.replace(pos, 4U, " TL ");
                    }
                }
            }
            drawLayoutGroup(tgtGrp);
        }

        // 8. Target Reticle Crosshair
        if (layout.reticle.visible) {
            for (const auto& line : layout.reticle.crosshairLines) {
                const cv::Point p1(static_cast<int>(std::round(w * static_cast<double>(line.start.x.value()))),
                    static_cast<int>(std::round(h * static_cast<double>(line.start.y.value()))));
                const cv::Point p2(static_cast<int>(std::round(w * static_cast<double>(line.end.x.value()))),
                    static_cast<int>(std::round(h * static_cast<double>(line.end.y.value()))));
                drawOutlinedLine(mat, p1, p2, theme, thickness);
            }
        }
    }

} // namespace

} // namespace Video::Filters

#endif // PELCOD_HAS_FILTERS
