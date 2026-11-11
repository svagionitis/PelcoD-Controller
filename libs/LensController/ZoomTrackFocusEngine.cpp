/// @file ZoomTrackFocusEngine.cpp
/// @brief Implementation of parfocal tracking engine, PCHIP spline, and LUT parser.

#include "ZoomTrackFocusEngine.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include "LensTypes.h"
#include "ZoomTrackTypes.h"

namespace LensControl {

namespace {

    [[nodiscard]] constexpr bool isAsciiDigit(char ch) noexcept
    {
        return ch >= '0' && ch <= '9';
    }

    [[nodiscard]] constexpr bool isAsciiSpace(char ch) noexcept
    {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    }

    std::string_view trimAscii(std::string_view str) noexcept
    {
        while (!str.empty() && isAsciiSpace(str.front())) {
            str.remove_prefix(1U);
        }
        while (!str.empty() && isAsciiSpace(str.back())) {
            str.remove_suffix(1U);
        }
        return str;
    }

    bool parseNumberList(
        std::string_view line, std::uint16_t* outValues, std::size_t expectedCount, std::size_t& actualCount) noexcept
    {
        actualCount = 0U;
        std::size_t i = 0U;
        const std::size_t len = line.size();

        while (i < len && actualCount < expectedCount) {
            while (i < len && (isAsciiSpace(line[i]) || line[i] == ',')) {
                ++i;
            }
            if (i >= len) {
                break;
            }
            if (!isAsciiDigit(line[i])) {
                return false;
            }

            std::uint32_t val = 0U;
            while (i < len && isAsciiDigit(line[i])) {
                val = (val * 10U) + static_cast<std::uint32_t>(line[i] - '0');
                if (val > 65535U) {
                    return false;
                }
                ++i;
            }

            outValues[actualCount] = static_cast<std::uint16_t>(val);
            ++actualCount;

            while (i < len && (isAsciiSpace(line[i]) || line[i] == ',')) {
                ++i;
            }
        }

        return actualCount == expectedCount;
    }

} // namespace

ZoomTrackFocusEngine::ZoomTrackFocusEngine() noexcept
{
    m_curveSet.curveCount = 0U;
    m_curveSet.activeIndex = 0U;
}

void ZoomTrackFocusEngine::setMode(InterpolationMode mode) noexcept
{
    m_mode = mode;
}

InterpolationMode ZoomTrackFocusEngine::mode() const noexcept
{
    return m_mode;
}

void ZoomTrackFocusEngine::setActiveCurve(std::size_t index) noexcept
{
    if (index < m_curveSet.curveCount) {
        m_curveSet.activeIndex = index;
    }
}

std::size_t ZoomTrackFocusEngine::activeCurve() const noexcept
{
    return m_curveSet.activeIndex;
}

std::size_t ZoomTrackFocusEngine::curveCount() const noexcept
{
    return m_curveSet.curveCount;
}

void ZoomTrackFocusEngine::clearCurves() noexcept
{
    m_curveSet.curveCount = 0U;
    m_curveSet.activeIndex = 0U;
}

bool ZoomTrackFocusEngine::addCurve(const TrackCurve& curve) noexcept
{
    if (!curve.isValid() || m_curveSet.curveCount >= kMaxCurves) {
        return false;
    }

    TrackCurve staged = curve;
    computePchip(staged);

    m_curveSet.curves[m_curveSet.curveCount] = staged;
    ++m_curveSet.curveCount;
    return true;
}

const TrackCurve* ZoomTrackFocusEngine::getCurve(std::size_t idx) const noexcept
{
    if (idx >= m_curveSet.curveCount) {
        return nullptr;
    }
    return &m_curveSet.curves[idx];
}

void ZoomTrackFocusEngine::computePchip(TrackCurve& curve) noexcept
{
    const std::size_t n = curve.knotCount;
    if (n < 2U) {
        return;
    }

    if (n == 2U) {
        const float h0
            = static_cast<float>(curve.knots[1].zoom.value()) - static_cast<float>(curve.knots[0].zoom.value());
        const float delta0 = (h0 > 0.0F)
            ? ((static_cast<float>(curve.knots[1].focus.value()) - static_cast<float>(curve.knots[0].focus.value()))
                  / h0)
            : 0.0F;
        curve.knots[0].slope = delta0;
        curve.knots[1].slope = delta0;
        return;
    }

    // Precompute interval widths and secant slopes
    float h[TrackCurve::kMaxKnots] { 0.0F };
    float delta[TrackCurve::kMaxKnots] { 0.0F };

    for (std::size_t i = 0U; i < n - 1U; ++i) {
        h[i] = static_cast<float>(curve.knots[i + 1U].zoom.value()) - static_cast<float>(curve.knots[i].zoom.value());
        if (h[i] > 0.0F) {
            delta[i] = (static_cast<float>(curve.knots[i + 1U].focus.value())
                           - static_cast<float>(curve.knots[i].focus.value()))
                / h[i];
        } else {
            delta[i] = 0.0F;
        }
    }

    // Compute interior slopes using Brodlie/Fritsch-Carlson weighted harmonic mean
    for (std::size_t i = 1U; i < n - 1U; ++i) {
        if ((delta[i - 1U] * delta[i]) <= 0.0F) {
            curve.knots[i].slope = 0.0F;
        } else {
            const float w1 = (2.0F * h[i]) + h[i - 1U];
            const float w2 = h[i] + (2.0F * h[i - 1U]);
            const float denom = (w1 / delta[i - 1U]) + (w2 / delta[i]);
            if (std::abs(denom) > 1e-7F) {
                curve.knots[i].slope = (w1 + w2) / denom;
            } else {
                curve.knots[i].slope = 0.0F;
            }
        }
    }

    // Endpoint 0
    float d0 = ((2.0F * h[0] + h[1]) * delta[0] - h[0] * delta[1]) / (h[0] + h[1]);
    if ((d0 * delta[0]) <= 0.0F) {
        d0 = 0.0F;
    } else if ((delta[0] * delta[1] < 0.0F) && (std::abs(d0) > std::abs(3.0F * delta[0]))) {
        d0 = 3.0F * delta[0];
    }
    curve.knots[0].slope = d0;

    // Endpoint n - 1
    const std::size_t nm1 = n - 1U;
    const std::size_t nm2 = n - 2U;
    const std::size_t nm3 = n - 3U;
    float dnm1 = ((2.0F * h[nm2] + h[nm3]) * delta[nm2] - h[nm2] * delta[nm3]) / (h[nm2] + h[nm3]);
    if ((dnm1 * delta[nm2]) <= 0.0F) {
        dnm1 = 0.0F;
    } else if ((delta[nm2] * delta[nm3] < 0.0F) && (std::abs(dnm1) > std::abs(3.0F * delta[nm2]))) {
        dnm1 = 3.0F * delta[nm2];
    }
    curve.knots[nm1].slope = dnm1;
}

MotorPosition ZoomTrackFocusEngine::evalPchip(const TrackCurve& curve, MotorPosition zoom) const noexcept
{
    const std::size_t n = curve.knotCount;
    if (n == 0U) {
        return MotorPosition { 0U };
    }
    if (n == 1U || zoom.value() <= curve.knots[0].zoom.value()) {
        return curve.knots[0].focus;
    }
    if (zoom.value() >= curve.knots[n - 1U].zoom.value()) {
        return curve.knots[n - 1U].focus;
    }

    // Binary search for bracket interval [k, k+1]
    std::size_t low = 0U;
    std::size_t high = n - 1U;
    while (low + 1U < high) {
        const std::size_t mid = low + ((high - low) / 2U);
        if (curve.knots[mid].zoom.value() <= zoom.value()) {
            low = mid;
        } else {
            high = mid;
        }
    }

    const std::size_t k = low;
    const float zk = static_cast<float>(curve.knots[k].zoom.value());
    const float zk1 = static_cast<float>(curve.knots[k + 1U].zoom.value());
    const float hk = zk1 - zk;
    if (hk <= 0.0F) {
        return curve.knots[k].focus;
    }

    const float z = static_cast<float>(zoom.value());
    const float t = (z - zk) / hk;
    const float t2 = t * t;
    const float t3 = t2 * t;

    const float h0 = (2.0F * t3) - (3.0F * t2) + 1.0F;
    const float h1 = t3 - (2.0F * t2) + t;
    const float h2 = (-2.0F * t3) + (3.0F * t2);
    const float h3 = t3 - t2;

    const float fk = static_cast<float>(curve.knots[k].focus.value());
    const float fk1 = static_cast<float>(curve.knots[k + 1U].focus.value());
    const float dk = curve.knots[k].slope;
    const float dk1 = curve.knots[k + 1U].slope;

    const float interpolated = (h0 * fk) + (h1 * hk * dk) + (h2 * fk1) + (h3 * hk * dk1);
    const float clamped = std::clamp(interpolated, 0.0F, 65535.0F);

    return MotorPosition { static_cast<std::uint16_t>(std::round(clamped)) };
}

MotorPosition ZoomTrackFocusEngine::evalLinear(const TrackCurve& curve, MotorPosition zoom) const noexcept
{
    const std::size_t n = curve.knotCount;
    if (n == 0U) {
        return MotorPosition { 0U };
    }
    if (n == 1U || zoom.value() <= curve.knots[0].zoom.value()) {
        return curve.knots[0].focus;
    }
    if (zoom.value() >= curve.knots[n - 1U].zoom.value()) {
        return curve.knots[n - 1U].focus;
    }

    std::size_t low = 0U;
    std::size_t high = n - 1U;
    while (low + 1U < high) {
        const std::size_t mid = low + ((high - low) / 2U);
        if (curve.knots[mid].zoom.value() <= zoom.value()) {
            low = mid;
        } else {
            high = mid;
        }
    }

    const std::size_t k = low;
    const float zk = static_cast<float>(curve.knots[k].zoom.value());
    const float zk1 = static_cast<float>(curve.knots[k + 1U].zoom.value());
    const float fk = static_cast<float>(curve.knots[k].focus.value());
    const float fk1 = static_cast<float>(curve.knots[k + 1U].focus.value());

    const float hk = zk1 - zk;
    if (hk <= 0.0F) {
        return curve.knots[k].focus;
    }

    const float z = static_cast<float>(zoom.value());
    const float t = (z - zk) / hk;
    const float interpolated = fk + t * (fk1 - fk);
    const float clamped = std::clamp(interpolated, 0.0F, 65535.0F);

    return MotorPosition { static_cast<std::uint16_t>(std::round(clamped)) };
}

MotorPosition ZoomTrackFocusEngine::evaluateFocus(MotorPosition zoom) const noexcept
{
    if (m_curveSet.curveCount == 0U) {
        return MotorPosition { 0U };
    }
    const TrackCurve& curve = m_curveSet.curves[m_curveSet.activeIndex];
    return (m_mode == InterpolationMode::PchipSpline) ? evalPchip(curve, zoom) : evalLinear(curve, zoom);
}

MotorPosition ZoomTrackFocusEngine::evaluateDistance(MotorPosition zoom, float distanceMeters) const noexcept
{
    if (m_curveSet.curveCount == 0U) {
        return MotorPosition { 0U };
    }
    if (m_curveSet.curveCount == 1U || distanceMeters <= 0.0F) {
        return evaluateFocus(zoom);
    }

    const float targetVergence = 1.0F / distanceMeters;

    // Find two bounding curves by vergence (where distance > 0 gives V = 1/D, and dist <= 0 gives V = 0)
    std::size_t lowerIdx = 0U;
    std::size_t upperIdx = 0U;
    float minVDiff = 1e9F;

    for (std::size_t i = 0U; i < m_curveSet.curveCount; ++i) {
        const float curV
            = (m_curveSet.curves[i].distanceMeters > 0.0F) ? (1.0F / m_curveSet.curves[i].distanceMeters) : 0.0F;
        const float diff = std::abs(curV - targetVergence);
        if (diff < minVDiff) {
            minVDiff = diff;
            lowerIdx = i;
        }
    }

    upperIdx = lowerIdx;
    for (std::size_t i = 0U; i < m_curveSet.curveCount; ++i) {
        const float curV
            = (m_curveSet.curves[i].distanceMeters > 0.0F) ? (1.0F / m_curveSet.curves[i].distanceMeters) : 0.0F;
        const float lowerV = (m_curveSet.curves[lowerIdx].distanceMeters > 0.0F)
            ? (1.0F / m_curveSet.curves[lowerIdx].distanceMeters)
            : 0.0F;

        if (curV > lowerV) {
            if (upperIdx == lowerIdx
                || curV < ((m_curveSet.curves[upperIdx].distanceMeters > 0.0F)
                           ? (1.0F / m_curveSet.curves[upperIdx].distanceMeters)
                           : 0.0F)) {
                upperIdx = i;
            }
        }
    }

    if (lowerIdx == upperIdx) {
        return (m_mode == InterpolationMode::PchipSpline) ? evalPchip(m_curveSet.curves[lowerIdx], zoom)
                                                          : evalLinear(m_curveSet.curves[lowerIdx], zoom);
    }

    const float vLower = (m_curveSet.curves[lowerIdx].distanceMeters > 0.0F)
        ? (1.0F / m_curveSet.curves[lowerIdx].distanceMeters)
        : 0.0F;
    const float vUpper = (m_curveSet.curves[upperIdx].distanceMeters > 0.0F)
        ? (1.0F / m_curveSet.curves[upperIdx].distanceMeters)
        : 0.0F;

    const float weight = std::clamp((targetVergence - vLower) / (vUpper - vLower), 0.0F, 1.0F);

    const float fLower = static_cast<float>(
        ((m_mode == InterpolationMode::PchipSpline) ? evalPchip(m_curveSet.curves[lowerIdx], zoom)
                                                    : evalLinear(m_curveSet.curves[lowerIdx], zoom))
            .value());

    const float fUpper = static_cast<float>(
        ((m_mode == InterpolationMode::PchipSpline) ? evalPchip(m_curveSet.curves[upperIdx], zoom)
                                                    : evalLinear(m_curveSet.curves[upperIdx], zoom))
            .value());

    const float blended = ((1.0F - weight) * fLower) + (weight * fUpper);
    const float clamped = std::clamp(blended, 0.0F, 65535.0F);

    return MotorPosition { static_cast<std::uint16_t>(std::round(clamped)) };
}

TrackCurveError ZoomTrackFocusEngine::parseString(std::string_view text) noexcept
{
    if (text.empty()) {
        return TrackCurveError::EmptyInput;
    }

    TrackCurveSet newSet { };
    std::size_t offset = 0U;
    const std::size_t totalLen = text.size();

    float pendingDistance = 0.0F;

    while (offset < totalLen) {
        // Read next line
        const std::size_t lineEnd = text.find_first_of("\r\n", offset);
        const std::size_t lineLen = (lineEnd == std::string_view::npos) ? (totalLen - offset) : (lineEnd - offset);
        std::string_view rawLine = text.substr(offset, lineLen);
        offset = (lineEnd == std::string_view::npos) ? totalLen : (lineEnd + 1U);
        if (offset < totalLen && text[offset - 1U] == '\r' && text[offset] == '\n') {
            ++offset;
        }

        std::string_view line = trimAscii(rawLine);
        if (line.empty()) {
            continue;
        }

        // Comment check & distance metadata extraction
        if (line.front() == '#') {
            constexpr std::string_view kDistTag = "# Distance:";
            const std::size_t tagPos = line.find(kDistTag);
            if (tagPos != std::string_view::npos) {
                std::string_view valStr = trimAscii(line.substr(tagPos + kDistTag.size()));
                if (valStr.find("Inf") != std::string_view::npos || valStr.find("inf") != std::string_view::npos) {
                    pendingDistance = 0.0F;
                } else {
                    float parsedDist = 0.0F;
                    std::size_t p = 0U;
                    while (p < valStr.size() && isAsciiDigit(valStr[p])) {
                        parsedDist = (parsedDist * 10.0F) + static_cast<float>(valStr[p] - '0');
                        ++p;
                    }
                    if (p < valStr.size() && valStr[p] == '.') {
                        ++p;
                        float fracScale = 0.1F;
                        while (p < valStr.size() && isAsciiDigit(valStr[p])) {
                            parsedDist += static_cast<float>(valStr[p] - '0') * fracScale;
                            fracScale *= 0.1F;
                            ++p;
                        }
                    }
                    pendingDistance = parsedDist;
                }
            }
            continue;
        }

        // Check for sample count header: "<N>:"
        const std::size_t colonPos = line.find(':');
        if (colonPos == std::string_view::npos) {
            continue;
        }

        std::string_view numPart = trimAscii(line.substr(0U, colonPos));
        if (numPart.empty()) {
            continue;
        }

        bool allDigits = true;
        std::uint32_t knotCount32 = 0U;
        for (char ch : numPart) {
            if (!isAsciiDigit(ch)) {
                allDigits = false;
                break;
            }
            knotCount32 = (knotCount32 * 10U) + static_cast<std::uint32_t>(ch - '0');
        }

        if (!allDigits || knotCount32 < 2U) {
            continue;
        }
        if (knotCount32 > kMaxKnots) {
            return TrackCurveError::CapacityExceeded;
        }
        if (newSet.curveCount >= kMaxCurves) {
            return TrackCurveError::CapacityExceeded;
        }

        const std::size_t knotCount = static_cast<std::size_t>(knotCount32);

        // Expect z: line
        std::string_view zLine { };
        while (offset < totalLen) {
            const std::size_t nextEnd = text.find_first_of("\r\n", offset);
            const std::size_t nextLen = (nextEnd == std::string_view::npos) ? (totalLen - offset) : (nextEnd - offset);
            std::string_view curLine = trimAscii(text.substr(offset, nextLen));
            offset = (nextEnd == std::string_view::npos) ? totalLen : (nextEnd + 1U);
            if (offset < totalLen && text[offset - 1U] == '\r' && text[offset] == '\n') {
                ++offset;
            }
            if (curLine.empty() || curLine.front() == '#') {
                continue;
            }
            zLine = curLine;
            break;
        }

        // Expect f: line
        std::string_view fLine { };
        while (offset < totalLen) {
            const std::size_t nextEnd = text.find_first_of("\r\n", offset);
            const std::size_t nextLen = (nextEnd == std::string_view::npos) ? (totalLen - offset) : (nextEnd - offset);
            std::string_view curLine = trimAscii(text.substr(offset, nextLen));
            offset = (nextEnd == std::string_view::npos) ? totalLen : (nextEnd + 1U);
            if (offset < totalLen && text[offset - 1U] == '\r' && text[offset] == '\n') {
                ++offset;
            }
            if (curLine.empty() || curLine.front() == '#') {
                continue;
            }
            fLine = curLine;
            break;
        }

        if (zLine.empty() || fLine.empty()) {
            return TrackCurveError::CountMismatch;
        }

        if (zLine.front() != 'z' && zLine.front() != 'Z') {
            return TrackCurveError::InvalidHeader;
        }
        if (fLine.front() != 'f' && fLine.front() != 'F') {
            return TrackCurveError::InvalidHeader;
        }

        const std::size_t zColon = zLine.find(':');
        const std::size_t fColon = fLine.find(':');
        if (zColon == std::string_view::npos || fColon == std::string_view::npos) {
            return TrackCurveError::InvalidHeader;
        }

        std::uint16_t zVals[kMaxKnots] { 0U };
        std::uint16_t fVals[kMaxKnots] { 0U };
        std::size_t zCount = 0U;
        std::size_t fCount = 0U;

        if (!parseNumberList(zLine.substr(zColon + 1U), zVals, knotCount, zCount) || zCount != knotCount) {
            return TrackCurveError::CountMismatch;
        }
        if (!parseNumberList(fLine.substr(fColon + 1U), fVals, knotCount, fCount) || fCount != knotCount) {
            return TrackCurveError::CountMismatch;
        }

        TrackCurve curve { };
        curve.knotCount = knotCount;
        curve.distanceMeters = pendingDistance;
        pendingDistance = 0.0F;

        for (std::size_t i = 0U; i < knotCount; ++i) {
            curve.knots[i].zoom = MotorPosition { zVals[i] };
            curve.knots[i].focus = MotorPosition { fVals[i] };
        }

        if (!curve.isValid()) {
            return TrackCurveError::NonMonotonicZoom;
        }

        computePchip(curve);
        newSet.curves[newSet.curveCount] = curve;
        ++newSet.curveCount;
    }

    if (newSet.curveCount == 0U) {
        return TrackCurveError::InsufficientKnots;
    }

    m_curveSet = newSet;
    return TrackCurveError::None;
}

TrackCurveError ZoomTrackFocusEngine::parseFile(std::string_view filePath) noexcept
{
    std::string pathStr { filePath };
    std::ifstream stream(pathStr.c_str(), std::ios::in | std::ios::binary);
    if (!stream.is_open()) {
        return TrackCurveError::FileNotFound;
    }

    std::ostringstream ss;
    ss << stream.rdbuf();
    std::string content = ss.str();

    return parseString(content);
}

std::string ZoomTrackFocusEngine::serializeString() const
{
    std::ostringstream ss;

    for (std::size_t c = 0U; c < m_curveSet.curveCount; ++c) {
        const TrackCurve& curve = m_curveSet.curves[c];
        if (curve.distanceMeters > 0.0F) {
            ss << "# Distance: " << curve.distanceMeters << "m\n";
        } else {
            ss << "# Distance: Infinity\n";
        }

        ss << curve.knotCount << ":\n";
        ss << "z: ";
        for (std::size_t i = 0U; i < curve.knotCount; ++i) {
            ss << curve.knots[i].zoom.value();
            if (i + 1U < curve.knotCount) {
                ss << ", ";
            }
        }
        ss << "\n";

        ss << "f: ";
        for (std::size_t i = 0U; i < curve.knotCount; ++i) {
            ss << curve.knots[i].focus.value();
            if (i + 1U < curve.knotCount) {
                ss << ", ";
            }
        }
        ss << "\n\n";
    }

    return ss.str();
}

bool ZoomTrackFocusEngine::serializeFile(std::string_view filePath) const noexcept
{
    std::string pathStr { filePath };
    std::ofstream stream(pathStr.c_str(), std::ios::out | std::ios::trunc);
    if (!stream.is_open()) {
        return false;
    }

    const std::string text = serializeString();
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return stream.good();
}

void ZoomTrackFocusEngine::startCalib(std::size_t knotCount, MotorPosition zMin, MotorPosition zMax) noexcept
{
    const std::size_t clampedCount = std::clamp(knotCount, static_cast<std::size_t>(2U), kMaxKnots);
    const std::uint16_t minVal = zMin.value();
    const std::uint16_t maxVal = std::max(zMin.value(), zMax.value());

    m_calibTotalKnots = clampedCount;
    m_calibCurrentKnot = 0U;
    m_calibZoomMin = MotorPosition { minVal };
    m_calibZoomMax = MotorPosition { maxVal };

    m_calibStagingCurve = TrackCurve { };
    m_calibStagingCurve.knotCount = clampedCount;
    m_calibStagingCurve.distanceMeters = 0.0F;

    for (std::size_t i = 0U; i < clampedCount; ++i) {
        const float fraction = static_cast<float>(i) / static_cast<float>(clampedCount - 1U);
        const float posF = static_cast<float>(minVal) + fraction * static_cast<float>(maxVal - minVal);
        const auto pos16 = static_cast<std::uint16_t>(std::round(posF));
        m_calibStagingCurve.knots[i].zoom = MotorPosition { pos16 };
        m_calibStagingCurve.knots[i].focus = MotorPosition { 0U };
    }

    m_isCalibrating = true;
}

bool ZoomTrackFocusEngine::isCalibrating() const noexcept
{
    return m_isCalibrating;
}

MotorPosition ZoomTrackFocusEngine::nextCalibZoom() const noexcept
{
    if (!m_isCalibrating || m_calibCurrentKnot >= m_calibTotalKnots) {
        return MotorPosition { 0U };
    }
    return m_calibStagingCurve.knots[m_calibCurrentKnot].zoom;
}

void ZoomTrackFocusEngine::recordCalibKnot(MotorPosition focusPos) noexcept
{
    if (!m_isCalibrating || m_calibCurrentKnot >= m_calibTotalKnots) {
        return;
    }

    m_calibStagingCurve.knots[m_calibCurrentKnot].focus = focusPos;
    ++m_calibCurrentKnot;

    if (m_calibCurrentKnot >= m_calibTotalKnots) {
        m_isCalibrating = false;
        static_cast<void>(addCurve(m_calibStagingCurve));
    }
}

void ZoomTrackFocusEngine::cancelCalib() noexcept
{
    m_isCalibrating = false;
    m_calibCurrentKnot = 0U;
    m_calibTotalKnots = 0U;
    m_calibStagingCurve = TrackCurve { };
}

} // namespace LensControl
