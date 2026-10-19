/// @file FujinonParser.cpp
/// @brief Implementation of response decoders for Fujinon SX800 / SX801 frames.

#include "FujinonParser.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <array>
#include <string>
#include <vector>

#include "FujinonBuilder.h"

namespace PelcoD {

bool FujinonParser::isValidResponse(const std::vector<std::uint8_t>& frame) noexcept
{
    if (frame.empty() || frame[0] != PelcoDFrame::SyncByte) {
        return false;
    }

    if (frame.size() == PelcoDFrame::StandardFrameSize) {
        return PelcoDFrame::isValidFrame(frame);
    }

    if (frame.size() == PelcoDFrame::QueryResponseSize) {
        const std::uint8_t expected = PelcoDFrame::calculateChecksum(&frame[1], 16U);
        return frame[17] == expected;
    }

    return false;
}

bool FujinonParser::isFujinonResponse(const std::vector<std::uint8_t>& frame) noexcept
{
    if (!isValidResponse(frame)) {
        return false;
    }

    if (frame.size() == PelcoDFrame::StandardFrameSize) {
        // Focus (0x81), Zoom (0x83), FW (0x8B), Lens Status (0x8D), Manual (0x8F), Zoom std (0x5D),
        // Shutter (0x97), ISO (0x9B), Iris (0x9D), RTC (0x3B), or Original Resp (0xF0, 0xF1)
        return (frame[2] == 0x00U
                   && (frame[3] == 0x81U || frame[3] == 0x83U || frame[3] == 0x8BU || frame[3] == 0x8DU
                       || frame[3] == 0x8FU || frame[3] == 0x5DU || frame[3] == 0x97U || frame[3] == 0x9BU
                       || frame[3] == 0x9DU))
            || (frame[3] == 0x3BU) || (frame[2] == 0xF0U) || (frame[2] == 0xF1U);
    }

    if (frame.size() == PelcoDFrame::QueryResponseSize) {
        return true;
    }

    return false;
}

bool FujinonParser::parseQueryFocus(const std::vector<std::uint8_t>& frame, std::uint16_t& focusPos) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0x00U && frame[3] == 0x81U) {
        focusPos = static_cast<std::uint16_t>((static_cast<std::uint16_t>(frame[4]) << 8U) | frame[5]);
        return true;
    }
    return false;
}

bool FujinonParser::parseQueryZoom(const std::vector<std::uint8_t>& frame, std::uint16_t& zoomPos) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0x00U && frame[3] == 0x83U) {
        zoomPos = static_cast<std::uint16_t>((static_cast<std::uint16_t>(frame[4]) << 8U) | frame[5]);
        return true;
    }
    return false;
}

bool FujinonParser::parseQuerySerialNumber(const std::vector<std::uint8_t>& frame, std::string& serialStr)
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::QueryResponseSize) {
        return false;
    }

    // Bytes 2 to 9 contain ASCII characters of the serial number
    serialStr.clear();
    for (std::size_t i { 2U }; i <= 9U && i < frame.size(); ++i) {
        if (frame[i] >= 0x20U && frame[i] <= 0x7EU) {
            serialStr.push_back(static_cast<char>(frame[i]));
        } else {
            serialStr.clear();
            return false;
        }
    }
    return serialStr.size() == 8U;
}

bool FujinonParser::parseQueryFirmwareVersion(const std::vector<std::uint8_t>& frame, std::string& fwStr)
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0x00U && frame[3] == 0x8BU) {
        std::array<char, 16> buf {};
        const int written = std::snprintf(buf.data(), buf.size(), "v%u.%02x", static_cast<unsigned int>(frame[4]),
            static_cast<unsigned int>(frame[5]));
        if (written > 0 && static_cast<std::size_t>(written) < buf.size()) {
            fwStr.assign(buf.data(), static_cast<std::size_t>(written));
            return true;
        }
    }
    return false;
}

bool FujinonParser::parseQueryLensStatus(const std::vector<std::uint8_t>& frame, std::uint8_t& statusVal) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0x00U && frame[3] == 0x8DU) {
        statusVal = frame[4];
        return true;
    }
    return false;
}

bool FujinonParser::parsePhotoSettings(const std::vector<std::uint8_t>& frame, FujinonPhotoSettings& settings) noexcept
{
    if (!isValidResponse(frame)) {
        return false;
    }

    if (frame.size() == PelcoDFrame::StandardFrameSize) {
        if (frame[2] == 0xF0U && frame[3] == 0x1FU) {
            const auto target = frame[4];
            const auto val = frame[5];
            switch (target) {
            case 0x03U: // AF Area
                settings.afArea = val;
                return true;
            case 0x05U: // AF Sensitivity
                settings.afSensitivity = val;
                return true;
            case 0x0FU: { // Day/Night Mode
                const auto mode = toFujinonDayNightMode(val);
                if (!mode) {
                    return false;
                }
                settings.dayNightMode = *mode;
                return true;
            }
            case 0x11U: { // IR Wavelength
                const auto wave = toFujinonIRWavelength(val);
                if (!wave) {
                    return false;
                }
                settings.irWavelength = *wave;
                return true;
            }
            case 0x13U: { // OIS Mode
                const auto ois = toFujinonOISMode(val);
                if (!ois) {
                    return false;
                }
                settings.oisMode = *ois;
                return true;
            }
            default:
                break;
            }
        }
    } else if (frame.size() == PelcoDFrame::QueryResponseSize) {
        // 18-byte Query Photo Setting response format
        const auto mode = toFujinonDayNightMode(frame[4]);
        const auto wave = toFujinonIRWavelength(frame[5]);
        const auto ois = toFujinonOISMode(frame[6]);
        if (!mode || !wave || !ois) {
            return false;
        }
        settings.afArea = frame[2];
        settings.afSensitivity = frame[3];
        settings.dayNightMode = *mode;
        settings.irWavelength = *wave;
        settings.oisMode = *ois;
        return true;
    }

    return false;
}

bool FujinonParser::parseImageQualitySettings(
    const std::vector<std::uint8_t>& frame, FujinonImageQualitySettings& settings) noexcept
{
    if (!isValidResponse(frame)) {
        return false;
    }

    if (frame.size() == PelcoDFrame::StandardFrameSize) {
        if (frame[2] == 0xF0U && frame[3] == 0x2BU) {
            const auto target = frame[4];
            const auto val = frame[5];
            switch (target) {
            case 0x21U: // VLC Filter
                settings.vlcFilter = (val == 0x01U);
                return true;
            case 0x23U: { // WDR
                const auto wdr = toFujinonWDRLevel(val);
                if (!wdr) {
                    return false;
                }
                settings.wdr = *wdr;
                return true;
            }
            case 0x27U: { // De-Heat Haze
                const auto haze = toFujinonHeatHazeLevel(val);
                if (!haze) {
                    return false;
                }
                settings.heatHaze = *haze;
                return true;
            }
            case 0x29U: { // Defog
                const auto defog = toFujinonDefogLevel(val);
                if (!defog) {
                    return false;
                }
                settings.defog = *defog;
                return true;
            }
            case 0x2BU: // Brightness
                settings.brightness = val;
                return true;
            case 0x2DU: // Contrast
                settings.contrast = val;
                return true;
            case 0x2FU: // Saturation
                settings.saturation = val;
                return true;
            case 0x31U: // Sharpness
                settings.sharpness = val;
                return true;
            case 0x37U: { // Digital Zoom
                const auto dzoom = toFujinonDigitalZoom(val);
                if (!dzoom) {
                    return false;
                }
                settings.digitalZoom = *dzoom;
                return true;
            }
            case 0x39U: // NR
                settings.noiseReduction = val;
                return true;
            default:
                break;
            }
        }
    } else if (frame.size() == PelcoDFrame::QueryResponseSize) {
        // 18-byte Query Image Quality response format
        const auto wdr = toFujinonWDRLevel(frame[3]);
        const auto haze = toFujinonHeatHazeLevel(frame[4]);
        const auto defog = toFujinonDefogLevel(frame[5]);
        const auto dzoom = toFujinonDigitalZoom(frame[10]);
        if (!wdr || !haze || !defog || !dzoom) {
            return false;
        }
        settings.vlcFilter = (frame[2] == 0x01U);
        settings.wdr = *wdr;
        settings.heatHaze = *haze;
        settings.defog = *defog;
        settings.brightness = frame[6];
        settings.contrast = frame[7];
        settings.saturation = frame[8];
        settings.sharpness = frame[9];
        settings.digitalZoom = *dzoom;
        settings.noiseReduction = frame[11];
        return true;
    }

    return false;
}

bool FujinonParser::parseManualSettings(
    const std::vector<std::uint8_t>& frame, FujinonManualSettings& settings) noexcept
{
    if (!isValidResponse(frame)) {
        return false;
    }

    if (frame.size() == PelcoDFrame::StandardFrameSize) {
        if (frame[2] == 0x00U && frame[3] == 0x8FU) {
            settings.manualIris = frame[4];
            settings.manualShutter = frame[5];
            return true;
        }
    } else if (frame.size() == PelcoDFrame::QueryResponseSize) {
        settings.manualIris = frame[2];
        settings.manualShutter = frame[3];
        settings.manualISO = frame[4];
        return true;
    }

    return false;
}

bool FujinonParser::parseQueryZoomStandard(const std::vector<std::uint8_t>& frame, std::uint16_t& zoomPos) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0x00U && frame[3] == 0x5DU) {
        zoomPos = static_cast<std::uint16_t>((static_cast<std::uint16_t>(frame[4]) << 8U) | frame[5]);
        return true;
    }
    return false;
}

bool FujinonParser::parseFineImageSettings(
    const std::vector<std::uint8_t>& frame, FujinonFineImageSettings& settings, std::uint8_t target) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0xF0U && frame[3] == 0xFFU) {
        const auto opcode = (frame[4] != 0x00U) ? frame[4] : target;
        const auto val = frame[5];
        switch (opcode) {
        case 0xEBU:
            settings.brightness = val;
            return true;
        case 0xEDU:
            settings.contrast = val;
            return true;
        case 0xEFU:
            settings.saturation = val;
            return true;
        case 0xF1U:
            settings.sharpness = val;
            return true;
        case 0xF5U:
            settings.wbShiftRed = val;
            return true;
        case 0xF7U:
            settings.wbShiftBlue = val;
            return true;
        default:
            return true;
        }
    }
    return false;
}

bool FujinonParser::parseDayNightExSettings(
    const std::vector<std::uint8_t>& frame, FujinonDayNightExSettings& settings, std::uint8_t target) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0xF1U && frame[3] == 0x1FU) {
        const auto opcode
            = (frame[4] != 0x00U && target == 0x00U && frame[4] != 0x09U && frame[4] != 0x0BU) ? frame[4] : target;
        const auto data1 = frame[4];
        const auto data2 = frame[5];
        switch (opcode) {
        case 0x01U: {
            const auto mode = toFujinonDayNightModeEx(data2);
            if (!mode) {
                return false;
            }
            settings.mode = *mode;
            return true;
        }
        case 0x03U:
            settings.dayToNightThreshold = data2;
            return true;
        case 0x05U:
            settings.nightToDayThreshold = data2;
            return true;
        case 0x07U:
            settings.autoDelaySeconds = data2;
            return true;
        case 0x09U:
            settings.dayStartHour = data1;
            settings.dayStartMinute = data2;
            return true;
        case 0x0BU:
            settings.nightStartHour = data1;
            settings.nightStartMinute = data2;
            return true;
        case 0x0DU:
            settings.opticalFilterDayIRPass = (data2 == 0x01U);
            return true;
        case 0x0FU:
            settings.opticalFilterNightIRPass = (data2 == 0x01U);
            return true;
        default:
            return true;
        }
    }
    return false;
}

bool FujinonParser::parseZoomFocusExSettings(
    const std::vector<std::uint8_t>& frame, FujinonZoomFocusExSettings& settings, std::uint8_t target) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0xF1U && frame[3] == 0x2FU) {
        const auto opcode = (frame[4] != 0x00U) ? frame[4] : target;
        const auto val = frame[5];
        switch (opcode) {
        case 0x25U:
            settings.zoomSpeedEx = val;
            return true;
        case 0x27U:
            settings.focusSpeedEx = val;
            return true;
        case 0x21U:
        case 0x37U: {
            const auto dmode = toFujinonDigitalZoomMode(val);
            if (!dmode) {
                return false;
            }
            settings.digitalZoomMode = *dmode;
            return true;
        }
        default:
            return true;
        }
    }
    return false;
}

bool FujinonParser::parseQueryRTC(
    const std::vector<std::uint8_t>& frame, std::uint8_t& data1, std::uint8_t& data2) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[3] == 0x3BU) {
        data1 = frame[4];
        data2 = frame[5];
        return true;
    }
    return false;
}

bool FujinonParser::parseQueryManualShutter(const std::vector<std::uint8_t>& frame, std::uint8_t& shutterVal) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0x00U && frame[3] == 0x97U) {
        shutterVal = frame[5];
        return true;
    }
    return false;
}

bool FujinonParser::parseQueryManualISO(const std::vector<std::uint8_t>& frame, std::uint8_t& isoVal) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0x00U && frame[3] == 0x9BU) {
        isoVal = frame[5];
        return true;
    }
    return false;
}

bool FujinonParser::parseQueryManualIris(const std::vector<std::uint8_t>& frame, std::uint8_t& irisVal) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0x00U && frame[3] == 0x9DU) {
        irisVal = frame[5];
        return true;
    }
    return false;
}

bool FujinonParser::parseQueryDisplay(
    const std::vector<std::uint8_t>& frame, std::uint8_t& target, std::uint8_t& settingVal) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0xF0U && frame[3] == 0x5FU) {
        target = frame[4];
        settingVal = frame[5];
        return true;
    }
    return false;
}

bool FujinonParser::parseQueryKeySetting(
    const std::vector<std::uint8_t>& frame, std::uint8_t& target, std::uint8_t& keyVal) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0xF0U && frame[3] == 0xAFU) {
        target = frame[4];
        keyVal = frame[5];
        return true;
    }
    return false;
}

bool FujinonParser::parseQueryMovieCount(const std::vector<std::uint8_t>& frame, std::uint16_t& count) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0xF0U && frame[3] == 0xB3U) {
        count = static_cast<std::uint16_t>((static_cast<std::uint16_t>(frame[4]) << 8U) | frame[5]);
        return true;
    }
    return false;
}

bool FujinonParser::parseQueryMovieYear(const std::vector<std::uint8_t>& frame, std::uint16_t& year) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0xF0U && frame[3] == 0xB7U) {
        year = static_cast<std::uint16_t>((static_cast<std::uint16_t>(frame[4]) << 8U) | frame[5]);
        return true;
    }
    return false;
}

bool FujinonParser::parseQueryMovieDate(
    const std::vector<std::uint8_t>& frame, std::uint8_t& month, std::uint8_t& day) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0xF0U && frame[3] == 0xBBU) {
        month = frame[4];
        day = frame[5];
        return true;
    }
    return false;
}

bool FujinonParser::parseQueryMovieTime(
    const std::vector<std::uint8_t>& frame, std::uint8_t& hour, std::uint8_t& min) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0xF0U && frame[3] == 0xBFU) {
        hour = frame[4];
        min = frame[5];
        return true;
    }
    return false;
}

bool FujinonParser::parseQueryLogData(
    const std::vector<std::uint8_t>& frame, std::uint8_t& data1, std::uint8_t& data2) noexcept
{
    if (!isValidResponse(frame) || frame.size() != PelcoDFrame::StandardFrameSize) {
        return false;
    }
    if (frame[2] == 0xF0U && frame[3] == 0xD3U) {
        data1 = frame[4];
        data2 = frame[5];
        return true;
    }
    return false;
}

bool FujinonParser::updateFujinonStatus(const std::vector<std::uint8_t>& frame, FujinonStatus& status)
{
    if (!isValidResponse(frame)) {
        return false;
    }

    std::uint16_t focusVal { 0U };
    if (parseQueryFocus(frame, focusVal)) {
        status.absoluteFocusPosition = focusVal;
        return true;
    }

    std::uint16_t zoomVal { 0U };
    if (parseQueryZoom(frame, zoomVal) || parseQueryZoomStandard(frame, zoomVal)) {
        status.absoluteZoomPosition = zoomVal;
        status.focalLengthMm = FujinonBuilder::rawZoomToFocalLengthMm(zoomVal, status.digitalZoom);
        return true;
    }

    std::string serial;
    if (parseQuerySerialNumber(frame, serial)) {
        status.serialNumber = serial;
        return true;
    }

    std::string fw;
    if (parseQueryFirmwareVersion(frame, fw)) {
        status.firmwareVersion = fw;
        return true;
    }

    std::uint8_t lensStat { 0U };
    if (parseQueryLensStatus(frame, lensStat)) {
        status.lensStatus = lensStat;
        return true;
    }

    FujinonPhotoSettings photo;
    if (parsePhotoSettings(frame, photo)) {
        status.afArea = photo.afArea;
        status.afSensitivity = photo.afSensitivity;
        status.dayNightMode = photo.dayNightMode;
        status.irWavelength = photo.irWavelength;
        status.oisMode = photo.oisMode;
        return true;
    }

    FujinonImageQualitySettings img;
    if (parseImageQualitySettings(frame, img)) {
        status.vlcFilter = img.vlcFilter;
        status.wdrLevel = img.wdr;
        status.heatHazeLevel = img.heatHaze;
        status.defogLevel = img.defog;
        status.brightness = img.brightness;
        status.contrast = img.contrast;
        status.saturation = img.saturation;
        status.sharpness = img.sharpness;
        status.digitalZoom = img.digitalZoom;
        status.noiseReduction = img.noiseReduction;
        status.focalLengthMm = FujinonBuilder::rawZoomToFocalLengthMm(status.absoluteZoomPosition, status.digitalZoom);
        return true;
    }

    FujinonManualSettings man;
    if (parseManualSettings(frame, man)) {
        status.manualIris = man.manualIris;
        status.manualShutter = man.manualShutter;
        status.manualISO = man.manualISO;
        return true;
    }

    std::uint8_t shutterVal { 0U };
    if (parseQueryManualShutter(frame, shutterVal)) {
        status.manualShutter = shutterVal;
        return true;
    }

    std::uint8_t isoVal { 0U };
    if (parseQueryManualISO(frame, isoVal)) {
        status.manualISO = isoVal;
        return true;
    }

    std::uint8_t irisVal { 0U };
    if (parseQueryManualIris(frame, irisVal)) {
        status.manualIris = irisVal;
        return true;
    }

    std::uint16_t movCnt { 0U };
    if (parseQueryMovieCount(frame, movCnt)) {
        status.movieCount = movCnt;
        return true;
    }

    std::uint16_t movYr { 0U };
    if (parseQueryMovieYear(frame, movYr)) {
        status.lastMovieYear = movYr;
        return true;
    }

    std::uint8_t movMonth { 0U };
    std::uint8_t movDay { 0U };
    if (parseQueryMovieDate(frame, movMonth, movDay)) {
        status.lastMovieMonth = movMonth;
        status.lastMovieDay = movDay;
        return true;
    }

    std::uint8_t movHr { 0U };
    std::uint8_t movMn { 0U };
    if (parseQueryMovieTime(frame, movHr, movMn)) {
        status.lastMovieHour = movHr;
        status.lastMovieMinute = movMn;
        return true;
    }

    if (parseFineImageSettings(frame, status.fineImageSettings)) {
        return true;
    }

    if (parseDayNightExSettings(frame, status.dayNightExSettings)) {
        return true;
    }

    if (parseZoomFocusExSettings(frame, status.zoomFocusExSettings)) {
        return true;
    }

    std::uint8_t rtcD1 { 0U };
    std::uint8_t rtcD2 { 0U };
    if (parseQueryRTC(frame, rtcD1, rtcD2)) {
        if (frame.size() == PelcoDFrame::StandardFrameSize) {
            const std::uint8_t subOp { frame[2] };
            switch (subOp) {
            case 0x01U: // Second
                status.baseStatus.deviceTime.second = rtcD2;
                return true;
            case 0x03U: // Hour / Minute
                status.baseStatus.deviceTime.hour = rtcD1;
                status.baseStatus.deviceTime.minute = rtcD2;
                return true;
            case 0x05U: // Month / Day
                status.baseStatus.deviceTime.month = rtcD1;
                status.baseStatus.deviceTime.day = rtcD2;
                return true;
            case 0x07U: // Year
                status.baseStatus.deviceTime.year
                    = static_cast<std::uint16_t>((static_cast<std::uint16_t>(rtcD1) << 8U) | rtcD2);
                return true;
            default: // Generic / 0x00
                status.baseStatus.deviceTime.hour = rtcD1;
                status.baseStatus.deviceTime.minute = rtcD2;
                return true;
            }
        }
        return true;
    }

    // OSD / Operation frames
    if (frame.size() == PelcoDFrame::StandardFrameSize) {
        if (frame[2] == 0x00U && frame[3] == 0x2BU) {
            const auto af = toFujinonAutoFocusMode(frame[5]);
            if (af) {
                status.autoFocusMode = *af;
                return true;
            }
            return false;
        }
        if (frame[2] == 0xF0U) {
            switch (frame[3]) {
            case 0x43U: // DayTime display
                status.timeDisplay = (frame[5] == 0x01U);
                return true;
            case 0x45U: { // DayTime position
                const auto pos = toFujinonOSDPosition(frame[5]);
                if (pos) {
                    status.timePosition = *pos;
                    return true;
                }
                return false;
            }
            case 0x47U: // Title display
                status.titleDisplay = (frame[5] == 0x01U);
                return true;
            case 0x4BU: { // Title position
                const auto pos = toFujinonOSDPosition(frame[5]);
                if (pos) {
                    status.titlePosition = *pos;
                    return true;
                }
                return false;
            }
            case 0x4DU: // ID display
                status.idDisplay = (frame[5] == 0x01U);
                return true;
            case 0x4FU: { // ID position
                const auto pos = toFujinonOSDPosition(frame[5]);
                if (pos) {
                    status.idPosition = *pos;
                    return true;
                }
                return false;
            }
            case 0x51U: // Reticle display
                status.reticleDisplay = (frame[5] == 0x01U);
                return true;
            case 0x5FU: { // Query Display Response
                const auto target = frame[4];
                const auto val = frame[5];
                switch (target) {
                case 0x43U:
                    status.timeDisplay = (val == 0x01U);
                    return true;
                case 0x45U: {
                    const auto pos = toFujinonOSDPosition(val);
                    if (pos) {
                        status.timePosition = *pos;
                        return true;
                    }
                    return false;
                }
                case 0x47U:
                    status.titleDisplay = (val == 0x01U);
                    return true;
                case 0x4BU: {
                    const auto pos = toFujinonOSDPosition(val);
                    if (pos) {
                        status.titlePosition = *pos;
                        return true;
                    }
                    return false;
                }
                case 0x4DU:
                    status.idDisplay = (val == 0x01U);
                    return true;
                case 0x4FU: {
                    const auto pos = toFujinonOSDPosition(val);
                    if (pos) {
                        status.idPosition = *pos;
                        return true;
                    }
                    return false;
                }
                case 0x51U:
                    status.reticleDisplay = (val == 0x01U);
                    return true;
                default:
                    return true;
                }
            }
            case 0x55U: // Anti-aliasing
                status.antialiasing = (frame[5] == 0x01U);
                return true;
            case 0x73U: // RS485 termination
                status.rs485Termination = (frame[5] == 0x01U);
                return true;
            case 0x85U: { // Language
                const auto lang = toFujinonLanguage(frame[5]);
                if (lang) {
                    status.language = *lang;
                    return true;
                }
                return false;
            }
            default:
                break;
            }
        }
    }

    return false;
}

} // namespace PelcoD
