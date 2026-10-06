/// @file DjiTelemetrySource.cpp
/// @brief Implementation of the DJI MP4 telemetry facade.

#include "DjiTelemetrySource.h"

#include "DjiSt0601Mapper.h"
#include "DjiSubtitleParser.h"
#include "Mp4TextTrackReader.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <string>
#include <utility>

namespace Dji {

bool isMp4File(std::string_view path)
{
    std::ifstream f(std::string(path), std::ios::binary);
    if (!f.is_open()) {
        return false;
    }
    std::array<char, 8U> hdr {};
    f.read(hdr.data(), static_cast<std::streamsize>(hdr.size()));
    if (f.gcount() != static_cast<std::streamsize>(hdr.size())) {
        return false;
    }
    return (hdr[4] == 'f') && (hdr[5] == 't') && (hdr[6] == 'y') && (hdr[7] == 'p');
}

bool loadDjiTrack(std::string_view path, std::vector<TimedTelemetry>& out, const DjiLoadOptions& opts)
{
    out.clear();
    Mp4TextTrackReader reader {};
    if (!reader.open(path)) {
        return false;
    }

    std::vector<std::pair<double, DjiTelemetrySample>> parsed {};
    parsed.reserve(reader.sampleCount());
    for (std::size_t i { 0U }; i < reader.sampleCount(); ++i) {
        const auto text { reader.readText(i) };
        if (!text.has_value()) {
            continue;
        }
        auto sample { parseDjiText(*text) };
        if (sample.has_value()) {
            parsed.emplace_back(reader.samples()[i].timeSec, std::move(*sample));
        }
    }
    if (parsed.empty()) {
        return false;
    }

    DjiMapConfig cfg {};
    cfg.absAltIsHae = opts.absAltIsHae;
    cfg.deriveFov = opts.deriveFov && reader.videoAspect().has_value();
    cfg.sensorAspect = reader.videoAspect().value_or(0.0);
    cfg.platform = reader.encoderName();

    if (opts.utcOffsetMin.has_value()) {
        cfg.utcOffsetMin = *opts.utcOffsetMin;
    } else {
        const auto firstWithTime { std::find_if(
            parsed.cbegin(), parsed.cend(), [](const auto& p) { return p.second.localTimeUs.has_value(); }) };
        const auto creation { reader.creationUtcUs() };
        if ((firstWithTime != parsed.cend()) && creation.has_value()) {
            cfg.utcOffsetMin = deriveUtcOffset(*firstWithTime->second.localTimeUs, *creation).value_or(0);
        }
    }

    out.reserve(parsed.size());
    for (const auto& p : parsed) {
        out.push_back(TimedTelemetry { p.first, mapToSt0601(p.second, cfg) });
    }
    std::stable_sort(
        out.begin(), out.end(), [](const TimedTelemetry& a, const TimedTelemetry& b) { return a.timeSec < b.timeSec; });
    return true;
}

} // namespace Dji
