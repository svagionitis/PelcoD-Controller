/// @file FujinonParser.cpp
/// @brief Implementation of response decoders for Fujinon SX800 / SX801 frames.

#include "FujinonParser.h"
#include "FujinonBuilder.h"

#include <iomanip>
#include <sstream>

namespace PelcoD {

bool FujinonParser::isFujinonResponse(const std::vector<std::uint8_t>& frame) noexcept
{
    if (frame.size() == PelcoDFrame::StandardFrameSize) {
        if (frame[0] != PelcoDFrame::SyncByte) {
            return false;
        }
        // Focus (0x81), Zoom (0x83), FW (0x8B), Lens Status (0x8D), Manual (0x8F), Zoom std (0x5D), RTC (0x3B),
        // or Original Resp (0xF0, 0xF1)
        return (frame[2] == 0x00U
                   && (frame[3] == 0x81U || frame[3] == 0x83U || frame[3] == 0x8BU || frame[3] == 0x8DU
                       || frame[3] == 0x8FU || frame[3] == 0x5DU || frame[3] == 0x3BU))
            || (frame[2] == 0xF0U) || (frame[2] == 0xF1U);
    }

    if (frame.size() == PelcoDFrame::QueryResponseSize) {
        return frame[0] == PelcoDFrame::SyncByte;
    }

    return false;
}

bool FujinonParser::parseQueryFocus(const std::vector<std::uint8_t>& frame, std::uint16_t& focusPos) noexcept
{
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
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
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
        return false;
    }
    if (frame[2] == 0x00U && frame[3] == 0x83U) {
        zoomPos = static_cast<std::uint16_t>((static_cast<std::uint16_t>(frame[4]) << 8U) | frame[5]);
        return true;
    }
    return false;
}

bool FujinonParser::parseQuerySerialNumber(const std::vector<std::uint8_t>& frame, std::string& serialStr) noexcept
{
    if (frame.size() != PelcoDFrame::QueryResponseSize || frame[0] != PelcoDFrame::SyncByte) {
        return false;
    }

    // Bytes 2 to 9 contain ASCII characters of the serial number
    serialStr.clear();
    for (std::size_t i = 2U; i <= 9U && i < frame.size(); ++i) {
        if (frame[i] >= 0x20U && frame[i] <= 0x7EU) {
            serialStr.push_back(static_cast<char>(frame[i]));
        } else {
            serialStr.clear();
            return false;
        }
    }
    return serialStr.size() == 8U;
}

bool FujinonParser::parseQueryFirmwareVersion(const std::vector<std::uint8_t>& frame, std::string& fwStr) noexcept
{
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
        return false;
    }
    if (frame[2] == 0x00U && frame[3] == 0x8BU) {
        std::ostringstream oss;
        oss << "v" << static_cast<int>(frame[4]) << "." << std::setw(2) << std::setfill('0') << std::hex
            << static_cast<int>(frame[5]);
        fwStr = oss.str();
        return true;
    }
    return false;
}

bool FujinonParser::parseQueryLensStatus(const std::vector<std::uint8_t>& frame, std::uint8_t& statusVal) noexcept
{
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
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
    if (frame.size() == PelcoDFrame::StandardFrameSize && frame[0] == PelcoDFrame::SyncByte) {
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
            case 0x0FU: // Day/Night Mode
                settings.dayNightMode = static_cast<FujinonDayNightMode>(val);
                return true;
            case 0x11U: // IR Wavelength
                settings.irWavelength = static_cast<FujinonIRWavelength>(val);
                return true;
            case 0x13U: // OIS Mode
                settings.oisMode = static_cast<FujinonOISMode>(val);
                return true;
            default:
                break;
            }
        }
    } else if (frame.size() == PelcoDFrame::QueryResponseSize && frame[0] == PelcoDFrame::SyncByte) {
        // 18-byte Query Photo Setting response format
        settings.afArea = frame[2];
        settings.afSensitivity = frame[3];
        settings.dayNightMode = static_cast<FujinonDayNightMode>(frame[4]);
        settings.irWavelength = static_cast<FujinonIRWavelength>(frame[5]);
        settings.oisMode = static_cast<FujinonOISMode>(frame[6]);
        return true;
    }

    return false;
}

bool FujinonParser::parseImageQualitySettings(
    const std::vector<std::uint8_t>& frame, FujinonImageQualitySettings& settings) noexcept
{
    if (frame.size() == PelcoDFrame::StandardFrameSize && frame[0] == PelcoDFrame::SyncByte) {
        if (frame[2] == 0xF0U && frame[3] == 0x2BU) {
            const auto target = frame[4];
            const auto val = frame[5];
            switch (target) {
            case 0x21U: // VLC Filter
                settings.vlcFilter = (val == 0x01U);
                return true;
            case 0x23U: // WDR
                settings.wdr = static_cast<FujinonWDRLevel>(val);
                return true;
            case 0x27U: // De-Heat Haze
                settings.heatHaze = static_cast<FujinonHeatHazeLevel>(val);
                return true;
            case 0x29U: // Defog
                settings.defog = static_cast<FujinonDefogLevel>(val);
                return true;
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
            case 0x37U: // Digital Zoom
                settings.digitalZoom = static_cast<FujinonDigitalZoom>(val);
                return true;
            case 0x39U: // NR
                settings.noiseReduction = val;
                return true;
            default:
                break;
            }
        }
    } else if (frame.size() == PelcoDFrame::QueryResponseSize && frame[0] == PelcoDFrame::SyncByte) {
        // 18-byte Query Image Quality response format
        settings.vlcFilter = (frame[2] == 0x01U);
        settings.wdr = static_cast<FujinonWDRLevel>(frame[3]);
        settings.heatHaze = static_cast<FujinonHeatHazeLevel>(frame[4]);
        settings.defog = static_cast<FujinonDefogLevel>(frame[5]);
        settings.brightness = frame[6];
        settings.contrast = frame[7];
        settings.saturation = frame[8];
        settings.sharpness = frame[9];
        settings.digitalZoom = static_cast<FujinonDigitalZoom>(frame[10]);
        settings.noiseReduction = frame[11];
        return true;
    }

    return false;
}

bool FujinonParser::parseManualSettings(
    const std::vector<std::uint8_t>& frame, FujinonManualSettings& settings) noexcept
{
    if (frame.size() == PelcoDFrame::StandardFrameSize && frame[0] == PelcoDFrame::SyncByte) {
        if (frame[2] == 0x00U && frame[3] == 0x8FU) {
            settings.manualIris = frame[4];
            settings.manualShutter = frame[5];
            return true;
        }
    } else if (frame.size() == PelcoDFrame::QueryResponseSize && frame[0] == PelcoDFrame::SyncByte) {
        settings.manualIris = frame[2];
        settings.manualShutter = frame[3];
        settings.manualISO = frame[4];
        return true;
    }

    return false;
}

bool FujinonParser::parseQueryZoomStandard(const std::vector<std::uint8_t>& frame, std::uint16_t& zoomPos) noexcept
{
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
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
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
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
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
        return false;
    }
    if (frame[2] == 0xF1U && frame[3] == 0x1FU) {
        const auto opcode
            = (frame[4] != 0x00U && target == 0x00U && frame[4] != 0x09U && frame[4] != 0x0BU) ? frame[4] : target;
        const auto data1 = frame[4];
        const auto data2 = frame[5];
        switch (opcode) {
        case 0x01U:
            settings.mode = static_cast<FujinonDayNightModeEx>(data2);
            return true;
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
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
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
        case 0x37U:
            settings.digitalZoomMode = static_cast<FujinonDigitalZoomMode>(val);
            return true;
        default:
            return true;
        }
    }
    return false;
}

bool FujinonParser::parseQueryRTC(
    const std::vector<std::uint8_t>& frame, std::uint8_t& data1, std::uint8_t& data2) noexcept
{
    if (frame.size() != PelcoDFrame::StandardFrameSize || frame[0] != PelcoDFrame::SyncByte) {
        return false;
    }
    if (frame[2] == 0x00U && frame[3] == 0x3BU) {
        data1 = frame[4];
        data2 = frame[5];
        return true;
    }
    return false;
}

bool FujinonParser::updateFujinonStatus(const std::vector<std::uint8_t>& frame, FujinonStatus& status) noexcept
{
    if (frame.size() == PelcoDFrame::GeneralResponseSize) {
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

    if (parseFineImageSettings(frame, status.fineImageSettings)) {
        return true;
    }

    if (parseDayNightExSettings(frame, status.dayNightExSettings)) {
        return true;
    }

    if (parseZoomFocusExSettings(frame, status.zoomFocusExSettings)) {
        return true;
    }

    // OSD / Operation frames
    if (frame.size() == PelcoDFrame::StandardFrameSize && frame[0] == PelcoDFrame::SyncByte) {
        if (frame[2] == 0xF0U) {
            switch (frame[3]) {
            case 0x43U: // DayTime display
                status.timeDisplay = (frame[5] == 0x01U);
                return true;
            case 0x45U: // DayTime position
                status.timePosition = static_cast<FujinonOSDPosition>(frame[5]);
                return true;
            case 0x47U: // Title display
                status.titleDisplay = (frame[5] == 0x01U);
                return true;
            case 0x4BU: // Title position
                status.titlePosition = static_cast<FujinonOSDPosition>(frame[5]);
                return true;
            case 0x4DU: // ID display
                status.idDisplay = (frame[5] == 0x01U);
                return true;
            case 0x4FU: // ID position
                status.idPosition = static_cast<FujinonOSDPosition>(frame[5]);
                return true;
            case 0x51U: // Reticle display
                status.reticleDisplay = (frame[5] == 0x01U);
                return true;
            case 0x55U: // Anti-aliasing
                status.antialiasing = (frame[5] == 0x01U);
                return true;
            case 0x73U: // RS485 termination
                status.rs485Termination = (frame[5] == 0x01U);
                return true;
            case 0x85U: // Language
                status.language = static_cast<FujinonLanguage>(frame[5]);
                return true;
            default:
                break;
            }
        }
    }

    return false;
}

} // namespace PelcoD
