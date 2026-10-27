/// @file SpectrogramColorMap.cpp
/// @brief Implementation of high-contrast spectral colormaps (Inferno, Viridis, Tactical Green, Jet).

#include "SpectrogramColorMap.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <string>

namespace Math {

namespace {

    struct ColorStop {
        double position { 0.0 };
        RgbColor color {};
    };

    RgbColor interpolateStops(double val, const ColorStop* stops, std::size_t count) noexcept
    {
        if (stops == nullptr || count == 0U) {
            return RgbColor {};
        }

        const double t = std::clamp(val, 0.0, 1.0);

        if (t <= stops[0].position) {
            return stops[0].color;
        }
        if (t >= stops[count - 1U].position) {
            return stops[count - 1U].color;
        }

        for (std::size_t i = 0U; i < count - 1U; ++i) {
            if (t >= stops[i].position && t <= stops[i + 1U].position) {
                const double span = stops[i + 1U].position - stops[i].position;
                const double alpha = (span > 1e-6) ? ((t - stops[i].position) / span) : 0.0;

                const double r = (1.0 - alpha) * static_cast<double>(stops[i].color.r)
                    + alpha * static_cast<double>(stops[i + 1U].color.r);
                const double g = (1.0 - alpha) * static_cast<double>(stops[i].color.g)
                    + alpha * static_cast<double>(stops[i + 1U].color.g);
                const double b = (1.0 - alpha) * static_cast<double>(stops[i].color.b)
                    + alpha * static_cast<double>(stops[i + 1U].color.b);

                return RgbColor { static_cast<std::uint8_t>(std::clamp(std::round(r), 0.0, 255.0)),
                    static_cast<std::uint8_t>(std::clamp(std::round(g), 0.0, 255.0)),
                    static_cast<std::uint8_t>(std::clamp(std::round(b), 0.0, 255.0)) };
            }
        }

        return stops[count - 1U].color;
    }

    constexpr std::array<ColorStop, 6U> INFERNO_STOPS { { { 0.00, { 0U, 0U, 4U } }, { 0.20, { 40U, 11U, 84U } },
        { 0.40, { 101U, 21U, 110U } }, { 0.60, { 186U, 54U, 85U } }, { 0.80, { 249U, 140U, 10U } },
        { 1.00, { 252U, 255U, 164U } } } };

    constexpr std::array<ColorStop, 5U> VIRIDIS_STOPS { { { 0.00, { 68U, 1U, 84U } }, { 0.25, { 59U, 82U, 139U } },
        { 0.50, { 33U, 145U, 140U } }, { 0.75, { 94U, 201U, 98U } }, { 1.00, { 253U, 231U, 37U } } } };

    constexpr std::array<ColorStop, 5U> TACTICAL_GREEN_STOPS { { { 0.00, { 0U, 0U, 0U } }, { 0.30, { 0U, 45U, 15U } },
        { 0.60, { 0U, 140U, 50U } }, { 0.85, { 50U, 220U, 110U } }, { 1.00, { 200U, 255U, 230U } } } };

    constexpr std::array<ColorStop, 5U> JET_STOPS { { { 0.00, { 0U, 0U, 140U } }, { 0.25, { 0U, 140U, 255U } },
        { 0.50, { 0U, 255U, 140U } }, { 0.75, { 255U, 220U, 0U } }, { 1.00, { 255U, 0U, 0U } } } };

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

    std::string out {};
    out.reserve(48U);

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