/**
 * @file ThermalFilters.h
 * @brief Video filters for thermal and infrared processing, false-color palettes, isotherm isolation, and hotspot radiometry.
 */

#pragma once

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

#include "DecoderTypes.h"
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#ifndef VIDEOFILTERS_API
#define VIDEOFILTERS_API
#endif

#if defined(PELCOD_HAS_FILTERS)

namespace PelcoD::Video::Filters {

/**
 * @enum FalseColorPalette
 * @brief Predefined thermal and false-color palettes inspired by Sightline IDD / EAN-Enhancement.
 */
enum class FalseColorPalette {
    WhiteHot, ///< Grayscale (white is hot, black is cold)
    BlackHot, ///< Inverted grayscale (black is hot, white is cold)
    Iron256, ///< Thermal Iron progression (black -> purple -> red -> yellow -> white)
    Jet, ///< Classic rainbow / jet spectrum
    Rainbow, ///< Multi-color rainbow spectrum
    HotCold, ///< Blue (cold) to Red (hot)
    IceFire, ///< Deep blue to vibrant red/orange saturation detector
    HotIron, ///< Hot iron color curve
    Turbo, ///< Google Turbo smooth colormap
    Bone, ///< Bone colormap with subtle blue/gray undertones
    UserPalette ///< Custom 256-entry user defined LUT
};

/**
 * @class FalseColorFilter
 * @brief Maps grayscale / thermal intensity to false-color palettes or custom 256-entry LUTs.
 */
class VIDEOFILTERS_API FalseColorFilter : public IFrameProcessor {
public:
    /**
     * @brief Constructor.
     * @param palette Desired false-color palette.
     */
    FalseColorFilter(FalseColorPalette palette = FalseColorPalette::Iron256);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setPalette(FalseColorPalette palette)
    {
        m_palette = palette;
    }
    FalseColorPalette getPalette() const
    {
        return m_palette;
    }

    /**
     * @brief Sets custom 256-entry RGB LUT (must contain 256 * 3 = 768 bytes).
     */
    void setUserPalette(const std::vector<std::uint8_t>& lut256x3);
    const std::vector<std::uint8_t>& getUserPalette() const
    {
        return m_userPalette;
    }

    /**
     * @brief Loads custom user palette from binary file (256x3 bytes RGB or YUV format).
     */
    bool loadUserPaletteFromFile(const std::string& filepath, bool isYuv = false);

    /**
     * @brief Saves current custom user palette to binary file.
     */
    bool saveUserPaletteToFile(const std::string& filepath, bool asYuv = false) const;

    /**
     * @brief Generates smoothly interpolated palette between specified control points.
     * @param controlPoints Map of intensity (0-255) to RGB triplet {R, G, B}.
     * @param smooth If true, applies smoothing across color bands.
     */
    void generateInterpolatedPalette(const std::map<std::uint8_t, std::vector<std::uint8_t>>& controlPoints, bool smooth = true);

private:
    FalseColorPalette m_palette;
    std::vector<std::uint8_t> m_userPalette; // 768 bytes (256 * RGB)
    void initDefaultUserPalette();
};

/**
 * @class IsothermFilter
 * @brief Isolates critical temperature/intensity bands with alert colors while rendering background in monochrome.
 */
class VIDEOFILTERS_API IsothermFilter : public IFrameProcessor {
public:
    enum class Preset {
        Custom, ///< Custom threshold range
        HumanBody, ///< Narrow band for personnel body heat (~140 to 180 in 8-bit luma)
        HighHeat ///< High intensity threshold for fire, engines, and muzzle flashes (> 200)
    };

    enum class HighlightColor {
        Red, ///< Tactical Alert Red (RGB 255, 0, 0)
        Amber, ///< High-Vis Amber (RGB 255, 191, 0)
        Cyan, ///< Electric Cyan (RGB 0, 255, 255)
        Iron256 ///< Thermal colormap slice
    };

    IsothermFilter(int lowThreshold = 140, int highThreshold = 180, HighlightColor color = HighlightColor::Red,
        bool whiteHotBackground = true);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setPreset(Preset preset);
    Preset getPreset() const
    {
        return m_preset;
    }

    void setThresholds(int low, int high);
    int getLowThreshold() const
    {
        return m_lowThreshold;
    }
    int getHighThreshold() const
    {
        return m_highThreshold;
    }

    void setHighlightColor(HighlightColor color)
    {
        m_color = color;
    }
    HighlightColor getHighlightColor() const
    {
        return m_color;
    }

    void setWhiteHotBackground(bool whiteHot)
    {
        m_whiteHotBackground = whiteHot;
    }
    bool isWhiteHotBackground() const
    {
        return m_whiteHotBackground;
    }

private:
    Preset m_preset;
    int m_lowThreshold;
    int m_highThreshold;
    HighlightColor m_color;
    bool m_whiteHotBackground;
};

/**
 * @class HotspotTrackerFilter
 * @brief Automatically locates, tracks, and annotates peak thermal hot and cold spots with optical bore radiometry.
 */
class VIDEOFILTERS_API HotspotTrackerFilter : public IFrameProcessor {
public:
    struct RadiometryStats {
        int hotX { 0 };
        int hotY { 0 };
        std::uint8_t hotVal { 0 };
        int coldX { 0 };
        int coldY { 0 };
        std::uint8_t coldVal { 0 };
        std::uint8_t centerMean { 0 };
    };

    HotspotTrackerFilter(bool showOverlay = true, int centerBoxSize = 32);

    void process(std::uint8_t* data, int width, int height, PixelFormat format) override;

    void setShowOverlay(bool show)
    {
        m_showOverlay = show;
    }
    bool getShowOverlay() const
    {
        return m_showOverlay;
    }

    void setCenterBoxSize(int size)
    {
        m_centerBoxSize = size;
    }
    int getCenterBoxSize() const
    {
        return m_centerBoxSize;
    }

    RadiometryStats getStats() const;

private:
    bool m_showOverlay;
    int m_centerBoxSize;
    mutable std::mutex m_statsMutex;
    RadiometryStats m_stats;
};

} // namespace PelcoD::Video::Filters

namespace PelcoD::Video {
using Filters::FalseColorFilter;
using Filters::FalseColorPalette;
using Filters::HotspotTrackerFilter;
using Filters::IsothermFilter;
} // namespace PelcoD::Video

#endif // PELCOD_HAS_FILTERS

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
