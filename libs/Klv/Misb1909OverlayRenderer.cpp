/// @file Misb1909OverlayRenderer.cpp
/// @brief Implementation of MISB ST 1909.1 non-destructive tactical HUD layout generator.

#include "Misb1909OverlayRenderer.h"

#include <cstddef>
#include <cstdint>
#include <ctime>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace Klv {

namespace {

    constexpr double kPi { 3.14159265358979323846 };
    constexpr double kDegToRad { kPi / 180.0 };

    [[nodiscard]] double wrap360(double deg) noexcept
    {
        deg = std::fmod(deg, 360.0);
        if (deg < 0.0) {
            deg += 360.0;
        }
        return deg;
    }

} // namespace

std::string Misb1909OverlayRenderer::formatSt1909Angle(std::optional<double> deg, int digitsBefore, int digitsAfter)
{
    if (!deg.has_value()) {
        return "N/A";
    }

    char numBuf[64];
    std::snprintf(numBuf, sizeof(numBuf), "%.*f", digitsAfter, *deg);
    const std::string str(numBuf);

    const auto dotPos = str.find('.');
    const std::string before = (dotPos != std::string::npos) ? str.substr(0U, dotPos) : str;
    const std::string after = (dotPos != std::string::npos) ? str.substr(dotPos) : "";

    std::string padded;
    if (static_cast<int>(before.length()) < digitsBefore) {
        padded = std::string(static_cast<std::size_t>(digitsBefore - static_cast<int>(before.length())), ' ') + before;
    } else {
        padded = before;
    }

    return padded + after + "*";
}

std::string Misb1909OverlayRenderer::formatSt1909Meters(std::optional<double> meters, int digits)
{
    if (!meters.has_value()) {
        return "N/A";
    }

    const auto intVal = static_cast<long long>(std::round(*meters));
    char numBuf[32];
    std::snprintf(numBuf, sizeof(numBuf), "%lld", intVal);
    const std::string str(numBuf);

    std::string padded;
    if (static_cast<int>(str.length()) < digits) {
        padded = std::string(static_cast<std::size_t>(digits - static_cast<int>(str.length())), ' ') + str;
    } else {
        padded = str;
    }

    return padded + "m";
}

std::string Misb1909OverlayRenderer::formatSt1909IsoTime(std::optional<std::uint64_t> epochUs)
{
    if (!epochUs.has_value() || *epochUs == 0U) {
        return "N/A";
    }

    const auto sec = static_cast<std::time_t>(*epochUs / 1000000ULL);
    const auto frac10 = static_cast<int>((*epochUs % 1000000ULL) / 100000ULL);

    std::tm tmBuf {};
#if defined(_WIN32)
    gmtime_s(&tmBuf, &sec);
#else
    gmtime_r(&sec, &tmBuf);
#endif

    char dateBuf[64];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%dT%H:%M:%S", &tmBuf);

    char fullBuf[80];
    std::snprintf(fullBuf, sizeof(fullBuf), "%s.%dZ", dateBuf, frac10);
    return std::string(fullBuf);
}

std::string Misb1909OverlayRenderer::toMgrsString(std::optional<double> latDeg, std::optional<double> lonDeg)
{
    if (!latDeg.has_value() || !lonDeg.has_value()) {
        return "N/A";
    }

    const double lat = *latDeg;
    const double lon = *lonDeg;

    if (lat < -80.0 || lat > 84.0 || lon < -180.0 || lon > 180.0) {
        return "N/A";
    }

    // WGS-84 Ellipsoid parameters
    constexpr double a { 6378137.0 };
    constexpr double f { 1.0 / 298.257223563 };
    constexpr double e2 { 2.0 * f - f * f };
    constexpr double ePrime2 { e2 / (1.0 - e2) };
    constexpr double k0 { 0.9996 };

    int zone = static_cast<int>((lon + 180.0) / 6.0) + 1;
    if (zone < 1) {
        zone = 1;
    } else if (zone > 60) {
        zone = 60;
    }

    const double lambda0 = static_cast<double>(zone * 6 - 183) * kDegToRad;
    const double phi = lat * kDegToRad;
    const double lambda = lon * kDegToRad;
    const double deltaLambda = lambda - lambda0;

    const double sinPhi = std::sin(phi);
    const double cosPhi = std::cos(phi);
    const double tanPhi = std::tan(phi);

    const double n = a / std::sqrt(1.0 - e2 * sinPhi * sinPhi);
    const double t = tanPhi * tanPhi;
    const double c = ePrime2 * cosPhi * cosPhi;
    const double aTerm = cosPhi * deltaLambda;

    // Meridian distance
    constexpr double e4 { e2 * e2 };
    constexpr double e6 { e4 * e2 };
    const double m = a
        * ((1.0 - e2 / 4.0 - 3.0 * e4 / 64.0 - 5.0 * e6 / 256.0) * phi
            - (3.0 * e2 / 8.0 + 3.0 * e4 / 32.0 + 45.0 * e6 / 1024.0) * std::sin(2.0 * phi)
            + (15.0 * e4 / 256.0 + 45.0 * e6 / 1024.0) * std::sin(4.0 * phi)
            - (35.0 * e6 / 3072.0) * std::sin(6.0 * phi));

    const double easting = k0 * n
            * (aTerm + (1.0 - t + c) * std::pow(aTerm, 3.0) / 6.0
                + (5.0 - 18.0 * t + t * t + 72.0 * c - 58.0 * ePrime2) * std::pow(aTerm, 5.0) / 120.0)
        + 500000.0;

    double northing = k0
        * (m
            + n * tanPhi
                * (aTerm * aTerm / 2.0 + (5.0 - t + 9.0 * c + 4.0 * c * c) * std::pow(aTerm, 4.0) / 24.0
                    + (61.0 - 58.0 * t + t * t + 600.0 * c - 330.0 * ePrime2) * std::pow(aTerm, 6.0) / 720.0));

    if (lat < 0.0) {
        northing += 10000000.0;
    }

    // Latitude Band letter (C to X, omitting I and O)
    constexpr char kLatBands[] = "CDEFGHJKLMNPQRSTUVWX";
    const int bandIdx = std::clamp(static_cast<int>((lat + 80.0) / 8.0), 0, 19);
    const char bandLetter = kLatBands[bandIdx];

    // 100km Grid Square column letter
    constexpr char kGridColLetters[] = "ABCDEFGHJKLMNPQRSTUVWXYZ"; // 24 letters
    const int setCol = (zone - 1) % 3;
    const int colBase = setCol * 8 + 1;
    const auto colVal = static_cast<int>(easting / 100000.0);
    const int colIdx = (colBase + colVal - 2) % 24;
    const char colLetter = kGridColLetters[colIdx >= 0 ? colIdx : colIdx + 24];

    // 100km Grid Square row letter
    constexpr char kGridRowLetters[] = "ABCDEFGHJKLMNPQRSTUV"; // 20 letters
    const auto rowVal = static_cast<int>(std::fmod(northing / 100000.0, 20.0));
    const int rowBase = (zone % 2 == 1) ? 0 : 5;
    const int rowIdx = (rowBase + rowVal) % 20;
    const char rowLetter = kGridRowLetters[rowIdx >= 0 ? rowIdx : rowIdx + 20];

    const auto eRemainder = static_cast<unsigned int>(static_cast<long long>(std::round(easting)) % 100000LL);
    const auto nRemainder = static_cast<unsigned int>(static_cast<long long>(std::round(northing)) % 100000LL);

    char mgrsBuf[32];
    std::snprintf(mgrsBuf, sizeof(mgrsBuf), "%02d%c%c%c %05u %05u", zone, bandLetter, colLetter, rowLetter, eRemainder,
        nRemainder);
    return std::string(mgrsBuf);
}

TacticalOverlayLayout Misb1909OverlayRenderer::buildLayout(
    const UasDatalinkMessage& msg, const Misb1909Config& config) noexcept
{
    TacticalOverlayLayout layout {};
    layout.activeThemeColor = getPaletteColor(config.palette);

    const bool isMinimal = (config.density == SymbologyDensity::Minimal);

    // 1. Group 1: Main Sensor Group (Top-Left, 2.0% col, 3.7% row, Left, Downwards)
    {
        layout.mainSensorGroup.groupId = 1U;
        layout.mainSensorGroup.groupTitle = "Main Sensor";
        layout.mainSensorGroup.anchor = NormalizedPoint { NormalizedCoord(0.02F), NormalizedCoord(0.037F) };
        layout.mainSensorGroup.alignment = TextAlignment::Left;
        layout.mainSensorGroup.stackDirection = StackDirection::Downwards;
        layout.mainSensorGroup.visible = !isMinimal;

        const std::string name = msg.imageSourceSensor.value_or("N/A");
        layout.mainSensorGroup.lines.push_back(name.empty() ? "N/A" : name);

        const std::string azStr = "REL AZ " + formatSt1909Angle(msg.sensorRelAzimuthDeg, 5, 4);
        layout.mainSensorGroup.lines.push_back(azStr);

        const std::string elStr = "REL EL " + formatSt1909Angle(msg.sensorRelElevationDeg, 5, 4);
        layout.mainSensorGroup.lines.push_back(elStr);
    }

    // 2. Group 2: Classification & Releasability Group (Top-Center, 50.0% col, 3.7% row, Center, Downwards)
    {
        layout.securityGroup.groupId = 2U;
        layout.securityGroup.groupTitle = "Security Classification";
        layout.securityGroup.anchor = NormalizedPoint { NormalizedCoord(0.50F), NormalizedCoord(0.037F) };
        layout.securityGroup.alignment = TextAlignment::Center;
        layout.securityGroup.stackDirection = StackDirection::Downwards;
        layout.securityGroup.visible = config.showSecurityBanners;

        std::string classText = "UNCLASSIFIED";
        if (msg.security.has_value()) {
            switch (msg.security->classification) {
            case SecurityClassification::Unclassified:
                classText = "UNCLASSIFIED";
                break;
            case SecurityClassification::Restricted:
                classText = "RESTRICTED";
                break;
            case SecurityClassification::Confidential:
                classText = "CONFIDENTIAL";
                break;
            case SecurityClassification::Secret:
                classText = "SECRET";
                break;
            case SecurityClassification::TopSecret:
                classText = "TOP SECRET";
                break;
            }

            std::string fullBanner = "// " + classText;
            if (!msg.security->classifyingCountry.empty()) {
                fullBanner += " // " + msg.security->classifyingCountry;
            }
            if (!msg.security->caveats.empty()) {
                fullBanner += " // " + msg.security->caveats;
            }
            fullBanner += " //";
            layout.securityGroup.lines.push_back(fullBanner);
        } else {
            layout.securityGroup.lines.push_back("// UNCLASSIFIED //");
        }
    }

    // 3. Group 3: Platform Information Group (Top-Right, 98.0% col, 3.7% row, Right, Downwards)
    {
        layout.platformGroup.groupId = 3U;
        layout.platformGroup.groupTitle = "Platform Information";
        layout.platformGroup.anchor = NormalizedPoint { NormalizedCoord(0.98F), NormalizedCoord(0.037F) };
        layout.platformGroup.alignment = TextAlignment::Right;
        layout.platformGroup.stackDirection = StackDirection::Downwards;
        layout.platformGroup.visible = !isMinimal;

        std::string platName;
        if (msg.platformDesignation.has_value() && !msg.platformDesignation->empty()) {
            platName = *msg.platformDesignation;
            if (msg.platformTailNumber.has_value() && !msg.platformTailNumber->empty()) {
                platName += " " + *msg.platformTailNumber;
            }
        } else if (msg.platformTailNumber.has_value() && !msg.platformTailNumber->empty()) {
            platName = *msg.platformTailNumber;
        } else {
            platName = "N/A";
        }
        layout.platformGroup.lines.push_back(platName);

        if (config.coordinateMode == CoordinateDisplayMode::Mgrs) {
            const std::string mgrs = toMgrsString(msg.sensorLatitudeDeg, msg.sensorLongitudeDeg);
            layout.platformGroup.lines.push_back("MGRS " + mgrs);
        } else {
            layout.platformGroup.lines.push_back(formatSt1909Angle(msg.sensorLatitudeDeg, 3, 4) + " LAT");
            layout.platformGroup.lines.push_back(formatSt1909Angle(msg.sensorLongitudeDeg, 4, 4) + " LON");
        }

        std::optional<double> alt;
        bool isHae { false };
        if (msg.sensorAltitudeHaeM.has_value()) {
            alt = msg.sensorAltitudeHaeM;
            isHae = true;
        } else if (msg.sensorTrueAltitudeM.has_value()) {
            alt = msg.sensorTrueAltitudeM;
            isHae = false;
        }

        const std::string qual = isHae ? "HAE" : "MSL";
        layout.platformGroup.lines.push_back(formatSt1909Meters(alt, 5) + " " + qual + " ALT");
    }

    // 4. Group 4: True North Arrow Group (Mid-Left, 2.0% col, 50.0% row, Center, Centered)
    {
        layout.northArrowGroup.groupId = 4U;
        layout.northArrowGroup.groupTitle = "True North Arrow";
        layout.northArrowGroup.anchor = NormalizedPoint { NormalizedCoord(0.02F), NormalizedCoord(0.50F) };
        layout.northArrowGroup.alignment = TextAlignment::Center;
        layout.northArrowGroup.stackDirection = StackDirection::Downwards;
        layout.northArrowGroup.visible = config.showNorthArrow && !isMinimal;

        const double hdg = msg.platformHeadingDeg.value_or(0.0) + msg.sensorRelAzimuthDeg.value_or(0.0);
        char northBuf[32];
        std::snprintf(northBuf, sizeof(northBuf), "HDG %03.0f*", wrap360(hdg));
        layout.northArrowGroup.lines.push_back(northBuf);
    }

    // 5. Group 5: Laser Sensor Group (Bottom-Left, 2.0% col, 96.3% row, Left, Upwards)
    {
        layout.laserSensorGroup.groupId = 5U;
        layout.laserSensorGroup.groupTitle = "Laser Sensor";
        layout.laserSensorGroup.anchor = NormalizedPoint { NormalizedCoord(0.02F), NormalizedCoord(0.963F) };
        layout.laserSensorGroup.alignment = TextAlignment::Left;
        layout.laserSensorGroup.stackDirection = StackDirection::Upwards;
        layout.laserSensorGroup.visible = config.showLaserGroup && !isMinimal;

        layout.laserSensorGroup.lines.push_back("Laser");
        layout.laserSensorGroup.lines.push_back("Laser OFF");
    }

    // 6. Group 6: Date/Time Group (Bottom-Center, 50.0% col, 96.3% row, Center, Upwards)
    {
        layout.dateTimeGroup.groupId = 6U;
        layout.dateTimeGroup.groupTitle = "Date and Time";
        layout.dateTimeGroup.anchor = NormalizedPoint { NormalizedCoord(0.50F), NormalizedCoord(0.963F) };
        layout.dateTimeGroup.alignment = TextAlignment::Center;
        layout.dateTimeGroup.stackDirection = StackDirection::Upwards;
        layout.dateTimeGroup.visible = !isMinimal;

        layout.dateTimeGroup.lines.push_back("MT " + formatSt1909IsoTime(msg.precisionTimeStampUs));
    }

    // 7. Group 7: Target Group (Bottom-Right, 98.0% col, 96.3% row, Right, Upwards)
    {
        layout.targetGroup.groupId = 7U;
        layout.targetGroup.groupTitle = "Target Data";
        layout.targetGroup.anchor = NormalizedPoint { NormalizedCoord(0.98F), NormalizedCoord(0.963F) };
        layout.targetGroup.alignment = TextAlignment::Right;
        layout.targetGroup.stackDirection = StackDirection::Upwards;
        layout.targetGroup.visible = true; // Always visible for tactical range

        layout.targetGroup.lines.push_back(formatSt1909Meters(msg.slantRangeM, 7) + " SR");
        if (!isMinimal) {
            layout.targetGroup.lines.push_back(formatSt1909Meters(msg.targetWidthM, 7) + " TW");
            layout.targetGroup.lines.push_back(formatSt1909Angle(msg.sensorHfovDeg, 3, 4) + " HFOV");
            layout.targetGroup.lines.push_back(formatSt1909Angle(msg.sensorVfovDeg, 3, 4) + " VFOV");

            if (config.coordinateMode == CoordinateDisplayMode::Mgrs) {
                const std::string tgtMgrs = toMgrsString(msg.frameCenterLatDeg, msg.frameCenterLonDeg);
                layout.targetGroup.lines.push_back("FC MGRS " + tgtMgrs);
            } else {
                layout.targetGroup.lines.push_back(formatSt1909Angle(msg.frameCenterLatDeg, 3, 4) + " FC LAT");
                layout.targetGroup.lines.push_back(formatSt1909Angle(msg.frameCenterLonDeg, 4, 4) + " FC LON");
            }

            const std::string elQual = msg.frameCenterElevHaeM.has_value() ? "HAE" : "MSL";
            const auto elev = msg.frameCenterElevHaeM.has_value() ? msg.frameCenterElevHaeM : msg.frameCenterElevM;
            layout.targetGroup.lines.push_back(formatSt1909Meters(elev, 5) + " " + elQual + " FC EL");
        }
    }

    // 8. Group 8: Reticle & Boresight Group (Center 50.0% col, 50.0% row)
    {
        layout.reticle.visible = true;
        layout.reticle.center = NormalizedPoint { NormalizedCoord(0.50F), NormalizedCoord(0.50F) };
        layout.reticle.armLengthNorm = 0.025F;
        layout.reticle.centerGapNorm = 0.008F;

        const float cx = 0.50F;
        const float cy = 0.50F;
        const float arm = layout.reticle.armLengthNorm;
        const float gap = layout.reticle.centerGapNorm;

        // Left arm
        layout.reticle.crosshairLines.push_back(HudLineItem {
            NormalizedPoint { NormalizedCoord(cx - arm), NormalizedCoord(cy) },
            NormalizedPoint { NormalizedCoord(cx - gap), NormalizedCoord(cy) }, 1.5F, false, layout.activeThemeColor });
        // Right arm
        layout.reticle.crosshairLines.push_back(HudLineItem {
            NormalizedPoint { NormalizedCoord(cx + gap), NormalizedCoord(cy) },
            NormalizedPoint { NormalizedCoord(cx + arm), NormalizedCoord(cy) }, 1.5F, false, layout.activeThemeColor });
        // Top arm
        layout.reticle.crosshairLines.push_back(HudLineItem {
            NormalizedPoint { NormalizedCoord(cx), NormalizedCoord(cy - arm) },
            NormalizedPoint { NormalizedCoord(cx), NormalizedCoord(cy - gap) }, 1.5F, false, layout.activeThemeColor });
        // Bottom arm
        layout.reticle.crosshairLines.push_back(HudLineItem {
            NormalizedPoint { NormalizedCoord(cx), NormalizedCoord(cy + gap) },
            NormalizedPoint { NormalizedCoord(cx), NormalizedCoord(cy + arm) }, 1.5F, false, layout.activeThemeColor });
    }

    // 9. Flight Symbology: Artificial Horizon & Pitch Ladder
    if (config.showHorizon && !isMinimal) {
        layout.pitchLadder.visible = true;
        layout.pitchLadder.rollAngleDeg = static_cast<float>(msg.platformRollDeg.value_or(0.0));

        constexpr float horizonGap { 0.05F };
        layout.pitchLadder.horizonLeft = NormalizedPoint { NormalizedCoord(0.25F), NormalizedCoord(0.50F) };
        layout.pitchLadder.horizonRight = NormalizedPoint { NormalizedCoord(0.75F), NormalizedCoord(0.50F) };

        const float step = (config.ladderStepDegrees > 0.0F) ? config.ladderStepDegrees : 5.0F;
        const float maxPitch = (config.maxLadderPitchDegrees > 0.0F) ? config.maxLadderPitchDegrees : 20.0F;
        const float currentPitch = static_cast<float>(msg.platformPitchDeg.value_or(0.0));

        constexpr float kPxPerDegNorm { 0.008F }; // Normalized pitch sensitivity
        constexpr float rungHalfW { 0.06F };
        constexpr float tabLen { 0.012F };

        for (float deg = -maxPitch; deg <= maxPitch; deg += step) {
            if (std::abs(deg) < 0.1F) {
                continue; // Skip zero (represented by center horizon line)
            }

            const float relativePitch = deg - currentPitch;
            const float rungY = 0.50F - relativePitch * kPxPerDegNorm;

            if (rungY < 0.10F || rungY > 0.90F) {
                continue; // Viewport clipping
            }

            PitchLadderRung rung {};
            rung.pitchDeg = deg;
            rung.isDashed = (deg < 0.0F);

            // Left segment
            rung.leftStart
                = NormalizedPoint { NormalizedCoord(0.50F - horizonGap - rungHalfW), NormalizedCoord(rungY) };
            rung.leftEnd = NormalizedPoint { NormalizedCoord(0.50F - horizonGap), NormalizedCoord(rungY) };

            // Right segment
            rung.rightStart = NormalizedPoint { NormalizedCoord(0.50F + horizonGap), NormalizedCoord(rungY) };
            rung.rightEnd = NormalizedPoint { NormalizedCoord(0.50F + horizonGap + rungHalfW), NormalizedCoord(rungY) };

            // Vertical tick tabs pointing towards horizon
            const float tabY = (deg > 0.0F) ? (rungY + tabLen) : (rungY - tabLen);
            rung.leftTabEnd = NormalizedPoint { rung.leftStart.x, NormalizedCoord(tabY) };
            rung.rightTabEnd = NormalizedPoint { rung.rightEnd.x, NormalizedCoord(tabY) };

            layout.pitchLadder.rungs.push_back(rung);
        }
    } else {
        layout.pitchLadder.visible = false;
    }

    // 10. Flight Symbology: Compass Heading Tape
    if (config.showHeadingTape && !isMinimal) {
        layout.headingTape.visible = true;
        const double heading = wrap360(msg.platformHeadingDeg.value_or(0.0) + msg.sensorRelAzimuthDeg.value_or(0.0));
        layout.headingTape.currentHeadingDeg = static_cast<float>(heading);

        char hdgBuf[16];
        std::snprintf(hdgBuf, sizeof(hdgBuf), "%03.0f*", heading);
        layout.headingTape.digitalReadout = hdgBuf;

        constexpr float tapeY { 0.075F };
        constexpr float tapeLeft { 0.25F };
        constexpr float tapeRight { 0.75F };
        constexpr float tapeW { tapeRight - tapeLeft };

        layout.headingTape.tapeStart = NormalizedPoint { NormalizedCoord(tapeLeft), NormalizedCoord(tapeY) };
        layout.headingTape.tapeEnd = NormalizedPoint { NormalizedCoord(tapeRight), NormalizedCoord(tapeY) };
        layout.headingTape.caretTip = NormalizedPoint { NormalizedCoord(0.50F), NormalizedCoord(tapeY + 0.005F) };

        const double tapeSpan = (config.tapeSpanDegrees > 0.0F) ? static_cast<double>(config.tapeSpanDegrees) : 60.0;
        const float pxPerDeg = tapeW / static_cast<float>(tapeSpan);

        const int startDeg = static_cast<int>(std::floor(heading - tapeSpan / 2.0));
        const int endDeg = static_cast<int>(std::ceil(heading + tapeSpan / 2.0));

        for (int d = startDeg; d <= endDeg; ++d) {
            if (d % 5 != 0) {
                continue;
            }

            const float offsetDeg = static_cast<float>(d) - static_cast<float>(heading);
            const float tickX = 0.50F + offsetDeg * pxPerDeg;

            if (tickX < tapeLeft || tickX > tapeRight) {
                continue;
            }

            int wrapped = d % 360;
            if (wrapped < 0) {
                wrapped += 360;
            }

            const bool isMajor = (d % 10 == 0);
            const float tickLen = isMajor ? 0.016F : 0.008F;

            HeadingTapeTick tick {};
            tick.headingDeg = static_cast<float>(wrapped);
            tick.isMajor = isMajor;
            tick.bottom = NormalizedPoint { NormalizedCoord(tickX), NormalizedCoord(tapeY) };
            tick.top = NormalizedPoint { NormalizedCoord(tickX), NormalizedCoord(tapeY - tickLen) };

            if (isMajor) {
                if (wrapped == 0)
                    tick.label = "N";
                else if (wrapped == 45)
                    tick.label = "NE";
                else if (wrapped == 90)
                    tick.label = "E";
                else if (wrapped == 135)
                    tick.label = "SE";
                else if (wrapped == 180)
                    tick.label = "S";
                else if (wrapped == 225)
                    tick.label = "SW";
                else if (wrapped == 270)
                    tick.label = "W";
                else if (wrapped == 315)
                    tick.label = "NW";
                else {
                    char lblBuf[16];
                    std::snprintf(lblBuf, sizeof(lblBuf), "%02d", wrapped / 10);
                    tick.label = lblBuf;
                }
            }

            layout.headingTape.ticks.push_back(tick);
        }
    } else {
        layout.headingTape.visible = false;
    }

    return layout;
}

} // namespace Klv
