/// @file SpectrogramColorMap.cpp
/// @brief Implementation of high-contrast spectral colormaps (Inferno, Viridis, Tactical Green, Jet).

#include "SpectrogramColorMap.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Math {

namespace {

struct ColorStop {
    double position { 0.0 };
    RgbColor color {};
};

RgbColor interpolateStops(double val, const ColorStop* stops, std::size_t count) noexcept
{
    const double t = std::clamp(val, 0.0, 1.0);

    if (t <= stops[0].position) {
        return stops[0].color;
    }
    if (t >= stops[count - 1].position) {
        return stops[count - 1].color;
    }

    for (std::size_t i = 0; i < count - 1; ++i) {
        if (t >= stops[i].position && t <= stops[i + 1].position) {
            const double span = stops[i + 1].position - stops[i].position;
            const double alpha = (span > 1e-6) ? ((t - stops[i].position) / span) : 0.0;

            const double r = (1.0 - alpha) * stops[i].color.r + alpha * stops[i + 1].color.r;
            const double g = (1.0 - alpha) * stops[i].color.g + alpha * stops[i + 1].color.g;
            const double b = (1.0 - alpha) * stops[i].color.b + alpha * stops[i + 1].color.b;

            return RgbColor {
                static_cast<std::uint8_t>(std::clamp(std::round(r), 0.0, 255.0)),
                static_cast<std::uint8_t>(std::clamp(std::round(g), 0.0, 255.0)),
                static_cast<std::uint8_t>(std::clamp(std::round(b), 0.0, 255.0))
            };
        }
    }

    return stops[count - 1].color;
}

constexpr std::array<ColorStop, 6> INFERNO_STOPS { {
    { 0.00, { 0, 0, 4 } },
    { 0.20, { 40, 11, 84 } },
    { 0.40, { 101, 21, 110 } },
    { 0.60, { 186, 54, 85 } },
    { 0.80, { 249, 140, 10 } },
    { 1.00, { 252, 255, 164 } }
} };

constexpr std::array<ColorStop, 5> VIRIDIS_STOPS { {
    { 0.00, { 68, 1, 84 } },
    { 0.25, { 59, 82, 139 } },
    { 0.50, { 33, 145, 140 } },
    { 0.75, { 94, 201, 98 } },
    { 1.00, { 253, 231, 37 } }
} };

constexpr std::array<ColorStop, 5> TACTICAL_GREEN_STOPS { {
    { 0.00, { 0, 0, 0 } },
    { 0.30, { 0, 45, 15 } },
    { 0.60, { 0, 140, 50 } },
    { 0.85, { 50, 220, 110 } },
    { 1.00, { 200, 255, 230 } }
} };

constexpr std::array<ColorStop, 5> JET_STOPS { {
    { 0.00, { 0, 0, 140 } },
    { 0.25, { 0, 140, 255 } },
    { 0.50, { 0, 255, 140 } },
    { 0.75, { 255, 220, 0 } },
    { 1.00, { 255, 0, 0 } }
} };

} // namespace

RgbColor SpectrogramColorMap::mapNormalized(double value, Preset preset) noexcept
{
    switch (preset) {
    case Preset::Inferno:
        return interpolateStops(value, INFERNO_STOPS.data(), INFERNO_STOPS.size());
    case Preset::Viridis:
        return interpolateStops(value, VIRIDIS_STOPS.data(), VIRIDIS_STOPS.size());
    case Preset::TacticalGreen:
        return interpolateStops(value, TACTICAL_GREEN_STOPS.data(), TACTICAL_GREEN_STOPS.size());
    case Preset::Jet:
        return interpolateStops(value, JET_STOPS.data(), JET_STOPS.size());
    }
    return INFERNO_STOPS[0].color;
}

RgbColor SpectrogramColorMap::mapDb(double db, double minDb, double maxDb, Preset preset) noexcept
{
    const double span = maxDb - minDb;
    if (span <= 1e-6) {
        return mapNormalized(0.0, preset);
    }
    const double norm = std::clamp((db - minDb) / span, 0.0, 1.0);
    return mapNormalized(norm, preset);
}

std::string SpectrogramColorMap::mapHalfBlockAnsi(double normTop, double normBottom, Preset preset)
{
    const auto top = mapNormalized(normTop, preset);
    const auto bot = mapNormalized(normBottom, preset);

    std::string out;
    out.reserve(48);

    // Foreground color (top half)
    out += "\x1b[38;2;";
    out += std::to_string(top.r);
    out += ";";
    out += std::to_string(top.g);
    out += ";";
    out += std::to_string(top.b);
    out += "m";

    // Background color (bottom half)
    out += "\x1b[48;2;";
    out += std::to_string(bot.r);
    out += ";";
    out += std::to_string(bot.g);
    out += ";";
    out += std::to_string(bot.b);
    out += "m";

    // Unicode Upper Half Block ▀ (U+2580: UTF-8 0xE2 0x96 0x80)
    out += "\xE2\x96\x80";

    // Reset attributes
    out += "\x1b[0m";

    return out;
}

} // namespace Math
